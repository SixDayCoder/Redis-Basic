//
// Created by Administrator on 2018/10/9.
//

#include "skiplist.h"

//随机一个层数返回,根据随机算法所使用的幂次定律，越大的值生成的几率越小
int slRandomLevel(void)
{
    int level = 1;
    while( (random() & 0xFFFF) < (SKIPLIST_P * 0xFFFF) ) level++;
    return (level < SKIPLIST_MAXLEVEL) ? level : SKIPLIST_MAXLEVEL;
}

//创建层数为level的跳跃表的节点
skiplistNode* slCreateNode(int level, double score, object* obj)
{
    skiplistNode* node = zmalloc(sizeof(*node) + level * sizeof(skiplistNodeLevel));
    node->obj = obj;
    node->score = score;
    return node;
}

//释放跳表节点
void slReleaseNode(skiplistNode* node)
{
    decRefCnt(node->obj);
    zfree(node);
}

//创建新的跳跃表
skiplist* slCreate()
{
    skiplist* sl = zmalloc(sizeof(*sl));
    //默认最大的层级是1,最底层
    sl->maxlevel = 1;
    sl->length = 0;
    //初始化表头节点
    sl->head = slCreateNode(SKIPLIST_MAXLEVEL, 0, NULL);
    sl->head->backward = NULL;
    for(int i = 0 ; i < SKIPLIST_MAXLEVEL; ++i)
    {
        sl->head->level[i].forward = NULL;
        sl->head->level[i].span = 0;
    }
    //初始化表尾节点
    sl->tail = NULL;
    return sl;
}

//释放跳表
void slRelease(skiplist* sl)
{
    skiplistNode* curr = sl->head->level[0].forward;
    skiplistNode* next = NULL;
    while(curr)
    {
        next = curr->level[0].forward;
        slReleaseNode(curr);
        curr = next;
    }
    zfree(sl);
}

//判断range是否在跳表的分值范围之内
int slIsInRange(skiplist* sl, skiplistRange* range)
{
    //先排除不合理的范围
    if( range->minScore > range->maxScore ||
        ( (range->minScore == range->maxScore) && (range->minInclude || range->maxInclude) ) )
    {
        return 0;
    }
    //检查最大分值
    skiplistNode* node = sl->tail;
    if(node == NULL || !SL_VAL_GT_RANGE_MIN(node->score, range) ) return 0;
    //检查最小分值
    node = sl->head->level[0].forward;
    if(node == NULL || !SL_VAL_LT_RANGE_MAX(node->score, range) ) return 0;
    return 1;
}

//跳表中第一个分值符合 range 中指定范围的节点。
skiplistNode *slFirstInRange(skiplist *sl, skiplistRange *range)
{
    //首先检查是否在范围内
    if(slIsInRange(sl, range)) return NULL;
    skiplistNode* curr = sl->head;
    for(int i = sl->maxlevel - 1; i >=0 ; i--)
    {
        //score <= range.min时迭代下一个节点
        while (curr->level[i].forward && !SL_VAL_GT_RANGE_MIN(curr->level[i].forward->score, range))
            curr = curr->level[i].forward;
    }
    curr = curr->level[0].forward;
    if(!curr) return NULL;
    //检查score <= max
    if(!SL_VAL_LT_RANGE_MAX(curr->score, range)) return NULL;
    return curr;
}

//跳表中最后一个分值符合 range 中指定范围的节点。
skiplistNode *slLastInRange(skiplist *sl, skiplistRange *range)
{
    //首先检查是否在范围内
    if(slIsInRange(sl, range)) return NULL;
    skiplistNode* curr = sl->head;
    for(int i = sl->maxlevel - 1; i >=0 ; i--)
    {
        //score <= range.max时迭代下一个节点
        while (curr->level[i].forward && !SL_VAL_LT_RANGE_MAX(curr->level[i].forward->score, range))
            curr = curr->level[i].forward;
    }
    if(!curr) return NULL;
    //检查score >= min
    if(!SL_VAL_GT_RANGE_MIN(curr->score, range)) return NULL;
    return curr;
}

