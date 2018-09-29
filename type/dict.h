//
// Created by Administrator on 2018/9/29.
//

#ifndef REDIS_BASIC_DICT_H
#define REDIS_BASIC_DICT_H

#include "memory.h"

#define HASH_TABLE_INIT_SIZE (4)
#define DICT_PROCESS_OK    ( 0)
#define DICT_PROCESS_ERROR (-1)
#define DICT_ITERATOR_IS_SAFE (1)


//哈希表的结点,键值对
typedef struct hashNode
{
    //键
    void* key;

    //值
    union
    {
        void* val;
        uint64_t u64;
        int64_t  s64;
    }v;

    //下一个键值对,用以构成链表
    struct hashNode* next;

}hashNode;

//哈希表
typedef struct hashTable
{

    //结点数组
    hashNode **table;

    //哈希表的size
    unsigned long size;

    //哈希表大小的掩码,用于计算索引值,总是等于 size - 1
    unsigned long sizemask;

    //哈希表已用结点的数目
    unsigned long used;

}hashTable;


//哈希表的特殊操作函数
typedef struct hashMethod
{
    //计算哈希值的函数
    unsigned int (*hashFunction)(const void* key);

    //复制键的函数
    void *(*keyDup) (void* privdata, const void* key);

    //复制值的函数
    void *(*valDup) (void* privdata, const void* obj);

    //键的比较函数
    int (*keyCompare)(void* privdata, const void* lkey, const void* rkey);

    //键的销毁函数
    void (*keyDestructor)(void* privdata, void* key);

    //销毁值的函数
    void (*valDestructor)(void* privadata, void* obj);

}hashMethod;


// 返回获取给定节点的键
#define HASH_NODE_GET_KEY(he) ((he)->key)

// 返回获取给定节点的值
#define HASH_NODE_GET_VAL(he) ((he)->v.val)

// 返回获取给定节点的有符号整数值
#define HASH_NODE_GET_SIGNED_INT(he) ((he)->v.s64)

// 返回给定节点的无符号整数值
#define HASH_NODE_GET_UNSIGNED_INT(he) ((he)->v.u64)


//字典的实现,使用两个哈希表
typedef struct dict
{
    //dict的操作函数
    hashMethod* method;

    //私有数据,作为method中某些函数的参数来使用
    void* privdata;

    //两个哈希表是为了实现渐进式rehash
    hashTable ht[2];

    //rehash的索引,当没有进行rehash时,该值是-1
    //进行rehash的时候,它是在ht[0].table的索引
    int rehashindex;

    //目前正在运行的安全迭代器的数目
    int iterators;
}dict;

typedef struct dictIterator
{
    dict* d;

    //正在被迭代的哈希表的id,只能是0或者是1
    int tableid;

    //迭代器当前指向的哈希表的索引的位置
    int index;

    //标记该迭代器是否安全,如果safe是1,那么迭代过程中可以update这个哈希表
    int safe;

    //当前迭代到的结点
    hashNode* entry;

    //当前迭代到的结点的下一个结点,因为在safe模式下可能会update结点,使用该指针防止结点丢失
    hashNode* nextEntry;

    //unsafe模式下的认证?
    long long fingerprint;
} dictIterator;

//释放给定字典节点的值
#define DICT_FREE_VAL(d, entry) do {\
    if((d)->method->valDestructor)\
        (d)->method->valDestructor((d)->privdata, (entry)->v.val)\
}while(0)

//设置给定字典节点的值
#define DICT_SET_VAL(d, entry, _val) do {\
    if((d)->method->valDup)\
        entry->v.val = (d)->method->valDup((d)->privdata, (_val));\
        else\
            entry->v.val = (_val);\
}while(0)

// 将一个有符号整数设为节点的值
#define DICT_SET_SIGNED_INT_VAL(entry, _val)  ( entry->v.s64 = _val)

// 将一个无符号整数设为节点的值
#define DICT_SET_UNSIGNED_INT_VAL(entry, _val) ( entry->v.u64 = _val)

// 释放给定字典节点的键
#define DICT_FREE_KEY(d, entry) \
    if ((d)->method->keyDestructor) \
        (d)->method->keyDestructor((d)->privdata, (entry)->key)

// 设置给定字典节点的键
#define DICT_SET_KEY(d, entry, _key) do { \
    if ((d)->method->keyDup) \
        entry->key = (d)->method->keyDup((d)->privdata, _key); \
    else \
        entry->key = (_key); \
} while(0)

// 比对两个key
#define DICT_COMPARE_KEY(d, lkey, rkey) ( ( (d)->method->keyCompare ) ? (d)->method->keyCompare((d)->privdata, lkey, rkey) : (lkey == rkey) )

// 计算给定键的哈希值
#define DICT_HASH_KEY(d, key) (d)->method->hashFunction(key)

// 返回给定字典的大小
#define DICT_TOTAL_SIZE(d) ((d)->ht[0].size+(d)->ht[1].size)

// 返回字典的已有节点数量
#define DICT_NODE_COUNT(d) ((d)->ht[0].used+(d)->ht[1].used)

// 查看字典是否正在 rehash
#define DICT_IS_REHASH(d) ((d)->rehashindex != -1)

//创建空字典
dict* dictCreate(hashMethod* method, void* privdata);

//扩展或者创建一个dict
//(1)如果字典的ht[0]是空的,新的哈希表就是0号
//(2)如果字典的ht[1]非空,新的哈希表设置为1号,并且置rehash标志,字典可进行rehash
//size不够大或者正在rehash,返回失败:成功创建新哈希表返回成功
int dictExpand(dict *d, unsigned long size);

//对字典执行n次渐进式的rehash
//返回0表示rehash完毕,否则表示仍然有键需要从0号哈希表移动到1号哈希表
int dictRehash(dict *d, int n);

//尝试创建一个新的结点,它的键是key,如果已经有这个key,返回NULL
hashNode *dictAddRaw(dict *d, void *key);

//尝试将给定的(key,val)插入到字典中,当且仅当key不在字典时才可以成功
int dictAdd(dict *d, void *key, void *val);
#endif //REDIS_BASIC_DICT_H
