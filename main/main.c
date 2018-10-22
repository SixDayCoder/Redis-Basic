#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include "redis.h"


/*-------------------------------------------global redis server---------------------------------------------------*/
extern redisServer gServer;

void initServer();

int serverTimer(struct EventLoop* eventLoop, long long id, void* clientData);

void sigtermHandler(int sig);

int main()
{
    //comment about signal
    /*
    * struct sigaction
    * (1) void (*sa_handler)(int)  ->  处理信号的handler
    * (2) sigset_t sa_mask         ->  在execute处理信号的handler时,屏蔽哪些其他的信号,默认当前信号是被屏蔽的
    * (3) int sa_flag              ->  最重要的是SA_SIGINFO,设定了这个标志位就该用下面这个handler
    * (4) void (*sa_sigaction)(int, siginfo_t*, void*) -> 也是处理信号的handler,在传递的信号想携带额外的参数的时候,用这个
    *
    * 设定编号为signo的信号的处理行为是action,并保留之前的action行为到oact中
    * int sigaction(int signo, const struct sigaction* act, struct sigaction* oact);
     */

    //终端连接结束引发SIGHUP,默认行为是关闭进程,这里SIG_IGN忽略
    signal(SIGHUP, SIG_IGN);
    //向关闭的管道写入时,引发SIGPIPE,默认行为是关闭进程,这里SIG_IGN忽略
    signal(SIGPIPE, SIG_IGN);
    //setup signal handler
    struct sigaction action;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;
    action.sa_handler = sigtermHandler;
    sigaction(SIGTERM, &action, NULL);

    initServer();

    startEventLoop(gServer.eventLoop);

    releaseEventLoop(gServer.eventLoop);

    return 0;
}

void sigtermHandler(int sig)
{
    printf("received SIGTREM, start shutdown");
    gServer.shutDownASAP = 1;
}

void initServer()
{
    //0.init server config
    gServer.shutDownASAP = 0;
    gServer.startTimestamp = 0;
    gServer.tcpkeepalive = 0;
    gServer.eventLoop = createEventLoop(128);
    memset(gServer.bindip, 0, sizeof(gServer.bindip));
    gServer.bindipCount = 0;
    gServer.port = 7777;
    gServer.listenBacklog = 5;
    memset(gServer.ipfd, 0, sizeof(gServer.ipfd));
    gServer.ipfdCount = 0;
    memset(gServer.neterrmsg, 0, sizeof(gServer.neterrmsg));

    //1.init server struct
    gServer.currClient = NULL;
    gServer.clients = listCreate();
    gServer.clients_to_close = listCreate();

    //2.listen and add event
    if(gServer.port != 0 && listenToPort(&gServer) == NET_ERR) {
        exit(1);
    }
    if(gServer.ipfdCount == 0) {
        printf("not listen to anywhere, exit\n");
        exit(1);
    }
    //create time event
    long long mills = 100;
    if(createTimeEvent(gServer.eventLoop, mills, serverTimer, NULL, NULL) == NET_ERR){
        printf("add time event fail\n");
        exit(1);
    }
    //create file event
    for(int i = 0 ; i < gServer.ipfdCount; ++i) {
        if(createFileEvent(gServer.eventLoop, gServer.ipfd[i], FILE_EVENT_READABLE, acceptTCPHandler, NULL) == NET_ERR) {
            printf("add file event fail\n");
            exit(1);
        }
    }
    //time
    gServer.startTimestamp = time(NULL);
}

int serverTimer(struct EventLoop* eventLoop, long long id, void* clientData)
{
    REDIS_NOT_USED(eventLoop);
    REDIS_NOT_USED(id);
    REDIS_NOT_USED(clientData);

    //收到SIGTERM信号,开始关闭服务器
    if(gServer.shutDownASAP) {
        shutdownServer(&gServer);
    }
    //clientTimer

    //dbTimer

    // time log
    time_t now = time(NULL);
    struct tm* info = localtime(&now);
    //printf("now : %s\n", asctime(info));
    //返回调用间隔
    return 100;
}