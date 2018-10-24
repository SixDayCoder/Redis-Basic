//
// Created by Administrator on 2018/10/24.
//

//哈希对象的实现

#include "object.h"
#include "ziplist.h"

//--------------------------------------------------------hash table object-----------------------------------------------------------//
//ziplist编码的哈希表对象
object* createHashObject()
{
    unsigned char* zl = ziplistNew();
    object* o = createObject(OBJECT_TYPE_HASH, zl);
    o->encoding = OBJECT_ENCODING_ZIPLIST;
    return o;
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