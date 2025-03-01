#ifndef MYCACHE_LRUCACHE_H
#define MYCACHE_LRUCACHE_H

#include <list>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <cstring>

#include "CachePolicy.h"

namespace mycache {

//LRU缓存模板类前向声明
template <typename K, typename V>class LruCache;

//LRU缓存节点模板类
template <typename K, typename V>
class LruNode 
{
public:
    LruNode(const K& k, const V& v) 
    : key(k)
    , value(v)
    , prev(nullptr)
    , next(nullptr) {}

    //必要的访问工具
    K getKey() const { return key; }
    V getValue() const { return value; }
    void setValue(const V& v) { value = v; }

    friend class LruCache<K,V>;

private:
    K key;
    V value;
    std::shared_ptr<LruNode<K,V>> prev;
    std::shared_ptr<LruNode<K,V>> next;
};

//基础LRU缓存模板类
template <typename K, typename V>
class LruCache: public CachePolicy<K,V>
{
public:
    //重新定义类型名非常重要，确保不会出现类型混乱
    using LruNodeType = LruNode<K,V>;
    using LruNodePtr = std::shared_ptr<LruNodeType>;
    using LruNodeMap = std::unordered_map<K, LruNodePtr>;

private:
    size_t capacity;    //缓存容量
    std::mutex mtx;     //互斥锁，用于线程同步
    LruNodeMap nodeMap; //节点哈希表，快速访问
    LruNodePtr head;    //虚拟头结点，最久未使用
    LruNodePtr tail;    //虚拟尾结点，最近使用

private:
    //移除节点
    void removeNode(LruNodePtr node)
    {
        //从链表中移除节点
        node->prev->next = node->next;
        node->next->prev = node->prev;
    }
    //插入节点
    void insertNode(LruNodePtr node)
    {
        //将节点插入到最近使用的位置
        node->next = tail;
        node->prev = tail->prev;
        tail->prev->next = node;
        tail->prev = node;
    }
    //淘汰节点
    void kickOut()
    {
        //移除最久未使用的节点
        LruNodePtr leastRecentNode = head->next;
        removeNode(leastRecentNode);
        nodeMap.erase(leastRecentNode->getKey());
    }
    //添加新节点
    void addNewNode(const K& key, const V& value)
    {
        //若缓存空间已满，则替换掉最久未使用的节点
        if(nodeMap.size() >= capacity)
        {
            kickOut();
        }
        //创建新节点并插入到最近使用的位置
        LruNodePtr newNode = std::make_shared<LruNodeType>(key,value);
        insertNode(newNode);
        nodeMap[key] = newNode;
    }
    //移动节点到最近使用的位置
    void moveNodeToRecent(LruNodePtr node)
    {
        //将节点移动到最近使用的位置
        removeNode(node);
        insertNode(node);
    }
    //更新节点
    void updateNode(LruNodePtr node, const V& value)
    {
        //更新节点的值并移动到最近使用的位置
        node->setValue(value);
        moveNodeToRecent(node);
    }

public:
    explicit LruCache(size_t n) : capacity(n) 
    {
        //创建头尾虚拟节点
        head = std::make_shared<LruNodeType>(K(), V());
        tail = std::make_shared<LruNodeType>(K(), V());
        head->next = tail;
        tail->prev = head;
    }
    ~LruCache()override = default;
    
    //添加缓存数据
    void put(const K& key, const V& value)override 
    {
        if(capacity <= 0)return;
        
        //上锁，避免多线程访问
        std::lock_guard<std::mutex> lock(mtx);
        auto it = nodeMap.find(key);
        if(it != nodeMap.end())
        {
            //节点已存在，更新节点的值并移动到最近使用的位置
            LruNodePtr node = it->second;
            updateNode(node, value);
        }
        else
        {
            //节点不存在，创建新节点并插入到最近使用的位置
            addNewNode(key, value);
        }
    }

