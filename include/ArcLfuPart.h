#ifndef MYCACHE_ARCLFUCACHE_H
#define MYCACHE_ARCLFUCACHE_H

#include <unordered_map>
#include <memory>
#include <mutex>
#include <cstring>
#include <list>
#include "ArcNode.h"

namespace mycache {

template <typename K, typename V>
class ArcLfuPart
{
public:
    using NodeType = ArcNode<K, V>;
    using NodePtr = std::shared_ptr<NodeType>;
    using NodeMap = std::unordered_map<K, NodePtr>;
    using FreqMap = std::unordered_map<size_t, std::list<NodePtr>>;

private:
    size_t mainCapacity;            // 主缓存容量 
    size_t ghostCapacity;           // 幽灵缓存容量
    size_t minFreq;                 // 最小访问频次
    size_t transformThreshold;      // 转移阈值
    std::mutex mtx;                 // 互斥锁

    NodeMap mainCache;              // 主缓存
    NodeMap ghostCache;             // 幽灵缓存

    FreqMap freqListMap;            // 访问频次列表

    NodePtr ghostHead;              // 幽灵链表头结点
    NodePtr ghostTail;              // 幽灵链表尾结点

public:
    explicit ArcLfuPart(size_t capacity,  size_t TransformThreshold)
    : mainCapacity(capacity)
    , ghostCapacity(capacity)
    , minFreq(0)
    , transformThreshold(TransformThreshold)
    {
        initialiaze();
    }

    bool put(const K& key, const V& value)
    {
        // 如果主缓存容量为空，则返回false
        if(mainCapacity <= 0)return false;

        std::lock_guard<std::mutex> lock(mtx);

        auto it = mainCache.find(key);
        if(it != mainCache.end())
        {
            return updateMainNode(it->second, value);
        }
        return addNewNode(key, value);
    }

    bool get(const K& key, V& value)
    {   
        if(mainCapacity <= 0)return false;

        std::lock_guard<std::mutex> lock(mtx);

        auto it = mainCache.find(key);  
        if(it != mainCache.end())
        {
            value = it->second->getValue();
            updateNodeFrequency(it->second);
            return true;
        }
        return false;
    }

    V get(const K& key)
    {
        V value{};
        get(key,value);
        return value;
    }

    bool checkGhost(const K& key)
    {
        auto it = ghostCache.find(key);
        if(it != ghostCache.end())
        {
            removeGhostNode(it->second);
            ghostCache.erase(key);
            return true;
        }
        return false;
    }

    void increaseCapacity()
    {
        mainCapacity++;
    }

    bool decreaseCapacity()
    {
        if(mainCapacity <= 0)
        {   
            return false;
        }
        if(mainCapacity == mainCache.size())
        {// 如果主缓存已满，则移除访问频次最低的节点
            evictLeastFrequent();
        }
        mainCapacity--;
        return true;
    }

private:
    void initialiaze()
    {
        ghostHead = std::make_shared<NodeType>(K(), V());
        ghostTail = std::make_shared<NodeType>(K(), V());
        ghostHead->next = ghostTail;
        ghostTail->prev = ghostHead;
    }
    // 添加新节点到主缓存中
    bool addNewNode(const K& key, const V& value)
    {
        if(mainCache.size() >= mainCapacity){
            evictLeastFrequent();
        }
        
        //将新节点添加到频率列表中
        NodePtr node = std::make_shared<NodeType>(key, value);
        if(freqListMap.find(1) == freqListMap.end()){
            freqListMap[1] = std::list<NodePtr>();
        }
        freqListMap[1].push_back(node);
        minFreq = 1;

        mainCache[key] = node;
        return true;
    }
    // 更新主缓存中的节点
    bool updateMainNode(NodePtr node, const V& value)
    {
        node->setValue(value);
        updateNodeFrequency(node);
        return true;
    }   
    // 更新节点的访问频次并移动到新链表中
    void updateNodeFrequency(NodePtr node)
    {
        size_t oldFreq = node->getAccessCount();
        node->increaseAccessCount();
        size_t newFreq = node->getAccessCount();

        //从旧链表中移除
        auto &oldList = freqListMap[oldFreq];
        oldList.remove(node);
        if(oldList.empty()){
            freqListMap.erase(oldFreq);
            if(minFreq == oldFreq){
                minFreq = newFreq;
            }
        }
        
        //添加到新链表中
        if(freqListMap.find(newFreq) == freqListMap.end()){
            freqListMap[newFreq] = std::list<NodePtr>();
        }
        freqListMap[newFreq].push_back(node);
    }
    // 移除访问频次最低的节点
    void evictLeastFrequent()
    {
        auto &minFreqList = freqListMap[minFreq];
        if(freqListMap.empty()||minFreqList.empty())return;

        NodePtr node = minFreqList.front();
        minFreqList.pop_front();

        // 如果链表为空，则删除该链表
        if(minFreqList.empty())
        {
            freqListMap.erase(minFreq);
            //更新最小访问频次
            minFreq = freqListMap.empty() ? 0 : freqListMap.begin()->first;
        }

        // 将节点添加到幽灵缓存中
        if(ghostCache.size() >= ghostCapacity)
        {
            evictOldestGhost();
        }
        addToGhost(node);

        // 从主缓存中移除该节点
        mainCache.erase(node->getKey());
    }
    // 将节点添加到幽灵缓存中
    void addToGhost(NodePtr node)
    {
        //重置访问次数
        node->accessCount = 1;

        //添加到幽灵链表尾部
        node->next = ghostTail;
        node->prev = ghostTail->prev;
        ghostTail->prev->next = node;
        ghostTail->prev = node;
        
        //添加到幽灵缓存
        ghostCache[node->getKey()] = node;
    }
    // 从幽灵链表中移除节点
    void removeGhostNode(NodePtr node)
    {
        node->prev->next = node->next;
        node->next->prev = node->prev;
    }   

    void evictOldestGhost()
    {
        if(ghostCache.empty()||ghostHead->next == ghostTail)return;
        NodePtr node = ghostHead->next;
        removeGhostNode(node);
        ghostCache.erase(node->getKey());
    }
};
}
#endif //MYCACHE_ARCLFUCACHE_H