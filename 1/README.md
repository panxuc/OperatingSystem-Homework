# 实验1：进程间同步/互斥问题——银行柜员服务问题

## 问题描述

银行有 $n$ 个柜员负责为顾客服务，顾客进入银行先取一个号码，然后等着叫号。当某个柜员空闲下来，就叫下一个号。

编程实现该问题，用 P、V 操作实现柜员和顾客的同步。

## 实现要求

1. 某个号码只能由一名顾客取得；
2. 不能有多于一个柜员叫同一个号；
3. 有顾客的时候，柜员才叫号；
4. 无柜员空闲的时候，顾客需要等待；
5. 无顾客的时候，柜员需要等待。

## 设计思路

编程语言：C++

编程平台：Linux

本题的关键为信号量和 P、V 操作的实现。本题由于情景较为简单，可以使用一个信号量来实现柜员和顾客的同步。

首先，对于每一个顾客和柜员，都需要一个线程来模拟其行为。顾客线程负责到达银行、取号、等待叫号、服务和离开；柜员线程负责等待顾客、叫号、服务顾客和等待下一个顾客。

通过一个队列 `customerQueue` 来实现取号和叫号的同步。当顾客到达银行时，将其加入队列；当柜员空闲时，从队列中取出顾客进行服务。

信号量保护了如下临界区：

- `customerQueue`：
  - P 操作：在顾客线程中，当顾客到达时执行 P 操作来获取信号量，确保只有一个线程可以访问和修改 `customerQueue`。这样可以防止多个顾客线程同时向 `customerQueue` 中添加顾客，导致数据竞争。
  - V 操作：在顾客线程将顾客加入 `customerQueue` 后执行 V 操作，释放信号量，允许其他线程访问 `customerQueue`。
- `tellers`：
  - P 操作：在柜员线程中，当柜员空闲时执行 P 操作来获取信号量，确保只有一个线程可以访问和修改 `customerQueue` 和顾客的状态。这样可以防止多个柜员线程同时从 `customerQueue` 中取出顾客，导致数据竞争。
  - V 操作：在处理完顾客或确定所有顾客已服务后，柜员线程执行 V 操作，释放信号量，允许其他线程继续访问共享资源。

## 代码说明

### 自定义数据结构

由于顾客信息较为复杂，故单独设置 `Customer` 类型来存储起信息，包括编号、到达时间、服务时间、是否已服务、开始服务时间、离开银行时间和服务柜员编号。

```cpp
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
```

### 信号量操作

使用 `semget` 创建信号量，`semctl` 初始化信号量，`semop` 执行 P、V 操作。

```cpp
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

int main()
{
    // ...
    int semid = semget(IPC_PRIVATE, 1, 0666 | IPC_CREAT); // 创建信号量
    semun sem_union;                                      // 信号量联合
    sem_union.val = 1;                                    // 信号量值
    semctl(semid, 0, SETVAL, sem_union);                  // 初始化信号量
    // ...
    semctl(semid, 0, IPC_RMID); // 删除信号量
    // ...
}
```

### 全局变量

定义全局变量，包括顾客列表、顾客队列、柜员状态、顾客线程和柜员线程。

```cpp
const int TELLER_NUM = 4; // 柜员数量

const char *INPUT_FILENAME = "bankteller.in";   // 输入文件名
const char *OUTPUT_FILENAME = "bankteller.out"; // 输出文件名

time_t init_time = time(NULL); // 相对时间基准

std::vector<Customer> customers;            // 顾客列表
std::queue<Customer *> customerQueue;       // 顾客队列
std::vector<int> tellers(TELLER_NUM, true); // 柜员状态，`true` 表示空闲
std::vector<std::thread> customerThreads;   // 顾客线程
std::vector<std::thread> tellerThreads;     // 柜员线程
```

### 顾客线程

顾客线程首先需要等待到达时间，然后执行 P 操作将自己加入队列，最后执行 V 操作释放信号量。

```cpp
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
```

### 柜员线程

柜员线程需要循环等待顾客，当有顾客时执行 P 操作取出顾客进行服务，最后执行 V 操作释放信号量。

```cpp
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
```

### 主函数

主函数负责读取顾客信息、创建顾客线程和柜员线程、等待线程结束、输出顾客信息、关闭文件和删除信号量。

```cpp
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
```

## 实验结果

首先使用样例数据进行测试。

输入文件 `bankteller.in`：

```
1 1 10
2 5 2
3 6 3
```

输出文件 `bankteller.out`：

```
顾客编号	到达时间	开始时间	离开时间	服务柜员
1	1	1	11	0
2	5	5	7	1
3	6	6	9	3
```

终端输出：

```
顾客1在1到达银行！
顾客1将由柜员0服务！
顾客2在5到达银行！
顾客2将由柜员1服务！
顾客3在6到达银行！
顾客3将由柜员3服务！
顾客2已被柜员1服务！
顾客3已被柜员3服务！
顾客1已被柜员0服务！
./bankteller  1.08s user 8.90s system 90% cpu 11.003 total
```

