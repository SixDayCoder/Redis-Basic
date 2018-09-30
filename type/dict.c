//
// Created by Administrator on 2018/9/29.
//

#include "dict.h"
#include <limits.h>
#include <assert.h>


//指示字典是否可以rehash
static int gDictCanResize = 1;

//无视gDictCanResize的值,如果字典已经使用的节点数/字典的size >= 强制resize的比率,进行rehash
static  int gReforeceResizeRatio = 5;

static void dictEnableResize() { gDictCanResize = 1;}

static void dictDisableResize() { gDictCanResize = 0;}

//根据需要初始化字典或者扩展字典
static int dictExpandIfNeeded(dict* d)
{
    //正在rehash,无需扩展
    if(DICT_IS_REHASH(d)) return DICT_PROCESS_OK;

    //1.如果0号哈希表是空(没有任何结点),初始化
    if(d->ht[0].size == 0)
    {
        return dictExpand(d, HASH_TABLE_INIT_SIZE);
    }

    //2.否则,看看是不是需要扩展
    //(1)字典已使用的结点和字典的大小接近1:1,并且dictCanResize是真
    //(2)字典已使用的结点和字典的大小超过了dictForeceResizeRatio
    unsigned long used = d->ht[0].used;
    unsigned long size = d->ht[0].size;
    if(used >= size)
    {
        //扩展的size数至少是原来的size的两倍
        if(gDictCanResize || (used / size >= gReforeceResizeRatio))
            return dictExpand(d, d->ht[0].size * 2);
    }

    return DICT_PROCESS_OK;
}

//计算键在key字典中的索引值,返回-1表示键已经存在
//如果d正在rehash,那么返回的索引是相对于1号哈希表的,rehash时插入的结点总是到1号哈希表
static int dictKeyIndex(dict* d, const void* key)
{
    unsigned long hash = 0;
    unsigned long index = 0;
    unsigned long tableid = 0;
    hashNode *node;

    //如果正在rehash,做一次渐进式rehash
    if(DICT_IS_REHASH(d)) dictRehash(d, 1);

    hash = DICT_HASH_KEY(d, key);
    for(tableid = 0; tableid <= 1; ++tableid)
    {
        index = hash & d->ht[tableid].sizemask;
        node = d->ht[tableid].table[index];
        while(node)
        {
            //键已经存在
            if(DICT_COMPARE_KEY(d, key, node->key))
            {
                return -1;
            }
            node = node->next;
        }
        //如果while结束后没有返回,说明0号哈希表不包含key
        //如果现在没有进行rehash,说明1号哈希表是没有key的,那么就会进行下一次循环去1号哈希表中查找key
        //否则就会直接break
        if(!DICT_IS_REHASH(d)) break;
    }

    //返回索引值
    return (int)index;
}

//初始化字典的各项属性
static int dictReset(dict* d, hashMethod* method, void* privdata)
{
    hashTableInit(&d->ht[0]);
    hashTableInit(&d->ht[1]);

    d->method = method;
    d->privdata = privdata;
    d->rehashindex = -1;
    d->iterators = 0;
    return DICT_PROCESS_OK;
}

//删除字典中键为key的node,如果isfree是1那么调用d的键值对释放函数
static int dictDeleteHelper(dict* d, const void* key, int isfree)
{
    if(DICT_IS_EMPTY(d)) return DICT_PROCESS_ERROR;

    //如果正在rehash,做一次渐进式rehash
    if(DICT_IS_REHASH(d)) dictRehash(d, 1);

    unsigned long hash = DICT_HASH_KEY(d, key);
    unsigned long index = 0;
    hashNode* node = NULL;
    hashNode* prenode = NULL;
    for(int tableid = 0 ; tableid <= 1; ++tableid)
    {
        index = hash & d->ht[tableid].sizemask;
        node = d->ht[tableid].table[index];
        prenode = NULL;
        while(node)
        {
            //找到目标结点
            if(DICT_COMPARE_KEY(d, key, node->key))
            {
                //如果前置结点不为空,续上链表
                if(prenode)
                {
                    prenode->next = node->next;
                }
                //否则,node肯定是头结点
                else
                {
                    d->ht[tableid].table[index] = node->next;
                }
                //是否调用释放函数
                if(isfree)
                {
                    DICT_FREE_HASH_NODE_KEY(d, node);
                    DICT_FREE_HASH_NODE_VAL(d, node);
                }
                //释放结点自身
                zfree(node);
                //更新计数
                d->ht[0].used--;
                return DICT_PROCESS_OK;
            }
            //否则进行迭代
            prenode = node;
            node = node->next;
        }
    }
    //否则没有找到要被删除的key
    return DICT_PROCESS_ERROR;
}