//跳表sl插入新的节点
skiplistNode* slInsert(skiplist *sl, double score, object* obj)
{
    //待更新的节点,update[i]表示第(i+1)层第一个分值>=score的节点
    skiplistNode* update[SKIPLIST_MAXLEVEL] = { NULL };
    //排位
    unsigned int rank[SKIPLIST_MAXLEVEL] = { 0 };

    skiplistNode* curr = sl->head;
    //从最高层往下寻找
    for(int i = sl->maxlevel - 1; i >= 0; i--)
    {
        //如果i不是最高层,那么rank的初始值是它上一层的rank值
        //各个层的rank一层层累积
        rank[i] = ( i == (sl->maxlevel-1) ? 0 : rank[i + 1] );
        while(curr->level[i].forward &&
              ( curr->level[i].forward->score < score ||
              ( curr->level[i].forward->score == score && compareStringObject(curr->level[i].forward->obj, obj) < 0)))
        {
            //记录沿途跨越了多少个节点
            rank[i] += curr->level[i].span;
            //移动到下一个节点
            curr = curr->level[i].forward;
        }
        update[i] = curr;
    }
    //随机一个数作为新节点的层数
    int level = slRandomLevel();
    //如果新节点的层数比之前跳表最大的层数要大,要进行层数的提升
    //这意味着要对以前未使用的层进行初始化
    if(level > sl->maxlevel)
    {
        for(int i = sl->maxlevel; i < level; i++)
        {
            rank[i] = 0;
            update[i] = sl->head;
            update[i]->level[i].span = (unsigned int)sl->length;
        }
        sl->maxlevel = level;
    }
    else
    {
        //如果说level比以前的跳表的level要小,[level, maxlevel)这个范围内的节点的span值要增1
        for(int i = level; i < sl->maxlevel; i++) update[i]->level[i].span++;
    }
    //创建新节点
    skiplistNode* node = slCreateNode(level, score, obj);
    for(int i = 0 ; i < level; ++i)
    {
        node->level[i].forward = update[i]->level[i].forward;
        node->level[i].span = update[i]->level[i].span - (rank[0] - rank[1]);
        update[i]->level[i].forward = node;
        update[i]->level[i].span = (rank[0] - rank[i]) + 1;
    }
    //设置新节点的back指针
    node->backward = (update[0] == sl->head) ? NULL : update[0];
    if(node->level[0].forward)
    {
        node->level[0].forward->backward = node;
    }
    else
    {
        sl->tail = node;
    }
    //跳表节点数+1
    sl->length++;
    return node;
}

//删除函数,被delete, deleteRangeByScore, deleteByRank调用
void slDeleteNode(skiplist* sl, struct skiplistNode* node, struct skiplistNode** update)
{
    for(int i = 0 ; i < sl->maxlevel; i++)
    {
        if(update[i]->level[i].forward == node)
        {
            //udpate[i]的跨度  = update[i]的原跨度 + 要删除的节点node的跨度 - 1(node本身占一个跨度)
            update[i]->level[i].span += node->level[i].span - 1;
            update[i]->level[i].forward = node->level[i].forward;
        }
        else update[i]->level[i].span--;
    }
    //更新和被删除节点有关联的指针的forward和backward
    if(node->level[0].forward)
    {
        node->level[0].forward->backward = node->backward;
    }
    else
    {
        sl->tail = node->backward;
    }
    //更新跳表的最大层数,只有删除的节点在跳表的最高层的时候才会执行
    while(sl->maxlevel > 1 && sl->head->level[sl->maxlevel - 1].forward == NULL)
        sl->maxlevel--;
    //跳表节点数目-1
    sl->length--;
}

