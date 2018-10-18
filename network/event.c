//
// Created by Administrator on 2018/10/16.
//

#include "event.h"
#include "memory.h"
#include "utlis.h"
#include <unistd.h>
#include <poll.h>
#include <errno.h>

#ifdef HAVE_EPOLL
#include "epoll.c"
#else
#include "select.c"
#endif


#define EVENTLOOP_CLEAR(eventloop) do {\
    if(eventLoop)\
    {\
        zfree(eventLoop->fileEvents);\
        zfree(eventLoop->readyFileEvents);\
        zfree(eventLoop);\
    }\
}while(0);


EventLoop* createEventLoop(int setsize)
{
    EventLoop* eventLoop;

    if((eventLoop = zmalloc(sizeof(*eventLoop))) == NULL)
    {
        EVENTLOOP_CLEAR(eventLoop);
        return NULL;
    }

    eventLoop->fileEvents = zmalloc(sizeof(FileEvent) * setsize);
    eventLoop->readyFileEvents = zmalloc(sizeof(ReadyFileEvent) * setsize);
    if(eventLoop->fileEvents == NULL || eventLoop->readyFileEvents == NULL)
    {
        EVENTLOOP_CLEAR(eventLoop);
        return NULL;
    }

    eventLoop->setsize = setsize;
    eventLoop->lastDoTimeEvent = time(NULL);
    eventLoop->timeEventHead = NULL;
    eventLoop->timeEventNextId = 0;
    eventLoop->stop = 0;
    eventLoop->maxfd = -1;
    eventLoop->beforeSleepCallBack = NULL;
    if(createAPIData(eventLoop) == -1)
    {
        EVENTLOOP_CLEAR(eventLoop);
        return NULL;
    }

    for(int i = 0 ; i < setsize; ++i)
        eventLoop->fileEvents[i].mask = FILE_EVENT_NONE;
    return eventLoop;
}

void releaseEventLoop(EventLoop* eventLoop)
{
    releaseAPIData(eventLoop);
    EVENTLOOP_CLEAR(eventLoop);
}

void stopEventLoop(EventLoop* eventLoop)
{
    if(eventLoop) eventLoop->stop = 1;
}

//根据mask的值来监听fd的状态,fd可用时执行callback
int createFileEvent(EventLoop* eventLoop, int fd, int mask, FileEventCallBack* fileEventCallBack, void* clientData)
{
    if(fd >= eventLoop->setsize)
    {
        errno = ERANGE;
        return EVENT_ERR;
    }
    FileEvent* event = &eventLoop->fileEvents[fd];
    //监听fd指定的事件
    if(addEvent(eventLoop, fd, mask) == -1) return EVENT_ERR;

    event->mask |= mask;
    if(mask & FILE_EVENT_READABLE) event->readCallBack = fileEventCallBack;
    if(mask & FILE_EVENT_WRITEBABLE) event->writeCallBack = fileEventCallBack;

    event->clientData = clientData;
    //如果有必要更新事件循环的最大fd
    if(fd > eventLoop->maxfd) eventLoop->maxfd = fd;

    return EVENT_OK;
}

//将fd从mask指定的监听队列中删除(mask指示了哪种类型)
void deleteFileEvent(EventLoop *eventLoop, int fd, int mask)
{
    if(fd >= eventLoop->setsize) return;

    FileEvent* event = &eventLoop->fileEvents[fd];
    //如果没有设置过事件监听类型return
    if(event->mask == FILE_EVENT_NONE);

    //去掉mask对应的事件类型
    event->mask = event->mask & (~mask);
    if(fd == eventLoop->maxfd && event->mask == FILE_EVENT_NONE)
    {
        //从大往小找第一个事件不为none的event
        int i = 0;
        for(i = eventLoop->maxfd - 1; i >= 0; --i)
            if(eventLoop->fileEvents[i].mask != FILE_EVENT_NONE)
                break;
        eventLoop->maxfd = i;
    }
    delEvent(eventLoop, fd, mask);
}

//获取监听fd事件的类型掩码
int getFileEventsMask(EventLoop* eventLoop, int fd)
{
    if(fd >= eventLoop->setsize) return FILE_EVENT_NONE;
    return eventLoop->fileEvents[fd].mask;
}


//将millsecons毫秒加到当前时间,返回值写在sec和ms中
static void addMillsOnNow(long long millseconds, long long *sec, long long* ms)
{
    long long nowSec = 0;
    long long nowMs = 0;
    getTime(&nowSec, &nowMs);

    long long whenSec = nowSec + millseconds / 1000;
    long long whenMs = nowMs + millseconds % 1000;

    if(whenMs >= 1000)
    {
        whenSec++;
        whenMs -= 1000;
    }

    *sec = whenSec;
    *ms = whenMs;
}

