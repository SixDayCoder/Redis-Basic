//
// Created by Administrator on 2018/10/18.
//

#include "handler.h"
#include "redis.h"
#include <errno.h>
#include <stdio.h>
#include <unistd.h>

extern redisServer gServer;

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
    } else {
        //nread == -1 and errnor = EAGAIN
        gServer.currClient = NULL;
        return;
    }
    //处理input
    //processinputbuffer(client)
    gServer.currClient = NULL;
}

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
    int clientfd;
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