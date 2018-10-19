//
// Created by Administrator on 2018/10/19.
//

#ifndef REDIS_BASIC_REDIS_H
#define REDIS_BASIC_REDIS_H

#include "network.h"
#include "handler.h"
#include "event.h"

#define  REDIS_NOT_USED(v) ((void)v)
#define  REDIS_BIND_IP_LENGTH (16)
#define  REDIS_NET_ERR_MSG_LENGTH (256)
#define  REDIS_OK (0)
#define  REDIS_ERR (-1)

typedef struct redisServer
{
    /*--------------------------------------params------------------------------------------------*/
    //关闭服务器标识
    int shutDownASAP;

    //启动服务器时间
    time_t  startTimestamp;

    /*--------------------------------------netwrok------------------------------------------------*/
    //事件循环
    EventLoop* eventLoop;

    //绑定的ip地址
    char* bindip[REDIS_BIND_IP_LENGTH];

    //绑定的ip地址个数
    int  bindipCount;

    //监听的端口
    int port;

    //监听的backlog
    int listenBacklog;

    //server的fd
    int ipfd[REDIS_BIND_IP_LENGTH];

    //server的fd的count
    int ipfdCount;

    //错误信息
    char neterrmsg[REDIS_NET_ERR_MSG_LENGTH];


} redisServer;

redisServer gServer;

int listenToPort(redisServer* server);

void closeListenSockets(redisServer* server);

int shutdownServer(redisServer* server);
#endif //REDIS_BASIC_REDIS_H
