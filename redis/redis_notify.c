//
// Created by Administrator on 2018/10/25.
//

#include "redis.h"

int keyspaceEventStr2Flags(char* evetnstr)
{
    char *p = evetnstr;
    int c, flags = 0;
    while((c = *p++) != '\0') {
        switch(c) {
            case 'A': flags |= REDIS_NOTIFY_ALL; break;
            case 'g': flags |= REDIS_NOTIFY_GENERIC; break;
            case '$': flags |= REDIS_NOTIFY_STRING; break;
            case 'l': flags |= REDIS_NOTIFY_LIST; break;
            case 's': flags |= REDIS_NOTIFY_SET; break;
            case 'h': flags |= REDIS_NOTIFY_HASH; break;
            case 'z': flags |= REDIS_NOTIFY_ZSET; break;
            case 'x': flags |= REDIS_NOTIFY_EXPIRED; break;
            case 'e': flags |= REDIS_NOTIFY_EVICTED; break;
            case 'K': flags |= REDIS_NOTIFY_KEYSPACE; break;
            case 'E': flags |= REDIS_NOTIFY_KEYEVENT; break;
                // 不能识别
            default: return -1;
        }
    }

    return flags;
}

sds keyspaceEventFlags2Str(int flags)
{
    sds res;

    res = sdsempty();
    if ((flags & REDIS_NOTIFY_ALL) == REDIS_NOTIFY_ALL) {
        res = sdscatlen(res,"A",1);
    } else {
        if (flags & REDIS_NOTIFY_GENERIC) res = sdscatlen(res,"g",1);
        if (flags & REDIS_NOTIFY_STRING) res = sdscatlen(res,"$",1);
        if (flags & REDIS_NOTIFY_LIST) res = sdscatlen(res,"l",1);
        if (flags & REDIS_NOTIFY_SET) res = sdscatlen(res,"s",1);
        if (flags & REDIS_NOTIFY_HASH) res = sdscatlen(res,"h",1);
        if (flags & REDIS_NOTIFY_ZSET) res = sdscatlen(res,"z",1);
        if (flags & REDIS_NOTIFY_EXPIRED) res = sdscatlen(res,"x",1);
        if (flags & REDIS_NOTIFY_EVICTED) res = sdscatlen(res,"e",1);
    }
    if (flags & REDIS_NOTIFY_KEYSPACE) res = sdscatlen(res,"K",1);
    if (flags & REDIS_NOTIFY_KEYEVENT) res = sdscatlen(res,"E",1);

    return res;
}

void notifyKeysapceEvent(int type, char* event, object* key, int dbid)
{
    //TODO
}

