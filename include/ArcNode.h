#ifndef MYCACHE_ARCNODECACHE_H
#define MYCACHE_ARCNODECACHE_H

#include <memory>

namespace mycache {

template <typename K, typename V>
class ArcNode
{
private:
    K key;
    V value;
    bool dirty;
    size_t accessCount;                 //访问次数
    std::shared_ptr<ArcNode<K,V>> prev;
    std::shared_ptr<ArcNode<K,V>> next;
public:
    ArcNode(const K& k, const V& v) 
    : key(k)
    , value(v)
    , dirty(false)
    , accessCount(1)
    , prev(nullptr)
    , next(nullptr) {}

    //必要的访问工具
    K getKey() const { return key; }
    V getValue() const { return value; }
    size_t getAccessCount() const { return accessCount; }   
    void setValue(const V& v) { value = v; }
    void increaseAccessCount() { accessCount++; }

    template<typename Key, typename Value> friend class ArcLruPart;
    template<typename Key, typename Value> friend class ArcLfuPart;
};
}
#endif //MYCACHE_ARCNODECACHE_H