    //访问缓存数据
    bool get(const K& key, V& value)override
    {
        if(capacity <= 0)return false;

        //上锁，避免多线程访问
        std::lock_guard<std::mutex> lock(mtx);
        auto it = nodeMap.find(key);
        if(it != nodeMap.end())
        {
            //节点存在，移动到最近使用的位置
            LruNodePtr node = it->second;
            moveNodeToRecent(node);
            value = node->getValue();
            return true;
        }
        return false;
    }

    //访问缓存数据并直接返回键值
    V get(const K& key)override
    {
        V value{};
        get(key, value);
        return value;
    }

    //删除缓存数据
    void remove(const K& key)
    {
        if(capacity <= 0)return;

        //上锁，避免多线程访问
        std::lock_guard<std::mutex> lock(mtx);
        auto it = nodeMap.find(key);
        if(it != nodeMap.end())
        {
            //节点存在，从哈希表和链表中移除
            LruNodePtr node = it->second;
            removeNode(node);
        }
    }

    //清空缓存
    void purge()
    {
        nodeMap.clear();
        head->next = tail;
        tail->prev = head;
    }
};

//LRU-k缓存模板类
template<typename K, typename V>
class LruKCache:public LruCache<K,V>
{
private:
    int k;                                          //判断是否进入缓存队列的k值
    std::unique_ptr<LruCache<K,size_t>> historyList;//访问历史记录链表

public:
    LruKCache(size_t n, size_t historyCapacity, int k_) 
    : LruCache<K,V>(n) 
    , k(k_)
    , historyList(std::make_unique<LruCache<K,size_t>>(historyCapacity))
    {}
    ~LruKCache()override = default;

    void put(const K& key, const V& value) 
    {
        V tempValue{};
        if(LruCache<K,V>::get(key,tempValue))
        {
            //若已存在于缓存中，则更新缓存数据并移动到最近使用的位置
            LruCache<K,V>::put(key,value);
            return;
        }

        //获取历史访问次数
        int historyCount = historyList->get(key);
        historyList->put(key, historyCount + 1);

        //判断是否应该进入缓存队列
        if(historyCount >= k)
        {
            //移除历史访问记录
            historyList->remove(key);
            //放入缓存队列
            LruCache<K,V>::put(key,value);
            return;
        }
    }

    bool get(const K& key, V& value)
    {
        //获取历史访问次数
        int historyCount = historyList->get(key);
        //更新历史访问次数
        historyList->put(key,historyCount+1);
        //获取缓存数据,但不一定存在
        return LruCache<K,V>::get(key,value);
    }
    V get(const K& key)
    {
        //获取访问次数
        int historyCount = historyList->get(key);
        //更新访问次数
        historyList->put(key,historyCount+1);
        //获取缓存数据,但不一定存在
        return LruCache<K,V>::get(key);
    }

};

template<typename K, typename V>
class HashLruCache
{
private:
    size_t capacity;                                        //缓存容量
    int sliceNum;                                           //切片数量
    std::vector<std::unique_ptr<LruCache<K,V>>> sliceCaches;//切片缓存

private:
    //将key转换为对应hash值
    size_t Hash(const K& key)
    {   
        std::hash<K> hashFunc;
        return hashFunc(key);
    }

public:
    HashLruCache(size_t n, int sliceNum_)
    : capacity(n)
    , sliceNum(sliceNum_) 
    {
        size_t sliceSize = ceil(capacity / static_cast<double>(sliceNum));
        for(int i=0;i<sliceNum;++i)
        {
            sliceCaches.emplace_back(std::make_unique<LruCache<K,V>>(sliceSize));
        }
    }
    void put(const K& key, const V& value) 
    {
        //将key转换为对应hash值，计算出对应的切片索引
        size_t sliceIndex = Hash(key) % sliceNum;
        sliceCaches[sliceIndex]->put(key,value);
    }
    bool get(const K& key, V& value)
    {
        //将key转换为对应hash值，计算出对应的切片索引
        size_t sliceIndex = Hash(key) % sliceNum;
        return sliceCaches[sliceIndex]->get(key,value);
    }
    V get(const K& key)
    {
        V value{};
        get(key,value);
        return value;
    }
};

}
#endif