//
// Created by Administrator on 2018/10/9.
//

#include "utlis.h"
#include <sys/time.h>

//unix时间戳:微秒
long long ustime()
{
    struct timeval tv;
    long long ust;
    gettimeofday(&tv, NULL);
    ust = ((long long)tv.tv_sec) * 1000000;
    ust += tv.tv_usec;
    return ust;
}

//unix时间戳:毫秒
long long mstime()
{
    return ustime()/1000;
}

//获取LRU时间
unsigned int LRU()
{
    //TODO:
    unsigned int ret =  ( (unsigned int)mstime() / 1000 ) & ( (1 << 24) - 1) ;
    return ret;
}

////16bit小端转大端
//void memrev16(void* p)
//{
//    unsigned char* x = p;
//    unsigned char  t = 0;
//    t = x[0];
//    x[0] = x[1];
//    x[1] = t;
//}
//
////32bit小端转大端
//void memrev32(void* p)
//{
//    unsigned char* x = p;
//    unsigned char  t = 0;
//    t = x[0];
//    x[0] = x[3];
//    x[3] = t;
//
//    t = x[1];
//    x[1] = x[2];
//    x[2] = t;
//}
//
////64bit小端转大端
//void memrev64(void* p)
//{
//    unsigned char* x = p;
//    unsigned char  t = 0;
//    t = x[0];
//    x[0] = x[7];
//    x[7] = t;
//
//    t = x[1];
//    x[1] = x[6];
//    x[6] = t;
//
//    t = x[2];
//    x[2] = x[5];
//    x[5] = t;
//
//    t = x[3];
//    x[3] = x[4];
//    x[4] = t;
//}
//
////16bit的unsigned int, 小端转大端
//uint16_t intrev16(uint16_t v)
//{
//    memrev16(&v);
//    return v;
//}
//
////32bit的unsigned int, 小端转大端
//uint32_t intrev32(uint32_t v)
//{
//    memrev32(&v);
//    return v;
//}
//
////64bit的unsigned int, 小端转大端
//uint64_t intrev64(uint64_t v)
//{
//    memrev64(&v);
//    return v;
//}
