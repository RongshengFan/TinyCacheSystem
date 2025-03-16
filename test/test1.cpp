/* 循环扫描访问测试，测试缓存命中率*/

#include <random>
#include <iostream>
#include <thread>
#include <vector>
#include <chrono>
#include <cassert>
#include "../include/CachePolicy.h"
#include "../include/LfuCache.h"
#include "../include/LruCache.h"
#include "../include/ArcCache.h"
#include "../include/ClockCache.h"  
// 测试命中率的通用函数
template <typename Cache>
void testHitRate(Cache& cache, size_t testDataSize, std::string cacheName) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, testDataSize - 1);

    size_t hit = 0;
    size_t miss = 0;

    const int loopSize = 200;         
    // 获取初始时间
    auto start = std::chrono::high_resolution_clock::now();

    // 先填充数据
    for (int key = 0; key < loopSize * 2; ++key) {
        cache.put(key, key + 1);
    }
        
    // 然后进行访问测试
    int pos = 0;
    for (int i = 0; i < testDataSize; ++i) {
        int key;
        if (i % 100 < 70) {  // 70%顺序扫描
            key = pos;
            pos = (pos + 1) % loopSize;
        } else if (i % 100 < 85) {  // 15%随机跳跃
            key = gen() % loopSize;
        } else {  // 15%访问范围外数据
            key = loopSize + (gen() % loopSize);
        }
        
        int value;
        if (cache.get(key, value)) {
            hit++;
        }
        else {
            miss++;
            cache.put(key, key + 1);
        }
    }        

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    std::cout << "-----------循环扫描访问测试--------------\n";
    std::cout << "测试缓存：    " << cacheName << std::endl;
    std::cout << "命中次数：    " << hit << std::endl;
    std::cout << "未命中次数：  " << miss << std::endl;
    std::cout << "命中率：      " << static_cast<double>(hit) / (hit + miss) * 100 << "%\n";
    std::cout << "测试用时：    " << duration << "ms\n\n";
    std::cout << "----------------------------------------\n";
}

int main() {
    size_t cacheCapacity = 100;
    size_t testDataSize = 100000;

    // 测试 LRU 缓存命中率
    mycache::LruCache<int, int> lruCache(cacheCapacity);
    testHitRate(lruCache, testDataSize, "LRU Cache");  
    
    // 测试 LFU 缓存命中率
    mycache::LfuCache<int, int> lfuCache(cacheCapacity);
    testHitRate(lfuCache, testDataSize, "LFU Cache");

    // 测试 Clock 缓存命中率
    mycache::ClockCache<int, int> clockCache(cacheCapacity);
    testHitRate(clockCache, testDataSize, "Clock Cache");

    // 测试 ARC 缓存命中率
    mycache::ArcCache<int, int> arcCache(cacheCapacity);
    testHitRate(arcCache, testDataSize, "ARC Cache");

    return 0;
}