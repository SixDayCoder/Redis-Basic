//
// Created by Administrator on 2018/10/19.
//

#ifndef REDIS_BASIC_REDIS_H
#define REDIS_BASIC_REDIS_H

#include "network.h"
#include "handler.h"
#include "event.h"
#include "sds.h"
#include "list.h"

#define  REDIS_NOT_USED(v) ((void)v)
#define  REDIS_BIND_IP_LENGTH (16)
#define  REDIS_NET_ERR_MSG_LENGTH (256)
#define  REDIS_OK (0)
#define  REDIS_ERR (-1)
#define  REDIS_IO_BUFFER_LEN (16 * 1024)

/* Client flags */
#define REDIS_SLAVE (1<<0)                    /* This client is a slave server */
#define REDIS_MASTER (1<<1)                  /* This client is a master server */
#define REDIS_MONITOR (1<<2)                /* This client is a slave monitor, see MONITOR */
#define REDIS_MULTI (1<<3)                 /* This client is in a MULTI context */
#define REDIS_BLOCKED (1<<4)              /* The client is waiting in a blocking operation */
#define REDIS_DIRTY_CAS (1<<5)           /* Watched keys modified. EXEC will fail. */
#define REDIS_CLOSE_AFTER_REPLY (1<<6)  /* Close after writing entire reply. */
#define REDIS_UNBLOCKED (1<<7)         /* This client was unblocked and is stored in server.unblocked_clients */
#define  REDIS_CLIENT_REPLAY_CHUNCK_SIZE (16 * 1024)


/*------------------------------------------------------decalre redis client------------------------------------------------------------*/
typedef struct redisClient
{
    //客户端状态
    int flags;

    //socket描述符
    int fd;

    //创建客户端时间
    time_t createtime;

    //上次和服务器交互的时间
    time_t lastinteraction;

    //查询缓冲区
    sds querybuf;

    //回复缓冲区
    int replaypos;

    char replaybuf[REDIS_CLIENT_REPLAY_CHUNCK_SIZE];

} redisClient;

redisClient* createClient(int fd);

void releaseClient(redisClient* c);

void processInput(redisClient* c);


/*------------------------------------------------------decalre redis server------------------------------------------------------------*/

typedef struct redisServer
{
    /*--------------------------------------params------------------------------------------------*/
    //关闭服务器标识
    int shutDownASAP;

    //启动服务器时间
    time_t  startTimestamp;

    //是否开启tcp_keepalive
    int tcpkeepalive;

    /*--------------------------------------client------------------------------------------------*/
    //当前客户端
    redisClient* currClient;

    //保存所有客户端状态
    list* clients;

    //保存待关闭的客户端
    list* clients_to_close;

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

int listenToPort(redisServer* server);

void closeListenSockets(redisServer* server);

int shutdownServer(redisServer* server);
#endif //REDIS_BASIC_REDIS_H
