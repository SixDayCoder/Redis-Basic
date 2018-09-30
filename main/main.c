#include <stdio.h>
#include <hash.h>
#include "sds.h"
#include "list.h"
#include "hash.h"
#include "dict.h"

unsigned int TestHashFunction (const void* key)
{
    sds s = (sds)key;
    size_t len = sdslen(s);
    return Time33Hash(s, (int)len);
}

int TestSDSKeyCompare (void* privdata, const void* lkey, const void* rkey)
{
    DICT_PRIVDATA_NOTUSED(privdata);
    sds ls = (sds)lkey;
    sds rs = (sds)rkey;
    return sdscmp(ls, rs);
}

void TestSDSKeyDtor(void* privdata, void* key)
{
    DICT_PRIVDATA_NOTUSED(privdata);
    sds s = (sds)key;
    sdsfree(s);
}

void TestSDSValDtor(void* privdata, void* obj)
{
    DICT_PRIVDATA_NOTUSED(privdata);
    sds s = (sds)obj;
    sdsfree(s);
}

int main()
{
    hashMethod method = {
            TestHashFunction,
            NULL,
            NULL,
            TestSDSKeyCompare,
            TestSDSKeyDtor,
            TestSDSValDtor,
    };
    sds k1 = sdsnew("key1"); sds v1 = sdsnew("val1");
    sds k2 = sdsnew("key2"); sds v2 = sdsnew("val2");
    sds k3 = sdsnew("key3"); sds v3 = sdsnew("val3");
    dict *d = dictCreate(&method, NULL);
    dictExpand(d, 16);
    dictAddPair(d, k1, v1);
    dictAddPair(d, k2, v2);
    dictAddPair(d, k3, v3);
    dictIterator *iter = dictGetIterator(d);
    hashNode* node;
    while( (node = dictNext(iter) ) != NULL)
    {
        sds k = (sds)node->key;
        sds v = (sds)node->v.val;
        printf("k is : %s, v is : %s\n", k, v);
    }
    dictRelease(d);
    return 0;
}