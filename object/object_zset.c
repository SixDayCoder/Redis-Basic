//
// Created by Administrator on 2018/10/24.
//

//有序列表对象的实现

#include "object.h"
#include "ziplist.h"

//--------------------------------------------------------hash table object-----------------------------------------------------------//

hashMethod ZSetObjectDictHashMethod = {
        encObjHash,   //hash calc
        NULL,            //key copy ctor
        NULL,            //val copy ctor
        encObjKeyCompare, //key compare
        encObjDtor,       // key dtor
        NULL,                 // val dtor
};

//skiplist编码的zset
object* createZSetObject()
{
    zset* zs = zmalloc(sizeof(*zs));
    zs->dict = dictCreate(&ZSetObjectDictHashMethod, NULL);
    zs->sl = slCreate();

    object* o = createObject(OBJECT_TYPE_ZSET, zs);
    //默认SKIPLIST编码
    o->encoding = OBJECT_ENCODING_SKIPLIST;

    return o;
}

//ziplist编码的zset
object* createZSetZiplistObject()
{
    unsigned char* zl = ziplistNew();
    object* o = createObject(OBJECT_TYPE_ZSET, zl);
    o->encoding = OBJECT_ENCODING_ZIPLIST;
    return o;
}

void releaseZSetObject(object* obj)
{
    zset* zs = NULL;
    switch(obj->type) {
        case OBJECT_ENCODING_SKIPLIST:
            zs = obj->ptr;
            dictRelease(zs->dict);
            slRelease(zs->sl);
            zfree(zs);
            break;
        case OBJECT_ENCODING_ZIPLIST:
            zfree(obj->ptr);
            break;
        default:
            exit(1);
    }
}