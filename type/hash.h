//
// Created by Administrator on 2018/9/30.
//

#ifndef REDIS_BASIC_HASH_H
#define REDIS_BASIC_HASH_H


#include "memory.h"
#define HASH_TABLE_INIT_SIZE (4)


//哈希计算算法:by Austin Appleby
unsigned int MurmurHash2(const void *key, int len);

//哈希计算算法:time33 DJBX33A，Bernstein's hash
unsigned int Time33Hash(const unsigned char* buf, int len);

//哈希计算算法:Thomas Wang's 32 bit Mix Function
unsigned int dictIntHashFunction(unsigned int key);

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
    void (*valDestructor)(void* privdata, void* obj);

}hashMethod;


// 返回获取给定节点的键
#define HASH_NODE_GET_KEY(he) ((he)->key)

// 返回获取给定节点的值
#define HASH_NODE_GET_VAL(he) ((he)->v.val)

// 返回获取给定节点的有符号整数值
#define HASH_NODE_GET_SIGNED_INT(he) ((he)->v.s64)

// 返回给定节点的无符号整数值
#define HASH_NODE_GET_UNSIGNED_INT(he) ((he)->v.u64)

// 将一个有符号整数设为节点的值
#define HASH_NODE_SET_SIGNED_INT_VAL(entry, _val)  ( entry->v.s64 = _val)

// 将一个无符号整数设为节点的值
#define HASH_NODE_SET_UNSIGNED_INT_VAL(entry, _val) ( entry->v.u64 = _val)

//初始化哈希表的各项属性
void hashTableInit(hashTable* ht);

//计算第一个大于等于size的2的整数次幂的值,用作哈希表的值
unsigned long hashNextPowerOf2(unsigned long size);


#endif //REDIS_BASIC_HASH_H
