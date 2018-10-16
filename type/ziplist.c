//
// Created by Administrator on 2018/10/11.
//

#include "ziplist.h"
#include "utlis.h"
#include <limits.h>
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
        case ZIP_INT_24B: return 3;
        case ZIP_INT_32B: return 4;
        case ZIP_INT_64B: return 8;
        default: return 0;//4位整数,用encoding的后四位即可保存
    }
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
    if(ptr == NULL) return (rawlen < ZIP_BIGLEN_FLAG) ? 1 : sizeof(rawlen) + 1;

    //1字节
    if(rawlen < ZIP_BIGLEN_FLAG)
    {
        ptr[0] = (unsigned char)rawlen;
        return 1;
    }
    //5字节,第1个字节存BIGLEN标志,后4个字节存长度信息
    else
    {
        ptr[0] = ZIP_BIGLEN_FLAG;
        memcpy(ptr + 1, &rawlen, sizeof(rawlen));
        return 1 + sizeof(rawlen);
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
    ZIP_ENTRY_ENCODING(ptr, encoding);
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
            len = ( (ptr[0] & 0x3f) << 8  | ptr[1] );
        }
        else if(encoding == ZIP_STR_32B)
        {
            lensize = 5;
            len = ptr[1] << 24 | ptr[2] << 16 | ptr[3] << 8 | ptr[4];
        }
        else
        {
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

    zlentryDecodeLength(ptr + prevlensize, &encoding, &lensize, &len);
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
            //特殊的,四位整数,存储[0,12],直接放到encoding的后四个bit
            //val = 0时,encoding =  11110001
            //val = 12时,encoding = 11111101
            *encoding = (unsigned char)(ZIP_INT_IMM_MIN + value);
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
        rawlensize = zlentryEncodePrevLength(NULL, rawlen);

        //如果没有后续节点了那么跳出
        if(p[rawlen] == ZIP_END_FALG) break;

        next = ziplistEntry(p + rawlen);
        if(next.prevlen == rawlen) break;

        //此时必定需要对next进行扩展
        if(next.prevlensize < rawlensize)
        {
            extrasize = rawlensize - next.prevlensize;
            offset = p - zl;
            zl = ziplistResize(zl, (uint32_t)(zlbytes + extrasize));
            p = zl + offset;
            nextp = p + rawlen;
            noffset = nextp - zl;
            if( (zl + ZIPLIST_TAIL_OFFSET(zl)) != nextp)
            {
                ZIPLIST_TAIL_OFFSET(zl) = ZIPLIST_TAIL_OFFSET(zl) + (uint32_t)extrasize;
            }

            memmove(nextp + rawlensize, nextp + next.prevlensize, zlbytes - noffset - next.prevlensize - 1);
            zlentryEncodePrevLength(nextp, (uint32_t)rawlen);
            p += rawlen;
            zlbytes += extrasize;
        }
        else
        {
            //next节点的headersize是5,新插入的结点是1,不缩小next,直接把1字节大小的rawlen放到5字节的内存中
            if(next.prevlensize > rawlensize)
            {
                p += rawlen;
                p[0] = ZIP_BIGLEN_FLAG;
                memcpy(p + 1, &rawlen, sizeof(rawlen));
            }
            //next.prevlensize == rawlensize
            else
            {
                zlentryEncodePrevLength(p + rawlen, (uint32_t)rawlen);
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

//以encoding指定的编码方式读取并返回pint指int值
static int64_t ziplistLoadInt(unsigned char* pint, unsigned char encoding)
{
    int16_t i16;
    int32_t i32;
    int64_t i64, ret = 0;

    if (encoding == ZIP_INT_08B) {
        ret = ((int8_t*)pint)[0];
    } else if (encoding == ZIP_INT_16B) {
        memcpy(&i16, pint, sizeof(i16));
        ret = i16;
    } else if (encoding == ZIP_INT_32B) {
        memcpy(&i32, pint, sizeof(i32));
        ret = i32;
    } else if (encoding == ZIP_INT_24B) {
        i32 = 0;
        memcpy(((uint8_t*)&i32) + 1, pint ,sizeof(i32) - sizeof(uint8_t));
        ret = i32 >> 8;
    } else if (encoding == ZIP_INT_64B) {
        memcpy(&i64, pint, sizeof(i64));
        ret = i64;
    } else if (encoding >= ZIP_INT_IMM_MIN && encoding <= ZIP_INT_IMM_MAX) {
        ret = (encoding & ZIP_INT_IMM_MASK) - 1;
    } else {
        assert(NULL);
    }
    return ret;
}

//从p位置开始连续删除num个entry,返回删除之后的ziplist
static unsigned char* ziplistRemove(unsigned char* zl, unsigned char* p, unsigned int num)
{
    zlentry first, tail;

    uint32_t deleted = 0;
    uint32_t totsize = 0;
    int nextdiff = 0;
    first = ziplistEntry(p);
    for(int i = 0 ; p[0] != ZIP_END_FALG && i < num; ++i)
    {
        p += zlentrySize(p);
        deleted++;
    }

    //总共删除的bytes
    totsize = (uint32_t)(p - first.p);
    if(totsize > 0)
    {
        //如果删除num个节点之后,还没有到ziplist的结束标志
        if(p[0] != ZIP_END_FALG)
        {
            nextdiff = ziplistPrevLenByteDiff(p, first.prevlen);
            p -= nextdiff;
            zlentryEncodePrevLength(p, first.prevlen);
            ZIPLIST_TAIL_OFFSET(zl) = ZIPLIST_TAIL_OFFSET(zl) - totsize;
            tail = ziplistEntry(p);
            if(p[tail.headersize + tail.len] != ZIP_END_FALG)
            {
                ZIPLIST_TAIL_OFFSET(zl) = ZIPLIST_TAIL_OFFSET(zl) + nextdiff;
            }
            memmove(first.p, p, ZIPLIST_BYTES(zl) - (p - zl) - 1);
        }
        //删除num个节点之后没有节点了
        else
        {
            ZIPLIST_TAIL_OFFSET(zl) = (uint32_t)(first.p - zl - first.prevlen);
        }
    }

    size_t offset = first.p - zl;
    zl = ziplistResize(zl, ZIPLIST_BYTES(zl) - totsize + nextdiff);
    ZIPLIST_INCR_LENGTH(zl, -deleted);
    p = zl + offset;
    if(nextdiff != 0)
        zl = ziplistCascadeUpdate(zl, p);

    return  zl;
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
        unsigned char* ptail = ZIPLIST_TAIL_ENTRY(zl);
        if(ptail[0] != ZIP_END_FALG)
        {
            //获取当前entry的字节数,新插入节点后就是新节点的prevlen
            prevlen = zlentrySize(ptail);
        }
    }

    long long val = 0;
    unsigned char encoding = 0;
    uint32_t reqlen = 0;
    //如果字符串s可以被解析成整数
    if(ziplistTryEncode2Int(s, slen, &val, &encoding))
    {
        reqlen = zlentryIntSize(encoding);
    }
    //否则肯定用字符串的方式存储
    else
    {
        reqlen = slen;
    }

    //1)前置节点的长度
    reqlen += zlentryEncodePrevLength(NULL, prevlen);

    //2)根据encoding获取当前节点的长度
    reqlen += zlentryEncodeLength(NULL, encoding, slen);

    //只要新节点不是插入到ziplist的末尾
    //插入新节点就要考虑新节点的后继节点的prevlensize属性是否需要变化
    int nextdiff = (p[0] != ZIP_END_FALG) ?  ziplistPrevLenByteDiff(p, reqlen) : 0;

    //对zl调整size的时候可能会发生首地址变化,因此先记录下便宜量
    int offset = (int)(p - zl);

    //新分配的内存 = zl原来的内存 + 新插入的entry的大小 + 新节点的后继节点的header需要扩展的字节数
    zl = ziplistResize(zl, zlbytes + reqlen + nextdiff);

    p = zl + offset;

    if(p[0] != ZIP_END_FALG)
    {
        memmove(p + reqlen, p - nextdiff, zlbytes - offset + 1 + nextdiff);

        zlentryEncodePrevLength(p + reqlen, reqlen);

        ZIPLIST_TAIL_OFFSET(zl) = ZIPLIST_TAIL_OFFSET(zl) + reqlen;

        zlentry tail = ziplistEntry(p + reqlen);
        if(p[reqlen + tail.headersize + tail.len] != ZIP_END_FALG)
        {
            ZIPLIST_TAIL_OFFSET(zl) = ZIPLIST_TAIL_OFFSET(zl) + nextdiff;
        }
    }
    else
    {
        ZIPLIST_TAIL_OFFSET(zl) = (uint32_t)(p - zl);
    }

    if(nextdiff != 0)
    {
        offset = (int)(p - zl);
        zl = ziplistCascadeUpdate(zl, p + reqlen);
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
    unsigned char* pos = ZIPLIST_HEAD_ENTRY(zl);
    return ziplistInsert(zl, pos, s, slen);
}

//将长度为slen的字符串s插入到压缩列表的表尾
unsigned char *ziplistPushBack(unsigned char *zl, unsigned char *s, unsigned int slen)
{
    unsigned char* pos = ZIPLIST_TAIL_ENTRY(zl);
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

//返回zl的索引为index的zlentry的指针
unsigned char* ziplistIndex(unsigned char* zl, int index)
{
    //索引为正从head->tail
    //索引为负从tail->head, 负索引从-1开始
    unsigned char* item = NULL;
    zlentry e;
    if(index < 0)
    {
        index = (-index) - 1;
        item = ZIPLIST_TAIL_ENTRY(zl);
        if(item[0] != ZIP_END_FALG)
        {
            e = ziplistEntry(item);
            //从tail->head,head结点的prelen是0
            while(e.prevlen > 0 && index--)
            {
                item -= e.prevlen;
                e = ziplistEntry(item);
            }
        }
    }
    else
    {
        item = ZIPLIST_HEAD_ENTRY(zl);
        while(item[0] != ZIP_END_FALG && index--)
            item += zlentrySize(item);
    }

    return (item[0] == ZIP_END_FALG || index > 0) ? NULL : item;
}

//p指向zl的某个entry,该函数返回该entry的下一个entry
unsigned char *ziplistNext(unsigned char *zl, unsigned char *p)
{
    //p指向末尾标志返回NULL
    if(p[0] == ZIP_END_FALG) return NULL;
    p += zlentrySize(p);
    //p的下一个节点指向末尾标志也返回NULL
    if(p[0] == ZIP_END_FALG) return NULL;
    return p;
}

//p指向zl的某个entry,该函数返回该entry的前一个entry
unsigned char *ziplistPrev(unsigned char *zl, unsigned char *p)
{
    if(p[0] == ZIP_END_FALG)
    {
        //尝试取出尾结点
        p = ZIPLIST_TAIL_ENTRY(zl);
        return (p[0] == ZIP_END_FALG) ? NULL : p;
    }
    //指向head节点,不存在前置节点
    else if(p == ZIPLIST_HEAD_ENTRY(zl))
    {
        return NULL;
    }
    zlentry e = ziplistEntry(p);
    assert(e.prevlen > 0);
    return p - e.prevlen;
}

//取出p指向的entry的值,字符串保存在sval中,整数值保存在lval中,get成功返回1,如果不是字符串sval是NULL
unsigned int ziplistGet(unsigned char *p, unsigned char **sval, unsigned int *slen, long long *lval)
{
    if(p == NULL || p[0] == ZIP_END_FALG) return 0;
    //初始化,默认是get整数值
    if(sval) *sval = NULL;

    zlentry e = ziplistEntry(p);
    if(ZIP_IS_STR_ENCODE(e.encoding))
    {
        if(sval)
        {
            *slen = e.len;
            *sval = p + e.headersize;
        }
    }
    else
    {
        if(lval)
        {
            *lval = (long long) ziplistLoadInt(p + e.headersize, e.encoding);
        }
    }

    return 1;
}

//删除zl中*p指向的entry,并更新*p指向的位置
unsigned char *ziplistDelete(unsigned char *zl, unsigned char **p)
{
    size_t offset = *p - zl;
    zl = ziplistRemove(zl, *p, 1);
    *p = zl + offset;
    return zl;
}

//从index索引开始连续删除num个节点
unsigned char *ziplistDeleteRange(unsigned char *zl, unsigned int index, unsigned int num)
{
    unsigned char* p = ziplistIndex(zl, index);
    return (p == NULL) ? zl : ziplistRemove(zl, p, num);
}

//p指向的entry的content和s进行比较,相等返回1不相等返回0
unsigned int ziplistCompare(unsigned char *p, unsigned char *s, unsigned int slen)
{
    if(p[0] == ZIP_END_FALG) return 0;

    zlentry e = ziplistEntry(p);
    if(ZIP_IS_STR_ENCODE(e.encoding))
    {
        if(e.len == slen)
            return memcmp(p + e.headersize, s, slen) == 0 ? 1 : 0;
    }
    else
    {
        long long sval, zval;
        unsigned char encoding = 0;
        if(ziplistTryEncode2Int(s, slen, &sval, &encoding))
        {
            zval = ziplistLoadInt(p + e.headersize, e.encoding);
            return zval == sval;
        }
    }
    return 0;
}

//寻找和vstr相等的entry,并返回该entry的首指针
//skip表示每次比对之前都跳过skip个entry
unsigned char *ziplistFind(unsigned char *p, unsigned char *vstr, unsigned int vlen, unsigned int skip)
{
    unsigned char* data = NULL;
    int skipcnt = 0;

    while(p[0] != ZIP_END_FALG)
    {
        long long val = 0;
        unsigned char encoding = 0;
        unsigned char vencoding = 0;
        uint32_t prelensize, lensize, len;

        zlentryDecodePrevLengthSize(p, &prelensize);
        zlentryDecodeLength(p + prelensize, &encoding, &lensize, &len);
        data = p + prelensize + lensize;
        if(skipcnt == 0)
        {
            if(ZIP_IS_STR_ENCODE(encoding))
            {
                if(len == vlen && memcmp(data, vstr, vlen) == 0) return p;
            }
            else
            {
                //传入的值有可能被编码,在第一次进行值比对的时候进行解码
                if(vencoding == 0)
                {
                    //如果vstr没有被编码设置vencoding为UCHAR_MAX
                    if(!ziplistTryEncode2Int(vstr, vlen, &val, &encoding))
                    {
                        vencoding = UCHAR_MAX;
                    }
                    assert(vencoding);
                }
                //说明vstr是被编码过的
                if(vencoding != UCHAR_MAX)
                {
                    long long decodeval = ziplistLoadInt(data, encoding);
                    if(decodeval == val) return p;
                }
            }
            skipcnt += skip;
        }
        else
        {
            skipcnt--;
        }

        //指针后移到下一个entry
        p = data + len;
    }

    //没有找到
    return NULL;
}

//打印ziplist的信息
void ziplistLog(unsigned char* zl)
{
    printf("{totalbytes : %d, entry count : %d, tail offset : %d\n", ZIPLIST_BYTES(zl), ziplistEntryCount(zl), ZIPLIST_TAIL_OFFSET(zl));
    unsigned char* p = ZIPLIST_HEAD_ENTRY(zl);
    char buf[64] = { 0 };
    int index = 0;
    zlentry e;
    while(p[0] != ZIP_END_FALG)
    {
        e = ziplistEntry(p);
        printf("{addr : 0x%08lx, index : %2d, offset : %5ld, rl : %5u, headsize : %2u, prevlen : %5u, prevlensize : %2u, datalen : %5u}",
               (long unsigned)p,
               index,
               (p - zl),
               e.headersize + e.len,
               e.headersize,
               e.prevlen,
               e.prevlensize,
               e.len);
        p += e.headersize;
        if(ZIP_IS_STR_ENCODE(e.encoding))
        {
            memset(buf, 0, sizeof(buf));
            memcpy(buf, p, e.len);
            buf[e.len] = '\0';
            printf(" {string : %s}\n", buf);
        }
        else
        {
            printf(" {integer : %lld}\n", (long long)ziplistLoadInt(p, e.encoding));
        }
        p += e.len;
        index++;
    }
}