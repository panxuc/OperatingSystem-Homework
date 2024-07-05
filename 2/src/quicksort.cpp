#include <iostream>
#include <fstream>

#include <algorithm>
#include <mutex>
#include <random>
#include <thread>
#include <vector>

const int RANDOM = 1000000;      // 随机数数量
const int RANDOM_MIN = 0;        // 随机数最小值
const int RANDOM_MAX = 10000000; // 随机数最大值
const int THRESHOLD = 1000;      // 最小分割阈值
const int MAX_THREADS = 20;      // 最大线程数量

const char *RANDOM_FILENAME = "random.in";  // 随机数文件名
const char *SORTED_FILENAME = "sorted.out"; // 排序后文件名

int threads = 0; // 线程数量
std::mutex mtx;  // 互斥锁

/// @brief 生成随机数
/// @param n 随机数数量
/// @param min 随机数最小值
/// @param max 随机数最大值
/// @return `std::vector<int>` 随机数数组
std::vector<int> generateRandomNumbers(int n, int min, int max)
{
    std::random_device rd;       // 随机数种子
    std::vector<int> numbers(n); // 随机数数组
    std::generate(numbers.begin(), numbers.end(), [&]
                  { return rd() % (max - min + 1) + min; }); // 生成随机数
    return numbers;
}

/// @brief 快速排序
/// @param arr 待排序数组
/// @param left 左边界
/// @param right 右边界
void quicksort(std::vector<int> &arr, int left, int right)
{
    if (left < right) // 递归终止条件
    {
        if (right - left < THRESHOLD) // 若数组长度小于 1000，使用 std::sort 排序
        {
            std::sort(arr.begin() + left, arr.begin() + right + 1);
            return;
        }

        int pivot = arr[left + (right - left) / 2]; // 选取中间元素作为 pivot
        int i = left, j = right;
        while (i <= j) // 分割数组
        {
            while (arr[i] < pivot)
                i++;
            while (arr[j] > pivot)
                j--;
            if (i <= j)
            {
                std::swap(arr[i], arr[j]);
                i++;
                j--;
            }
        }

        std::thread left_thread, right_thread;        // 左右子线程
        bool spawn_left = false, spawn_right = false; // 是否生成子线程

        {
            std::lock_guard<std::mutex> lock(mtx);                // 加锁
            spawn_left = (left < j) && (threads < MAX_THREADS);   // 判断是否生成左子线程
            spawn_right = (i < right) && (threads < MAX_THREADS); // 判断是否生成右子线程
        }

        if (spawn_left)
        {
            threads++;                                                    // 线程数量加一
            std::cout << "threads: " << threads << std::endl;             // 输出线程数量
            left_thread = std::thread(quicksort, std::ref(arr), left, j); // 生成左子线程
        }
        else if (left < j)
        {
            quicksort(arr, left, j);
        }

        if (spawn_right)
        {
            threads++;                                                      // 线程数量加一
            std::cout << "threads: " << threads << std::endl;               // 输出线程数量
            right_thread = std::thread(quicksort, std::ref(arr), i, right); // 生成右子线程
        }
        else if (i < right)
        {
            quicksort(arr, i, right);
        }

        if (left_thread.joinable()) // 等待左子线程结束
        {
            left_thread.join();
            {
                std::lock_guard<std::mutex> lock(mtx); // 加锁
                threads--;                             // 线程数量减一
            }
        }
        if (right_thread.joinable()) // 等待右子线程结束
        {
            right_thread.join();
            {
                std::lock_guard<std::mutex> lock(mtx); // 加锁
                threads--;                             // 线程数量减一
            }
        }
    }
}

int main()
{
    std::fstream randomFile(RANDOM_FILENAME, std::ios::out);
    std::fstream sortedFile(SORTED_FILENAME, std::ios::out);

    // 生成随机数
    std::vector<int> data = generateRandomNumbers(RANDOM, RANDOM_MIN, RANDOM_MAX);
    std::cout << "Random numbers generated" << std::endl;

    // 将随机数写入文件
    for (auto i : data)
    {
        randomFile << i << "\t";
    }
    std::cout << "Random numbers written to file" << std::endl;

    // 排序
    quicksort(data, 0, data.size() - 1);
    std::cout << "Numbers sorted" << std::endl;

    // 将排序后的数字写入文件
    for (auto i : data)
    {
        sortedFile << i << "\t";
    }
    std::cout << "Sorted numbers written to file" << std::endl;

    // 检查排序是否正确
    for (auto i = 1; i < data.size(); i++)
    {
        if (data[i] < data[i - 1])
        {
            std::cerr << "Sort failed" << std::endl;
            return 1;
        }
    }
    std::cout << "Sort succeeded" << std::endl;

    randomFile.close();
    sortedFile.close();
    return 0;
}
