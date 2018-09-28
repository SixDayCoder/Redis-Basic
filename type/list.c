//
// Created by Administrator on 2018/9/28.
//

#include "list.h"

//创建新链表,失败返回NULL
list* listCreate(void)
{
    list *ls;

    ls = zmalloc(sizeof(*ls));
    if(!ls) return NULL;

    ls->head = ls->tail = NULL;
    ls->len = 0;
    ls->dup = NULL;
    ls->free = NULL;
    ls->match = NULL;

    return ls;
}

//释放链表和链表中所有的node
void listRealease(list *ls)
{
    if(!ls) return;

    listNode *curr = LIST_HEAD(ls);
    listNode *next = NULL;
    size_t len = LIST_LENGTH(ls);

    while(len--)
    {
        next = curr->next;
        //如果设置了释放函数,记得调用
        if(ls->free)
        {
            ls->free(curr->value);
        }
        //回收内存
        zfree(curr);
        //迭代
        curr = next;
    }
    //回收内存
    zfree(ls);
}

//添加结点到list的头部
list* listPushHead(list *ls, void* value)
{
    if(!ls || !value) return ls;

    listNode *node = zmalloc(sizeof(*node));
    if(!node) return NULL;
    node->value = value;

    if(ls->len == 0)
    {
        ls->head = ls->tail = node;
        node->prev = node->next = NULL;
    }
    else
    {
        node->prev = NULL;
        node->next = ls->head;
        ls->head->prev = node;
        ls->head = node;
    }

    ls->len++;
    return ls;
}


//添加结点到list的尾部
list* listPushTail(list *ls, void* value)
{
    if(!ls || !value) return ls;

    listNode *node = zmalloc(sizeof(*node));
    if(!node) return NULL;
    node->value = value;

    if(ls->len == 0)
    {
        ls->head = ls->tail = node;
        node->prev = node->next = NULL;
    }
    else
    {
        node->next = NULL;
        node->prev = ls->tail;
        ls->tail->next = node;
        ls->tail = node;
    }

    ls->len++;
    return ls;
}

//将值为value的node插在old_node的前面或后面,由after决定
list* listInsertNode(list *ls, listNode *oldnode, void *value, int after)
{
    if(!ls || !value || !oldnode) return ls;

    listNode *node = zmalloc(sizeof(*node));
    if(!node) return NULL;
    node->value = value;

    //插入到oldNode的后面
    if(after)
    {
        node->prev = oldnode;
        node->next = oldnode->next;
        //如果oldnode是尾部结点
        if(ls->tail == oldnode)
        {
            ls->tail = node;
        }
    }
    //否则插入到oldnode的前面
    else
    {
        node->next = oldnode;
        node->prev = oldnode->prev;
        //如果oldnode是头部结点
        if(ls->tail == oldnode)
        {
            ls->tail = node;
        }
    }

    if(node->prev != NULL)
    {
        node->prev->next = node;
    }

    if(node->next != NULL)
    {
        node->next->prev = node;
    }

    ls->len++;
    return ls;
}

//删除ls的node结点
void listDelNode(list *ls, listNode *node)
{
    if(!ls) return;

    //前置结点调整
    if(node->prev)
    {
        node->prev->next = node->next;
    }
    else
    {
        ls->head = node->next;
    }

    //后序结点调整
    if(node->next)
    {
        node->next->prev = node->prev;
    }
    else
    {
        ls->tail = node->prev;
    }

    //若free函数存在,调用free函数清除结点的值
    if(ls->free) ls->free(node->value);

    //回收node的内存
    zfree(node);

    //元素数目更改
    ls->len--;
}