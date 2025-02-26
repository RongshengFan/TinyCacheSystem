#MyCache
#高并发缓存系统 

## 项目介绍
本项目使用多个页面替换策略实现一个线程安全的缓存系统：
- LRU：最近最久未使用
- LFU：最近不经常使用
- ARC：自适应替换

对于LRU和LFU策略，在其基础的缓存策略上进行了相应的优化：

- LRU优化：
    - LRU-k：一定程度上防止热点数据被冷数据挤出容器而造成缓存污染等问题
    - LRU分片：对多线程下的高并发访问有性能上的优化

- LFU优化：
    - 引入最大平均访问频次：解决过去的热点数据最近一直没被访问，却仍占用缓存等问题
    - LFU分片：对多线程下的高并发访问有性能上的优化
    
##文件结构

——include:              #包含各种缓存策略的实现头文件
|-- CachePolicy.h       #缓存策略基类
|-- LruCache.h          #LRU、LRU-k、HashLRU缓存策略实现
|-- LfuCache.h          #LFU、HashLFU缓存策略实现 
|-- ArcNode.h           #ARC缓存策略链表节点构建
|-- ArcCache.h          #ARC缓存策略实现
|-- ArcLruCache.h       #ARC-LRU部分缓存策略实现
|-- ArcLfuCache.h       #ARC-LFU部分缓存策略实现
——test：                #测试代码
|-- test1.cpp           #测试代码1
|-- test2.cpp           #测试代码2
|-- test3.cpp           #测试代码3
——image:                #测试图片        
|-hit-rate-test:        #命中率测试图片
|-high-QPS-test：       #并发性测试图片 

## 系统环境 
——Windows 11
——VsCode
## 编译
也可以在linux下编译：
创建一个build文件夹并进入
```
mkdir build && cd build
```
生成构建文件
```
cmake ..
```
构建项目
```
make
```
如果要清理生成的可执行文件
```
make clean
```

## 运行
```
./main
```

## 测试结果
不同缓存策略缓存命中率测试对比结果见image文件夹下
（ps: 该测试代码只是尽可能地模拟真实的访问场景，但是跟真实的场景仍存在一定差距，测试结果仅供参考。）

