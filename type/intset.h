//
// Created by Administrator on 2018/10/11.
//

#ifndef REDIS_BASIC_INTSET_H
#define REDIS_BASIC_INTSET_H

#include "memory.h"


#define INTSET_ENCODING_INT16 (sizeof(int16_t))
#define INTSET_ENCODING_INT32 (sizeof(int32_t))
#define INTSET_ENCODING_INT64 (sizeof(int64_t))

typedef struct intset
{
    //编码方式,只能是上述的三种的某一种
    uint32_t encoding;

    //整数集合的元素数目
    uint32_t length;

    //保存元素的数组
    int8_t contents[];

}intset;

//创建新的整数集合
intset *intsetNew(void);

//整数集合中添加新的元素,是否成功写在success中
intset *intsetAdd(intset *is, int64_t value, uint8_t *success);

//整数集合中删除新的元素,是否成功写在success中
intset *intsetRemove(intset *is, int64_t value, int *success);

//整数集合中是否存在值为value的元素
uint8_t intsetExist(intset *is, int64_t value);

//随机返回整数集合中的一个元素
int64_t intsetRandom(intset *is);

//获取整数集合中下标为pos的元素,结果放在value中
uint8_t intsetGet(intset *is, uint32_t pos, int64_t *value);

//整数集合占用内存的大小
size_t intsetBlobLen(intset *is);

//判断value的编码方式
uint32_t intsetValEncodingType(int64_t v);


#endif //REDIS_BASIC_INTSET_H
