#include <stdio.h>
#include <hash.h>
#include "sds.h"
#include "list.h"
#include "hash.h"
#include "dict.h"
#include "skiplist.h"
#include "intset.h"
#include "ziplist.h"
#include "utlis.h"

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

void TestHash()
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
}

void TestSkipList()
{
    object* o1 = createObject(OBJECT_TYPE_STRING, sdsnew("123"));
    object* o2 = createObject(OBJECT_TYPE_STRING, sdsnew("456"));
    object* o3 = createObject(OBJECT_TYPE_STRING, sdsnew("789"));
    object* o4 = createObject(OBJECT_TYPE_STRING, sdsnew("111"));

    skiplist* sl = slCreate();
    slInsert(sl, 1.0, o1);
    slInsert(sl, 3.0, o2);
    slInsert(sl, 2.0, o3);
    slInsert(sl, 0.0, o4);

    printf("len : %lu\n", sl->length);
    printf("rank o1 : %lu, rank o2 : %lu, rank o3 : %lu, rank o4 : %lu\n", slGetRank(sl, 1.0, o1), slGetRank(sl, 3.0, o2), slGetRank(sl, 2.0, o3), slGetRank(sl, 0.0, o4));


//    slDelete(sl, 1.0, o1);
//    printf("delete one : len : %lu\n", sl->length);
//
//    slDelete(sl, 3.0, o2);
//    printf("delete one : len : %lu\n", sl->length);
//
//    slDelete(sl, 2.0, o3);
//    printf("delete one : len : %lu\n", sl->length);
//
//    slDelete(sl, 0.0, o4);
//    printf("delete one : len : %lu\n", sl->length);

    skiplistRange range;
    range.minScore = 0.0;
    range.maxScore = 10.0;
    range.minInclude = 1;
    range.maxInclude = 1;
    slDeleteRangeByScore(sl, &range);
    printf("delete range : len : %lu\n", sl->length);
}

void TestIntSet()
{
    intset* is = intsetNew();
    printf("init\n");
    printf("len is %u, totalsize is %lu, encoding is %u\n", is->length, intsetBlobLen(is), is->encoding);

    uint8_t success = 0;
    for(int i = 0 ; i < 100; ++i)
    {
        int64_t val = (i % 2 == 0) ? ( (i - 31432) * 123 + 432 ) : (i * 3124 - 78);
        is = intsetAdd(is, val, &success);
    }
    int count = 0;
    //upgrade
    printf("--------------------------------------------------------------------------------------------\n");
    int64_t v = 312312331343432;
    is = intsetAdd(is, v, &success);
    printf("after upgrade : len is %u, totalsize is %lu, encoding is %u\n", is->length, intsetBlobLen(is), is->encoding);
    v = 0;
    intsetGet(is, is->length - 1, &v);
    printf("max is %lld\n", v);
    for(int i = 0 ; i < is->length; i++)
    {
        int64_t val = 0;
        intsetGet(is, i, &val);
        printf("%lld ", (long long)val);
        count++;
        if(count == 10)
        {
            printf("\n");
            count = 0;
        }
    }
}

int main()
{
    return 0;
}