//创建时间事件,返回其id
long long createTimeEvent(EventLoop* eventLoop, long long millseconds, TimeEventCallBack* timeEventCallBack, void* clientData, EventFinalizerCallBack* finalizerCallBack)
{
    long long id = eventLoop->timeEventNextId + 1;

    TimeEvent* te = zmalloc(sizeof(*te));
    if(te == NULL) return EVENT_ERR;

    te->id = id;

    //时间事件在millseconds毫秒后触发
    addMillsOnNow(millseconds, &te->whenSec, &te->whenMs);
    te->timeCallBack = timeEventCallBack;
    te->finalizerCallBack = finalizerCallBack;
    te->clientData = clientData;
    te->next = eventLoop->timeEventHead;
    eventLoop->timeEventHead->next = te;
    eventLoop->timeEventNextId = id;
    return id;
}

//删除时间事件
int deleteTimeEvent(EventLoop *eventLoop, long long id)
{
    TimeEvent* te = eventLoop->timeEventHead;
    TimeEvent *prev = NULL;
    while(te)
    {
        if(te->id == id)
        {
            //删除的是头节点
            if(prev == NULL)
            {
                eventLoop->timeEventHead = te->next;
            }
            else
            {
                prev->next = te->next;
            }
            if(te->finalizerCallBack) te->finalizerCallBack(eventLoop, te->clientData);
            zfree(te);
            return EVENT_OK;
        }
        prev = te;
        te = te->next;
    }
    return EVENT_ERR;
}

//寻找当前时间点可以执行的时间事件
static TimeEvent* findNearestTimer(EventLoop* eventLoop)
{
    TimeEvent* te = eventLoop->timeEventHead;
    TimeEvent* nearest = te;

    while(te)
    {
        if( te->whenSec < nearest->whenSec ||
           (te->whenSec == nearest->whenSec && te->whenMs < nearest->whenSec))
            nearest = te;
        te = te->next;
    }
    return nearest;
}

//执行时间事件
static int executeTimeEvents(EventLoop* eventLoop)
{
    TimeEvent* te = NULL;
    time_t now = time(NULL);
    int processed = 0;

    //重置事件的到达时间
    if(now < eventLoop->lastDoTimeEvent)
    {
        te = eventLoop->timeEventHead;
        while(te)
        {
            te->whenSec = 0;
            te = te->next;
        }
    }
    eventLoop->lastDoTimeEvent = now;

    long long maxid = eventLoop->timeEventNextId - 1;
    te = eventLoop->timeEventHead;
    while(te)
    {
        //跳过无效事件
        if(te->id > maxid)
        {
            te = te->next;
            continue;
        }
        long long nowSec = 0;
        long long nowMs = 0;
        getTime(&nowSec, &nowMs);
        if(nowSec > te->whenSec || (nowSec == te->whenSec && nowMs >= te->whenMs))
        {
            int retval = te->timeCallBack(eventLoop, te->id, te->clientData);
            processed++;
            //记录是否需要循环执行该时间事件
            if(retval != TIME_EVENT_NO_MORE)
            {
                addMillsOnNow(retval, &te->whenSec, &te->whenMs);
            }
            //否则要删除该时间事件
            else
            {
                deleteTimeEvent(eventLoop, te->id);
            }

            //执行事件之后,事件列表可能已经被改变,将te放回表头继续执行
            te = eventLoop->timeEventHead;
        }
        else
        {
            te = te->next;
        }
    }

    return processed;
}

