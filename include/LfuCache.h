#ifndef MYCACHE_LFUCACHE_H
#define MYCACHE_LFUCACHE_H

#include <cmath>
#include <memory>
#include <unordered_map>
#include <thread>
#include <mutex>

#include "CachePolicy.h"

namespace mycache {

template <typename K, typename V> class LfuCache;

template <typename K, typename V> 
class freqList
{
private:
    struct Node
    {
        K key;
        V value;
        int freq;                    //标识每个双向链表中节点的访问频次
        std::shared_ptr<Node> prev;
        std::shared_ptr<Node> next;
        Node() : freq(1),  prev(nullptr), next(nullptr) {}
        Node(const K& k, const V& v) : key(k), value(v), freq(1), prev(nullptr), next(nullptr) {}
    };

    using NodePtr = std::shared_ptr<Node>;
    NodePtr head;   //虚拟头结点
    NodePtr tail;   //虚拟尾结点
    int freq;       //标识当前双向链表中节点的共同访问频次

public:
    //构造函数
    explicit freqList(int n) : freq(n)
    {
        head = std::make_shared<Node>();
        tail = std::make_shared<Node>();
        head->next = tail;
        tail->prev = head;
    }
    //判断链表是否为空
    bool isEmpty()const
    {
        return head->next == tail;
    }
    //添加节点到链表尾部
    void addNode(NodePtr node)
    {
        if(!node||!head||!tail)return;
        node->next = tail;
        node->prev = tail->prev;
        tail->prev->next = node;
        tail->prev = node;
    }
    //移除链表中的节点
    void removeNode(NodePtr node)
    {
        if(!node||!head||!tail)return;
        if(!node->prev||!node->next)return;
        node->prev->next = node->next;
        node->next->prev = node->prev;
        node->prev = nullptr;
        node->next = nullptr;
    }
    //获取链表中的第一个节点
    NodePtr getFirstNode()const
    {
        return head->next;
    }

    friend class LfuCache<K, V>;
};

template <typename K, typename V> 
class LfuCache : public CachePolicy<K, V>
{
public:
    using LfuNodeType = typename freqList<K, V>::Node;
    using LfuNodePtr = std::shared_ptr<LfuNodeType>;
    using LfuNodeMap = std::unordered_map<K, LfuNodePtr>;
    using LfufreqMap = std::unordered_map<int, freqList<K, V>*>;
private:
    size_t capacity;                                        //缓存容量
    int minFreq;                                            //最小访问缓存频次
    int maxAverageNum;                                      //最大平均访问缓存频次数
    int curAverageNum;                                      //当前平均访问缓存频次数
    int curTotalNum;                                        //当前访问缓存频次总数
    std::mutex mtx;                                       //互斥锁
    LfuNodeMap nodeMap;                                     //节点哈希表，快速访问节点
    LfufreqMap freqListMap;                                 //访问频次哈希表，快速访问频次对应的双向链表

private:
    //添加缓存
    void putInternal(const K& key, const V& value);
    //获取缓存
    void getInternal(LfuNodePtr node, V& value);
    //移除频次链表中的节点
    void removeFromFreqList(LfuNodePtr node);
    //添加节点到频次链表中
    void addToFreqList(LfuNodePtr node);
    //增加平均访问频次
    void addFreqNum();
    //减少平均访问频次
    void decreaseFreqNum(int num);
    //更新最小访问频次
    void updateMinFreq();
    //处理超过最大平均访问频次的情况
    void HandleOverMaxAverageNum();
    //淘汰缓存中的过期数据
    void kickOut();

public:
    LfuCache(size_t n, int maxAveNum = 10) 
    : capacity(n)
    , minFreq(0)
    , maxAverageNum(maxAveNum)
    , curAverageNum(0)
    , curTotalNum(0){}

