//
// Created by Administrator on 2018/10/11.
//

#include "ziplist.h"

/**
空白 ziplist 示例图

area        |<---- ziplist header ---->|<-- end -->|

size          4 bytes   4 bytes 2 bytes  1 byte
            +---------+--------+-------+-----------+
component   | zlbytes | zltail | zllen | zlend     |
            |         |        |       |           |
value       |  1011   |  1010  |   0   | 1111 1111 |
            +---------+--------+-------+-----------+
                                       ^
                                       |
                               ZIPLIST_ENTRY_HEAD
                                       &
address                        ZIPLIST_ENTRY_TAIL
                                       &
                               ZIPLIST_ENTRY_END

非空 ziplist 示例图

area        |<---- ziplist header ---->|<----------- entries ------------->|<-end->|

size          4 bytes  4 bytes  2 bytes    ?        ?        ?        ?     1 byte
            +---------+--------+-------+--------+--------+--------+--------+-------+
component   | zlbytes | zltail | zllen | entry1 | entry2 |  ...   | entryN | zlend |
            +---------+--------+-------+--------+--------+--------+--------+-------+
                                       ^                          ^        ^
address                                |                          |        |
                                ZIPLIST_ENTRY_HEAD                |   ZIPLIST_ENTRY_END
                                                                  |
                                                        ZIPLIST_ENTRY_TAIL
**/

//(1)zlbytes : 压缩列表的total字节数
//(2)zltail  : 压缩列表中,最后一个entry的起始地址的offset, zl + zltail = (last entry's begin address)
//(3)zllen   : 压缩列表的entry数目
//(4)zlend   : 压缩列表的结束标志


// 返回 ziplist 表头的大小
//zlbytes(32) + zltail(32) + zllen(16)
#define ZIPLIST_HEADER_SIZE  (sizeof(uint32_t)*2 + sizeof(uint16_t))

//获取ziplist的zlbytes属性
#define ZIPLIST_BYTES(zl)  (*((uint32_t*)(zl)))

//获取ziplist的zltail属性
#define ZIPLIST_TAIL(zl)  (*((uint32_t*)((zl) + sizeof(uint32_t))))

//获取ziplist的zllen属性
#define ZIPLIST_LEN(zl)  (*((uint16_t*)((zl) + 2 * sizeof(uint32_t))))

//返回ziplist的第一个entry的首地址
#define ZIPLIST_HEAD_ENTRY_ADDR(zl) ( (zl) + ZIPLIST_HEADER_SIZE)

//返回ziplist的最后一个entry的首地址
#define ZIPLIST_END_ENRTY_ADDR(zl) ( (zl) + ZIPLIST_TAIL(zl) )

//创建新的压缩列表
unsigned char* ziplistNew()
{
    //表头大小+末尾zend的大小
    uint32_t bytes = ZIPLIST_HEADER_SIZE + 1;

    //表头和表尾分配内存
    unsigned char* zl = zmalloc(bytes);

    //初始化属性
    ZIPLIST_BYTES(zl) = bytes;
    ZIPLIST_TAIL(zl) = ZIPLIST_HEADER_SIZE;
    ZIPLIST_LEN(zl) = 0;

    //设置表尾
    zl[bytes - 1] = ZIP_END;

    return zl;
}

//根据p指向的位置将长度为slen的字符串s插入到zl中,返回插入后的zl
static unsigned char *ziplistInsert(unsigned char *zl, unsigned char *p, unsigned char *s, unsigned int slen)
{

}

//将长度为slen的字符串s插入到压缩列表中,where:ZIPLIST_HEAD插入表头,ZIPLIST_TAIl插入表尾
unsigned char *ziplistPush(unsigned char *zl, unsigned char *s, unsigned int slen, int where)
{
    unsigned char* p = (where == ZIPLIST_HEAD) ? ZIPLIST_HEAD_ENTRY_ADDR(zl) : ZIPLIST_END_ENRTY_ADDR(zl);
    return ziplistInsert(zl, p, s, slen);
}