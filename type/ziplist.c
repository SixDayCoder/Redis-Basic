//
// Created by Administrator on 2018/10/11.
//

#include "ziplist.h"
#include "utlis.h"
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

//rawlen是前置节点的长度,该函数会根据rawlen对长度编码,如果ptr不会空会把长度信息写在ptr中
//ptr为空仅返回编码rawlen需要的字节数
static unsigned int zlentryEncodePrevLength(unsigned char* ptr, uint32_t rawlen)
{
    if(ptr == NULL) return (rawlen < ZIP_BIGLEN_FLAG) ? 1 : sizeof(uint32_t) + 1;

    //1字节
    if(rawlen < ZIP_BIGLEN_FLAG)
    {
        ptr[0] = rawlen;
        return 1;
    }
    //5字节,第1个字节存BIGLEN标志,后4个字节存长度信息
    else
    {
        ptr[0] = ZIP_BIGLEN_FLAG;
        memcpy(ptr + 1, &rawlen, sizeof(rawlen));
        return 1 + sizeof(uint32_t);
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

//根据当前entry的encoding和content的长度,获取编码entry长度信息所需要的字节数(可能是1,2,5)
//如果ptr不为空那么该字节数会被写入到ptr并返回,否则直接返回
static unsigned int zlentryEncodeLength(unsigned char* ptr, unsigned char encoding, uint32_t rawlen)
{
    unsigned char len = 1;
    //最大5个字节来encode字符串的类型
    unsigned char buf[5];

    if(ZIP_IS_STR_ENCODE(encoding))
    {
        //63,1个byte
        if(rawlen <= 0x3f)
        {
            if(!ptr) return len;
            buf[0] = ZIP_STR_06B | rawlen;
        }
        //16383, 2个byte
        else if(rawlen <= 0x3fff)
        {
            len += 1;
            if(!ptr) return len;
            buf[0] = ZIP_STR_14B | ( (rawlen >> 8) & 0x3f );
            buf[1] = rawlen & 0xff;
        }
        //2^32 - 1, 5个byte
        else
        {
            len += 4;
            if(!ptr) return len;
            buf[0] = ZIP_STR_32B;
            buf[1] = (rawlen >> 24) & 0xff;
            buf[2] = (rawlen >> 16) & 0xff;
            buf[3] = (rawlen >> 8)  & 0xff;
            buf[4] = rawlen & 0xff;
        }
    }
    //否则是整数编码
    else
    {
        if(!ptr) return len;
        buf[0] = encoding;
    }

    //将编码后的长度写入p
    memcpy(ptr, buf, len);

    //返回编码所需的字节数
    return len;
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

//尝试将entry的content的字符串编码为整数
//1)成功val保存int,encoding保存编码方式,返回1
//1)成功val保存int,encoding保存编码方式,返回1
static int ziplistTryEncode2Int(unsigned char* content, unsigned int len, long long* val, unsigned char* encoding)
{
    if(val == NULL || encoding == NULL) return 0;
    //忽略太长或者太短的字符串
    if(len >= 32 || len == 0) return 0;

    //尝试把content解析成数字
    long long value = 0;
    if(string2ll((char*)content, len, &value))
    {
        if(value >= 0 && value <= 12)
        {
            //特殊的,四位整数,存储[0,12]
            //val = 0时,encoding =  11110001
            //val = 12时,encoding = 11111101
            *encoding = (unsigned char)ZIP_INT_IMM_MIN + (unsigned char)value;
        }
        else if(value >= INT8_MIN && value <= INT8_MAX)
        {
            *encoding = ZIP_INT_08B;
        }
        else if(value >= INT16_MIN && value <= INT16_MAX)
        {
            *encoding = ZIP_INT_16B;
        }
        else if(value >= INT24_MIN && value <= INT24_MAX)
        {
            *encoding = ZIP_INT_24B;
        }
        else if(value >= INT32_MIN && value <= INT32_MAX)
        {
            *encoding = ZIP_INT_32B;
        }
        else if(value >= INT64_MIN && value <= INT64_MAX)
        {
            *encoding = ZIP_INT_64B;
        }
        else
        {
            return 0;
        }
        *val = value;
        return 1;
    }

    return 0;
}


static int ziplistPrevLenByteDiff(unsigned char* p, unsigned int len)
{
    unsigned int prevlensize = 0;
    zlentryDecodePrevLengthSize(p, &prevlensize);
    return  zlentryEncodePrevLength(NULL, len) - prevlensize;
}

//调整zl的大小为size字节
static unsigned char* ziplistResize(unsigned char* zl, uint32_t size)
{
    //扩展不改变zl的原有元素
    zl = zrealloc(zl, size);

    ZIPLIST_BYTES(zl) = size;

    zl[size - 1] = ZIP_END_FALG;

    return zl;
}

//* 当将一个新节点添加到某个节点之前的时候，
//* 如果原节点的 header 空间不足以保存新节点的长度，
//* 那么就需要对原节点的 header 空间进行扩展（从 1 字节扩展到 5 字节）。
//* 但是，当对原节点进行扩展之后，原节点的下一个节点的 prevlen 可能出现空间不足，
//* 这种情况在多个连续节点的长度都接近 ZIP_BIGLEN 时可能发生。
static unsigned char* ziplistCascadeUpdate(unsigned char* zl, unsigned char* p)
{
    unsigned char* nextp = NULL;
    size_t zlbytes = ZIPLIST_BYTES(zl);
    size_t rawlen , rawlensize, extrasize;
    size_t offset, noffset;
    zlentry curr, next;

    while(p[0] != ZIP_END_FALG)
    {
        curr = ziplistEntry(p);
        rawlen = curr.headersize + curr.len;
        rawlensize = zlentryEncodeLength(p, curr.encoding, rawlen);

        //如果没有后续节点了那么跳出
        if(p[rawlen] == ZIP_END_FALG) break;

        next = ziplistEntry(p + rawlen);
        if(next.prevlen == rawlen) break;

        //此时必定需要对next进行扩展
        if(next.prevlensize < rawlensize)
        {
            offset = p - zl;
            extrasize = rawlensize - next.prevlensize;
            zl = ziplistResize(zl, zlbytes + extrasize);
            p = zl + offset;
            nextp = p + rawlen;
            noffset = nextp - zl;
            if(zl + ZIPLIST_TAIL_OFFSET(zl) != nextp)
            {
                ZIPLIST_TAIL_OFFSET(zl) = ZIPLIST_TAIL_OFFSET(zl) + extrasize;
            }
            memmove(nextp + rawlensize, nextp + next.prevlensize, zlbytes - noffset - next.prevlensize - 1);
            zlentryEncodePrevLength(nextp, rawlen);
            p += rawlen;
            zlbytes += extrasize;
        }
        else
        {
            //next节点的headersize是5,新插入的结点是1,不缩小next,直接把1字节大小的rawlen放到5字节的内存中
            if(next.prevlensize > rawlensize)
            {
                p[0] = ZIP_BIGLEN_FLAG;
                memcpy(p + 1, &rawlen, sizeof(rawlen));
            }
            //next.prevlensize == rawlensize
            else
            {
                zlentryEncodePrevLength(p + rawlen, rawlen);
            }
            break;
        }
    }
    return zl;
}

//以encoding指定的编码方式将整数值val写入到p
static void ziplistSaveInt(unsigned char*p, int64_t value, unsigned char encoding)
{
    int16_t i16;
    int32_t i32;
    int64_t i64;

    if(encoding == ZIP_INT_08B)
    {
        ((int8_t*)p)[0] = (int8_t)value;
    }
    else if(encoding == ZIP_INT_16B)
    {
        i16 = (int16_t)value;
        memcpy(p, &i16, sizeof(i16));
    }
    else if(encoding == ZIP_INT_24B)
    {
        i32 = value << 8;
        memcpy(p, ((uint8_t*)&i32) + 1, sizeof(i32) - sizeof(uint8_t));
    }
    else if(encoding == ZIP_INT_32B)
    {
        i32 = (int32_t)value;
        memcpy(p, &i32, sizeof(i32));
    }
    else if(encoding == ZIP_INT_64B)
    {
        i64 = value;
        memcpy(p, &i64, sizeof(i64));
    }
    else if(encoding >= ZIP_INT_IMM_MIN && encoding <= ZIP_INT_IMM_MAX)
    {
        //特殊的4位整数把本身就存到了encoding之中的后四位,什么都不做
    }
    else assert(NULL);
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
    uint32_t zlbytes = ZIPLIST_BYTES(zl);
    uint32_t prevlen = 0;
    zlentry e;

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

    long long val = 0;
    unsigned char encoding = 0;
    uint32_t entryLen = 0;
    //如果字符串s可以被解析成整数
    if(ziplistTryEncode2Int(s, slen, &val, &encoding))
    {
        entryLen = zlentryIntSize(encoding);
    }
    //否则肯定用字符串的方式存储
    else
    {
        entryLen = slen;
    }

    //1)前置节点的长度
    entryLen += zlentryEncodePrevLength(NULL, prevlen);

    //2)根据encoding获取当前节点的长度
    entryLen += zlentryEncodeLength(NULL, encoding, slen);

    //只要新节点不是插入到ziplist的末尾
    //插入新节点就要考虑新节点的后继节点的prevlensize属性是否需要变化
    int nextdiff = (p[0] == ZIP_END_FALG) ?  ziplistPrevLenByteDiff(p, entryLen) : 0;

    //对zl调整size的时候可能会发生首地址变化,因此先记录下便宜量
    int offset = p - zl;

    //新分配的内存 = zl原来的内存 + 新插入的entry的大小 + 新节点的后继节点的header需要扩展的字节数
    zl = ziplistResize(zl, zlbytes + entryLen + nextdiff);

    p = zl + offset;

    if(p[0] != ZIP_END_FALG)
    {
        memmove(p + entryLen, p - nextdiff, zlbytes - offset + 1 + nextdiff);
        zlentryEncodePrevLength(p + entryLen, entryLen);
        ZIPLIST_TAIL_OFFSET(zl) = ZIPLIST_TAIL_OFFSET(zl) + entryLen;
        zlentry tail = ziplistEntry(p + entryLen);
        if(p[entryLen + tail.headersize + tail.len] != ZIP_END_FALG)
        {
            ZIPLIST_TAIL_OFFSET(zl) = ZIPLIST_TAIL_OFFSET(zl) + nextdiff;
        }
    }
    else
    {
        ZIPLIST_TAIL_OFFSET(zl) = p - zl;
    }

    if(nextdiff != 0)
    {
        offset = p - zl;
        zl = ziplistCascadeUpdate(zl, p + entryLen);
        p = zl + offset;
    }

    p += zlentryEncodePrevLength(p, prevlen);

    p += zlentryEncodeLength(p, encoding, slen);

    if(ZIP_IS_STR_ENCODE(encoding))
    {
        memcpy(p, s, slen);
    }
    else
    {
        ziplistSaveInt(p, val, encoding);
    }

    ZIPLIST_INCR_LENGTH(zl, 1);
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

//获取压缩列表的结点个数
unsigned int ziplistEntryCount(unsigned char* zl)
{
    unsigned int cnt = 0;
    if(ZIPLIST_LEN(zl) < UINT16_MAX)
    {
        cnt = ZIPLIST_LEN(zl);
        return cnt;
    }
    //结点数>UINT16_MAX时有多少元素需要遍历一下
    unsigned char *p = zl + ZIPLIST_HEADER_SIZE;
    while(p[0] != ZIP_END_FALG)
    {
        p += zlentrySize(p);
        cnt++;
    }
    return cnt;
}