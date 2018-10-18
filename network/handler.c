//
// Created by Administrator on 2018/10/18.
//

#include "handler.h"
#include <errno.h>
#include <stdio.h>

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
                return;
            }
        }
    }
}