//
// Created by Administrator on 2018/10/11.
//

#ifndef REDIS_BASIC_ZIPLIST_H
#define REDIS_BASIC_ZIPLIST_H

#include "memory.h"
#include <stdint.h>

#define ZIPLIST_HEAD (1)
#define ZIPLIST_TAIL (0)

//结束标志符
#define ZIP_END (255)

//5字节长度标志
#define ZIP_FIVE_BYTES_LEN_FLAG (254)

//字符串编码掩码
#define ZIP_ENCODING_STR_MASK (0xc0)

//整数编码掩码
#define ZIP_ENCODING_INT_MASK (0x30)

//字符串编码类型
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

//判断给定编码是否是字符串编码
#define ZIP_IS_STR(_encod) ( ( (_encod) & ZIP_ENCODING_STR_MASK) ) < ZIP_ENCODING_STR_MASK )

typedef struct zlentry
{
    //前置节点的长度, 编码preLen所需的字节大小
    uint32_t prevLen, prevLenSize;

    //当前节点的长度, 编码len所需的字节大小
    uint32_t len, lenSize;

    //当前节点header的大小, 等于prevLenSize + lenSize
    uint32_t headerSize;

    //当前节点希望使用的编码类型
    unsigned char encoding;

    //指向当前节点的指针
    unsigned char* p;
} zlentry ;

//创建新的压缩列表
unsigned char* ziplistNew();

//将长度为slen的字符串s插入到压缩列表中,where:ZIPLIST_HEAD插入表头,ZIPLIST_TAIl插入表尾
unsigned char *ziplistPush(unsigned char *zl, unsigned char *s, unsigned int slen, int where);

#endif //REDIS_BASIC_ZIPLIST_H
