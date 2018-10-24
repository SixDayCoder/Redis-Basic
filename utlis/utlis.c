//
// Created by Administrator on 2018/10/9.
//

#include "utlis.h"
#include <string.h>
#include <limits.h>
#include <sys/time.h>
#include <time.h>

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
    unsigned int ret =  ( (unsigned int)mstime() / 1000 ) & ( (1 << 24) - 1) ;
    return ret;
}

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

//long long转为字符串,成功返回1,否则返回0
int ll2string(char *s, size_t len, long long value)
{
    //31 + 1(符号位)
    char buf[32], *p;
    unsigned long long v;
    size_t l;

    if (len == 0) return 0;
    v = (value < 0) ? -value : value;
    p = buf + 31; /* point to the last character */
    do {
        *p-- = '0' + (v % 10);
        v /= 10;
    } while(v);
    if (value < 0) {
        *p-- = '-';
    }
    p++;

    l = 32 - (p - buf);
    if (l + 1 > len) {
        l = len-1; /* Make sure it fits, including the nul term */
    }
    memcpy(s, p, l);
    s[l] = '\0';
    return (int)l;
}

//获取当前时间的秒和毫秒
void getTime(long long* second, long long* millisecond)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    *second = tv.tv_sec;
    *millisecond = tv.tv_usec / 1000;
}