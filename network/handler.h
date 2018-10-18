//
// Created by Administrator on 2018/10/18.
//

#ifndef REDIS_BASIC_HANDLER_H
#define REDIS_BASIC_HANDLER_H

#include "network.h"
#include "event.h"

#define MAX_ACCEPT_PER_CALL (128)
#define IP_STR_LEN   (16)
#define PORT_STR_LEN (32)
#define PEER_STR_LEN (IP_STR_LEN + PORT_STR_LEN)

void acceptTCPHandler(EventLoop* eventLoop, int fd, void* privadata, int mask);

#endif //REDIS_BASIC_HANDLER_H
