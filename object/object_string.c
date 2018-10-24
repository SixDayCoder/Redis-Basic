//
// Created by Administrator on 2018/10/24.
//

//字符串对象的实现

#include "object.h"
#include "sds.h"
#include "utlis.h"

#include <limits.h>
#include <assert.h>

//--------------------------------------------------------string object-----------------------------------------------------------//
#define OBJECT_ENCODING_EMBSTR_MAX_SIZE (39)

//ptr指向sds,创建RAW_ENCODING
object* createRawStringObject(char* ptr, size_t len)
{
    return createObject(OBJECT_ENCODING_RAW, sdsnewlen(ptr, len));
}

//嵌入式字符串的内存和object一起分配,该字符串不可修改,一起分配节约内存
object* createEmbeddedStringObject(char* ptr, size_t len)
{
    object* obj = zmalloc(sizeof(*obj) + sizeof(struct sdshdr) + len + 1);
    //obj + 1 == obj + sizeof(obj) -> sdshdr分配内存的起始位置
    struct sdshdr* sh = (void*)(obj + 1);

    obj->type = OBJECT_TYPE_STRING;
    obj->encoding = OBJECT_ENCODING_EMBSTR;
    //sh + 1 == sh + sizeof(sh) -> sh->buf分配内存的起始位置
    obj->ptr = sh + 1;
    obj->refcnt = 1;
    obj->lru = LRU();

    sh->len = len;
    sh->free = 0;
    if(ptr) {
        memcpy(sh->buf, ptr, len);
        sh->buf[len] = '\0';
    } else {
        memset(sh->buf, 0, len + 1);
    }
    return obj;
}


//根据len的长度确定是分配RAW_STRING还是EMBSTR
object* createStringObject(char* ptr, size_t len)
{
    if(len <= OBJECT_ENCODING_EMBSTR_MAX_SIZE) {
        return createEmbeddedStringObject(ptr, len);
    } else {
        return createRawStringObject(ptr, len);
    }
}

//根据传入的整数值创建字符串对象,编码可以是INT或者是RAW
object* createStringObjectFromLL(long long value)
{
    //TODO:如果在共享整数的范围,直接返回共享对象
    object* o = NULL;
    if(value >= LONG_MIN && value <= LONG_MAX) {
        o = createObject(OBJECT_TYPE_STRING, NULL);
        o->encoding = OBJECT_ENCODING_INT;
        //no need to allocate memory
        //if value == 12345, o->ptr = 0x3039
        o->ptr = (void*)((long)value);
    } else {
        o = createObject(OBJECT_TYPE_STRING, sdsfromlonglong(value));
    }
    return o;
}

//根据传入的long double值创建字符串对象
object* createaStringObjectFromLongDouble(long double value)
{
    char buf[256];
    //使用17位精度小数
    int len = snprintf(buf, sizeof(buf), "%.17LF", value);

    //移除尾部的0,例如3.140000->3.14
    if(strchr(buf, '.') != NULL) {
        char* p = buf + len - 1;
        while(*p == '0') {
            p--;
            len--;
        }
        if(*p == '.') len--;
    }
    return createStringObject(buf, len);
}

//复制字符串对象,两者拥有相同的编码,输出对象不是共享对象(输出对象的refcount总是1)
object* copyStringObject(object* o)
{
    assert(o->type == OBJECT_TYPE_STRING);

    object* obj = NULL;
    switch(o->encoding) {
        case OBJECT_ENCODING_RAW:
            return createRawStringObject(o->ptr, sdslen(o->ptr));
        case OBJECT_ENCODING_EMBSTR:
            return createEmbeddedStringObject(o->ptr, sdslen(o->ptr));
        case OBJECT_ENCODING_INT:
            obj = createObject(OBJECT_TYPE_STRING, NULL);
            obj->encoding = OBJECT_ENCODING_INT;
            obj->ptr = o->ptr;
            return obj;
        default:
            printf("copyStringObejct : wrong encoding\n");
            break;
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

void releaseStringObject(object* obj)
{
    if(obj->encoding == OBJECT_ENCODING_RAW)
    {
        sdsfree(obj->ptr);
    }
}