//处理所有到达的时间事件和已就绪的文件事件
//eventType = EVENT_TYPE_NONE, 函数不作动作直接返回
//eventType include EVENT_TYPE_ALL,  处理所有类型的事件
//eventType include EVENT_TYPE_FILE, 处理就绪的文件事件
//eventType include EVENT_TYPE_TIME, 处理所有的时间事件
//eventType include EVENT_TYPE_DONT_WAIT, 处理完所有非阻塞事件后立即返回
//返回处理过的事件的数目
int executeEvents(EventLoop *eventLoop, int eventType)
{
    if( !(eventType & EVENT_TYPE_FILE) && !(eventType & EVENT_TYPE_TIME) ) return 0;

    //执行的事件数目
    int processed = 0;
    //(1)文件事件存在(2)或者是时间事件
    if(eventLoop->maxfd != -1 || ( (eventType & EVENT_TYPE_TIME) && !(eventType & EVENT_TYPE_DONT_WAIT)) )
    {
        struct timeval tv;
        struct timeval *tvp = &tv;
        TimeEvent* nearest = findNearestTimer(eventLoop);
        //判断是否可以执行时间事件
        if(nearest)
        {
            long long nowSec = 0;
            long long nowMs = 0;
            getTime(&nowSec, &nowMs);
            //计算还需要多少毫秒才能执行时间事件
            long long mills = (nearest->whenSec - nowSec) * 1000 + (nearest->whenMs - nowMs);
            //说明现在还不能执行时间事件
            if(mills > 0)
            {
                tvp->tv_sec = mills / 1000;
                tvp->tv_usec = (mills % 1000) * 1000;
            }
            //如果事件差<0说明事件可以执行,将秒和毫秒=0,即不阻塞
            else
            {
                tvp->tv_sec = 0;
                tvp->tv_usec = 0;
            }
        }
        //判断是否可以执行文件事件
        else
        {
            //不阻塞立即执行
            if(eventType & EVENT_TYPE_DONT_WAIT)
            {
                tvp->tv_sec = 0;
                tvp->tv_usec = 0;
            }
            //否则,一直阻塞到有文件事件发生
            else
            {
                tvp = NULL;
            }
        }

        int numevents = apiPoll(eventLoop, tvp);
        for(int i = 0; i < numevents; ++i)
        {
            FileEvent* e = &eventLoop->fileEvents[eventLoop->readyFileEvents[i].fd];
            int mask = eventLoop->readyFileEvents[i].mask;
            int fd = eventLoop->readyFileEvents[i].fd;
            int exclude = 0;
            //确保一次执行读写事件的某一个
            if(e->mask & mask & FILE_EVENT_READABLE)
            {
                exclude = 1;
                e->readCallBack(eventLoop, fd, e->clientData, mask);
            }
            if(e->mask & mask & FILE_EVENT_WRITEBABLE)
            {
                if(!exclude || e->readCallBack != e->writeCallBack)
                {
                    e->writeCallBack(eventLoop, fd, e->clientData, mask);
                }
            }
            processed++;
        }
    }
    //执行时间事件
    if(eventType & EVENT_TYPE_TIME)
        processed += executeTimeEvents(eventLoop);
    return processed;
}

//在给定的毫秒数内等待,直到fd可读或者可写或异常
int eventLoopWait(int fd, int mask, long long milliseconds)
{
    struct pollfd pfd;
    memset(&pfd, 0, sizeof(pfd));
    pfd.fd = fd;
    if(mask & FILE_EVENT_READABLE)   pfd.events |= POLLIN;
    if(mask & FILE_EVENT_WRITEBABLE) pfd.events |= POLLOUT;

    int retmask = 0;
    int retval = poll(&pfd, 1, (int)milliseconds);
    if(retval == 1)
    {
        if(pfd.events & POLLIN)  retmask |= FILE_EVENT_READABLE;
        if(pfd.events & POLLOUT) retmask |= FILE_EVENT_WRITEBABLE;
        if(pfd.events & POLLERR) retmask |= FILE_EVENT_WRITEBABLE;
        if(pfd.events & POLLHUP) retmask |= FILE_EVENT_WRITEBABLE;
        return retmask;
    }
    return retval;
}

//事件循环主循环
void startEventLoop(EventLoop* eventLoop)
{
    eventLoop->stop = 0;
    while(!eventLoop->stop)
    {
        if(eventLoop->beforeSleepCallBack)
            eventLoop->beforeSleepCallBack(eventLoop);
        executeEvents(eventLoop, EVENT_TYPE_ALL);
    }
}

//调整事件循环的intsize
int resizeSetSize(EventLoop* eventLoop, int setsize)
{
    if(setsize  == eventLoop->setsize) return EVENT_OK;
    if(eventLoop->maxfd >= setsize) return EVENT_ERR;

    apiResize(eventLoop, setsize);
    eventLoop->fileEvents = zrealloc(eventLoop->fileEvents, sizeof(FileEvent) * setsize);
    eventLoop->readyFileEvents = zrealloc(eventLoop->readyFileEvents, sizeof(FileEvent) * setsize);
    eventLoop->setsize = setsize;
    //初始化新添加的FD的mask是NONE
    for(int i = eventLoop->maxfd + 1; i < setsize; ++i)
        eventLoop->fileEvents[i].mask = FILE_EVENT_NONE;
    return EVENT_OK;
}