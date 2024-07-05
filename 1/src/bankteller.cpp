#include <iostream>
#include <fstream>

#include <queue>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <thread>
#include <unistd.h>
#include <vector>

const int TELLER_NUM = 4; // 柜员数量

const char *INPUT_FILENAME = "bankteller.in";   // 输入文件名
const char *OUTPUT_FILENAME = "bankteller.out"; // 输出文件名

time_t init_time = time(NULL); // 相对时间基准

/// @brief 信号量联合
union semun
{
    int val;               // Value for SETVAL
    semid_ds *buf;         // Buffer for IPC_STAT, IPC_SET
    unsigned short *array; // Array for GETALL, SETALL
};

/// @brief P 操作
/// @param semid
/// @param index
void P(int semid, int index)
{
    sembuf sem;             // 信号量操作结构
    sem.sem_num = index;    // 信号量编号
    sem.sem_op = -1;        // 信号量操作
    sem.sem_flg = SEM_UNDO; // 操作标志
    semop(semid, &sem, 1);  // 执行操作
}

/// @brief V 操作
/// @param semid
/// @param index
void V(int semid, int index)
{
    sembuf sem;             // 信号量操作结构
    sem.sem_num = index;    // 信号量编号
    sem.sem_op = 1;         // 信号量操作
    sem.sem_flg = SEM_UNDO; // 操作标志
    semop(semid, &sem, 1);  // 执行操作
}

/// @brief 顾客
struct Customer
{
    int id;              // 编号
    int arrive_time;     // 到达银行时间
    int service_time;    // 需要服务时间
    bool served = false; // 是否已服务
    int start_time = 0;  // 开始服务时间
    int leave_time = 0;  // 离开银行时间
    int teller = 0;      // 服务柜员编号
};

std::vector<Customer> customers;            // 顾客列表
std::queue<Customer *> customerQueue;       // 顾客队列
std::vector<int> tellers(TELLER_NUM, true); // 柜员状态，`true` 表示空闲
std::vector<std::thread> customerThreads;   // 顾客线程
std::vector<std::thread> tellerThreads;     // 柜员线程

/// @brief 是否所有顾客已服务
/// @return `true` 所有顾客已服务，`false` 尚有顾客未服务
bool allCustomersServed()
{
    for (Customer &customer : customers)
    {
        if (!customer.served)
        {
            return false;
        }
    }
    return true;
}

/// @brief 顾客线程
/// @param semid 信号量 ID
/// @param customer 顾客
void customerThread(int semid, Customer &customer)
{
    std::this_thread::sleep_for(std::chrono::seconds(customer.arrive_time)); // 等待到达时间
    P(semid, 0);
    customerQueue.push(&customer); // 加入队列
    std::cout << "顾客" << customer.id << "在" << customer.arrive_time << "到达银行！" << std::endl;
    V(semid, 0);
}

/// @brief 柜员线程
/// @param semid 信号量 ID
/// @param tellerIndex 柜员编号
void tellerThread(int semid, int tellerIndex)
{
    do
    {
        P(semid, 0);
        if (!customerQueue.empty()) // 有顾客
        {
            Customer *customer = customerQueue.front();    // 取出顾客
            customerQueue.pop();                           // 从队列中移除
            customer->teller = tellerIndex;                // 记录服务柜员
            customer->start_time = time(NULL) - init_time; // 记录开始服务时间
            std::cout << "顾客" << customer->id << "将由柜员" << tellerIndex << "服务！" << std::endl;
            V(semid, 0);

            tellers[tellerIndex] = false;                                              // 标记柜员忙
            std::this_thread::sleep_for(std::chrono::seconds(customer->service_time)); // 服务时间
            customer->served = true;                                                   // 标记已服务
            customer->leave_time = time(NULL) - init_time;                             // 记录离开时间
            tellers[tellerIndex] = true;                                               // 标记柜员空闲
            std::cout << "顾客" << customer->id << "已被柜员" << tellerIndex << "服务！" << std::endl;
        }
        else // 无顾客
        {
            if (allCustomersServed()) // 所有顾客已服务
            {
                V(semid, 0);
                break;
            }
            V(semid, 0);
        }
    } while (true);
}

/// @brief 主函数
/// @return 程序退出状态
int main()
{
    std::ifstream input(INPUT_FILENAME);   // 打开输入文件
    std::ofstream output(OUTPUT_FILENAME); // 打开输出文件

    int semid = semget(IPC_PRIVATE, 1, 0666 | IPC_CREAT); // 创建信号量
    semun sem_union;                                      // 信号量联合
    sem_union.val = 1;                                    // 信号量值
    semctl(semid, 0, SETVAL, sem_union);                  // 初始化信号量

    while (!input.eof()) // 读取顾客信息
    {
        Customer customer;
        input >> customer.id >> customer.arrive_time >> customer.service_time;
        customers.push_back(customer);
    }

    input.close(); // 关闭输入文件

    for (auto i = 0; i < TELLER_NUM; i++) // 创建柜员线程
    {
        tellerThreads.push_back(std::thread(tellerThread, semid, i));
    }

    for (auto &customer : customers) // 创建顾客线程
    {
        customerThreads.push_back(std::thread(customerThread, semid, std::ref(customer)));
    }

    for (auto &thread : customerThreads) // 等待顾客线程结束
    {
        thread.join();
    }

    for (auto &thread : tellerThreads) // 等待柜员线程结束
    {
        thread.join();
    }

    output << "顾客编号\t到达时间\t开始时间\t离开时间\t服务柜员" << std::endl; // 输出表头

    for (auto &customer : customers) // 输出顾客信息
    {
        output << customer.id << "\t"
               << customer.arrive_time << "\t"
               << customer.start_time << "\t"
               << customer.leave_time << "\t"
               << customer.teller << std::endl;
    }

    output.close(); // 关闭输出文件

    semctl(semid, 0, IPC_RMID); // 删除信号量

    return 0; // 退出
}
