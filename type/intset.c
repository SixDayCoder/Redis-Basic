//
// Created by Administrator on 2018/10/11.
//

#include "intset.h"
#include "utlis.h"

//调整整数集合的
static intset* intsetResize(intset* is, uint32_t len)
{
    uint32_t size = len * is->encoding;
    is = zrealloc(is, sizeof(intset) + size);
    return is;
}

//根据给定的编码方式,返回在pos位置的元素
static int64_t intsetGetEncoded(intset* is, int pos, uint32_t encode)
{
    if(encode == INTSET_ENCODING_INT64)
    {
        int64_t v64;
        memcpy(&v64, ((int64_t*)is->contents) + pos, sizeof(int64_t));
        return v64;
    }
    else if(encode == INTSET_ENCODING_INT32)
    {
        int32_t v32;
        memcpy(&v32, ((int32_t*)is->contents) + pos, sizeof(int32_t));
        return v32;
    }
    else if(encode == INTSET_ENCODING_INT16)
    {
        int16_t v16;
        memcpy(&v16, ((int16_t*)is->contents) + pos, sizeof(int16_t));
        return v16;
    }
}

//返回is在pos位置的元素
static int64_t intsetGetPos(intset* is, int pos)
{
    return intsetGetEncoded(is, pos, is->encoding);
}

//在intset位置为pos的地方设置val
static void intsetSetVal(intset* is, int pos, int64_t val)
{
    uint32_t encoding = is->encoding;
    if(encoding == INTSET_ENCODING_INT64)
    {
        ((int64_t*)is->contents)[pos] = val;
    }
    else if(encoding == INTSET_ENCODING_INT32)
    {
        ((int32_t*)is->contents)[pos] = (int32_t)val;
    }
    else if(encoding == INTSET_ENCODING_INT16)
    {
        ((int16_t*)is->contents)[pos] = (int16_t)val;
    }
}

//提升整数集合的类型并插入新元素val
static intset* intsetUpgradeAndAdd(intset* is, int64_t val)
{
    //current 当前编码
    uint32_t cEncoding = is->encoding;

    //new  新编码
    uint32_t nEncoding = intsetValEncodingType(val);

    //当前intset长度
    uint32_t length = is->length;

    //val一定满足: val<intset所有元素 || val > intset所有元素
    int isInsertToHead = val < 0 ? 1 : 0;

    is->encoding = nEncoding;
    intsetResize(is, length + 1);
    while(length--)
    {
        //取末尾的元素
        int64_t v = intsetGetEncoded(is, length, cEncoding);
        //如果是要插入intset的首部,那么所有的元素实际上都要后移一个pos,尾部则不需要后移
        //不需要后移时isInsertToHead是0
        intsetSetVal(is, length + isInsertToHead, v);
    }
    if(isInsertToHead)
        intsetSetVal(is, 0, val);
    else
        intsetSetVal(is, is->length, val);
    is->length += 1;
    return is;
}

//intset的二分查找实现,找到返回1且pos是val所在的索引,找不到返回0,且pos是val可以插入的索引
static uint8_t intsetSearch(intset* is, int64_t val, uint32_t* pos)
{
    if(!pos) return 0;

    int left = 0;
    int right = is->length - 1;
    int mid = -1;
    //intset为空
    if(is->length == 0)
    {
        *pos = 0;
        return 0;
    }
    else
    {
        //val > max那么返回尾部
        if(val > intsetGetPos(is, is->length- 1))
        {
            *pos = is->length;
            return 0;
        }
        //val < min那么返回头部
        if(val < intsetGetPos(is, 0))
        {
            *pos = 0;
            return 0;
        }
        //否则二分
        int64_t curr = 0;
        while(left <= right)
        {
            mid = (left + right) / 2;
            curr = intsetGetPos(is, mid);
            if(curr < val)
                left = mid + 1;
            else if(curr > val)
                right = mid - 1;
            else
                break;
        }
        if(val == curr)
        {
            *pos = (uint32_t)mid;
            return 1;
        }
        else
        {
            *pos = 0;
            return 0;
        }
    }
}

