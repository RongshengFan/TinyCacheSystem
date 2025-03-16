#ifndef MYCACHE_ARCLRUCACHE_H
#define MYCACHE_ARCLRUCACHE_H

#include <unordered_map>
#include <memory>
#include <mutex>
#include <cstring>

#include "ArcNode.h"

namespace mycache {

template <typename K, typename V>
class ArcLruPart{
public:
    using NodeType = ArcNode<K, V>;
    using NodePtr = std::shared_ptr<NodeType>;
    using NodeMap = std::unordered_map<K, NodePtr>;

private:
    size_t mainCapacity;            //主缓存容量
    size_t ghostCapacity;           //幽灵缓存容量
    size_t transformThreshold;      //转移阈值
    std::mutex mtx;                //互斥锁

    NodeMap mainCache;              //主缓存
    NodeMap ghostCache;             //幽灵缓存

    NodePtr mainHead;               //主链表虚拟头节点
    NodePtr mainTail;               //主链表虚拟尾节点
    
    NodePtr ghostHead;              //幽灵链表虚拟头节点
    NodePtr ghostTail;              //幽灵链表虚拟尾节点
    
public:
    explicit ArcLruPart(size_t capacity, size_t TransformThreshold)
    : mainCapacity(capacity)
    , ghostCapacity(capacity)
    , transformThreshold(TransformThreshold)
    {
        initialize();
    }

    //插入/更新缓存
    bool put(const K& key, const V& value)
    {
        if(mainCapacity <= 0)
        {
            return false;
        }

        std::lock_guard<std::mutex> lock(mtx);
        auto it = mainCache.find(key);
        if(it != mainCache.end())
        {
            //更新缓存
            updateMainNode(it->second, value);
            return true;
        }

        //插入新缓存
        addNewNode(key, value);
        return true;
    }

    //获取缓存
    bool get(const K& key, V& value, bool& shouldTransform)
    {
        if(mainCapacity <= 0)
        {
            return false;
        }

        std::lock_guard<std::mutex> lock(mtx);
        auto it = mainCache.find(key);
        if(it != mainCache.end())
        {
            //更新访问次数，同时判断是否该转移到LFU部分
            shouldTransform = updateAccessCount(it->second);
            value = it->second->getValue();
            return true;
        }
        return false;
    }

    //获取缓存
    V get(const K& key)
    {
        V value{};
        get(key, value);
        return value;
    }

    //检查节点是否在ghost缓存中，是则移回主缓存
    bool checkGhost(const K& key)
    {
        std::lock_guard<std::mutex> lock(mtx);
        auto it = ghostCache.find(key);
        if(it != ghostCache.end()){
            removeGhostNode(it->second);
            addNewNode(key, it->second->getValue());
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
            mainCapacity = 0;
            return false;
        }
        if(mainCapacity >= mainCache.size())
        {
            //缓存已满则移除最近最久未用节点
            evictLeastRecent();
        }

        mainCapacity--;
        return true;
    }

private:
    void initialize()
    {
        mainHead = std::make_shared<NodeType>(K(), V());
        mainTail = std::make_shared<NodeType>(K(), V());
        mainHead->next = mainTail;
        mainTail->prev = mainHead;

        ghostHead = std::make_shared<NodeType>(K(), V());
        ghostTail = std::make_shared<NodeType>(K(), V());
        ghostHead->next = ghostTail;
        ghostTail->prev = ghostHead;
    }
    //移除节点
    void removeNode(NodePtr node)
    {
        node->prev->next = node->next;
        node->next->prev = node->prev;
    }
    //添加节点到主链表尾部
    void addToMainTail(NodePtr node)
    {
        node->next = mainTail;
        node->prev = mainTail->prev;
        mainTail->prev->next = node;
        mainTail->prev = node;
    }
    //移动节点到主链表尾部
    void moveToMainTail(NodePtr node)
    {
        removeNode(node);
        addToMainTail(node);
    }
    //添加新节点到主缓存
    bool addNewNode(const K& key, const V& value)
    {
        if(mainCache.size() >= mainCapacity)
        {
            //淘汰缓存
            evictLeastRecent();
        }
        NodePtr node = std::make_shared<NodeType>(key, value);
        addToMainTail(node);
        mainCache[key] = node;
        return true;
    }
    //更新主链表中的节点
    bool updateMainNode(NodePtr node, const V& value)
    {
        node->setValue(value);
        moveToMainTail(node);
        return true;
    }
    //更新访问次数
    bool updateAccessCount(NodePtr node)
    {
        moveToMainTail(node);
        node->increaseAccessCount();

        return node->getAccessCount() >= transformThreshold;
    }
    //从主链表中移除节点
    void removeMainNode(NodePtr node)
    {
        removeNode(node);           
    }
    //从幽灵链表中移除节点
    void removeGhostNode(NodePtr node)
    {
        removeNode(node);       
    }
    //添加节点到幽灵缓存
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
    //淘汰主缓存中的最近最久未使用数据
    void evictLeastRecent()
    {
        if(mainCache.empty()||mainHead->next == mainTail)
        {
            return;
        }
        NodePtr node = mainHead->next;

        //添加到幽灵缓存
        if(ghostCache.size() >= ghostCapacity)
        {
            //空间不足，淘汰幽灵缓存节点
            evictOldestGhost();
        }
        addToGhost(node);

        //从主缓存中移除节点
        removeMainNode(node);
        mainCache.erase(node->getKey());
    }
    //淘汰幽灵缓存中的最旧数据
    void evictOldestGhost()
    {
        if(ghostCache.empty()||ghostHead->next == ghostTail)return;
    
        NodePtr node = ghostHead->next;
        removeGhostNode(node);
        ghostCache.erase(node->getKey());
    }

};
}
#endif //MYCACHE_ARCLRUCACHE_H