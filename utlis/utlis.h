//
// Created by Administrator on 2018/10/9.
//

#ifndef REDIS_BASIC_UTLIS_H
#define REDIS_BASIC_UTLIS_H

#include <stdint.h>
#include <sys/types.h>

//unix时间戳:微秒
long long ustime();

//unix时间戳:毫秒
long long mstime();

//获取LRU时间
unsigned int LRU();

////16bit小端转大端
//void memrev16(void* p);
//
////32bit小端转大端
//void memrev32(void* p);
//
////64bit小端转大端
//void memrev64(void* p);
//
////16bit的unsigned int, 小端转大端
//uint16_t intrev16(uint16_t v);
//
////32bit的unsigned int, 小端转大端
//uint32_t intrev32(uint32_t v);
//
////64bit的unsigned int, 小端转大端
//uint64_t intrev64(uint64_t v);

//字符串转为longlong,成功返回1否则返回0
int string2ll(const char* s, size_t slen, long long* val);

//获取当前时间的秒和毫秒
void getTime(long long* second, long long* millisecond);
#endif //REDIS_BASIC_UTLIS_H
