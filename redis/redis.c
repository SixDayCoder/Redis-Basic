//
// Created by Administrator on 2018/10/19.
//
#include "redis.h"
#include "handler.h"

#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>


//global
redisServer gServer;

/*------------------------------------------------------impl redis client------------------------------------------------------------*/

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
    cli->fd = fd;

    cli->querybuf = sdsempty();

    cli->createtime = time(NULL);

    cli->lastinteraction = 0;

    cli->replaypos = 0;


    listPushTail(gServer.clients, cli);
}

void releaseClient(redisClient* c)
{
    if(gServer.currClient == c) {
        gServer.currClient = NULL;
    }

    sdsfree(c->querybuf);
    c->querybuf = NULL;

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
    if(sdslen(c->querybuf)) {
        printf("input is %s\n", c->querybuf);
    }
}

/*------------------------------------------------------impl redis server------------------------------------------------------------*/

int listenToPort(redisServer* server)
{
    //默认强制绑定本机ip
    if(server->bindipCount == 0) {
        server->bindip[0] = NULL;
    }

    int i = 0;
    int count = 0;
    for(i = 0; i < server->bindipCount || i == 0; i++) {
        //do create tcp server
        if(server->bindip[i] == NULL) {
            server->ipfd[count] = netTCPServer(server->port, NULL, server->listenBacklog, server->neterrmsg);
            if(server->ipfd[count] != NET_ERR) {
                netSetNonBlock(server->ipfd[count], NULL);
                count++;
            }
//            server->ipfd[count] = netTCP6Server(server->port, NULL, server->listenBacklog, server->neterrmsg);
//            if(server->ipfd[count] != NET_ERR) {
//                netSetNonBlock(server->ipfd[count], NULL);
//                count++;
//            }
            //本地ipv4和ipv6都绑定成功,break掉
            if(count > 0) break;
        }
//        //ipv6
//        else if(strchr(server->bindip[i], ':')) {
//            server->ipfd[count] = netTCP6Server(server->port, NULL, server->listenBacklog, server->neterrmsg);
//        }
        //ipv4
        else {
            server->ipfd[count] = netTCPServer(server->port, NULL, server->listenBacklog, server->neterrmsg);
        }
        //create tcp server error
        if(server->ipfd[count] == NET_ERR) {
            printf("Create TCP Server Error %s:%d:%s\n", server->bindip[i] ? server->bindip[i] : "*", server->port, server->neterrmsg);
            return NET_ERR;
        }
        //success
        netSetNonBlock(server->ipfd[count], NULL);
        count++;
    }
    server->ipfdCount = count;
    return NET_OK;
}

void closeListenSockets(redisServer* server)
{
    for(int i = 0 ;i < server->ipfdCount; ++i) {
        close(server->ipfd[i]);
    }
    printf("close adll listen sockets\n");
}

int shutdownServer(redisServer* server)
{
    if(server->shutDownASAP == 0) return REDIS_ERR;
    closeListenSockets(server);
    return REDIS_OK;
}

