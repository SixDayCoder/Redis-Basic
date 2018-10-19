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

//void TestIntSet()
//{
//    intset* is = intsetNew();
//    printf("init\n");
//    printf("len is %u, totalsize is %lu, encoding is %u\n", is->length, intsetBlobLen(is), is->encoding);
//
//    uint8_t success = 0;
//    for(int i = 0 ; i < 100; ++i)
//    {
//        int64_t val = (i % 2 == 0) ? ( (i - 31432) * 123 + 432 ) : (i * 3124 - 78);
//        is = intsetAdd(is, val, &success);
//    }
//    int count = 0;
//    //upgrade
//    printf("--------------------------------------------------------------------------------------------\n");
//    int64_t v = 312312331343432;
//    is = intsetAdd(is, v, &success);
//    printf("after upgrade : len is %u, totalsize is %lu, encoding is %u\n", is->length, intsetBlobLen(is), is->encoding);
//    v = 0;
//    intsetGet(is, is->length - 1, &v);
//    printf("max is %lld\n", v);
//    for(int i = 0 ; i < is->length; i++)
//    {
//        int64_t val = 0;
//        intsetGet(is, i, &val);
//        printf("%lld ", (long long)val);
//        count++;
//        if(count == 10)
//        {
//            printf("\n");
//            count = 0;
//        }
//    }
//}