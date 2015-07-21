/*
 * 基于epoll的服务器，单线程就可以维持与多个客户端的连接并分别接收数据
 */

#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/epoll.h>

#define PORT           3333
#define EVENT_BUF_SIZE 64

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

    // epoll_create() 创建一个 epoll 实例，并要求内核分配一个可以保存 size 个描述符的空间
    // size 不是可以保存的最大的描述符个数，而只是给内核规划内部结构的一个暗示。(现在，size被
    // 忽略。从 Linux 2.6.8 以后，参数 size 不再使用，但必需大于零。内核动态地分配数据结构
    // 而不这个初始的暗示)
    //
    int epollfd = epoll_create(64);
    if (epollfd < 0) {
        printf("epoll create failed\r\n");
        close(server_sockfd);
        return 0;
    }

    struct epoll_event event;
    event.data.fd = server_sockfd;
    event.events  = EPOLLIN;

    // 这个系统调用对文件描述符 epfd 引用的 epoll 实例进行控制操作。它要求对目标文件描述符 fd 进行一个操作 op。
    // op 的有效值是：
    // EPOLL_CTL_ADD
    //    把目标文件描述符 fd 注册到由 epfd 引用的 epoll 实例上并把相应的事件 event 与内部的 fd 相链接。
    // EPOLL_CTL_MOD
    //    更改目标文件描述符 fd 相关联的事件 event。
    // EPOLL_CTL_DEL
    //
    // 从由 epfd 引用的 epoll 实例中删除目标文件描述符 fd。event 被忽略，并全可以是 NULL(但是请看 BUGS)。
    // events 成员是下列有效的事件类型的位组合：
    // EPOLLIN
    //    相关联的文件对 read(2) 操作有效。
    // EPOLLOUT
    //    相关联的文件对 write(2) 操作有效。
    // EPOLLRDHUP (从 Linux 2.6.17 开始)
    //    流套接口对端关闭连接，或把写端关闭。(当使用边缘触发时这个标志可以简化检测对端关闭的代码。)
    // EPOLLPRI
    //    存在对 read(2) 操作有效的紧急数据。
    // EPOLLERR
    //    相关联的文件描述符发生错误状态。epoll_wait(2) 总是等待这个事件，它不需要在 events 中设置。
    // EPOLLHUP
    //    相关联的文件描述符延迟。epoll_wait(2) 总是等待这个事件，不需要在 events 中设置。
    // EPOLLET
    //    把相关联的文件描述符设置为边缘触发行为。对 epoll 默认的行为是水平触发的。参看 epoll(7) 来了解更多关于边缘和水平触发事件机制。
    // EPOLLONESHOT (从 Linux 2.6.2 开始)
    //    把相关联的文件描述符设置为一次性有效。这就是说在 epoll_wait(2) 抛出一个事件之后相关联的文件描述符将不再报告其他事件。用户必须
    //    使用 EPOLL_CTL_MOD 调用 epoll_ctl() 来重新设置文件描述符的新的事件掩码。
    //
    // 返回值
    // 成功时，epoll_ctl() 返回零。错误时，epoll_ctl() 返回 -1 并把 errno 设置为合适的值。
    //
    int ret = epoll_ctl(epollfd, EPOLL_CTL_ADD, server_sockfd, &event);
    if (ret < 0) {
        printf("event register failed\r\n");
        close(server_sockfd);
        return 0;
    }

    struct epoll_event events_occur[EVENT_BUF_SIZE];
    int  i;
    int  sin_size = sizeof(struct sockaddr);
    char buffer[1024];
    for (;;) {
        // 当有相应的事件发生时，epoll_wait返回需要处理的事件的个数，这个个数不会超过EVENT_BUF_SIZE，
        // 如果超时返回0，发生错误就返回-1
        //
        ret = epoll_wait(epollfd, events_occur, EVENT_BUF_SIZE, 10000);
        if (ret < 0) {
            printf("epoll wait error\r\n");
            continue;
        }
        else if (ret == 0) {
            printf("epoll time out\r\n");
            continue;
        }

        for (i = 0; i < ret; i++) {
            if (events_occur[i].events & EPOLLIN) {
                if (events_occur[i].data.fd == server_sockfd) {
                    accept_sockfd = accept(server_sockfd, (struct sockaddr*)&accept_addr, &sin_size);
                    if (accept_sockfd < 0) {
                        printf("accept error\r\n");
                        continue;
                    }
                    printf("connected by %s:%d\r\n", inet_ntoa(accept_addr.sin_addr), accept_addr.sin_port);

                    struct epoll_event client_event;
                    client_event.data.fd = accept_sockfd;
                    client_event.events = EPOLLIN;
                    ret = epoll_ctl(epollfd, EPOLL_CTL_ADD, accept_sockfd, &client_event);
                    if (ret < 0) {
                        printf("register client socket event error\r\n");
                    }
                }
                else {
                    ret = recv(events_occur[i].data.fd, buffer, sizeof(buffer)-1, 0);
                    if (ret < 0) {
                        printf("recv error\r\n");
                        continue;
                    }
                    buffer[ret] = '\0';
                    printf("recv: %s\r\n", buffer);
                }
            }
        }
    }

    return 0;
}
