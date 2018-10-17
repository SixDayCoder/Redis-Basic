//
// Created by Administrator on 2018/10/16.
//

#ifndef REDIS_BASIC_EVENT_H
#define REDIS_BASIC_EVENT_H

#include <time.h>

#define NET_OK (0)

#define NET_ERR (-1)

//文件事件的状态字
#define FILE_EVENT_NONE (0)

#define FILE_EVENT_READABLE (1)

#define FILE_EVENT_WRITEBABLE (2)

//时间事件是否需要再次执行
#define  TIME_EVENT_NO_MORE (-1)

//事件的flag

#define EVENT_TYPE_NONE (0)

#define EVENT_TYPE_FILE (1)

#define EVENT_TYPE_TIME (2)

#define EVENT_TYPE_ALL  ( EVENT_TYPE_FILE | EVENT_TYPE_TIME )

#define EVENT_TYPE_DONT_WAIT (4)

struct EventLoop;
typedef void FileEventCallBack(struct EventLoop* eventLoop, int fd, void* clientData, int mask);
typedef int  TimeEventCallBack(struct EventLoop* eventLoop, long long id, void* clientData);
typedef void EventFinalizerCallBack(struct EventLoop* eventLoop, void* clientData);
typedef void BeforeSleepCallBack(struct EventLoop* eventLoop);

//文件事件
typedef struct FileEvent
{
    //type掩码
    int mask;

    //读事件回调
    FileEventCallBack* readCallBack;

    //写事件回调
    FileEventCallBack* writeCallBack;

    //私有数据,客户端连接时可能会用到
    void* clientData;
}FileEvent;

//已就绪文件事件
typedef struct ReadyFileEvent
{
    //就绪文件描述符
    int fd;

    //就绪事件掩码
    int mask;
}ReadyFileEvent;

//时间事件
typedef struct TimeEvent
{
    //时间事件的唯一标志
    long long id;

    //事件的到达时间
    long long whenSec; //seconds
    long long whenMs;  //millseconds;

    //事件处理回调
    TimeEventCallBack* timeCallBack;

    //事件的析构函数
    EventFinalizerCallBack* finalizerCallBack;

    //私有数据,客户端连接时可能会用到
    void* clientData;

    //指向下一个时间事件
    struct TimeEvent* next;
}TimeEvent;

//事件循环结构
typedef struct EventLoop
{
    //已注册的最大描述符
    int maxfd;

    //已监视的fd集合的大小
    int setsize;

    //生成时间事件的id
    long long timeEventNextId;

    //上次指向时间事件的timestamp
    time_t lastDoTimeEvent;

    //已注册的文件事件
    FileEvent* fileEvents;

    //已注册的就绪文件事件
    ReadyFileEvent* readyFileEvents;

    //已注册的时间事件的头结点
    TimeEvent* timeEventHead;

    //事件循环的开关
    int stop;

    //用于区分select,poll,epoll,kqueue等api的数据
    void* apidata;

    //在真正的事件循环execute之前要做的操作
    BeforeSleepCallBack* beforeSleepCallBack;

}EventLoop;


//创建事件循环结构
EventLoop* createEventLoop(int setsize);

//回收事件循环结构
void releaseEventLoop(EventLoop* eventLoop);

//停止事件循环
void stopEventLoop(EventLoop* eventLoop);

//根据mask的值来监听fd的状态,fd可用时执行callback
int createFileEvent(EventLoop* eventLoop, int fd, int mask, FileEventCallBack* fileEventCallBack, void* clientData);

//将fd从mask指定的监听队列中删除(mask指示了哪种类型)
void deleteFileEvent(EventLoop *eventLoop, int fd, int mask);

//获取监听fd事件的类型掩码
int getFileEventsMask(EventLoop* eventLoop, int fd);

//创建时间事件,返回其id
long long createTimeEvent(EventLoop* eventLoop, long long millseconds, TimeEventCallBack* timeEventCallBack, void* clientData, EventFinalizerCallBack* finalizerCallBack);

//删除时间事件
int deleteTimeEvent(EventLoop *eventLoop, long long id);

//处理所有到达的时间事件和已就绪的文件事件
//eventType = EVENT_TYPE_NONE, 函数不作动作直接返回
//eventType include EVENT_TYPE_ALL,  处理所有类型的事件
//eventType include EVENT_TYPE_FILE, 处理就绪的文件事件
//eventType include EVENT_TYPE_TIME, 处理所有的时间事件
//eventType include EVENT_TYPE_DONT_WAIT, 处理完所有非阻塞事件后立即返回
//返回处理过的事件的数目
int executeEvents(EventLoop *eventLoop, int eventType);

//在给定的毫秒数内等待,直到fd可读或者可写或异常
int eventLoopWait(int fd, int mask, long long milliseconds);

//事件循环主循环
void startEventLoop(EventLoop* eventLoop);

//调整事件循环的intsize
int resizeSetSize(EventLoop* eventLoop, int setsize);
#endif //REDIS_BASIC_EVENT_H
