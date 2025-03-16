/* 热点数据访问测试，测试缓存命中率*/

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
    const int HOT_KEYS = 3;            
    const int COLD_KEYS = 5000;

    for (int i = 0; i < testDataSize; ++i) {
        int key;
        if (i % 100 < 40) {  // 40%概率访问热点
            key = gen() % HOT_KEYS;
        } else {  // 60%概率访问冷数据
            key = HOT_KEYS + (gen() % COLD_KEYS);
        }
        
        int value;
        if (cache.get(key, value)) { // 假设初始所有键对应的值均为0
            hit++;
        } else {
            miss++;
            cache.put(key, key + 1);
        }
    }

    std::cout << "-----------热点数据访问测试--------------\n";
    std::cout << "测试缓存：    " << cacheName << std::endl;
    std::cout << "命中次数：    " << hit << std::endl;
    std::cout << "未命中次数：  " << miss << std::endl;
    std::cout << "命中率：      " << static_cast<double>(hit) / (hit + miss) * 100 << "%\n";
    std::cout << "----------------------------------------\n";
}

int main() {
    size_t cacheCapacity = 50;
    size_t testDataSize = 10000;

    // 测试 LRU 缓存命中率
    mycache::LruCache<int, int> lruCache(cacheCapacity);
    testHitRate(lruCache, testDataSize, "LRU Cache");

    // 测试 LRU-K 缓存命中率
    mycache::LruKCache<int, int> lrukCache(cacheCapacity, cacheCapacity/2, 2);
    testHitRate(lrukCache,  testDataSize, "LRU-K Cache");

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