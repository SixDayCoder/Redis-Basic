//
// Created by Administrator on 2018/10/18.
//

#ifndef REDIS_BASIC_NETWORK_H
#define REDIS_BASIC_NETWORK_H

#include <stdint.h>

#define NET_OK  ( 0)
#define NET_ERR (-1)
#define NET_ERROR_LEN (256)


int netTCPServer(int port, char* bindaddr, int backlog, char* errmsg);

int netTCP6Server(int port, char *bindaddr, int backlog, char *errmsg);

int netTCPAccept(int serversock, char *ip, uint32_t iplen, int *port, char *errmsg);

//创建非阻塞TCP链接
int netTCPNonBlockConnect(char* addr, int port, char* errmsg);

//设置fd为非阻塞
int netSetNonBlock(int fd, char* errmsg);

//fd读取count个字节放入buf
int netRead(int fd, char* buf, int count);

//buf取出count字节写入fd
int netWrite(int fd, char* buf, int count);

//获取服务器本机的IP和端口号
int netGetServerSockName(int fd, char* ip, uint32_t iplen, int *port);

//获取链接的客户端的IP和端口号
int netGetPeerName(int fd, char* ip, uint32_t iplen, int* port);

//打开或关闭Nagle
int netSetTCPNoDelay(int fd, int open, char* errmsg);

//修改keepalve选项
int netKeepAlive(int fd, int interval, char* errmsg);

#endif //REDIS_BASIC_NETWORK_H
