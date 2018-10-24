//
// Created by Administrator on 2018/10/9.
//

#ifndef REDIS_BASIC_OBJECT_H
#define REDIS_BASIC_OBJECT_H

#include "dict.h"
#include "skiplist.h"

#include <stdint.h>
#include <inttypes.h>
#include <endian.h>

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
    OBJECT_ENCODING_ZIPMAP = 3,  //弃用
    OBJECT_ENCODING_LINKED_LIST = 4,
    OBJECT_ENCODING_ZIPLIST = 5,
    OBJECT_ENCODING_INTSET = 6,
    OBJECT_ENCODING_SKIPLIST = 7,
    OBJECT_ENCODING_EMBSTR = 8,
};

#define SDS_ENCODING_OBJECT(objptr) (objptr->encoding == OBJECT_ENCODING_RAW || objptr->encoding == OBJECT_ENCODING_EMBSTR)

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


int encObjKeyCompare(void *privdata, const void *lkey, const void* rkey);

void encObjDtor(void* privdata, void* val);

unsigned int encObjHash(const void* key);

object* createObject(int type, void* ptr);

void incRefCnt(object* obj);

void decRefCnt(object* obj);

object* getRawDecodeObject(object* o);

//---------------------------------------------------------string object---------------------------------------------------------//

object* createRawStringObject(char* ptr, size_t len);

object* createEmbeddedStringObject(char* ptr, size_t len);

object* createStringObject(char* ptr, size_t len);

object* createStringObjectFromLL(long long value);

object* createaStringObjectFromLongDouble(long double value);

object* copyStringObject(object* o);

int compareStringObject(object* lhs, object* rhs);

int equalStringObject(object* lhs, object* rhs);

void releaseStringObject(object* obj);

//---------------------------------------------------------list object---------------------------------------------------------//

object* createListObject();

object* createZiplistObject();

void releaseListObject(object* obj);

//---------------------------------------------------------set object---------------------------------------------------------//

object* createSetObject();

object* createIntsetObject();

void releaseSetObject(object* obj);

//---------------------------------------------------------zset object---------------------------------------------------------//

typedef struct zset
{
    //key是name,value是分数,支持O(1)复杂度按key取分值
    dict* dict;

    //支持O(logn)按分值取rank
    skiplist* sl;
}zset;

object* createZSetObject();

object* createZSetZiplistObject();

void releaseZSetObject(object* obj);

//---------------------------------------------------------hash object---------------------------------------------------------//

object* createHashObject();

void releaseHashTableObject(object* obj);

#endif //REDIS_BASIC_OBJECT_H
