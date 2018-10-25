//
// Created by Administrator on 2018/10/24.
//

#include "redis.h"
#include "handler.h"
#include <unistd.h>

extern redisServer gServer;

redisClient* createClient(int fd)
{
    if(fd == -1) {
        return NULL;
    }
    redisClient* cli = zmalloc(sizeof(*cli));
    char errmsg[REDIS_NET_ERR_MSG_LENGTH];

    //非阻塞
    netSetNonBlock(fd, NULL);
    //禁用nagle
    netSetTCPNoDelay(fd, 0, errmsg);
    //设置keepalive
    if(gServer.tcpkeepalive) {
        netKeepAlive(fd, gServer.tcpkeepalive, errmsg);
    }
    //recv from client
    if(createFileEvent(gServer.eventLoop, fd, FILE_EVENT_READABLE, recvFromClient, cli) == NET_ERR) {
        close(fd);
        zfree(cli);
        return NULL;
    }

    //init all information
    selectDB(cli, 0);

    cli->fd = fd;

    cli->querybuf = sdsempty();

    cli->createtime = time(NULL);

    cli->lastinteraction = 0;

    cli->replypos = 0;

    cli->reply = listCreate();

    cli->sendlen = 0;

    listPushTail(gServer.clients, cli);
}

void releaseClient(redisClient* c)
{
    if(gServer.currClient == c) {
        gServer.currClient = NULL;
    }

    sdsfree(c->querybuf);
    c->querybuf = NULL;

    listRealease(c->reply);
    c->reply = NULL;

    if(c->fd != -1) {
        deleteFileEvent(gServer.eventLoop, c->fd, FILE_EVENT_READABLE);
        deleteFileEvent(gServer.eventLoop, c->fd, FILE_EVENT_WRITEBABLE);
        close(c->fd);
        listNode* node = listSearchKey(gServer.clients, c);
        if(node != NULL) {
            listDelNode(gServer.clients, node);
        }
    }

    //last free
    zfree(c);
}

void processInput(redisClient* c)
{
    //TODO:添加一个echo回复给client
    char recv[256] = { 0 };
    memcpy(recv, c->querybuf, sdslen(c->querybuf));
    recv[sdslen(c->querybuf)] = '\0';
    printf("i recv : %s\n", recv);

    time_t now = time(NULL);
    struct tm* info = localtime(&now);
    sds r = sdsnew(asctime(info));
    r = sdscat(r, recv);
    listPushHead(c->reply, r);

    //添加文件写事件,下一次时间循环触发
    if( createFileEvent(gServer.eventLoop, c->fd, FILE_EVENT_WRITEBABLE, sendToClient, c) == -1) {
        printf("create write file event error\n");
        return;
    }
    //最后清除querybuf的内容
    sdsclear(c->querybuf);
}

int selectDB(redisClient* c, int id)
{
    if(id < 0 || id >= gServer.dbnum) {
        return REDIS_ERR;
    }

}