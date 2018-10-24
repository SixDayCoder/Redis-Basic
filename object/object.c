//
// Created by Administrator on 2018/10/9.
//

#include "object.h"
#include "sds.h"
#include "utlis.h"

#include <assert.h>
#include <limits.h>
#include <stdlib.h>


static int sdsKeyCompare(void *privdata, const void *lkey, const void* rkey)
{
    size_t lkeylen = sdslen((sds)lkey);
    size_t rkeylen = sdslen((sds)rkey);
    if(lkeylen != rkeylen) {
        return 0;
    }
    return memcmp(lkey, rkey, lkeylen) == 0;
}

//key只能是string或者是int
unsigned int encObjHash(const void* key)
{
    object* o = (object*)key;
    if(SDS_ENCODING_OBJECT(o)) {
        return MurmurHash2(o->ptr, (int)sdslen((sds)o->ptr));
    } else {
        if(o->encoding == OBJECT_ENCODING_INT) {
            char buf[32];
            int len = ll2string(buf, 32, (long)o->ptr);
            return MurmurHash2((unsigned char*)buf, len);
        } else {
            o = getRawDecodeObject(o);
            unsigned int hash = MurmurHash2(o->ptr, (int)sdslen((sds)o->ptr));
            //getRawDecodeObject会incrCnt
            decRefCnt(o);
            return hash;
        }
    }
}

int encObjKeyCompare(void *privdata, const void *lkey, const void* rkey)
{
    object* lo = (object*)lkey;
    object* ro = (object*)rkey;

    if(lo->encoding == OBJECT_ENCODING_INT && ro->encoding == OBJECT_ENCODING_INT) {
        return lo->ptr == ro->ptr;
    }
    lo = getRawDecodeObject(lo);
    ro = getRawDecodeObject(ro);
    int cmp = sdsKeyCompare(privdata, lo->ptr, ro->ptr);
    decRefCnt(lo);
    decRefCnt(ro);
    return cmp;
}

void encObjDtor(void* privdata, void* val)
{
    if(val == NULL) return;
    decRefCnt(val);
}

object* createObject(int type, void* ptr)
{
    object* obj = zmalloc(sizeof(*obj));

    obj->type = (unsigned int)type;
    obj->encoding = OBJECT_ENCODING_RAW;
    obj->ptr = ptr;
    obj->refcnt = 1;
    obj->lru = LRU();
    return obj;
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
            case OBJECT_TYPE_LIST:   releaseListObject(obj); break;
            case OBJECT_TYPE_SET:    releaseSetObject(obj); break;
            case OBJECT_TYPE_ZSET:   releaseZSetObject(obj); break;
            case OBJECT_TYPE_HASH:   releaseHashTableObject(obj); break;
            default: exit(1);
        }
        zfree(obj);
    }
    else
    {
        obj->refcnt--;
    }
}

//以新对象的形式返回输入对象的RAW编码,如果本身就是raw编码的那么输入对象的refcnt+1,返回输入对象
object* getRawDecodeObject(object* o)
{
    if(SDS_ENCODING_OBJECT(o)) {
        incRefCnt(o);
        return o;
    }
    //decode
    if(o->type == OBJECT_TYPE_STRING && o->encoding == OBJECT_ENCODING_INT) {
        char buf[32];
        int len = ll2string(buf, 32, (long)o->ptr);
        object* dec = createStringObject(buf, (size_t)len);
        return dec;
    }
    //error
    printf("unknown encoding type\n");
}