/*
 * 这是一个单线程的可以接收多个客户端连接并接收数据的程序
 */

#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <netinet/in.h>

#define PORT    3333
#define BACKLOG 128

int main() {
    int server_sockfd;
    int accept_sockfd;
    struct sockaddr_in server_addr;
    struct sockaddr_in accept_addr;

    if ((server_sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
        printf("create socket failed\r\n");
        return 0;
    }

    server_addr.sin_family      = AF_INET;
    server_addr.sin_port        = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_sockfd, (struct sockaddr*)&server_addr, sizeof(struct sockaddr)) < 0) {
        printf("bind error\r\n");
        close(server_sockfd);
        return 0;
    }

    if ((listen(server_sockfd, SOMAXCONN)) < 0) {
        printf("listen error\r\n");
        close(server_sockfd);
        return 0;
    }

    fd_set fd_read;
    int    fd_max = server_sockfd;
    int    client_sockfds[BACKLOG];
    int    client_sockfds_count = 0;
    struct timeval timeout;

    int  i = 0;
    int  ret;
    int  sin_size = sizeof(struct sockaddr);
    char buff[1024];
    for (;;) {
        timeout.tv_sec  = 5;
        timeout.tv_usec = 0;

        // FD_ZERO(fd_set *fdset)：清空fdset与所有文件句柄的联系。
        // FD_SET(int fd, fd_set *fdset)：建立文件句柄fd与fdset的联系。
        // FD_CLR(int fd, fd_set *fdset)：清除文件句柄fd与fdset的联系。
        // FD_ISSET(int fd, fdset *fdset)：检查fdset联系的文件句柄fd是否
        FD_ZERO(&fd_read);
        FD_SET(server_sockfd, &fd_read);
        for (i = 0; i < client_sockfds_count; i++) {
            FD_SET(client_sockfds[i], &fd_read);
        }

        // select参数:
        // 第1个参数一定要注意！！！！！标识的是所有fd中最大的那个+1，不是所有fd的总数量+1(Windows下这个参数没意义)
        // 第2个参数是检测可读性的fd集合
        // 第3个参数是检测可写性的fd集合
        // 第4个参数是检测异常性的fd集合
        // 第5个参数是超时设置.一定要注意select函数会修改timeval结构的值，所以每次循环都要为timeval结构体重新赋值
        ret = select(fd_max+1, &fd_read, NULL, NULL, &timeout);
        if (ret < 0) {
            printf("select error\r\n");
            continue;
        }
        else if (ret == 0) {
            printf("select time out\r\n");
            continue;
        }

        for (i = 0; i < client_sockfds_count; i++) {
            if (FD_ISSET(client_sockfds[i], &fd_read)) {
                ret = recv(client_sockfds[i], buff, 1023, 0);
                if (ret > 0) {
                    buff[ret] = 0;
                    printf("recv: %s\r\n", buff);
                }
                else {
                    printf("recv error\r\n");
                }
                FD_CLR(client_sockfds[i], &fd_read);
            }
        }

        // 服务端监听socket可读的时候就标识有客户端发来的连接请求报文，可以用这种方法避免
        // accept阻塞等待
        if (FD_ISSET(server_sockfd, &fd_read)) {
            accept_sockfd = accept(server_sockfd, (struct sockaddr*)&accept_addr, &sin_size);
            if (accept_sockfd <= 0) {
                printf("accept error\r\n");
            }
            else {
                printf("connected by %s:%d\r\n", inet_ntoa(accept_addr.sin_addr), accept_addr.sin_port);
                client_sockfds[client_sockfds_count] = accept_sockfd;
                client_sockfds_count++;
                if (accept_sockfd > fd_max) {
                    fd_max = accept_sockfd;
                }
            }
            FD_CLR(server_sockfd, &fd_read);
        }
    }

    return 0;
}
