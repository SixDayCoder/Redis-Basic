//
// Created by Administrator on 2018/9/28.
//

#include "list.h"

//创建新链表,失败返回NULL
list* listCreate(void)
{
    list *ls;

    ls = zmalloc(sizeof(*ls));
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

//复制一份ls内容和orig一致
list* listDup(list *orig)
{
    if(!orig) return NULL;
    list* copy = listCreate();
    copy->dup = orig->dup;
    copy->free = orig->free;
    copy->match = orig->match;

    listIter* iter;
    listNode* node;
    iter = listGetIterator(orig, LIST_ITER_DIRECTION_FORWARD);
    while( (node = listNext(iter)) != NULL)
    {
        void* value;
        if(copy->dup)
        {
            value = copy->dup(node->value);
            if(!value)
            {
                listRealease(copy);
                listReleaseIterator(iter);
                return  NULL;
            }
        }
        else
        {
            value = node->value;
        }

        if(listPushTail(copy, value) == NULL)
        {
            listRealease(copy);
            listReleaseIterator(iter);
            return  NULL;
        }
    }
    listReleaseIterator(iter);
    return copy;
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

//获取list的迭代器
listIter* listGetIterator(list *ls, int direction)
{
    if(!ls) return NULL;
    listIter *iter = zmalloc(sizeof(*iter));
    //根据迭代方向设置迭代器的起始节点
    if(direction == LIST_ITER_DIRECTION_BACKWARD)
    {
        iter->next = ls->tail;
        iter->direction = LIST_ITER_DIRECTION_BACKWARD;
    }
    else
    {
        iter->next = ls->head;
        iter->direction = LIST_ITER_DIRECTION_FORWARD;
    }
    return  iter;
}

//释放listIter
void listReleaseIterator(listIter *iter)
{
    zfree(iter);
}

//根据迭代器获取下一个listNode
listNode* listNext(listIter *iter)
{
    if(!iter) return NULL;
    listNode *curr = iter->next;
    if(curr != NULL)
    {
        //保存下次被迭代的结点,防止curr被删除后造成的iter的next的丢失
        if(iter->direction == LIST_ITER_DIRECTION_FORWARD)
        {
            iter->next = curr->next;
        }
        else
        {
            iter->next = curr->prev;
        }
    }
    return curr;
}

//将迭代器方向设置为forward,然后迭代器指向列表的头结点
void listIterResetForward(list *ls, listIter *iter)
{
    if(!iter || !ls) return;
    iter->next = ls->head;
    iter->direction = LIST_ITER_DIRECTION_FORWARD;
}

//将迭代器方向设置为backward,然后迭代器指向列表的尾结点
void listIterResetBackward(list *ls, listIter *iter)
{
    if(!iter || !ls) return;
    iter->next = ls->tail;
    iter->direction = LIST_ITER_DIRECTION_BACKWARD;
}

//在list中查找value为key的node
listNode *listSearchKey(list *ls, void *key)
{
    if(!ls) return NULL;

    listIter* iter;
    listNode* node;
    iter = listGetIterator(ls, LIST_ITER_DIRECTION_FORWARD);
    while((node = listNext(iter)) != NULL)
    {
        //存在对比函数
        if(ls->match)
        {
            if(ls->match(node->value, key))
            {
                listReleaseIterator(iter);
                return node;
            }
        }
        else
        {
            if(key == node->value)
            {
                listReleaseIterator(iter);
                return node;
            }
        }
    }
    //未找到
    listReleaseIterator(iter);
    return NULL;
}

//查找list[index],从0开始,如果index是负数,那么-1表示最后一个结点,相当于list[len-1]
listNode* listIndex(list *ls, long index)
{
    if(!ls) return NULL;
    listNode* node;
    //从表尾寻找
    if(index < 0)
    {
        index = (-index) - 1;
        node = ls->tail;
        while(index-- && node) node = node->prev;
    }
    //从表头寻找
    else
    {
        node = ls->head;
        while(index++ && node) node = node->next;
    }
    return node;
}

//旋转ls列表
void listRotate(list *ls)
{
    if(!ls) return;
    if(ls->len <= 1) return;
    //取出表尾结点
    listNode *node = ls->tail;
    ls->tail = ls->tail->prev;
    ls->tail->next = NULL;
    //插入到表头
    node->prev = NULL;
    ls->head->prev = node;
    node->next = ls->head;
    ls->head = node;
}
