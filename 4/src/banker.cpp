#include <iostream>
#include <fstream>
#include <vector>

class Banker
{
public:
    Banker(int np, int nr,
           std::vector<int> av,
           std::vector<std::vector<int>> ne,
           std::vector<std::vector<int>> al)
        : numProcesses(np), numResources(nr),
          available(av), need(ne), allocation(al)
    {
        if (numProcesses <= 0 || numResources <= 0)
        {
            throw std::invalid_argument("进程数和资源数必须大于零");
        }

        // 计算最大需求矩阵
        maximum.resize(numProcesses, std::vector<int>(numResources));
        for (int i = 0; i < numProcesses; i++)
        {
            for (int j = 0; j < numResources; j++)
            {
                maximum[i][j] = need[i][j] + allocation[i][j];
            }
        }
    }

    bool isSafe()
    {
        std::vector<int> work = available;             // 工作向量
        std::vector<bool> finish(numProcesses, false); // 完成向量
        std::vector<int> safeSequence;                 // 安全序列
        int count = 0;
        while (count < numProcesses) // 直到所有进程都完成
        {
            bool found = false;                    // 是否找到一个满足条件的进程
            for (int i = 0; i < numProcesses; i++) // 遍历所有进程
            {
                std::cout << "正在对进程" << i << "进行安全性检查" << std::endl;
                if (!finish[i]) // 进程 i 未完成
                {
                    std::cout << " -> 进程" << i << "未完成" << std::endl;
                    int j;
                    for (j = 0; j < numResources; j++) // 检查进程 i 的需求是否小于等于工作向量
                    {
                        if (need[i][j] > work[j]) // 进程 i 的需求大于工作向量
                        {
                            std::cout << " -> 进程" << i << "的需求大于工作向量" << std::endl;
                            break;
                        }
                    }
                    if (j == numResources) // 进程 i 的需求小于等于工作向量
                    {
                        std::cout << " -> 进程" << i << "的需求小于等于工作向量" << std::endl;
                        for (int k = 0; k < numResources; k++) // 将进程 i 已分配的资源释放到工作向量
                        {
                            work[k] += allocation[i][k];
                        }
                        finish[i] = true;          // 进程 i 完成
                        safeSequence.push_back(i); // 将进程 i 加入安全序列
                        count++;                   // 完成进程数加一
                        found = true;              // 找到一个满足条件的进程
                    }
                }
            }
            if (!found) // 未找到一个满足条件的进程
            {
                std::cout << "系统不安全" << std::endl;
                return false;
            }
        }
        std::cout << "系统安全" << std::endl;
        std::cout << "安全序列为：";
        for (int i = 0; i < numProcesses; i++)
        {
            std::cout << safeSequence[i] << " ";
        }
        std::cout << std::endl;
        return true;
    }

    bool requestResources(int process, std::vector<int> request)
    {
        if (process < 0 || process >= numProcesses)
        {
            std::cout << "无效的进程编号" << std::endl;
            return false;
        }

        if (request.size() != numResources)
        {
            std::cout << "请求资源向量大小错误" << std::endl;
            return false;
        }

        for (int i = 0; i < numResources; i++)
        {
            if (request[i] > need[process][i]) // 请求资源数大于需求资源数，拒绝请求
            {
                std::cout << "请求资源数大于需求资源数" << std::endl;
                return false;
            }
            if (request[i] > available[i]) // 请求资源数大于可用资源数，拒绝请求
            {
                std::cout << "请求资源数大于可用资源数" << std::endl;
                return false;
            }
        }
        for (int i = 0; i < numResources; i++) // 模拟分配资源
        {
            available[i] -= request[i];
            allocation[process][i] += request[i];
            need[process][i] -= request[i];
        }
        if (!isSafe()) // 系统不安全，回滚分配
        {
            for (int i = 0; i < numResources; i++)
            {
                available[i] += request[i];
                allocation[process][i] -= request[i];
                need[process][i] += request[i];
            }
            std::cout << "请求资源后系统不安全" << std::endl;
            return false;
        }
        std::cout << "请求资源成功" << std::endl;
        return true;
    }

    void print()
    {
        std::cout << "可用资源向量：";
        for (int i = 0; i < numResources; i++)
        {
            std::cout << available[i] << " ";
        }
        std::cout << std::endl;
        std::cout << "需求矩阵：" << std::endl;
        for (int i = 0; i < numProcesses; i++)
        {
            for (int j = 0; j < numResources; j++)
            {
                std::cout << need[i][j] << " ";
            }
            std::cout << std::endl;
        }
        std::cout << "已分配矩阵：" << std::endl;
        for (int i = 0; i < numProcesses; i++)
        {
            for (int j = 0; j < numResources; j++)
            {
                std::cout << allocation[i][j] << " ";
            }
            std::cout << std::endl;
        }
    }

private:
    int numProcesses;                         // 进程数
    int numResources;                         // 资源数
    std::vector<int> available;               // 可用资源向量
    std::vector<std::vector<int>> allocation; // 已分配矩阵
    std::vector<std::vector<int>> maximum;    // 最大需求矩阵
    std::vector<std::vector<int>> need;       // 需求矩阵
};

int main()
{
    std::ifstream fin("test.txt");
    if (!fin)
    {
        std::cerr << "无法打开文件" << std::endl;
        return 1;
    }
    int numTestCases;
    fin >> numTestCases; // 测试用例数
    if (numTestCases <= 0)
    {
        std::cerr << "无效的测试用例数" << std::endl;
        return 1;
    }
    for (auto t = 0; t < numTestCases; t++)
    {
        int numProcesses, numResources;
        fin >> numProcesses >> numResources;
        if (numProcesses <= 0 || numResources <= 0)
        {
            std::cerr << "无效的进程数或资源数" << std::endl;
            continue;
        }
        std::vector<std::vector<int>> need(numProcesses, std::vector<int>(numResources));
        for (auto i = 0; i < numProcesses; i++) // 读取需求矩阵
        {
            for (auto j = 0; j < numResources; j++)
            {
                fin >> need[i][j];
            }
        }
        std::vector<std::vector<int>> allocation(numProcesses, std::vector<int>(numResources));
        for (auto i = 0; i < numProcesses; i++) // 读取已分配矩阵
        {
            for (auto j = 0; j < numResources; j++)
            {
                fin >> allocation[i][j];
            }
        }
        std::vector<int> available(numResources);
        for (auto i = 0; i < numResources; i++) // 读取可用资源向量
        {
            fin >> available[i];
        }

        try
        {
            Banker banker(numProcesses, numResources, available, need, allocation);

            std::cout << "====================================" << std::endl
                      << "初始化状态：" << std::endl;
            banker.print();
            std::cout << "====================================" << std::endl
                      << std::endl;

            int process;
            std::vector<int> request(numResources);
            fin >> process;
            for (auto i = 0; i < numResources; i++) // 读取请求资源向量
            {
                fin >> request[i];
            }

            std::cout << "进程" << process << "请求资源：";
            for (auto i = 0; i < numResources; i++)
            {
                std::cout << request[i] << " ";
            }
            std::cout << std::endl;

            banker.requestResources(process, request);

            std::cout << "====================================" << std::endl
                      << "最终状态：" << std::endl;
            banker.print();
            std::cout << "====================================" << std::endl;
        }
        catch (const std::exception &e)
        {
            std::cerr << "发生错误：" << e.what() << std::endl;
        }
    }

    return 0;
}
