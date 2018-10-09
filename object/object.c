//
// Created by Administrator on 2018/10/9.
//

#include "object.h"
#include "sds.h"
#include "list.h"
#include "dict.h"

#include <stdlib.h>

object* createObject(int type, void* ptr)
{
    object* obj = zmalloc(sizeof(*obj));

    obj->type = (unsigned int)type;
    obj->encoding = OBJECT_ENCODING_RAW;
    obj->ptr = ptr;
    obj->refcnt = 1;
    obj->lru = 0;
}

void releaseStringObject(object* obj)
{
    if(obj->encoding == OBJECT_ENCODING_RAW)
    {
        sdsfree(obj->ptr);
    }
}

void releaseListObject(object* obj)
{
    switch(obj->encoding)
    {
        case OBJECT_ENCODING_LINKED_LIST:
            listRealease( (list*)obj->ptr);
            break;
        case OBJECT_ENCODING_ZIPLIST:
            zfree(obj->ptr);
            break;
        default:
            exit(1);
    }
}

void releaseSetObject(object* obj)
{
    switch(obj->encoding)
    {
        case OBJECT_ENCODING_HASH_TABLE:
            dictRelease( (dict*) obj->ptr);
            break;
        case OBJECT_ENCODING_INTSET:
            zfree(obj->ptr);
            break;
        default:
            exit(1);
    }
}

void releaseZSetObject(object* obj)
{
    //TODO:
}

void releaseHashTableObject(object* obj)
{
    switch(obj->encoding)
    {
        case OBJECT_ENCODING_HASH_TABLE:
            dictRelease( (dict*) obj->ptr);
            break;
        case OBJECT_ENCODING_ZIPLIST:
            zfree(obj->ptr);
            break;
        default:
            exit(1);
    }
}

void incRefCnt(object* obj)
{
    obj->refcnt++;
}

void decRefCnt(object* obj)
{
    if(obj->refcnt <= 0) exit(1);
    if(obj->refcnt == 1)
    {
        switch (obj->type)
        {
            case OBJECT_TYPE_STRING: releaseStringObject(obj); break;
            case OBJECT_TYPE_LIST: releaseListObject(obj); break;
            case OBJECT_TYPE_SET:  releaseSetObject(obj); break;
            case OBJECT_TYPE_ZSET: releaseZSetObject(obj); break;
            case OBJECT_TYPE_HASH: releaseHashTableObject(obj); break;
            default: exit(1);
        }
        zfree(obj);
    }
    else
    {
        obj->refcnt--;
    }
}

//对象转换为字符串进行比较
int compareStringObject(object* lhs, object* rhs)
{
    //TODO:
    return 0;
}

//如果两个对象在字符串形式上相等返回1
int equalStringObject(object* lhs, object* rhs)
{
    if(lhs->encoding == OBJECT_ENCODING_INT && rhs->encoding == OBJECT_ENCODING_INT)
    {
        return lhs->ptr == rhs->ptr;
    }
    else
    {
        //转换为字符串对象进行比较
        return compareStringObject(lhs, rhs) == 0;
    }
}