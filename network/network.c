#include "network.h"
#include <unistd.h>
#include <sys/socket.h>
#include <fcntl.h>

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
//#include <winsock2.h>
//#include <ws2tcpip.h>


/*
 * struct addrinfo
 *
 * 1.ai_flags : 指示如何处理地址和名字
 * AI_NUMERICHOST -> 以数字方式返回主机地址
 * AI_PASSIVE     -> 用于bind,一般用于server socket
 * AI_CANONNAME   -> 返回主机的规范名称
 * SO On
 *
 * 2.ai_family : 地址家族
 *   AF_INET
 *   AF_INET6
 *   AF_UNIX : unix域
 *   AF_UNSPEC : 未指定,也称协议无关(IPV4 and IPV6)
 *
 * 3.ai_socktype
 * SOCK_DGRAM  : UDP
 * SOCK_STREAM : TCP
 *
 * 4.ai_protocol
 * IPPROTO_IPV4
 * IPPROTO_IPV6
 * IPPROTO_TCP
 * IPPROTO_UDP
 * 建议的使用方式 : 仅使用flag, family, socktype, protocol, 其他域memset为0
 *
 * //支持ipv4和ipv6,获取主机的地址信息,存放在result中
 * getaddrinfo(const char* node, const char* servername, const struct addrinfo* hints, struct addrinfo** result)
 * 1.node : 主机名(www.baidu.com)或者是点分十进制的ip(127.0.0.1)地址, 如果hints->ai_flag = AI_NUMERICHOST, 那么node只能是点分十进制的地址
 * 2.servername : 可以是十进制的端口号port(8888), 也可以是"ftp","http"这种固定端口号的服务,可以设置成NULL
 * 3.hints  : 用户设定,只能设定flag, family, socktype, protocol, 其他域memset为0
 * 4.result : 解析之后的结果信息,必须用freeaddrinfo()来释放掉
 *
 * getaddrinfo()执行失败的retval可以使用gai_strerror(retval)来获取错误信息
 */

static void netSetError(char *errmsg, const char *fmt, ...)
{
    if(!errmsg) return;
    va_list  ap;
    va_start(ap, fmt);
    vsnprintf(errmsg, NET_ERROR_LEN, fmt, ap);
    va_end(ap);
    printf("%s\n", errmsg);
}


//设置地址为可重用

/*  关于地址重用
 *  作用 :
 *  端口复用防止服务器重启时之前绑定的端口还未释放或者程序突然退出而系统没有释放端口。
 *  这种情况下如果设定了端口复用，则新启动的服务器进程可以直接绑定端口
 *
 *  使用方式 :
 *  必须在(每次!)bind之前设置地址复用:
 *  fd = socket(xx, xx, xx);
 *  int on = 1;
 *  setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on))
 *  bind(fd)
 *  listen(fd)
 */

static int netSetAddrReuseable(int fd, char *errmsg)
{
    int yes = 1;
    if(setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1) {
        netSetError(errmsg, "setsocketopt SO_REUSEADDR: %s", strerror(errno));
        return NET_ERR;
    }
    return NET_OK;
}

static int netCommonAccept(int sock, struct sockaddr *sa, socklen_t *len, char *errmsg)
{
    int fd = 0;
    while(1)
    {
        fd = accept(sock, sa, len);
        if(fd == -1){
            //TODO:explain
            if(errno == EINTR)
                continue;
            else{
                netSetError(errmsg, "accept : %s", strerror(errno));
                return NET_ERR;
            }
        }
        break;
    }
    return fd;
}

static int netBindAndListen(int sock, struct sockaddr *sa, socklen_t len, int backlog, char *errmsg)
{
    if(bind(sock, sa, len) == -1)
    {
        netSetError(errmsg, "bind : %s", strerror(errno));
        close(sock);
        return NET_ERR;
    }
    if(listen(sock, backlog) == -1)
    {
        netSetError(errmsg, "listen : %s", strerror(errno));
        close(sock);
        return NET_ERR;
    }
    return NET_OK;
}

