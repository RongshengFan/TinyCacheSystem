#ifndef MYCACHE_CACHEPOLICY_H
#define MYCACHE_CACHEPOLICY_H

namespace mycache {

template <typename K, typename V>
class CachePolicy {
public:
	virtual ~CachePolicy() {}
    
    //向缓存中添加或更新键值对
    virtual void put(const K& key, const V& value) = 0;

    //根据键获取值，并更新该节点为最近使用的节点，访问成功返回true，否则返回false
	virtual bool get(const K& key,V& value) = 0;
    
    //重载的 get 方法，直接返回值
	virtual V get(const K& key) = 0;
};
}
#endif