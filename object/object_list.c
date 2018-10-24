//
// Created by Administrator on 2018/10/24.
//

//列表对象的实现

#include "object.h"
#include "list.h"
#include "ziplist.h"

//--------------------------------------------------------list object-----------------------------------------------------------//

static void* listFreeMethodWrapper(void *ptr)
{
    decRefCnt(ptr);
}

object* createListObject()
{
    list* ls = listCreate();
    LIST_SET_FREE_METHOD(ls, listFreeMethodWrapper);

    object* o = createObject(OBJECT_TYPE_LIST, ls);
    o->encoding = OBJECT_ENCODING_LINKED_LIST;
    return o;
}

//ziplist编码的list对象
object* createZiplistObject()
{
    unsigned char* zl = ziplistNew();
    object* o = createObject(OBJECT_TYPE_LIST, zl);
    o->encoding = OBJECT_ENCODING_ZIPLIST;
    return o;
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