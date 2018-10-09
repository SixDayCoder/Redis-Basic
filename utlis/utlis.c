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