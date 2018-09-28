//
// Created by Administrator on 2018/9/28.
//

#ifndef REDIS_BASIC_LIST_H
#define REDIS_BASIC_LIST_H

#include "memory.h"


//正向迭代,从头到尾
#define  LIST_ITER_DIRECTION_FORWARD  (0)
//逆向迭代,从尾到头
#define  LIST_ITER_DIRECTION_BACKWARD (1)

//双端队列
typedef struct listNode
{
    //前序结点
    struct listNode *prev;

    //后序结点
    struct listNode *next;

    //结点存储的值
    void *value;
} listNode;


//双端队列的迭代器,更方便访问list
typedef struct listIter
{
    //当前迭代到的结点
    listNode *next;

    //迭代方向
    int direction;
}listIter;


//链表结构
typedef struct list
{
    //表头结点
    listNode *head;

    //表尾结点
    listNode *tail;

    //结点数目
    size_t  len;

    //函数指针:结点复制函数
    void *(*dup)(void *ptr);

    //函数指针:结点值释放函数
    void *(*free)(void *ptr);

    //函数指针:节点值对比函数
    int  *(*match)(void *ptr);
}list;


#define LIST_LENGTH(_list) ( (_list)->len )
#define LIST_HEAD(_list) ( (_list)->head )
#define LIST_TAIL(_list) ( (_list)->tail )
#define LIST_DUP_METHOD(_list) ( (_list)->dup )
#define LIST_FREE_METHOD(_list) ( (_list)->free )
#define LIST_MATCH_METHOD(_list) ( (_list)->match )

#define LIST_SET_DUP_METHOD(_list, _method) ( (_list)->dup = (method) )
#define LIST_SET_FREE_METHOD(_list, _method) ( (_list)->free = (method) )
#define LIST_SET_MATCH_METHOD(_list, _method) ( (_list)->match = (method) )

#define LIST_NODE_PREV(_node) ( (_node)->prev )
#define LIST_NODE_NEXT(_node) ( (_node)->next )
#define LIST_NODE_VAL(_node)  ( (_node)->value )


//创建新链表,失败返回NULL
list* listCreate(void);

//释放链表和链表中所有的node
void listRealease(list *ls);

//添加结点到list的头部
list* listPushHead(list *ls, void* value);

//添加结点到list的尾部
list* listPushTail(list *ls, void* value);

//将值为value的node插在old_node的前面或后面,由after决定
list* listInsertNode(list *ls, listNode *oldnode, void *value, int after);

//删除ls的node结点
void listDelNode(list *ls, listNode *node);
#endif //REDIS_BASIC_LIST_H