对样例数据进行一些修改，以模拟顾客等待柜员的情况。

输入文件 `bankteller.in`：

```
1 1 10
2 5 2
3 6 3
4 1 10
5 5 2
6 6 3
7 1 10
8 5 2
9 6 3
```

输出文件 `bankteller.out`：

```
顾客编号	到达时间	开始时间	离开时间	服务柜员
1	1	1	11	2
2	5	5	7	1
3	6	11	14	0
4	1	1	11	0
5	5	7	9	1
6	6	11	14	2
7	1	1	11	3
8	5	9	11	1
9	6	11	14	3
```

终端输出：

```
顾客1在1到达银行！
顾客1将由柜员2服务！
顾客4在1到达银行！
顾客4将由柜员0服务！
顾客7在1到达银行！
顾客7将由柜员3服务！
顾客2在5到达银行！
顾客2将由柜员1服务！
顾客5在5到达银行！
顾客8在5到达银行！
顾客6在6到达银行！
顾客3在6到达银行！
顾客9在6到达银行！
顾客2已被柜员1服务！
顾客5将由柜员1服务！
顾客5已被柜员1服务！
顾客8将由柜员1服务！
顾客1已被柜员2服务！
顾客4已被柜员0服务！
顾客6将由柜员2服务！
顾客3将由柜员0服务！
顾客7已被柜员3服务！
顾客9将由柜员3服务！
顾客8已被柜员1服务！
顾客6已被柜员2服务！
顾客3已被柜员0服务！
顾客9已被柜员3服务！
./bankteller  1.64s user 6.27s system 56% cpu 14.007 total
```

可见，程序能够正确模拟银行柜员服务问题，满足题目要求。

## 思考题

1. 柜员人数和顾客人数对结果分别有什么影响？

    > 通常来说，柜员人数越多，顾客等待时间越短，服务效率越高。但是，柜员人数过多可能会导致资源浪费，因为柜员可能会空闲较长时间。柜员人数过少则可能导致顾客等待时间过长，服务效率降低。
    >
    > 相比于顾客人数，顾客到达时间分布和服务时间分布对结果的影响更大。如果顾客到达时间分布较为集中，柜员人数较少时可能会导致某些柜员忙于服务，而其他柜员空闲。如果顾客服务时间分布较长，柜员人数较少时可能会导致某些顾客等待时间过长。

2. 实现互斥的方法有哪些？各自有什么特点？效率如何？ 

    > - 锁变量：
    >   - 特点：使用一个简单的布尔变量来表示资源是否被锁定。线程在进入临界区前检查该变量，如果未锁定，则将其设置为已锁定。
    >   - 效率：实现简单，但存在“忙等待”问题，线程在等待时会一直循环检查锁变量，消耗CPU资源。不适合多核系统，因为多个线程可能同时读取和修改锁变量，导致竞争条件。
    > - 轮转法：
    >   - 特点：使用一个轮转变量（turn），表示当前允许进入临界区的线程。轮流让不同线程进入临界区，每个线程在进入前检查是否轮到自己。
    >   - 效率：同样存在忙等待问题。实现简单，但只适用于固定数量的线程，不适合动态线程数目的情况。
    > - Peterson 算法：
    >   - 特点：通过两个布尔数组和一个整型变量实现，确保两个线程之间的互斥访问。不依赖于忙等待，使用了一个更复杂的协议来处理竞争条件。
    >   - 效率：能保证两个线程的互斥访问，不适用于多个线程。效率高于简单锁变量和轮转法，但仍然存在忙等待。
    > - 硬件方法——禁止中断：
    >   - 特点：在临界区内禁止所有中断，确保当前线程不会被中断。
    >   - 效率：简单且有效，但只适用于单处理器系统。对多处理器系统无效，且禁止中断可能导致系统响应性降低。
    > - 硬件指令方法：
    >   - 特点：使用硬件支持的原子操作。
    >   - 效率：高效，适用于多处理器系统。硬件实现的原子操作能够有效避免忙等待，提升效率。
    > - 信号量：
    >   - 特点：使用一个计数器和两个原子操作（P操作和V操作）来控制对资源的访问。能实现更复杂的同步机制，如资源计数和优先级控制。
    >   - 效率：不存在忙等待，线程在等待时会被阻塞。适用于多处理器和多线程系统，灵活且高效。
    > - 管程：
    >   - 特点：高级并发编程抽象，封装共享资源和同步机制。提供条件变量和互斥锁来管理线程的访问和通信。
    >   - 效率：不存在忙等待，线程在等待条件满足时会被阻塞。提供了更高层次的抽象，易于编写和维护同步代码，效率较高。

## 源代码

```cpp
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
```