    ~LfuCache() override = default;
    //向缓存中添加或更新键值对
    void put(const K& key, const V& value) override
    {
        if(capacity <= 0)return;

        std::lock_guard<std::mutex> lock(mtx);
        auto it = nodeMap.find(key);
        if(it != nodeMap.end())
        {
            //更新节点的值
            LfuNodePtr node = it->second;
            node->value = value;
            //更新节点的访问频次
            getInternal(node, node->value);
            return;
        }
        putInternal(key, value);
    }
    //根据键获取值，并更新该节点为最近使用的节点，访问成功返回true，否则返回false
    bool get(const K& key, V& value)override
    {
        if(capacity <= 0)return false;

        std::lock_guard<std::mutex> lock(mtx);
        
        auto it = nodeMap.find(key);
        if(it != nodeMap.end())
        {
            LfuNodePtr node = it->second;
            value = node->value;
            //更新节点的访问频次
            getInternal(node, value);
            return true;
        }
        return false;
    }
    //重载的 get 方法，直接返回值
    V get(const K& key)override
    {
        V value{};
        get(key, value);
        return value;
    }
    //清空缓存
    void purge()
    {
        nodeMap.clear();
        freqListMap.clear();
    }
};

template <typename K, typename V>
void LfuCache<K, V>::putInternal(const K &key, const V &value)
{
    if(nodeMap.size()==capacity)
    {
        //淘汰缓存中的过期数据
        kickOut();
    }
    LfuNodePtr node = std::make_shared<LfuNodeType>(key, value);
    nodeMap[key] = node;
    addToFreqList(node);
    addFreqNum();
    minFreq = 1;
}

template <typename K, typename V>
void LfuCache<K, V>::getInternal(LfuNodePtr node, V &value)
{
    value = node->value;
    removeFromFreqList(node);
    node->freq++;
    addToFreqList(node);
    if(node->freq-1 == minFreq && freqListMap[minFreq]->isEmpty())
    {
        minFreq++;
    }
    addFreqNum();
}

template <typename K, typename V>
void LfuCache<K, V>::removeFromFreqList(LfuNodePtr node)
{
    if(!node)return;

    auto freq = node->freq;
    freqListMap[freq]->removeNode(node);
}

template <typename K, typename V>
void LfuCache<K, V>::addToFreqList(LfuNodePtr node)
{
    if(!node)return;
    auto freq = node->freq;
    if(freqListMap.find(freq) == freqListMap.end())
    {
        freqListMap[freq] = new freqList<K, V>(freq);
    }
    freqListMap[freq]->addNode(node);
}

template <typename K, typename V>
void LfuCache<K, V>::addFreqNum()
{
    curTotalNum++;

    if(nodeMap.empty())
    {
        curAverageNum = 0;
    }
    else 
    {
        curAverageNum = std::ceil(curTotalNum / nodeMap.size());
    }

    if(curTotalNum > maxAverageNum)
    {
        HandleOverMaxAverageNum();
    }
}

template <typename K, typename V>
void LfuCache<K, V>::decreaseFreqNum(int num)
{
    curTotalNum -= num;

    if(nodeMap.empty())
    {
        curAverageNum = 0;
    }
    else 
    {
        curAverageNum = std::ceil(curTotalNum / nodeMap.size());
    }
}

template <typename K, typename V>
void LfuCache<K, V>::updateMinFreq()
{
    if(nodeMap.empty())
    {
        minFreq = 0;
    }
    else
    {
        minFreq = nodeMap.begin()->second->freq;
        for(auto it = nodeMap.begin(); it != nodeMap.end(); ++it)
        {
            if(it->second->freq < minFreq)
            {
                minFreq = it->second->freq;
            }
        }
    }
}

template <typename K, typename V>
void LfuCache<K, V>::HandleOverMaxAverageNum()
{
    if(nodeMap.empty())return;

    for(auto it = nodeMap.begin(); it != nodeMap.end(); ++it)
    {
        if(it->second)continue;

        LfuNodePtr node = it->second;
        removeFromFreqList(node);
        node->freq -= maxAverageNum/2;
        addToFreqList(node);
    }
    updateMinFreq();
}

template <typename K, typename V>
void LfuCache<K, V>::kickOut()
{
    LfuNodePtr node = freqListMap[minFreq]->getFirstNode();
    removeFromFreqList(node);
    nodeMap.erase(node->key);
    decreaseFreqNum(node->freq);
}

template <typename K, typename V>
class HashLfuCache : public CachePolicy<K, V>
{
private:
    size_t capacity;                                            //缓存容量
    int sliceNum;                                               //切片数量
    std::vector<std::unique_ptr<LfuCache<K,V>>> sliceCaches;    //切片缓存

private:
    size_t Hash(const K& key)
    {
        std::hash<K> hashFunc;
        return hashFunc(key);
    }

public:
    HashLfuCache(size_t n, int sliceNum_) : capacity(n), sliceNum(sliceNum_)
    {
        sliceCaches.resize(sliceNum);
        size_t sliceSize =ceil(capacity / static_cast<double>(sliceNum));
        for (int i = 0; i < sliceNum; ++i)
        {
            sliceCaches[i] = std::make_unique<LfuCache<K,V>>(sliceSize);
        }
    }

    void put(const K& key, const V& value)
    {
        int sliceIndex = Hash(key) % sliceNum;
        sliceCaches[sliceIndex]->put(key, value);
    }

    bool get(const K& key, V& value)
    {  
        int sliceIndex = Hash(key) % sliceNum;
        return sliceCaches[sliceIndex]->get(key, value);
    }

    V get(const K& key)
    {
        V value{};
        get(key, value);
        return value;
    }
};

}

#endif