//释放ht中所有的元素
static void dictReleaseHelper(dict* d, hashTable* ht)
{
    if(!ht) return;
    if(!ht->table) return;

    //遍历整个哈希表
    for(int i = 0; i < ht->size; ++i)
    {
        //跳过空索引
        if(ht->table[i] == NULL) continue;

        hashNode* node = ht->table[i];
        hashNode* next = NULL;
        while(node)
        {
            next = node->next;
            DICT_FREE_HASH_NODE_KEY(d, node);
            DICT_FREE_HASH_NODE_VAL(d, node);
            ht->used--;
            node = next;
        }
    }

    zfree(ht->table);
    hashTableInit(ht);
}

//创建空字典
dict* dictCreate(hashMethod* method, void* privdata)
{
    dict *d = zmalloc(sizeof(*d));
    dictReset(d, method, privdata);
    return d;
}

//释放字典
void dictRelease(dict *d)
{
    if(!d) return;
    dictReleaseHelper(d, &d->ht[0]);
    dictReleaseHelper(d, &d->ht[1]);
    zfree(d);
}

//扩展或者创建一个dict
//(1)如果字典的ht[0]是空的,新的哈希表就是0号
//(2)如果字典的ht[1]非空,新的哈希表设置为1号,并且置rehash标志,字典可进行rehash
//size不够大或者正在rehash,返回失败:成功创建新哈希表返回成功
int dictExpand(dict *d, unsigned long size)
{
    //如果d正在rehash或者d的0号哈希表已经使用的节点数超过了给定的size
    if(DICT_IS_REHASH(d) || d->ht[0].used > size) return DICT_PROCESS_ERROR;

    //新哈希表 new hash table
    hashTable nht;
    unsigned long realsize = hashNextPowerOf2(size);
    nht.size = realsize;
    nht.sizemask = realsize - 1;
    //为哈希表分配空间
    nht.table = zcalloc(sizeof(hashNode*) * realsize);
    nht.used = 0;

    //1.如果0号哈希表是空,那么这是一次初始化
    if(d->ht[0].table == NULL)
    {
        d->ht[0] = nht;
    }
    //2.如果0号哈希表非空,那么必定是一次rehash
    else
    {
        d->ht[1] = nht;
        d->rehashindex = 0;//开启rehash,接下来可以对d进行rehash操作了
    }
    return DICT_PROCESS_OK;
}

//对字典执行n次渐进式的rehash
//返回0表示rehash完毕,否则表示仍然有键需要从0号哈希表移动到1号哈希表
int dictRehash(dict *d, int n)
{
    if(!DICT_IS_REHASH(d)) return 0;

    //执行n次rehash
    while(n--)
    {
        hashNode* curr;
        hashNode* next;
        //如果0号哈希表为空,那么迁移肯定完成了
        if(d->ht[0].used == 0)
        {
            zfree(d->ht[0].table);
            d->ht[0] = d->ht[1];
            hashTableInit(&d->ht[1]);
            d->rehashindex = -1;
            return 0;
        }
        //否则进行迁移
        assert(d->ht[0].size > (unsigned)d->rehashindex);
        //找到哈希表中第一非空索引
        while(d->ht[0].table[d->rehashindex] != NULL) d->rehashindex++;
        //非空索引的链表头结点
        curr = d->ht[0].table[d->rehashindex];
        while(curr)
        {
            next = curr->next;
            //重新计算(1)哈希表的哈希值(2)结点要插入的索引
            unsigned long hash = DICT_HASH_KEY(d, curr->key) & d->ht[1].sizemask;
            //结点插入
            curr->next = d->ht[1].table[hash];
            d->ht[1].table[hash] = curr;
            //计数器更新
            d->ht[0].used--;
            d->ht[1].used++;
            //下一个结点
            curr = next;
        }
        d->ht[0].table[d->rehashindex] = NULL;
        d->rehashindex++;
    }
    return 1;
}

