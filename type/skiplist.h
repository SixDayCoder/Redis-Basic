//
// Created by Administrator on 2018/10/9.
//

#ifndef REDIS_BASIC_SKIPLIST_H
#define REDIS_BASIC_SKIPLIST_H

#include "memory.h"
#include "../object/object.h"

struct skiplistNode;

//跳表节点层级信息
struct skiplistNodeLevel
{
    //前进指针
    struct skiplistNode* forward;

    //步进距离
    unsigned int span;

} skiplistNodeLevel ;


//跳表节点信息
typedef struct skiplistNode
{
    //结点保存的对象
    object* obj;

    //分值,排序用
    double score;

    //后退指针
    struct skiplistNode* backward;

    //层级信息
    struct skiplistNodeLevel level[];

} skiplistNode ;


#define SKIPLIST_MAXLEVEL 32   //跳表最大层数
#define SKIPLIST_P 0.25        //跳表节点提升层数的概率
//跳表
typedef struct skiplist
{
    skiplistNode* head;//head本身不存放节点
    skiplistNode* tail;

    //节点数目
    unsigned long length;

    //跳表最大节点的层数
    int maxlevel;

} skiplist ;

//表示跳表开闭区间的结构
typedef struct skiplistRange
{
    double minScore, maxScore;
    int minInclude; //最小值是否包含
    int maxInclude; //最大值是否包含
} skiplistRange ;

//_score是否>=(或者大于,根据minInclude参数来决定)的值
#define SL_VAL_GT_RANGE_MIN(_score, _range)  ( (_range->minInclude ) ? (_score >= _range->minScore) : (_score > _range->minScore) )

//_score是否<=(或者小于,根据maxInclude参数来决定)的值
#define SL_VAL_LT_RANGE_MAX(_score, _range)  ( (_range->maxInclude ) ? (_score <= _range->maxScore) : (_score < _range->maxScore) )

//随机一个层数返回,根据随机算法所使用的幂次定律，越大的值生成的几率越小
int slRandomLevel(void);

//创建层数为level的跳跃表的节点
skiplistNode* slCreateNode(int level, double score, object* obj);

//释放跳表节点
void slReleaseNode(skiplistNode* node);

//创建新的跳跃表
skiplist* slCreate();

//释放跳表
void slRelease(skiplist* sl);

//判断range是否在跳表的分值范围之内
int slIsInRange(skiplist* sl, skiplistRange* range);

//跳表中第一个分值符合 range 中指定范围的节点。
skiplistNode *slFirstInRange(skiplist *sl, skiplistRange *range);

//跳表中最后一个分值符合 range 中指定范围的节点。
skiplistNode *slLastInRange(skiplist *sl, skiplistRange *range);

//跳表sl插入新的节点
skiplistNode* slInsert(skiplist *sl, double score, object* obj);

//删除函数,被delete, deleteRangeByScore, deleteByRank调用
void slDeleteNode(skiplist* sl, struct skiplistNode* node, struct skiplistNode** update);

//跳表sl删除值为obj的节点(score还得相同)
int slDelete(skiplist *sl, double score, object* obj);

//查询给定分值和对象的结点在跳表中的排位置
//由于head节点也被计算在内,所以rank以1位起始
unsigned long slGetRank(skiplist* sl, double score, object* obj);

//根据排位获取跳表中的节点
skiplistNode* slGetNodeByRank(skiplist* sl, unsigned long rank);

#endif //REDIS_BASIC_SKIPLIST_H
