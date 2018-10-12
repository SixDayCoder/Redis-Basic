//
// Created by Administrator on 2018/10/11.
//

#include "ziplist.h"
#include <assert.h>

/*
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


//(1)zlbytes : 压缩列表的total字节数
//(2)zltail  : 压缩列表中,最后一个entry的起始地址的offset, zl + zltail = (last entry's begin address)
//(3)zllen   : 压缩列表的entry数目
//(4)zlend   : 压缩列表的结束标志

*/

//根据编码信息返回对应的编码的int占用的字节数
static uint32_t zlentryIntSize(unsigned char encoding)
{
    switch(encoding)
    {
        case ZIP_INT_08B: return 1;
        case ZIP_INT_16B: return 2;
        case ZIP_INT_32B: return 4;
        case ZIP_INT_64B: return 8;
        default:          return 0;
    }
}

//ptr指向entry,解码encoding信息
static void  zlentryDecodeEncoding(unsigned char* ptr, unsigned char* encoding)
{
    *encoding = ptr[0];
    if(*encoding < ZIP_ENCODING_STR_MASK)
        *encoding &= ZIP_ENCODING_STR_MASK;
}

//ptr指向entry,解码前置节点的lensize信息
static void zlentryDecodePrevLengthSize(unsigned char* ptr, uint32_t* lensize)
{
    //如果entry的prevlensize < BIGLEN标志,那么lensize是1
    if(ptr[0] < ZIP_BIGLEN_FLAG)
        *lensize = 1;
    else
        *lensize = 5;
}

//ptr指向entry,解码前置节点的lensize,len信息
static void zlentryDecodePrevLength(unsigned char* ptr, uint32_t* lensize, uint32_t* len)
{
    //首先解析lensize信息
    zlentryDecodePrevLengthSize(ptr, lensize);

    //然后解析len信息
    //1)如果lensize是1,显然其长度信息就是ptr[0]
    if(*lensize == 1)
    {
        *len = (uint32_t)ptr[0];
    }
    //2)如过lensize是5,长度信息在后4个byte中
    else
    {
        memcpy(len, ((char*)ptr + 1), sizeof(uint32_t));
    }
}

//ptr指向的entry,解码长度信息
static void zlentryDecodeLength(unsigned char* ptr, unsigned char* outencoding, uint32_t* outlensize, uint32_t* outlen)
{
    unsigned char encoding = 0;
    int lensize = 0;
    int len = 0;
    //encoding的size可能是1个字节 : 前两位是编码,后6位表示长度
    //encoding的size可能是2个字节 : 前两位是编码,后14位表示长度长度(小于等于16383字节的字符串)
    //encoding的size可能是5个字节 : 前两位是编码,后面4个字节(32位)表示长度(小于等于4294967295的字符串)
    zlentryDecodeEncoding(ptr, &encoding);
    //如果是字符串编码
    if(encoding < ZIP_ENCODING_STR_MASK)
    {
        if(encoding == ZIP_STR_06B)
        {
            lensize = 1;
            len = (ptr[0] & 0x3f);
        }
        else if(encoding == ZIP_STR_14B)
        {
            lensize = 2;
            len = ( ( (ptr[0] & 0x3f) << 8 ) | ptr[1] );
        }
        else if(encoding == ZIP_STR_32B)
        {
            lensize = 5;
            len = ptr[1] << 24 | ptr[2] << 16 | ptr[3] << 8 | ptr[4];
        }
        else
        {
            //error
            assert(NULL);
        }
    }
    //如果是整数编码,encoding的size是1
    else
    {
        lensize = 1;
        len = zlentryIntSize(encoding);
    }

    *outencoding = encoding;
    *outlen = (uint32_t)len;
    *outlensize = (uint32_t)lensize;
}

//ptr指向entry,获取entry占用内存的总字节数
static uint32_t zlentrySize(unsigned char* ptr)
{
    uint32_t prevlensize = 0;
    zlentryDecodePrevLengthSize(ptr, &prevlensize);

    uint32_t lensize = 0;
    uint32_t len = 0;
    unsigned char encoding = 0;

    zlentryDecodeLength(ptr, &encoding, &lensize, &len);
    return prevlensize + lensize + len;
}

//获取p指向的位置entry属性
static zlentry ziplistEntry(unsigned char *p)
{
    zlentry e;

    zlentryDecodePrevLength(p, &e.prevlensize, &e.prevlen);

    zlentryDecodeLength(p + e.prevlensize, &e.encoding, &e.lensize, &e.len);

    e.headersize = e.prevlensize + e.lensize;

    e.p = p;

    return  e;
}

//创建新的压缩列表
unsigned char* ziplistNew()
{
    //表头大小+末尾zend的大小
    uint32_t bytes = ZIPLIST_HEADER_SIZE + 1;

    //表头和表尾分配内存
    unsigned char* zl = zmalloc(bytes);

    //初始化属性
    ZIPLIST_BYTES(zl) = bytes;
    ZIPLIST_TAIL_OFFSET(zl) = ZIPLIST_HEADER_SIZE;
    ZIPLIST_LEN(zl) = 0;

    //设置表尾
    zl[bytes - 1] = ZIP_END_FALG;
    return zl;
}

//根据p指向的位置将长度为slen的字符串s插入到zl中,返回插入后的zl
static unsigned char *ziplistInsert(unsigned char *zl, unsigned char *p, unsigned char *s, unsigned int slen)
{
    uint32_t bytes = ZIPLIST_BYTES(zl);
    zlentry e;
    uint32_t prevlen = 0;
    if(p[0] != ZIP_END_FALG)
    {
        //不指向end_flag,说明zl非空,取出p指向的entry
        e = ziplistEntry(p);
        prevlen = e.prevlen;
    }
    else
    {
        //指向end_flag,两种情况
        //1)tail指向end_flag:空的ziplist
        //2)tail没有指向end_flag:说明新节点要插入到尾部,成为新的tail
        unsigned char* ptail = ZIPLIST_TAIL(zl);
        if(ptail[0] != ZIP_END_FALG)
        {
            //获取当前entry的字节数,新插入节点后就是新节点的prevlen
            prevlen = zlentrySize(p);
        }
    }

    //TODO:Do Insert Job
    return zl;
}

//将长度为slen的字符串s插入到压缩列表的表头
unsigned char *ziplistPushHead(unsigned char *zl, unsigned char *s, unsigned int slen)
{
    unsigned char* pos = ZIPLIST_HEAD_ENTRY_ADDR(zl);
    return ziplistInsert(zl, pos, s, slen);
}

//将长度为slen的字符串s插入到压缩列表的表尾
unsigned char *ziplistPushBack(unsigned char *zl, unsigned char *s, unsigned int slen)
{
    unsigned char* pos = ZIPLIST_END_ENTRY_ADDR(zl);
    return ziplistInsert(zl, pos, s, slen);
}