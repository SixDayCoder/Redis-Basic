//
// Created by Administrator on 2018/10/9.
//

#ifndef REDIS_BASIC_UTLIS_H
#define REDIS_BASIC_UTLIS_H

//unix时间戳:微秒
long long ustime();

//unix时间戳:毫秒
long long mstime();

//获取LRU时间
unsigned int LRU();
#endif //REDIS_BASIC_UTLIS_H
