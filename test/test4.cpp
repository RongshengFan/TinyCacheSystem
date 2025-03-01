#include <random>
#include <iostream>
#include <thread>
#include <vector>
#include <chrono>
#include <cassert>
#include "../include/CachePolicy.h"
#include "../include/LruCache.h"
#include "../include/LfuCache.h"
#include "../include/ArcCache.h"

// 并发测试的通用函数
template <typename Cache>
void testConcurrency(Cache& cache, size_t testDataSize, int numThreads, std::string cacheName) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, testDataSize - 1);

    // 定义线程函数
    auto task = [&](Cache& cache) {
        for (size_t i = 0; i < testDataSize; ++i) {
            int key = dis(gen);
            int value = 0;
            if (cache.get(key,value)) { // 如果存在，更新值
                cache.put(key, value + 1);
            } else { // 如果不存在，添加新值
                cache.put(key, key + 1);
            }
        }
    };

    // 获取初始时间
    auto start = std::chrono::high_resolution_clock::now();

    // 创建多个线程执行任务
    std::vector<std::thread> threads;
    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back(task, std::ref(cache));
    }

    // 等待所有线程完成
    for (auto& t : threads) {
        t.join();
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    // 计算 QPS
    double totalRequests = static_cast<double>(testDataSize * numThreads);
    double qps = totalRequests / (duration / 1000.0);

    std::cout << "测试缓存：    " << cacheName << std::endl;
    std::cout << "线程数：      " << numThreads << std::endl;
    std::cout << "测试用时：    " << duration << "ms" << std::endl;
    std::cout << "总请求数：    " << totalRequests << std::endl;
    std::cout << "QPS：" << qps << " queries/second" << std::endl;
    std::cout << "----------------------------------------\n";
}

int main() {
    size_t cacheCapacity = 100;
    size_t testDataSize = 100000;
    int numThreads = 5;

    // 测试 LRU 缓存的并发性
    mycache::LruCache<int, int> LruCache(cacheCapacity);
    testConcurrency(LruCache, testDataSize, numThreads, "LRU Cache");

    // 测试 LFU 缓存的并发性
    mycache::LfuCache<int, int> LfuCache(cacheCapacity);
    testConcurrency(LfuCache, testDataSize, numThreads, "LFU Cache");

    // 测试 ARC 缓存的并发性
    mycache::ArcCache<int, int> arcCache(cacheCapacity);
    testConcurrency(arcCache, testDataSize, numThreads, "ARC Cache");
    
    // 测试分片 LRU 缓存的并发性
    mycache::HashLruCache<int, int> hashLruCache(cacheCapacity, 5);
    testConcurrency(hashLruCache, testDataSize, numThreads, "Hash LRU Cache");

    // 测试分片 LFU 缓存的并发性
    mycache::HashLfuCache<int, int> hashLfuCache(cacheCapacity, 5);
    testConcurrency(hashLfuCache, testDataSize, numThreads, "Hash LFU Cache");

    return 0;
}