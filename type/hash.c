//
// Created by Administrator on 2018/9/30.
//

#include "hash.h"
#include <limits.h>

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