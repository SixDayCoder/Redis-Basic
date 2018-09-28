//
// Created by Administrator on 2018/9/25.
//

#include "sds.h"
#include <stdarg.h>
#include <assert.h>

//根据char指针,构造sds
sds sdsnew(const char* init)
{
    size_t initlen = (init == NULL) ? 0 : strlen(init);
    return  sdsnewlen(init, initlen);
}

//给定长度和初始数据,构造sds
sds sdsnewlen(const void* init, size_t initlen)
{
    struct sdshdr *sh;

    if(init)
    {
        sh = zmalloc(SDS_HEADER_SIZE + initlen + SDS_TAIL_SYMBOL_SIZE);
    }
    else
    {
        sh = zcalloc(SDS_HEADER_SIZE + initlen + SDS_TAIL_SYMBOL_SIZE);
    }

    //内存分配失败
    if(!sh)
    {
        return  NULL;
    }

    sh->len = initlen;
    //新的sds不预留空间
    sh->free = 0;

    if(initlen && init)
    {
        memcpy(sh->buf, init, initlen);
    }

    //以`\0`结尾,兼容部分C的函数
    sh->buf[initlen] = '\0';

    return (char*)sh->buf;
}

//构造空sds
sds sdsempty()
{
    return sdsnewlen("", 0);
}

//拷贝sds的副本
sds sdsdup(const sds s)
{
    return sdsnewlen(s, sdslen(s));
}

//析构sds
void sdsfree(sds s)
{
    if(!s)
    {
        return;
    }
    zfree(s - SDS_HEADER_SIZE);
}

//sds的末尾扩充addlen个byte大小的内存
sds sdsMakeRoomFor(sds s, size_t addlen)
{
    size_t free = sdsvail(s);
    if(free >= addlen)
    {
        return  s;
    }

    struct sdshdr *sh = SDS_SDSHDR(s);
    size_t  oldlen = sh->len;
    size_t  newlen = (oldlen + addlen);

    //根据新长度确定要分配的内存的大小
    if(newlen < SDS_MAX_PREALLOC_SIZE)
    {
        newlen <<= 1;
    }
    else
    {
        newlen += SDS_MAX_PREALLOC_SIZE;
    }

    struct  sdshdr *nsh = zrealloc(sh, SDS_HEADER_SIZE + newlen + SDS_TAIL_SYMBOL_SIZE);
    if(nsh == NULL)
    {
        return NULL;
    }
    nsh->free = newlen - oldlen;
    return nsh->buf;
}

//填充s到指定长度,末尾以0填充
sds sdsgrowzero(sds s, size_t len)
{
    struct sdshdr *sh = SDS_SDSHDR(s);
    size_t currlen = sh->len;
    if(len <= currlen)
    {
        return s;
    }

    size_t appendsize = len - currlen;
    s = sdsMakeRoomFor(s, appendsize);
    if(s == NULL)
    {
        return  NULL;
    }

    sh = SDS_SDSHDR(s);
    size_t  totlen = sh->len + sh->free;
    //重置新加的内存空间为0,等同于
    //memset(s + currlen, 0, appendsize )
    //sh->buf[len] = '\0'
    memset(s + currlen, 0, appendsize + 1);
    sh->len = len;
    sh->free = totlen - len;
    return  s;
}

//将长度为len的data数据添加到s的末尾
sds sdscatlen(sds s, const void* data, size_t len)
{
    size_t oldlen = sdslen(s);
    s = sdsMakeRoomFor(s, len);
    if(s == NULL)
    {
        return  NULL;
    }

    struct sdshdr *sh = SDS_SDSHDR(s);
    memcpy(s + oldlen, data, len);
    s[oldlen + len] = '\0';

    sh->len = oldlen + len;
    sh->free = sh->free - len;

    return s;
}

//将字符串str添加到s的末尾
sds sdscat(sds s, const char* str)
{
    return sdscatlen(s, str, strlen(str));
}

//将sds类型的data添加到s的末尾
sds sdscatsds(sds s, const sds data)
{
    struct sdshdr* sh = SDS_SDSHDR(data);
    return  sdscatlen(s, sh->buf, sh->len);
}

//复制字符串data的前len个字符替换s的内容
sds sdscpylen(sds s, const char* data, size_t len)
{
    struct sdshdr *sh = SDS_SDSHDR(s);
    size_t totlen = sh->free + sh->len;
    //如果空间不够先分配空间
    if(totlen < len)
    {
        s = sdsMakeRoomFor(s, len - totlen);
        if(s == NULL)
        {
            return  NULL;
        }
        sh = SDS_SDSHDR(s);
        totlen = sh->len + sh->free;
    }

    //copy data
    memcpy(s, data, len);
    s[len] = '\0';

    //update sh
    sh->len = len;
    sh->free = totlen - len;

    return s;
}

