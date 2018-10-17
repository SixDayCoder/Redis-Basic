//
// Created by Administrator on 2018/10/17.
//
#include "event.h"
#include "memory.h"
#include <sys/select.h>

//trfds是rfds的copy, twfds是wfds的copy, t取temp
//在select后使用rfds或者wfds可能不安全
typedef struct APIData {
    fd_set rfds, wfds;
    /* We need to have a copy of the fd sets as it's not safe to reuse
     * FD sets after select(). */
    fd_set trfds, twfds;
} APIData;

static int createAPIData(EventLoop *eventLoop) {
    APIData *data = zmalloc(sizeof(EventLoop));
    if (!data) return -1;
    FD_ZERO(&data->rfds);
    FD_ZERO(&data->wfds);
    eventLoop->apidata = data;
    return 0;
}

static void releaseAPIData(EventLoop* eventLoop)
{
    zfree(eventLoop->apidata);
}

static int addEvent(EventLoop* eventLoop, int fd, int mask)
{
    APIData* data = eventLoop->apidata;
    if(mask & FILE_EVENT_READABLE)   FD_SET(fd, &data->rfds);
    if(mask & FILE_EVENT_WRITEBABLE) FD_SET(fd, &data->wfds);
    return 0;
}

static void delEvent(EventLoop* eventLoop, int fd, int mask)
{
    APIData* data = eventLoop->apidata;
    if(mask & FILE_EVENT_READABLE)   FD_CLR(fd, &data->rfds);
    if(mask & FILE_EVENT_WRITEBABLE) FD_CLR(fd, &data->wfds);
}

static int apiPoll(EventLoop* eventLoop, struct timeval* tvp)
{
    APIData* data = eventLoop->apidata;
    //copy备份
    memcpy(&data->trfds, &data->rfds, sizeof(fd_set));
    memcpy(&data->twfds, &data->wfds, sizeof(fd_set));
    int readyCnt = 0;
    int ret = select(eventLoop->maxfd + 1, &data->trfds, &data->twfds, NULL, tvp);
    if(ret > 0)
    {
        for(int i = 0 ; i < eventLoop->maxfd; ++i)
        {
            int mask = 0;
            FileEvent* e = &eventLoop->fileEvents[i];
            if(e->mask == FILE_EVENT_NONE) continue;
            if(e->mask & FILE_EVENT_READABLE && FD_ISSET(i, &data->trfds))
                mask |= FILE_EVENT_READABLE;
            if(e->mask & FILE_EVENT_WRITEBABLE && FD_ISSET(i, &data->twfds))
                mask |= FILE_EVENT_WRITEBABLE;
            //加入就绪队列
            eventLoop->readyFileEvents[readyCnt].fd = i;
            eventLoop->readyFileEvents[readyCnt].mask = mask;
            readyCnt++;
        }
    }
    return readyCnt;
}

static int apiResize(EventLoop* eventLoop, int setsize)
{
    if(setsize >= FD_SETSIZE) return -1;
    return 0;
}

static char* apiName(void)
{
    return "select";
}