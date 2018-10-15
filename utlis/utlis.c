//
// Created by Administrator on 2018/10/9.
//

#include "utlis.h"
#include <limits.h>
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

//字符串转为longlong,成功返回1否则返回0
int string2ll(const char* s, size_t slen, long long* val)
{
    if(s == NULL || val == NULL || slen == 0) return 0;

    const char* p = s;
    size_t plen = 0;
    int negative = 0;

    //解析0的特殊情况
    if(slen == 1 && p[0] == '0')
    {
        *val = 0;
        return 1;
    }

    if(p[0] == '-')
    {
        negative = 1;
        plen++;
        p++;
        //只有负号的不合法情况
        if(plen == slen) return 0;
    }

    unsigned long v = 0;
    //用最高位数字初始化v
    if(p[0] >= '1' && p[0] <= '9')
    {
        v = (unsigned long)(p[0] - '0');
        p++;
        plen++;
    }
    else return 0;

    while(plen < slen && p[0] >= '1' && p[0] <= '9')
    {
        if(v > (ULLONG_MAX / 10)) return 0;//溢出
        v *= 10;

        if(v > (ULLONG_MAX - (p[0] - '0'))) return 0; //溢出
        v += (unsigned long)(p[0] - '0');

        p++;
        plen++;
    }

    //如果字符串有残留说明解析有问题,失败
    if(plen < slen) return 0;

    if(negative)
    {
        if(v > ( (unsigned long long)(-(LLONG_MIN + 1)) + 1) ) return 0;//转换为负数会溢出
        *val = -v;
    }
    else
    {
        if(v > LLONG_MAX) return 0;//溢出
        *val = v;
    }
    return 1;
}