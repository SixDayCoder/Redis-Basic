//
// Created by Administrator on 2018/10/9.
//

#ifndef REDIS_BASIC_UTLIS_H
#define REDIS_BASIC_UTLIS_H

#include <stdint.h>
#include <sys/types.h>

//unix时间戳:微秒
typedef long long ustime_t;
long long ustime();

//unix时间戳:毫秒
typedef long long mstime_t;
long long mstime();

//获取LRU时间
unsigned int LRU();

//字符串转为longlong,成功返回1否则返回0
int string2ll(const char* s, size_t slen, long long* val);

//long long转为字符串,成功返回1,否则返回0
int ll2string(char *s, size_t len, long long value);

//获取当前时间的秒和毫秒
void getTime(long long* second, long long* millisecond);
#endif //REDIS_BASIC_UTLIS_H
