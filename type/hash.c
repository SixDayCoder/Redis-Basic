//
// Created by Administrator on 2018/9/30.
//

#include "hash.h"
#include <limits.h>

//哈希计算算法:by Austin Appleby
unsigned int MurmurHash2(const void *key, int len)
{
    uint32_t seed = 5381;
    const uint32_t magic = 0x5bd1e995;
    const int rbits = 24;

    uint32_t hash = seed ^ len;
    const unsigned char *data = (const unsigned char*)key;
    while(len >= 4)
    {
        uint32_t k = *(uint32_t*)data;
        k *= magic;
        k ^= k >> rbits;
        k *= magic;

        hash *= magic;
        hash ^= k;

        data += 4;
        len -= 4;
    }

    /*余下的字节处理*/
    switch(len)
    {
        case 3: hash ^= data[2] << 16;
            break;
        case 2: hash ^= data[1] << 8;
            break;
        case 1: hash ^= data[0]; hash *= magic;
            break;
        default:break;
    };

    //混合
    hash ^= hash >> 13;
    hash *= magic;
    hash ^= hash >> 15;

    return (unsigned int)hash;
}

//哈希计算算法:time33 DJBX33A，Bernstein's hash,非常适合
unsigned int Time33Hash(const unsigned char* buf, int len)
{
    unsigned int hash = 5381;
    while(len--)
    {
        //hash * 33 + c
        hash = ( (hash << 5) + hash ) + (tolower(*buf++));
    }
    return hash;
}

unsigned int IntHash(unsigned int key)
{
    key += ~(key << 15);
    key ^=  (key >> 10);
    key +=  (key << 3);
    key ^=  (key >> 6);
    key += ~(key << 11);
    key ^=  (key >> 16);
    return key;
}

//初始化哈希表的各项属性
void hashTableInit(hashTable* ht)
{
    ht->table = NULL;
    ht->size = 0;
    ht->sizemask = 0;
    ht->used = 0;
}

//计算第一个大于等于size的2的整数次幂的值,用作哈希表的值
unsigned long hashNextPowerOf2(unsigned long size)
{
    if(size >= LONG_MAX) return LONG_MAX;

    unsigned long i = HASH_TABLE_INIT_SIZE;
    while(1)
    {
        if(i >= size) return i;
        i *= 2;
    }
}