//复制字符串data替换s的内容
sds sdscpy(sds s, const char* data)
{
    return sdscpylen(s, data, strlen(data));
}

//longlong转字符串,返回该字符串的长度
size_t sdsll2str(char *s, long long value)
{
    char *p;
    unsigned long long v = (unsigned long long)llabs(value);
    p = s;
    do {
        *p++ = '0'+ (v % 10);
        v /= 10;
    } while(v);
    if (value < 0) *p++ = '-';

    //计算字符串长度
    size_t ret = p-s;
    *p = '\0';
    //翻转字符串
    SDS_REVERSE(s, p);
    return ret;
}

//unsignedlonglong转字符串,返回该字符串的长度
size_t sdsull2str(char *s, unsigned long long v)
{
    char *p;
    p = s;
    do {
        *p++ = '0'+(v % 10);
        v /= 10;
    } while(v);

    //计算字符串长度
    size_t ret = p-s;
    *p = '\0';
    //翻转字符串
    SDS_REVERSE(s, p);
    return ret;
}

//将要打印的内容copy到sds的末尾并返回
sds sdscatvprintf(sds s, const char *fmt, va_list ap) {
    va_list cpy;
    char staticbuf[1024];
    char *buf = staticbuf;
    //为何要*2: 类似printf("i am %s")还会填充s,所以初始化buf的长度是fmt两倍的大小
    size_t buflen = strlen(fmt) * 2;

    //为了性能,尝试使用栈空间,如果不足再动态分配内存
    if (buflen > sizeof(staticbuf)) {
        buf = zmalloc(buflen);
        if (buf == NULL) {
            return NULL;
        }
    } else {
        buflen = sizeof(staticbuf);
    }

    //注:
    //int vsnprintf(char *str, size_t size, const char *format, va_list ap);
    //将可变参数格式化输出到一个字符数组
    //ret：-1表示格式化出错,很可能是encoding的问题
    //ret: 非负且<size才说明写入成功
    while (1) {
        va_copy(cpy, ap);
        int nret = vsnprintf(buf, buflen, fmt, cpy);
        if (nret > 0 && nret < buflen) {
            break;
        }
            //每次vsnprintf失败,扩充buf的长度为原先的两倍
        else {
            //如果是动态分配的buf,释放
            if (buf != staticbuf) {
                zfree(buf);
            }
            buflen *= 2;
            buf = zmalloc(buflen);
            if (buf == NULL) {
                return NULL;
            }
            continue;
        }
    }

    //copy到sds的末尾
    sds ret = sdscat(s, buf);
    //如果是动态分配的buf,释放
    if (buf != staticbuf) {
        zfree(buf);
    }
    return ret;
}

//将要打印的内容copy到sds的末尾并返回
sds sdscatprintf(sds s, const char *fmt, ...)
{
    va_list ap;
    //fetch the right args : the arg before ...
    va_start(ap, fmt);
    sds ret  = sdscatvprintf(s, fmt, ap);
    //set ap invalid
    va_end(ap);
    return ret;
}

