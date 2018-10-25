//
// Created by Administrator on 2018/10/24.
//

#include "redis.h"
#include "utlis.h"
#include <string.h>

extern  redisServer gServer;

unsigned int dictSDSHash(const void* key)
{
    return MurmurHash2((unsigned char*)key, (int)sdslen((char*)key));
}

unsigned int dictObjHash(const void *key) {
    const object *o = key;
    return MurmurHash2(o->ptr, (int)sdslen((sds)o->ptr));
}

int dictSDSKeyCompare(void *privdata, const void *lkey, const void* rkey)
{
    REDIS_NOT_USED(privdata);
    size_t lkeylen = sdslen((sds)lkey);
    size_t rkeylen = sdslen((sds)rkey);
    if(lkeylen != rkeylen) {
        return 0;
    }
    return memcmp(lkey, rkey, lkeylen) == 0;
}

int dictObjKeyCompare(void *privdata, const void *lkey, const void* rkey)
{
    const object *o1 = lkey, *o2 = rkey;
    return dictSDSKeyCompare(privdata, o1->ptr, o2->ptr);
}

void dictSDSDtor(void* privdata, void* val)
{
    REDIS_NOT_USED(privdata);
    sdsfree(val);
}

void dictListDtor(void* privdata, void* val)
{
    REDIS_NOT_USED(privdata);
    listRealease(val);
}

hashMethod DBDictHashMethod = {
    dictSDSHash,
    NULL,
    NULL,
    dictSDSKeyCompare,
    dictSDSDtor,
    encObjDtor
};

hashMethod DBExpireHashMethod = {
    dictSDSHash,
    NULL,
    NULL,
    dictSDSKeyCompare,
    NULL,
    NULL
};

//key is unencoded redis object, val is lists
hashMethod KeyListHashMethod = {
    dictObjHash,
    NULL,
    NULL,
    dictObjKeyCompare,
    encObjDtor,
    dictListDtor
};

//返回key的过期时间,没有返回-1
long long dbGetExpireTime(redisDB* db, object* key)
{
    hashNode* node = dictFind(db->expires, key->ptr);
    if(DICT_TOTAL_SIZE(db->expires) == 0 || node == NULL) {
        return -1;
    }
    //在过期space但是没有在main keysapce
    if(dictFind(db->dict, key->ptr) == NULL) {
        exit(1);
    }
    return HASH_NODE_GET_SIGNED_INT(node);
}

//key过期则删除,0表示没有过期,1表示过期并且删除
int dbExpireKeyIfNeeded(redisDB* db, object* key)
{
    mstime_t when = dbGetExpireTime(db, key);
    mstime_t now = mstime();

    if(when < 0 || now <= when) {
        return 0;
    }

    gServer.stateExpireKeys++;
    //TODO:send event
    return dbDeleteKey(db, key);
}

int dbExists(redisDB* db, object* key)
{
    return dictFind(db->dict, key->ptr) != NULL;
}

object* dbFindKey(redisDB* db, object* key)
{
    hashNode* node = dictFind(db->dict, key->ptr);
    if(node) {
        object* val = HASH_NODE_GET_VAL(node);
        val->lru = LRU();
        return val;
    }
    //不存在
    return NULL;
}

//为执行读操作而取出key,找到值会更新服务器的命中/不命中信息
object* dbFindKeyRead(redisDB* db, object* key)
{
    dbExpireKeyIfNeeded(db, key);
    object* o = dbFindKey(db, key);
    if(o == NULL) {
        gServer.stateKeySapceMiss++;
    }
    else {
        gServer.stateKeySpaceHits++;
    }
    return o;
}

//为执行读操作而取出key
object* dbFindKeyWrite(redisDB* db, object* key)
{
    dbExpireKeyIfNeeded(db, key);
    return dbFindKey(db, key);
}

//尝试将(k,v)插入,调用该函数的user需要自行对key,val的refcount变化
//如果已经存在停止
void dbAdd(redisDB* db, object* key, object* val)
{
    sds copy = sdsdup(key->ptr);
    int retval = dictAddPair(db, key, val);
    if(retval != REDIS_OK) {
        exit(1);
    }
}

void dbOverWrite(redisDB* db, object* key, object* val)
{
    hashNode* node = dictFind(db->dict, key->ptr);
    if(node == NULL) {
        exit(1);
    }
    dictReplacePair(db->dict, key->ptr, val);
}

int dbDeleteKey(redisDB* db, object* key)
{
    if(DICT_TOTAL_SIZE(db->expires) != 0) {
        dictDelete(db->expires, key->ptr);
    }
    if(dictDelete(db->dict, key->ptr) == DICT_PROCESS_OK) {
        return 1;
    }
    return 0;
}

//清空数据库所有数据
long long dbEmpty(void(callback)(void*))
{
    int removed = 0;
    for(int i = 0 ; i < gServer.dbnum; ++i) {
        removed += DICT_TOTAL_SIZE(gServer.db[i].dict);
        dictClear(gServer.db[i].dict);
        dictClear(gServer.db[i].expires);
    }
    return removed;
}