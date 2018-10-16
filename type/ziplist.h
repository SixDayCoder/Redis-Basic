//
// Created by Administrator on 2018/10/11.
//

#ifndef REDIS_BASIC_ZIPLIST_H
#define REDIS_BASIC_ZIPLIST_H

#include "memory.h"
#include <stdint.h>

//结束标志符
#define ZIP_END_FALG (255)

//5字节长度标志
#define ZIP_BIGLEN_FLAG (254)

//字符串编码掩码
#define ZIP_ENCODING_STR_MASK (0xc0)

//整数编码掩码
#define ZIP_ENCODING_INT_MASK (0x30)

//判断给定编码是否是字符串编码
#define ZIP_IS_STR_ENCODE(encode) (  ( (encode) & ZIP_ENCODING_STR_MASK ) < ZIP_ENCODING_STR_MASK )

//获取p指向的entry的encoding信息,存储在encoding中
#define ZIP_ENTRY_ENCODING(ptr, encoding) do {\
    (encoding) = ptr[0];\
    if( (encoding) < ZIP_ENCODING_STR_MASK) (encoding) &= ZIP_ENCODING_STR_MASK;\
}while(0);\

//字符串编码类型,0XB表示字符串的长度有几位, 06B表示字符串的长度用6个bits可以装下
#define ZIP_STR_06B (0 << 6)
#define ZIP_STR_14B (1 << 6)
#define ZIP_STR_32B (2 << 6)

//整数编码类型
#define ZIP_INT_16B (0xc0 | 0 << 4)
#define ZIP_INT_32B (0xc0 | 1 << 4)
#define ZIP_INT_64B (0xc0 | 2 << 4)
#define ZIP_INT_24B (0xc0 | 3 << 4)
#define ZIP_INT_08B (0xfe)

//4位整数编码的掩码和类型
#define ZIP_INT_IMM_MASK (0x0f)
#define ZIP_INT_IMM_MIN  (0xf1)    /* 11110001 */
#define ZIP_INT_IMM_MAX  (0xfd)    /* 11111101 */
#define ZIP_INT_IMM_VAL(v) (v & ZIP_INT_IMM_MASK)

//24 位整数的最大值和最小值
#define INT24_MAX (0x7fffff)
#define INT24_MIN (-INT24_MAX - 1)


/*
 *   ——————————————————————————————————————
 *   |  prev_entry_len | encoding | content|
 *   ———————————————————————————————————————
 *
 *   prev_entry_len可能是1个字节,也可能是5个字节
 *   encoding可能是1个字节
 *   (1)00xxxxxx -> content是长度小于等于63字节的字节数组
 *   (2)11000000 -> int16_t  -> 0xc0
 *   (3)11010000 -> int32_t  -> 0xc0 | 10000
 *   (4)11100000 -> int64_t  -> 0xc0 | 100000
 *   (5)11110000 -> 24位有符号整数 -> 0xc0 | 110000
 *   (6)11111110 -> 8位有符号整数  -> oxfe
 *   (7)1111xxxx
 *   encoding可能是2个字节 : 长度小于等于16383字节的字符串
 *   encoding可能是5个字节 : 长度小于等于4294967295的字符串
 *
 */
typedef struct zlentry
{
    //前置节点的长度, 编码prevlen所需的字节大小
    uint32_t prevlen, prevlensize;

    //当前节点的长度, 编码len所需的字节大小
    uint32_t len, lensize;

    //当前节点header的大小, 等于prevlensize + lensize
    uint32_t headersize;

    //当前节点希望使用的编码类型 : entry的len和lensize都和它本身的encoding有密切的关系
    unsigned char encoding;

    //指向当前节点的指针
    unsigned char* p;

} zlentry ;

//获取ziplist的zlbytes属性
#define ZIPLIST_BYTES(zl)  (*((uint32_t*)(zl)))

// 返回 ziplist 表头的大小, zlbytes(32) + zltail(32) + zllen(16)
#define ZIPLIST_HEADER_SIZE  (sizeof(uint32_t) * 2 + sizeof(uint16_t))

//获取ziplist的zltail的偏移
#define ZIPLIST_TAIL_OFFSET(zl)  (*((uint32_t*)((zl) + sizeof(uint32_t))))

//获取ziplist的tail结点
#define ZIPLIST_TAIL_ENTRY(zl)  ((zl)+ZIPLIST_TAIL_OFFSET(zl))

//获取ziplist的head结点
#define ZIPLIST_HEAD_ENTRY(zl)  ((zl)+ZIPLIST_HEADER_SIZE)

//获取ziplist的zllen属性
#define ZIPLIST_LEN(zl)  (*((uint16_t*)((zl) + 2 * sizeof(uint32_t))))

//增加ziplist的节点数目
#define ZIPLIST_INCR_LENGTH(zl, incr) do {\
    if(ZIPLIST_LEN(zl) < UINT16_MAX)\
        ZIPLIST_LEN(zl) = ZIPLIST_LEN(zl) + incr;\
}while(0);

//创建新的压缩列表
unsigned char* ziplistNew();

//将长度为slen的字符串s插入到压缩列表的表头
unsigned char *ziplistPushHead(unsigned char *zl, unsigned char *s, unsigned int slen);

//将长度为slen的字符串s插入到压缩列表的表尾
unsigned char *ziplistPushBack(unsigned char *zl, unsigned char *s, unsigned int slen);

//获取压缩列表的结点个数
unsigned int ziplistEntryCount(unsigned char* zl);

//返回zl的索引为index的zlentry的指针
unsigned char* ziplistIndex(unsigned char* zl, int index);

//p指向zl的某个entry,该函数返回该entry的下一个entry
unsigned char *ziplistNext(unsigned char *zl, unsigned char *p);

//p指向zl的某个entry,该函数返回该entry的前一个entry
unsigned char *ziplistPrev(unsigned char *zl, unsigned char *p);

//取出p指向的entry的值,字符串保存在sval中,整数值保存在lval中,get成功返回1,如果不是字符串sval是NULL
unsigned int ziplistGet(unsigned char *p, unsigned char **sval, unsigned int *slen, long long *lval);

//删除zl中*p指向的entry,并更新*p指向的位置
unsigned char *ziplistDelete(unsigned char *zl, unsigned char **p);

//从index索引开始连续删除num个节点
unsigned char *ziplistDeleteRange(unsigned char *zl, unsigned int index, unsigned int num);

//p指向的entry的content和s进行比较,相等返回1不相等返回0
unsigned int ziplistCompare(unsigned char *p, unsigned char *s, unsigned int slen);

//寻找和vstr相等的entry,并返回该entry的首指针
//skip表示每次比对之前都跳过skip个entry
unsigned char *ziplistFind(unsigned char *p, unsigned char *vstr, unsigned int vlen, unsigned int skip);

//打印ziplist的信息
void ziplistLog(unsigned char* zl);
#endif //REDIS_BASIC_ZIPLIST_H