static int netCommonTCPServer(int port, char *bindaddr, int backlog, int af, char *errmsg)
{
    char portbuf[6]; // strlen(65536)
    snprintf(portbuf, sizeof(portbuf), "%d", port);

    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = af;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;//用于bind的socket

    struct addrinfo *servinfo;
    int retval = 0;
    if( (retval = getaddrinfo(bindaddr, portbuf, &hints, &servinfo)) != 0)
    {
        netSetError(errmsg, "%s", gai_strerror(retval));
        freeaddrinfo(servinfo);
        return NET_ERR;
    }
    struct addrinfo* p;
    int sock = 0;
    for(p = servinfo; p != NULL; p = p->ai_next)
    {
        if((sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            continue;
        }
        if(netSetAddrReuseable(sock, errmsg) == NET_ERR) {
            freeaddrinfo(servinfo);
            return NET_ERR;
        }
        if(netBindAndListen(sock, p->ai_addr, p->ai_addrlen, backlog, errmsg) == NET_ERR) {
            freeaddrinfo(servinfo);
            return NET_ERR;
        }
        //success
        freeaddrinfo(servinfo);
        return sock;
    }
    if(p == NULL)
    {
        netSetError(errmsg, "unable to bind socket");
        freeaddrinfo(servinfo);
        return NET_ERR;
    }
}


#define NET_CONNECT_NONE (0)
#define NET_CONNECT_NONBLOCK (1)
#define TCP_ERROR_HANDLE(fd, servinfo) do {\
    if(fd != NET_ERR){\
        close(fd);\
        fd = NET_ERR;\
    }\
    freeaddrinfo(servinfo);\
}while(0);
static int netCommonTCPConnect(char* addr, int port, char* srcaddr, int flags, char* errmsg)
{
    char portbuf[6]; //strlen("65536")
    snprintf(portbuf, sizeof(portbuf), "%d", port);

    struct addrinfo hints;
    struct addrinfo* servinfo;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    int retval = 0;
    if((retval = getaddrinfo(addr, portbuf, &hints, &servinfo)) != 0)
    {
        netSetError(errmsg, "%s", gai_strerror(retval));
        return NET_ERR;
    }

    struct addrinfo* p;
    int sock = -1;
    for(p = servinfo; p != NULL; p = p->ai_next)
    {
        if((sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
            continue;
        if(netSetAddrReuseable(sock, errmsg) == NET_ERR){
            TCP_ERROR_HANDLE(sock, servinfo);
            return NET_ERR;
        }
        if(flags & NET_CONNECT_NONBLOCK && netSetNonBlock(sock, errmsg) != NET_OK){
            TCP_ERROR_HANDLE(sock, servinfo);
            return NET_ERR;
        }
        if(srcaddr)
        {
            int bound = 0;
            struct addrinfo* bservinfo;
            if((retval = getaddrinfo(srcaddr, NULL, &hints, &bservinfo)) != 0)
            {
                netSetError(errmsg, "%s", gai_strerror(retval));
                freeaddrinfo(servinfo);
                return NET_ERR;
            }
            struct addrinfo* b;
            for(b = bservinfo; b != NULL; b = b->ai_next)
            {
                if(bind(sock, b->ai_addr, b->ai_addrlen) != -1){
                    bound = 1;
                    break;
                }
            }
            if(!bound){
                netSetError(errmsg, "bind : %s", gai_strerror(retval));
                freeaddrinfo(servinfo);
                return NET_ERR;
            }
        }
        if(connect(sock, p->ai_addr, p->ai_addrlen) == -1){
            /* If the socket is non-blocking, it is ok for connect() to
             * return an EINPROGRESS error here. */
            if(errno == EINPROGRESS && (flags & NET_CONNECT_NONBLOCK)){
                freeaddrinfo(servinfo);
                return sock;
            }
            else{
                close(sock);
                sock = NET_ERR;
                continue;
            }
        }
        /* If we ended an iteration of the for loop without errors, we
        * have a connected socket. Let's return to the caller. */
        freeaddrinfo(servinfo);
        return sock;
    }
    if(p == NULL){
        netSetError(errmsg, "create socket : %s", strerror(errno));
        freeaddrinfo(servinfo);
        return NET_ERR;
    }
}

//解析host地址保存到ipbuf中
static int netParseHostAddress(char* host, char* ipbuf, uint32_t ipbuflen, int flags, char* errmsg) {
    struct addrinfo hints;
    struct addrinfo *info;
    memset(&hints, 0, sizeof(hints));

    if (flags > 0) hints.ai_flags = AI_NUMERICHOST;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    int retval = 0;
    if ((retval = getaddrinfo(host, NULL, &hints, &info)) != 0) {
        netSetError(errmsg, "%s", gai_strerror(retval));
        return NET_ERR;
    }
    if (info->ai_family == AF_INET) {
        struct sockaddr_in *sa = (struct sockaddr_in *) info->ai_addr;
        inet_ntop(AF_INET, &(sa->sin_addr), ipbuf, ipbuflen);
    }
    else
    {
        struct sockaddr_in6 *sa = (struct sockaddr_in6 *)info->ai_addr;
        inet_ntop(AF_INET6, &(sa->sin6_addr), ipbuf, ipbuflen);
    }

    freeaddrinfo(info);
    return NET_OK;
}

int netTCPServer(int port, char* bindaddr, int backlog, char* errmsg)
{
    return netCommonTCPServer(port, bindaddr, backlog, AF_INET, errmsg);
}

int netTCP6Server(int port, char *bindaddr, int backlog, char *errmsg)
{
    return netCommonTCPServer(port, bindaddr, backlog, AF_INET6, errmsg);
}

int netTCPAccept(int serversock, char *ip, uint32_t iplen, int *port, char *errmsg)
{
    //inet_ntop : 点分十进制和整数的转换
    int fd;
    struct sockaddr_storage sa;
    socklen_t salen = sizeof(sa);
    if((fd = netCommonAccept(serversock, (struct sockaddr *) &sa, &salen, errmsg)) == -1)
        return NET_ERR;

    //ipv4
    if(sa.ss_family == AF_INET)
    {
        struct sockaddr_in* s = (struct sockaddr_in*)&sa;
        if(ip)    inet_ntop(AF_INET, (void*)&(s->sin_addr), ip, iplen);
        if(port) *port = ntohs(s->sin_port);
    }
    //ipv6
    else
    {
        struct sockaddr_in6 *s = (struct sockaddr_in6 *)&sa;
        if (ip) inet_ntop(AF_INET6, (void*)&(s->sin6_addr),ip, iplen);
        if (port) *port = ntohs(s->sin6_port);
    }
    return fd;
}

int netRead(int fd, char* buf, int count)
{
    int nread = 0;
    int totlen = 0;
    while(totlen != count)
    {
        nread = read(fd, buf, count - totlen);
        if(nread == 0) return  totlen;
        if(nread == -1) return NET_ERR;
        totlen += nread;
        buf += nread;
    }
    return totlen;
}

int netWrite(int fd, char* buf, int count)
{
    int nwriteten = 0;
    int totlen = 0;
    while(totlen != count)
    {
        nwriteten = write(fd, buf, count - totlen);
        if(nwriteten == 0) return totlen;
        if(nwriteten == -1) return NET_ERR;
        totlen += nwriteten;
        buf += nwriteten;
    }
    return totlen;
}

//设置fd为非阻塞
int netSetNonBlock(int fd, char* errmsg)
{
    int flags = fcntl(fd, F_GETFL);
    if (flags == -1) {
        netSetError(errmsg, "fcntl(F_GETFL): %s", strerror(errno));
        return NET_ERR;
    }
    //这一步不可被信号中断
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        netSetError(errmsg, "fcntl(F_SETFL,O_NONBLOCK): %s", strerror(errno));
        return NET_ERR;
    }
    return NET_OK;
}

//获取服务器本机的IP和端口号
int netGetServerSockName(int fd, char* ip, uint32_t iplen, int *port)
{
    struct sockaddr_storage sa;
    socklen_t salen = sizeof(sa);

    if(getsockname(fd, (struct sockaddr*)&sa, &salen) == -1)
    {
        if(port) *port = 0;
        ip[0] = '?';
        ip[1] = '\0';
        return NET_ERR;
    }
    if(sa.ss_family == AF_INET)
    {
        struct sockaddr_in* s = (struct sockaddr_in*)&sa;
        if(ip)    inet_ntop(AF_INET, (void*)&(s->sin_addr), ip, iplen);
        if(port) *port = ntohs(s->sin_port);
    }
    else
    {
        struct sockaddr_in6 *s = (struct sockaddr_in6 *)&sa;
        if (ip) inet_ntop(AF_INET6, (void*)&(s->sin6_addr),ip, iplen);
        if (port) *port = ntohs(s->sin6_port);
    }
    return NET_OK;
}

//获取链接的客户端的IP和端口号
int netGetPeerName(int fd, char* ip, uint32_t iplen, int* port)
{
    struct sockaddr_storage sa;
    socklen_t salen = sizeof(sa);

    if(getpeername(fd, (struct sockaddr*)&sa, &salen) == -1)
    {
        if(port) *port = 0;
        ip[0] = '?';
        ip[1] = '\0';
        return NET_ERR;
    }
    if(sa.ss_family == AF_INET)
    {
        struct sockaddr_in* s = (struct sockaddr_in*)&sa;
        if(ip)    inet_ntop(AF_INET, (void*)&(s->sin_addr), ip, iplen);
        if(port) *port = ntohs(s->sin_port);
    }
    else
    {
        struct sockaddr_in6 *s = (struct sockaddr_in6 *)&sa;
        if (ip) inet_ntop(AF_INET6, (void*)&(s->sin6_addr),ip, iplen);
        if (port) *port = ntohs(s->sin6_port);
    }

    return NET_OK;
}
//创建非阻塞TCP链接
int netTCPNonBlockConnect(char* addr, int port, char* errmsg)
{
    return netCommonTCPConnect(addr, port, NULL, NET_CONNECT_NONBLOCK, errmsg);
}