//将要打印的内容copy到sds的末尾(很快)
sds sdscatfmt(sds s, char const *fmt, ...)
{
    struct sdshdr *sh = SDS_SDSHDR(s);
    char  next, buf[SDS_LONG_INT_STR_SIZE];
    char* str;
    size_t size;
    long long number;
    unsigned long long unumber;

    va_list ap;
    va_start(ap, fmt);

    const char* curr = fmt; //接下来要被解析的fmt中的字符
    size_t index = sh->len;  //sds的当前可被添加字符的buf下标
    while(*curr)
    {
        SDS_MAKE_SURE_HAVE_FREE(s, sh, 1);//确保s至少有一个byte的空间
        switch(*curr)
        {
            case '%':
                next = *(curr + 1);
                //curr++;
                switch(next)
                {
                    case 's'://s表示是c内置的char数组
                    case 'S'://S表示是redis内部的sds
                        str = va_arg(ap, char*);
                        size = (next == 's') ? strlen(str) : sdslen(str);
                        SDS_MAKE_SURE_HAVE_FREE(s, sh, size);
                        memcpy(s + index, str, size);
                        sh->len += size;
                        sh->free -= size;
                        index += size;
                        break;
                    case 'i'://int
                    case 'I'://long
                        number = (next == 'i') ? va_arg(ap, int) : va_arg(ap, long long);
                        memset(buf, 0, sizeof(buf));
                        size = (size_t)sdsll2str(buf, number);
                        SDS_MAKE_SURE_HAVE_FREE(s, sh, size);
                        memcpy(s + index, buf, size);
                        sh->len += size;
                        sh->free -= size;
                        index += size;
                        break;
                    case 'u'://unsigned int
                    case 'U'://unsigned long long
                        unumber = (next == 'u') ? va_arg(ap, unsigned int) : va_arg(ap, unsigned long long);
                        memset(buf, 0, sizeof(buf));
                        size = (size_t)sdsull2str(buf, unumber);
                        SDS_MAKE_SURE_HAVE_FREE(s, sh, size);
                        memcpy(s + index, buf, size);
                        sh->len += size;
                        sh->free -= size;
                        index += size;
                        break;
                    default:
                        s[index++] = next;
                        sh->len++;
                        sh->free--;
                        break;
                }
                break;
            default:
                s[index++] = *curr;
                sh->len++;
                sh->free--;
                break;
        }
        curr++;
    }
    va_end(ap);
    s[index] = '\0';
    return s;
}

//sds两端trim掉cset中所有的字符
sds sdstrim(sds s, const char *cset)
{
    struct sdshdr *sh = SDS_SDSHDR(s);

    char* sp = s;//start point
    char* ep = s + sh->len - 1;//end pointer

    //strchr(string, char) string中首次出现char的位置
    while(sp <= ep && strchr(cset, *sp)) sp++;
    while(ep >  sp && strchr(cset, *ep)) ep--;

    size_t len = (ep > sp) ? (ep - sp + 1) : 0;

    //如果有需要,需要把trim后的字符串前移
    if(sh->buf != sp)
    {
        //[sp, sp + len)移动到sh->buf
        memmove(sh->buf, sp, len);
    }
    sh->buf[len] = '\0';

    //更新sh的属性
    sh->free += sh->len - len;//oldlen - newlen;
    sh->len = len;
    return s;
}

//按照索引取sds字符串中的一段
void sdsrange(sds s, int start, int end)
{
    struct sdshdr *sh = SDS_SDSHDR(s);
    size_t oldlen = sh->len;

    if(oldlen == 0) return;;
    if(start < 0)
    {
        start += oldlen;
        if(start < 0) start = 0;
    }
    if(end < 0)
    {
        end += oldlen;
        if(end < 0) end = 0;
    }

    int newlen = (end > start) ? (end - start + 1) : 0;
    if(newlen != 0)
    {
        if(start >= (int)oldlen)
        {
            newlen = 0;
        }
        else if(end >= (int)oldlen)
        {
            end = (int)oldlen - 1;
            newlen = (end > start) ? (end - start + 1) : 0;
        }
    }
    else
    {
        start = 0;
    }

    if(start && newlen) memcpy(sh->buf, sh->buf + start, (size_t)newlen);
    sh->buf[newlen] = '\0';

    sh->free += (int)oldlen - newlen;
    sh->len = (size_t)newlen;
}

//置s为空字符串,不释放内存
void sdsclear(sds s)
{
    if(s == NULL)
    {
        return;
    }

    struct sdshdr *sh = SDS_SDSHDR(s);
    memset(sh->buf, 0, sh->len);
    sh->free += sh->len;
    sh->len = 0;
    sh->buf[0] = '\0';
}

//lhs == rhs 返回0, lhs < rhs返回负数, lhs > rhs返回正数
int sdscmp(const sds lhs, const sds rhs)
{
    size_t llen = sdslen(lhs);
    size_t rlen = sdslen(rhs);
    size_t minlen = llen < rlen ? llen : rlen;

    int cmp = memcmp(lhs, rhs, minlen);
    if(cmp == 0)
    {
        return  (int)(llen - rlen);
    }

    return cmp;
}

