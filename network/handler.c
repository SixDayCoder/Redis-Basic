//
// Created by Administrator on 2018/10/18.
//

#include "handler.h"
#include "redis.h"
#include "list.h"

#include <errno.h>
#include <stdio.h>
#include <unistd.h>

extern redisServer gServer;

static void acceptClientHandler(int fd, int clientstate)
{
    redisClient* c = createClient(fd);
    if(c == NULL) {
        printf("register fd event for the new client:%s (fd = %d)\n", strerror(errno), fd);
        close(fd);
        return;
    }
    c->flags |= clientstate;
    printf("welcome new client!fd is %d\n", fd);
}

void acceptTCPHandler(EventLoop* eventLoop, int serverfd, void* privadata, int mask)
{
    int max = MAX_ACCEPT_PER_CALL;
    char clientIP[IP_STR_LEN];
    char errmsg[256];
    int clientfd = -1;
    int clientport = 0;
    while(max--)
    {
        clientfd = netTCPAccept(serverfd, clientIP, IP_STR_LEN, &clientport, errmsg);
        if(clientfd == NET_ERR){
            if(errno != EWOULDBLOCK){
                printf("%s\n", errmsg);
            }
            return;
        }
        acceptClientHandler(clientfd, 0);
    }
}

void recvFromClient(EventLoop* eventLoop, int fd, void* privadata, int mask)
{
    REDIS_NOT_USED(eventLoop);
    REDIS_NOT_USED(mask);

    redisClient* c = (redisClient*)privadata;
    int nread = 0;
    size_t readsize = REDIS_IO_BUFFER_LEN;

    //设置当前客户端
    gServer.currClient = c;

    //缓冲分配足够的内存
    size_t qblen = sdslen(c->querybuf);
    c->querybuf = sdsMakeRoomFor(c->querybuf, readsize);

    //读取
    nread = read(fd, c->querybuf + qblen, readsize);
    //读取出错
    if(nread == -1) {
        if(errno == EAGAIN) {
            nread = 0;
        } else {
            printf("reading from client : %s\n", strerror(errno));
            releaseClient(c);
            return;
        }
        //遇到EOF
    } else if(nread == 0) {
        printf("client close connection\n");
        releaseClient(c);
        return;
    }
    //读取成功
    if(nread) {
        sdsIncrLen(c->querybuf, nread);
        c->lastinteraction = time(NULL);
        processInput(c);
    } else {
        //nread == -1 and errnor = EAGAIN
        gServer.currClient = NULL;
        return;
    }
    gServer.currClient = NULL;
}

void sendToClient(EventLoop* eventLoop, int fd, void* privadata, int mask)
{
    REDIS_NOT_USED(eventLoop);
    REDIS_NOT_USED(mask);

    redisClient* c = (redisClient*)privadata;
    //number of written
    int nwritten = 0;

    //total writen
    int totwritten = 0;

    //TODO:replace test
    sds reply = NULL;
    size_t replysize = 0;
    while(LIST_LENGTH(c->reply)) {
        reply = LIST_NODE_VAL(LIST_HEAD(c->reply));
        replysize = sdslen(reply);
        //无效的reply直接continue
        if(replysize == 0) {
            listDelNode(c->reply, LIST_HEAD(c->reply));
            continue;
        }
        //写入
        nwritten = write(fd, (char*)reply + c->sendlen, replysize - c->sendlen);
        //出错检查放到while后
        if(nwritten <= 0) break;
        c->sendlen += nwritten;
        totwritten += nwritten;
        //写完,删除
        if(c->sendlen == replysize) {
            listDelNode(c->reply, LIST_HEAD(c->reply));
            c->sendlen = 0;
        }
    }
    //出错检查
    if(nwritten == -1) {
        if(errno == EAGAIN) {
            nwritten = 0;
        }
        else {
            printf("send to client reply error : %s", strerror(errno));
            releaseClient(c);
            return;
        }
    }
    //如果reply全部写入到client,删除写事件
    if(LIST_LENGTH(c->reply) == 0) {
        c->sendlen = 0;
        deleteFileEvent(gServer.eventLoop, c->fd, FILE_EVENT_WRITEBABLE);
    }
}