//跳表sl删除值为obj的节点(score还得相同)
int slDelete(skiplist *sl, double score, object* obj)
{
    //待更新的节点,update[i]表示第(i+1)层第一个分值>=score的节点,或者是空
    skiplistNode* update[SKIPLIST_MAXLEVEL] = { NULL };
    skiplistNode* curr = sl->head;
    for(int i = sl->maxlevel - 1; i >= 0; i--)
    {
        while(curr->level[i].forward &&
             ( curr->level[i].forward->score < score ||
             ( curr->level[i].forward->score == score && compareStringObject(curr->level[i].forward->obj, obj) < 0)))
        {
            curr = curr->level[i].forward;
        }
        update[i] = curr;
    }
    //curr是score>=传入的score的值
    curr = curr->level[0].forward;
    if(curr && score == curr->score && equalStringObject(curr->obj, obj))
    {
        slDeleteNode(sl, curr, update);
        slReleaseNode(curr);
        return 1;
    }
    //失败
    return 0;
}

//删除所有在分值范围内的元素
unsigned long slDeleteRangeByScore(skiplist* sl, skiplistRange* range)
{
    skiplistNode* update[SKIPLIST_MAXLEVEL] = { NULL };
    skiplistNode* curr = sl->head;
    for(int i  = sl->maxlevel; i >= 0; i--)
    {
        while(curr->level[i].forward &&
             (range->minInclude ? curr->level[i].forward->score < range->minScore : curr->level[i].forward->score <= range->minScore))
        {
            curr = curr->level[i].forward;
        }
        //记录待删除的结点
        update[i] = curr;
    }
    unsigned long removed = 0;
    skiplistNode* next = NULL;
    curr = curr->level[0].forward;
    while(curr && (range->maxInclude ? curr->score <= range->maxScore : curr->score < range->maxScore))
    {
        next = curr->level[0].forward;
        slDeleteNode(sl, curr, update);
        slReleaseNode(curr);
        curr = next;
        removed++;
    }
    return removed;
}

//删除所有在排位范围内的元素
unsigned long slDeleteRangeByRank(skiplist* sl, unsigned int start, unsigned int end)
{
    skiplistNode* update[SKIPLIST_MAXLEVEL] = { NULL };
    skiplistNode* curr = sl->head;
    unsigned long span = 0;
    unsigned long removed = 0;

    for(int i  = sl->maxlevel; i >= 0; i--)
    {
        while(curr->level[i].forward && (span + curr->level[i].span < start))
        {
            span += curr->level[i].span;
            curr = curr->level[i].forward;
        }
        update[i] = curr;
    }

    curr = curr->level[0].forward;
    span++;
    while(curr && span <= end)
    {
        skiplistNode* next = curr->level[0].forward;
        slDeleteNode(sl, curr, update);
        slReleaseNode(curr);
        curr = next;
        removed++;
        span++;
    }
    return removed;
}

//查询给定分值和对象的结点在跳表中的排位置
//由于head节点也被计算在内,所以rank以1位起始
unsigned long slGetRank(skiplist* sl, double score, object* obj)
{
    skiplistNode* curr = sl->head;
    unsigned long rank = 0;
    for(int i = sl->maxlevel - 1; i >= 0; i--)
    {
        while(curr->level[i].forward && (curr->level[i].forward->score < score))
        {
            //累积跨越的结点数目
            rank += curr->level[i].span;
            curr = curr->level[i].forward;
        }
        if(curr->obj && equalStringObject(curr->obj, obj)) return rank;
    }
    //没找到
    return 0;
}

//根据排位获取跳表中的节点
skiplistNode* slGetNodeByRank(skiplist* sl, unsigned long rank)
{
    skiplistNode* curr = sl->head;
    unsigned long span = 0;
    for(int i = sl->maxlevel - 1; i >= 0; i--)
    {
        while(curr->level[i].forward && (span + curr->level[i].span <= rank))
        {
            span += curr->level[i].span;
            curr = curr->level[i].forward;
        }
        if(span == rank) return curr;
    }
    return NULL;
}