static void intsetMove(intset* is, uint32_t from, uint32_t to)
{
    void *src = NULL;
    void *dst = NULL;

    //要移动的元素的个数
    uint32_t count = is->length - from;

    //编码
    uint32_t encoding = is->encoding;

    //最终要移动的字节数目
    uint32_t bytes = 0;

    if(encoding == INTSET_ENCODING_INT64)
    {
        src = (int64_t*)is->contents + from;
        dst = (int64_t*)is->contents + to;
        bytes = count * sizeof(int64_t);
    }
    else if(encoding == INTSET_ENCODING_INT32)
    {
        src = (int32_t*)is->contents + from;
        dst = (int32_t*)is->contents + to;
        bytes = count * sizeof(int32_t);
    }
    else if(encoding == INTSET_ENCODING_INT16)
    {
        src = (int16_t*)is->contents + from;
        dst = (int16_t*)is->contents + to;
        bytes = count * sizeof(int16_t);
    }
    memmove(dst, src, bytes);
}

//创建新的整数集合
intset *intsetNew(void)
{
    intset* is = zmalloc(sizeof(intset));
    is->encoding = INTSET_ENCODING_INT16;
    is->length = 0;
    return is;
}

//整数集合中添加新的元素,是否成功写在success中
intset *intsetAdd(intset *is, int64_t value, uint8_t *success)
{
    if(!success) return is;
    uint32_t encoding = intsetValEncodingType(value);
    //如果加入的val的编码超过当前intset的编码,那么需要对intset进行类型的提升
    if(encoding > is->encoding)
    {
        *success = 1;
        return intsetUpgradeAndAdd(is, value);
    }
    //否则,不需要提升
    //(1)如果val已经存在,那么不需要插入,success是0
    uint32_t pos = 0;
    if(intsetSearch(is, value, &pos))
    {
        *success = 0;
        return is;
    }
    //(2)否则需要插入到指定位置
    else
    {
        is = intsetResize(is, is->length + 1);
        //如果要插入的位置不在末尾,肯定要移动元素之后再插入
        if(pos < is->length) intsetMove(is, pos, pos + 1);
        intsetSetVal(is, pos, value);
        is->length += 1;
        *success = 1;
        return is;
    }
}

//整数集合中删除新的元素,是否成功写在success中
intset *intsetRemove(intset *is, int64_t value, int *success)
{
    if(!success) return is;
    uint32_t encoding = intsetValEncodingType(value);
    uint32_t pos = 0;

    *success = 0;
    //encoding必须<=当前intset的编码才有可能找到
    if(encoding <= is->encoding && intsetSearch(is, value, &pos))
    {
        uint32_t oldlen = is->length;
        //如果要删除的元素不是尾部元素,那么要移动元素之后删除
        if(pos < oldlen - 1) intsetMove(is, pos + 1, pos);
        is = intsetResize(is, oldlen - 1);
        is->length -= 1;
        *success = 1;
    }
    return is;
}

//整数集合中是否存在值为value的元素
uint8_t intsetExist(intset* is, int64_t value)
{
    uint32_t encoding = intsetValEncodingType(value);
    uint32_t pos = 0;
    if( (encoding <= is->encoding) && intsetSearch(is, value, &pos) > 0) return 1;
    return 0;
}

//随机返回整数集合中的一个元素
int64_t intsetRandom(intset *is)
{
    return intsetGetPos(is, rand() % is->length);
}

//获取整数集合中下标为pos的元素,结果放在value中
uint8_t intsetGet(intset *is, uint32_t pos, int64_t *value)
{
    if(pos < is->length)
    {
        *value = intsetGetPos(is, pos);
        return  1;
    }
    return 0;
}

//整数集合占用内存的大小
size_t intsetBlobLen(intset *is)
{
    return sizeof(intset) + is->length * is->encoding;
}

//判断value的编码方式
uint32_t intsetValEncodingType(int64_t v)
{
    if(v < INT32_MIN || v > INT32_MAX)
        return INTSET_ENCODING_INT64;
    else if(v < INT16_MIN || v > INT16_MAX)
        return INTSET_ENCODING_INT32;
    else
        return INTSET_ENCODING_INT16;
}