//在字典中找到键为key的hashnode
hashNode* dictFind(dict *d, const void *key)
{
    //如果哈希表为空,返回NULL
    if(DICT_IS_EMPTY(d)) return NULL;

    //如果正在rehash,做一次渐进式rehash
    if(DICT_IS_REHASH(d)) dictRehash(d, 1);

    hashNode* node;
    unsigned long index = 0;
    unsigned long hash = DICT_HASH_KEY(d, key);
    for(int tableid = 0; tableid <= 1; tableid++)
    {
        index = hash & d->ht[tableid].sizemask;
        node = d->ht[tableid].table[index];
        while(node)
        {
            if(DICT_COMPARE_KEY(d, key, node->key))
            {
                return node;
            }
            node = node->next;
        }
    }
    //此处表明0号和1号哈希表都没有key
    return NULL;
}

//尝试创建一个新的结点,它的键是key,如果已经有这个key,返回NULL
hashNode *dictAddKey(dict *d, void *key)
{
    //如果正在rehash,做一次渐进式rehash
    if(DICT_IS_REHASH(d)) dictRehash(d, 1);

    //索引是-1表示键已经存在
    int index = dictKeyIndex(d, key);
    if(index == -1) return NULL;
    //正在rehash那么新插入的结点肯定要到1号哈希表中
    hashTable* ht = DICT_IS_REHASH(d) ? &d->ht[1] : &d->ht[0];
    hashNode* node = zmalloc(sizeof(*node));
    if(!node) return NULL;
    //新节点插入到链表头
    node->next = ht->table[index];
    ht->table[index] = node;
    //更新计数
    ht->used++;
    //设置新节点的键是key
    DICT_SET_HASH_NODE_KEY(d, node, key);

    return node;
}

//尝试将给定的(key,val)插入到字典中,当且仅当key不在字典时才可以成功
int dictAddPair(dict *d, void *key, void *val)
{
    hashNode* node = dictAddKey(d, key);
    if(!node) return DICT_PROCESS_ERROR;
    DICT_SET_HASH_NODE_VAL(d, node, val);
    return DICT_PROCESS_OK;
}

//将给定的键值插入到字典中,如果已经存在那么替换
int dictReplacePair(dict *d, void *key, void *val)
{
    //如果键值对不存在那么直接就可以插入成功
    if(dictAddPair(d, key, val) == DICT_PROCESS_OK) return DICT_PROCESS_OK;

    //否则说明已经存在该键值对,找出该节点
    hashNode* node = dictFind(d, key);
    //保存原有node的值
    hashNode tmp = *node;
    DICT_SET_HASH_NODE_VAL(d, node, val);
    DICT_FREE_HASH_NODE_VAL(d, &tmp);
    return DICT_PROCESS_OK;
}

//尝试创建一个新的结点,它的键是key,如果已经有这个key就返回那个node,否则会新加一个结点
hashNode* dictReplaceKey(dict *d, void *key)
{
    hashNode* node = dictFind(d, key);
    //如果key已经存在那么返回node,否则新加一个键为key的node
    return node == NULL ? dictAddKey(d, key) : node;
}

//删除键为key的结点,并且调用释放函数来释放key
int dictDelete(dict *d, const void *key)
{
    return dictDeleteHelper(d, key, 1);
}

//删除键为key的结点,不调用释放函数来释放key
int dictDeleteNoFree(dict *d, const void *key)
{
    return dictDeleteHelper(d, key, 0);
}