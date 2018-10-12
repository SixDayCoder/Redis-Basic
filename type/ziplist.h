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
#define ZIP_IS_STR_ENCODE(encode) ( ( (encode) & ZIP_ENCODING_STR_MASK) ) < ZIP_ENCODING_STR_MASK )

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


// 返回 ziplist 表头的大小, zlbytes(32) + zltail(32) + zllen(16)
#define ZIPLIST_HEADER_SIZE  (sizeof(uint32_t) * 2 + sizeof(uint16_t))

//获取ziplist的zlbytes属性
#define ZIPLIST_BYTES(zl)  (*((uint32_t*)(zl)))

//获取ziplist的zltail的偏移
#define ZIPLIST_TAIL_OFFSET(zl)  (*((uint32_t*)((zl) + sizeof(uint32_t))))

//获取ziplist的zltail属性
#define ZIPLIST_TAIL(zl)  ((zl)+ZIPLIST_TAIL_OFFSET(zl))


//获取ziplist的zllen属性
#define ZIPLIST_LEN(zl)  (*((uint16_t*)((zl) + 2 * sizeof(uint32_t))))

//返回ziplist的第一个entry的首地址
#define ZIPLIST_HEAD_ENTRY_ADDR(zl) ( (zl) + ZIPLIST_HEADER_SIZE)

//返回ziplist的最后一个entry的首地址
#define ZIPLIST_END_ENTRY_ADDR(zl) ( (zl) + ZIPLIST_TAIL_OFFSET(zl) )


//创建新的压缩列表
unsigned char* ziplistNew();

//将长度为slen的字符串s插入到压缩列表的表头
unsigned char *ziplistPushHead(unsigned char *zl, unsigned char *s, unsigned int slen);

//将长度为slen的字符串s插入到压缩列表的表尾
unsigned char *ziplistPushBack(unsigned char *zl, unsigned char *s, unsigned int slen);

#endif //REDIS_BASIC_ZIPLIST_H
