//
// Created by Administrator on 2018/10/24.
//

//集合对象的实现
#include "object.h"
#include "intset.h"
#include "hash.h"

//--------------------------------------------------------set object-----------------------------------------------------------//

hashMethod SetObjectDictHashMethod = {
        encObjHash,   //hash calc
        NULL,            //key copy ctor
        NULL,            //val copy ctor
        encObjKeyCompare, //key compare
        encObjDtor,       // key dtor
        NULL,                 // val dtor
};

object* createSetObject()
{
    dict* d = dictCreate(&SetObjectDictHashMethod, NULL);
    object* o = createObject(OBJECT_TYPE_SET, d);
    o->encoding = OBJECT_ENCODING_HASH_TABLE;
    return o;
}

object* createIntsetObject()
{
    intset* is = intsetNew();
    object* o = createObject(OBJECT_TYPE_SET, is);
    o->encoding = OBJECT_ENCODING_INTSET;
    return o;
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