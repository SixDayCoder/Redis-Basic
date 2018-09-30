//
// Created by Administrator on 2018/9/29.
//

#ifndef REDIS_BASIC_DICT_H
#define REDIS_BASIC_DICT_H

#include "memory.h"
#include "hash.h"

#define DICT_PROCESS_OK    ( 0)
#define DICT_PROCESS_ERROR (-1)

//字典私有数据不使用时编译错误
#define DICT_PRIVDATA_NOTUSED(V) ((void) V)


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
    hashNode* curr;

    //当前迭代到的结点的下一个结点,因为在safe模式下可能会update结点,使用该指针防止结点丢失
    hashNode* next;

    //unsafe模式下的认证?
    long long fingerprint;
} dictIterator;

//判断字典是否是空
#define DICT_IS_EMPTY(d) ( d->ht[0].size == 0 )

// 查看字典是否正在 rehash
#define DICT_IS_REHASH(d) ((d)->rehashindex != -1)

// 返回给定字典的大小
#define DICT_TOTAL_SIZE(d) ((d)->ht[0].size+(d)->ht[1].size)

// 返回字典的已有节点数量
#define DICT_NODE_COUNT(d) ((d)->ht[0].used+(d)->ht[1].used)

//释放给定字典节点的值
#define DICT_FREE_HASH_NODE_VAL(d, node) do {\
    if((d)->method->valDestructor)\
        (d)->method->valDestructor((d)->privdata, (node)->v.val);\
}while(0)

//设置给定字典节点的值
#define DICT_SET_HASH_NODE_VAL(d, node, _val) do {\
    if((d)->method->valDup)\
        node->v.val = (d)->method->valDup((d)->privdata, (_val));\
    else\
        node->v.val = (_val);\
}while(0)

// 释放给定字典节点的键
#define DICT_FREE_HASH_NODE_KEY(d, node) do {\
    if ((d)->method->keyDestructor) \
        (d)->method->keyDestructor((d)->privdata, (node)->key);\
} while(0)

// 设置给定字典节点的键
#define DICT_SET_HASH_NODE_KEY(d, node, _key) do { \
    if ((d)->method->keyDup) \
        node->key = (d)->method->keyDup((d)->privdata, _key); \
    else \
        node->key = (_key); \
} while(0)

// 比对两个key
#define DICT_COMPARE_KEY(d, lkey, rkey) ( ( (d)->method->keyCompare ) ? (d)->method->keyCompare((d)->privdata, lkey, rkey) : (lkey == rkey) )

// 计算给定键的哈希值
#define DICT_HASH_KEY(d, key) (d)->method->hashFunction(key)

//创建空字典
dict* dictCreate(hashMethod* method, void* privdata);

//释放字典
void dictRelease(dict *d);

//清空字典中哈希表所有的结点
void dictClear(dict* d);

//计算安全指纹
long long dictFingerprint(dict *d);

//扩展或者创建一个dict
//(1)如果字典的ht[0]是空的,新的哈希表就是0号
//(2)如果字典的ht[1]非空,新的哈希表设置为1号,并且置rehash标志,字典可进行rehash
//size不够大或者正在rehash,返回失败:成功创建新哈希表返回成功
int dictExpand(dict *d, unsigned long size);

//对字典执行n次渐进式的rehash
//返回0表示rehash完毕,否则表示仍然有键需要从0号哈希表移动到1号哈希表
int dictRehash(dict *d, int n);

//在字典中找到键为key的hashnode
hashNode* dictFind(dict *d, const void *key);

//尝试创建一个新的结点,它的键是key,如果已经有这个key,返回NULL
hashNode *dictAddKey(dict *d, void *key);

//尝试将给定的(key,val)插入到字典中,当且仅当key不在字典时才可以成功
int dictAddPair(dict *d, void *key, void *val);

//尝试创建一个新的结点,它的键是key,如果已经有这个key就返回那个node,否则会新加一个结点
hashNode* dictReplaceKey(dict *d, void *key);

//将给定的键值对插入到字典中,如果已经存在那么替换
int dictReplacePair(dict *d, void *key, void *val);

//删除键为key的结点,并且调用释放函数来释放key
int dictDelete(dict *d, const void *key);

//删除键为key的结点,不调用释放函数来释放key
int dictDeleteNoFree(dict *d, const void *key);

//如果结点值不为空返回结点的值
void* dictFetchValue(dict *d, const void *key);

//获取字典的不安全迭代器
dictIterator *dictGetIterator(dict *d);

//获取字典的安全迭代器
dictIterator *dictGetSafeIterator(dict *d);

//获取迭代器指向的字典的下一个哈希结点
hashNode* dictNext(dictIterator *iter);

//释放迭代器
void dictReleaseIterator(dictIterator *iter);

#endif //REDIS_BASIC_DICT_H
