//
// Created by Administrator on 2018/10/9.
//

#ifndef REDIS_BASIC_OBJECT_H
#define REDIS_BASIC_OBJECT_H


#define LRU_BITS (24)

enum
{
    OBJECT_TYPE_STRING = 0,
    OBJECT_TYPE_LIST = 1,
    OBJECT_TYPE_SET = 2,
    OBJECT_TYPE_ZSET = 3,
    OBJECT_TYPE_HASH = 4,
};

enum
{
    OBJECT_ENCODING_RAW = 0,
    OBJECT_ENCODING_INT = 1,
    OBJECT_ENCODING_HASH_TABLE = 2,
    OBJECT_ENCODING_ZIPMAP = 3,
    OBJECT_ENCODING_LINKED_LIST = 4,
    OBJECT_ENCODING_ZIPLIST = 5,
    OBJECT_ENCODING_INTSET = 6,
    OBJECT_ENCODING_SKIPLIST = 7,
    OBJECT_ENCODING_EMBSTR = 8,
};

typedef  struct object
{
    //类型
    unsigned type : 4;

    //编码方式
    unsigned encoding : 4;

    //最后一次被访问时间
    unsigned  lru  : LRU_BITS;

    //引用计数
    int refcnt;

    //指向实际值的指针
    void* ptr;

} object ;

object* createObject(int type, void* ptr);

void releaseStringObject(object* obj);

void releaseListObject(object* obj);

void releaseSetObject(object* obj);

void releaseZSetObject(object* obj);

void releaseHashTableObject(object* obj);

void incRefCnt(object* obj);

void decRefCnt(object* obj);

//对象转换为字符串进行比较
int compareStringObject(object* lhs, object* rhs);

//如果两个对象在字符串形式上相等返回1
int equalStringObject(object* lhs, object* rhs);

#endif //REDIS_BASIC_OBJECT_H
