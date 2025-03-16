#ifndef MYCACHE_ARCCACHE_H
#define MYCACHE_ARCCACHE_H

#include <cmath>
#include <vector>
#include <memory>
#include <unordered_map>
#include <thread>
#include <mutex>

#include "CachePolicy.h"
#include "ArcNode.h"
#include "ArcLruPart.h"
#include "ArcLfuPart.h"

namespace mycache {

template <typename K, typename V> 
class ArcCache : public CachePolicy<K, V>
{
private:
    size_t capacity;                            //缓存容量
    size_t transformThreshold;                  //转换阈值
    std::unique_ptr<ArcLruPart<K, V>> lruPart;  //LRU部分
    std::unique_ptr<ArcLfuPart<K, V>> lfuPart;  //LFU部分

private:
    //检查节点是否在幽灵缓存中
    bool checkGhostCache(const K& key)
    {
        bool inGhost = false;
        
        if(lruPart->checkGhost(key))
        {   
            //数据在LRU幽灵缓存中
            if(lfuPart->decreaseCapacity())
            {   
                //LFU容量足够，减少LFU容量，增加LRU容量
                lruPart->increaseCapacity();
            } 
            inGhost = true;
        }
        else if (lfuPart->checkGhost(key))
        {   
            //数据在LFU幽灵缓存中
            if(lruPart->decreaseCapacity())
            {   
                //LRU容量足够，减少LRU容量，增加LFU容量
                lfuPart->increaseCapacity();
            }
            inGhost = true;
        }
        return inGhost;
    }

public:
    explicit ArcCache(size_t Capacity = 10, size_t TransformThreshold = 3)
    : capacity(Capacity)
    , transformThreshold(TransformThreshold)
    , lruPart(std::make_unique<ArcLruPart<K, V>>(Capacity/2, TransformThreshold))
    , lfuPart(std::make_unique<ArcLfuPart<K, V>>(Capacity/2, TransformThreshold))
    {}

    ~ArcCache() override = default;
    //插入/更新缓存
    void put(const K& key, const V& value) override
    {
        bool inGhost = checkGhostCache(key);

        if(!inGhost)
        {   
            //数据不在幽灵缓存中,同时插到LRU部分和LFU部分中
            if(lruPart->put(key, value))
            {
                lfuPart->put(key, value);
            }
        }
        else 
        {
            //数据在幽灵缓存中，直接插入到LRU部分中,访问次数为1
            lruPart->put(key, value);
        }
    }
    //获取缓存
    bool get(const K& key, V& value) override
    {
        checkGhostCache(key);                   //检查是否在幽灵缓存中，同时更新容量

        bool sholdTransform = false;            //是否需要转移到LFU部分

        if(lruPart->get(key, value, sholdTransform))
        {   
            //数据在LRU部分中,
            if(sholdTransform)
            {   
                //访问频次达到阈值，需要转移到LFU部分
                lfuPart->put(key, value);
            }
            return true;
        }    
        else 
        {   
            //数据不在LRU部分中，查找LFU部分
            return lfuPart->get(key, value);
        }
    }
    //获取缓存值
    V get(const K& key) override
    {
        V value{};
        get(key, value);
        return value;
    }
};

}
#endif //MYCACHE_ARCCACHE_H