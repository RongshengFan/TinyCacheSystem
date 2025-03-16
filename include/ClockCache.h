#ifndef MYCACHE_CLOCKCACHE_H
#define MYCACHE_CLOCKCACHE_H
#include <Windows.h>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <cstring>
#include <cassert>
#include <vector>
#include "CachePolicy.h"

namespace mycache { 

template <typename K, typename V>
struct ClockNode
{
    K key;
    V value;
    bool reference;
    bool dirty;
    ClockNode(const K& k, const V& v) : key(k), value(v), reference(false), dirty(false) {}

    void setValue(const V& v){value = v;}
    K getKey() const { return key; }
    V getValue() const { return value; }    
};

template <typename K, typename V>
class ClockCache : public CachePolicy<K, V>
{
public:
    using ClockNodePtr = std::shared_ptr<ClockNode<K, V>>;
    using ClockNodeMap = std::unordered_map<K, size_t>;
    using ClockNodeVector = std::vector<ClockNodePtr>;
private:
    ClockNodeMap nodeMap;       // 键到数组索引的映射
    ClockNodeVector clockList;  // 缓存数组,环形链表
    size_t capacity;            // 缓存容量
    size_t size;                // 当前缓存大小
    size_t clockHand;           // 时钟指针
    std::mutex mtx;             // 互斥锁
public:
    explicit ClockCache(size_t capacity) : capacity(capacity), size(0), clockHand(0) {}
    ~ClockCache()override = default;
    bool get(const K& key, V& value) override
    {
        std::lock_guard<std::mutex> lock(mtx);
        auto it = nodeMap.find(key);
        if (it == nodeMap.end())
            return false;
        value = clockList[it->second]->getValue();
        clockList[it->second]->reference = true;
        return true;
    }

    V get(const K& key) override
    {
        V value{};
        get(key, value);
        return value;
    }
    void put(const K& key, const V& value) override
    {
        std::lock_guard<std::mutex> lock(mtx);
        auto it = nodeMap.find(key);
        if (it != nodeMap.end())
        {   // 存在，则更新节点
            clockList[it->second]->setValue(value);
            clockList[it->second]->reference = true;
            clockList[it->second]->dirty = true;
            return;
        }
        if (size < capacity)
        {   // 还有容量，直接添加新节点
            ClockNodePtr node = std::make_shared<ClockNode<K, V>>(key, value);
            clockList.push_back(node);
            nodeMap[key] = size;
            size++;
            return;
        }
        while (true)
        {  
            // 时钟指针指向的节点的引用位为1，将其置为0，指针后移
            if (clockList[clockHand]->reference)
            {
                clockList[clockHand]->reference = false;
                clockHand = (clockHand + 1) % capacity;
            }
            else
            {
                if (clockList[clockHand]->dirty)
                {   // 脏节点，将其写回
                    clockList[clockHand]->dirty = false;  
                    //Sleep(1); // 模拟写回时间

                    // 替换该节点
                    nodeMap.erase(clockList[clockHand]->getKey());
                    nodeMap[key] = clockHand;
                    clockList[clockHand] = std::make_shared<ClockNode<K, V>>(key, value);
                    clockList[clockHand]->reference = true;
                    return;
                }
                else
                {   // 该节点为干净节点，替换该节点
                    nodeMap.erase(clockList[clockHand]->getKey());
                    nodeMap[key] = clockHand;
                    clockList[clockHand] = std::make_shared<ClockNode<K, V>>(key, value);
                    clockList[clockHand]->reference = true;
                    return;
                }
            }
        }
    }
    void remove(const K& key)
    {
        // 删除节点
        std::lock_guard<std::mutex> lock(mtx);
        auto it = nodeMap.find(key);
        if (it == nodeMap.end())
            return;
        size_t index = it->second;
        nodeMap.erase(key);
        clockList.erase(clockList.begin() + index);
        size--;
    }
    void clear()
    {
        std::lock_guard<std::mutex> lock(mtx);
        nodeMap.clear();
        clockList.clear();
        size = 0;
    }
};
}

#endif //MYCACHE_CLOCKCACHE_H