//将s以分隔符sep划分,分成一个sds的数组存储在, count是该数组的长度
sds* sdssplitlen(const char *s, int len, const char *sep, int seplen, int *count)
{
    if(seplen < 1 || sep == NULL || count == NULL || s == NULL) return NULL;

    size_t slots = 5;
    sds*   list;
    list = zmalloc(slots * sizeof(sds));
    if(list == NULL) return NULL;

    if(len == 0)
    {
        *count = 0;
        return NULL;
    }

    int elements = 0;
    int index = 0;

    //len - (seplen - 1) : 如果s的最后一个字符(分隔符也可能是字符串)是分隔符,那么不需要再分割
    for(int i = 0 ; i  < (len - (seplen - 1)); i++)
    {
        //确保返回的list至少有下一个元素的空间+末尾元素
        if(slots < elements + 2)
        {
            slots *= 2;
            sds *newlist = zrealloc(list, sizeof(sds) * (slots));
            if(newlist == NULL) goto cleanup;
            list = newlist;
        }
        //匹配到了分隔符
        if((seplen == 1 && *(s + i) == sep[0]) || memcmp(s + i, sep, (size_t)seplen) == 0)
        {
            list[elements] = sdsnewlen(s + index, (size_t)(i - index));
            if(list[elements] == NULL) goto cleanup;
            elements++;
            index = i + seplen;
            i += (seplen - 1);
        }
    }

    //添加分割完后的最后一个元素
    list[elements] = sdsnewlen(s + index, (size_t)(len- index));
    if(list[elements] == NULL) goto cleanup;
    elements++;
    *count = elements;
    return  list;

    //goto:for clean datas
    cleanup:
    {
        sdslistfree(list, elements);
        *count = 0;
        return  NULL;
    }
}

//释放sds的数组
void sdslistfree(sds *sdslist, int count)
{
    if(!sdslist) return;
    while(count--)
    {
        sdsfree(sdslist[count]);
    }
    zfree(sdslist);
}

//字符全转为小写
void sdstolower(sds s)
{
    if(!s) return;
    size_t len = sdslen(s);
    for(size_t i = 0 ; i < len; ++i)
    {
        s[i] = tolower(s[i]);
    }
}

//字符全转为大写
void sdstoupper(sds s)
{
    if(!s) return;
    size_t len = sdslen(s);
    for(size_t i = 0 ; i < len; ++i)
    {
        s[i] = toupper(s[i]);
    }
}

//long long转sds
sds sdsfromlonglong(long long value)
{
    char buf[SDS_LONG_INT_STR_SIZE];
    size_t len = sdsll2str(buf, value);
    return  sdsnewlen(buf, len);
}

//将长度为len的字符串p,转义后追加到s的后边
sds sdscatrepr(sds s, const char *p, size_t len)
{
    //1.将引号添加到s的末尾
    s = sdscatlen(s, "\"", 1);

    //2.遍历字符串p
    while(len--)
    {
        switch(*p)
        {
            case '\\':
            case '"' :
                s = sdscatprintf(s, "\\%c", *p);
                break;
            case '\n':s = sdscatlen(s, "\\n", 2); break;
            case '\r':s = sdscatlen(s, "\\r", 2); break;
            case '\t':s = sdscatlen(s, "\\t", 2); break;
            case '\a':s = sdscatlen(s, "\\a", 2); break;
            case '\b':s = sdscatlen(s, "\\b", 2); break;
            default:
                //检查当前字符是否可打印
                if(isprint(*p))
                {
                    s = sdscatprintf(s, "%c", *p);
                }
                else
                {
                    s = sdscatprintf(s, "\\x%02x", (unsigned char)*p);
                }
                break;
        }
        p++;
    }
    return sdscatlen(s, "\"", 1);
}

//将s中的所有出现在from中的字符替换成to中的字符
sds sdsmapchars(sds s, const char *from, const char *to, size_t setlen)
{
    if(s == NULL || from == NULL || to == NULL) return s;

    if(strlen(from) != strlen(to)) return s;

    if(strlen(from) != setlen) return s;

    size_t len = sdslen(s);
    for(size_t i = 0 ; i < len; ++i)
    {
        for(size_t j = 0; j < setlen; ++j)
        {
            if(from[j] == s[i])
            {
                s[i] = to[j];
                break;
            }
        }
    }
    return s;
}

//s的剩余空间去掉incr,末尾补'\0'
void sdsIncrLen(sds s, int incr)
{
    if(!s) return;

    struct sdshdr *sh = SDS_SDSHDR(s);
    assert(sh->free >= incr);

    sh->free -= incr;
    sh->len += incr;
    s[sh->len] = '\0';
}

//回收s的剩余空间
sds sdsRemoveFreeSpace(sds s)
{
    struct sdshdr *sh = SDS_SDSHDR(s);
    sh = zrealloc(sh, SDS_HEADER_SIZE + sh->len + 1);
    sh->free = 0;
    return  sh->buf;
}