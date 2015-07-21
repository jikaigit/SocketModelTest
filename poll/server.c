/*
 * 使用poll模型的单线程服务器，可以与多个客户端保持连接并接收数据
 */

#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>

#define PORT    3333
#define BACKLOG 4000

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

    // struct pollfd 结构体有三个成员:
    // (1) fd: 用来保存socket描述符
    // (2) events: 设置关心的事件(位操作)
    // (3) revents: 实际发生的事件
    //
    // 可以设置如下事件：
    //    POLLIN    ：有数据可读。
    //    POLLRDNORM：有普通数据可读。
    //    POLLRDBAND：有优先数据可读。
    //    POLLPRI   ：有紧迫数据可读。
    //    POLLOUT   ：写数据不会导致阻塞。
    //    POLLWRNORM：写普通数据不会导致阻塞。
    //    POLLWRBAND：写优先数据不会导致阻塞。
    //
    // 此外，revents域中还可能返回下列事件：
    //    POLLERR ：指定的文件描述符发生错误。
    //    POLLHUP ：指定的文件描述符挂起事件。
    //    POLLNVAL：指定的文件描述符非法。
    //
    struct pollfd  pollfds[BACKLOG];
    int  pollfds_count = 0;
    int  ret;
    int  i;
    int  sin_size = sizeof(struct sockaddr);
    char buffer[1024];

    pollfds[0].fd = server_sockfd;
    pollfds[0].events = POLLRDNORM; // 只关心可读事件
    pollfds_count++;

    for (;;) {
        // 函数原型:
        //    int poll(struct pollfd fds[], nfds_t nfds, int timeout)；
        //
        // 参数说明:
        //    fds：是一个struct pollfd结构类型的数组，用于存放需要检测其状态的Socket描述符；每当调用这个
        //        函数之后，系统不会清空这个数组，操作起来比较方便；特别是对于socket连接比较多的情况下，在一定程
        //        度上可以提高处理的效率；这一点与select()函数不同，调用select()函数之后，select()函数会清空
        //        它所检测的socket描述符集合，导致每次调用select()之前都必须把socket描述符重新加入到待检测的
        //        集合中；因此，select()函数适合于只检测一个socket描述符的情况，而poll()函数适合于大量socket
        //        描述符的情况
        //    nfds：nfds_t类型的参数，用于标记数组fds中的结构体元素的总数量
        //    timeout：是poll函数调用阻塞的时间，单位是毫秒
        //
        // 返回值:
        //    >0 ：数组fds中准备好读、写或出错状态的那些socket描述符的总数量
        //    ==0：数组fds中没有任何socket描述符准备好读、写，或出错；此时poll超时，超时时间是timeout毫秒；换句
        //        话说，如果所检测的socket描述符上没有任何事件发生的话，那么poll()函数会阻塞timeout所指定的毫秒
        //        时间长度之后返回，如果timeout==0，那么poll() 函数立即返回而不阻塞，如果timeout==INFTIM，那
        //        么poll() 函数会一直阻塞下去，直到所检测的socket描述符上的感兴趣的事件发生是才返回，如果感兴趣的
        //        事件永远不发生，那么poll()就会永远阻塞下去；
        //    -1 ：poll函数调用失败，同时会自动设置全局变量errno；
        //
        // 如果待检测的socket描述符为负值，则对这个描述符的检测就会被忽略，也就是不会对成员变量events进行检测，在
        // events上注册的事件也会被忽略，poll()函数返回的时候，会把成员变量revents设置为0，表示没有事件发生；
        // 另外，poll() 函数不会受到socket描述符上的O_NDELAY标记和O_NONBLOCK标记的影响和制约，也就是说，不管
        // socket是阻塞的还是非阻塞的，poll()函数都不会收到影响；而select()函数则不同，select()函数会受到O_NDELAY标记
        // 和O_NONBLOCK标记的影响，如果socket是阻塞的socket，则调用select()跟不调用select()时的效果是一样的，
        // socket仍然是阻塞式TCP通讯，相反，如果socket是非阻塞的socket，那么调用select()时就可以实现非阻塞式TCP通讯；
        //
        ret = poll(pollfds, pollfds_count, 10000); // 10秒超时
        if (ret < 0) {
            printf("poll error\r\n");
            continue;
        }
        else if (ret == 0) {
            printf("poll time out\r\n");
            continue;
        }

        for (i = 1; i < pollfds_count; i++) {
            if (pollfds[i].revents & pollfds[i].events) {
                ret = recv(pollfds[i].fd, buffer, sizeof(buffer)-1, 0);
                if (ret < 0) {
                    printf("recv error\r\n");
                    continue;
                }
                buffer[ret] = '\0';
                printf("recv: %s\r\n", buffer);
            }
        }

        // 监听fd有可读事件到来时就表示有客户端发来的连接请求报文，可以利用这个特点避免调用accept阻塞在
        // 一个地方
        if (pollfds[0].revents & POLLRDNORM) {
            accept_sockfd = accept(server_sockfd, (struct sockaddr*)&accept_addr, &sin_size);
            if (accept_sockfd < 0) {
                printf("accept error\r\n");
                continue;
            }
            printf("connected by %s:%d\r\n", inet_ntoa(accept_addr.sin_addr), accept_addr.sin_port);
            pollfds[pollfds_count].fd     = accept_sockfd;
            pollfds[pollfds_count].events = POLLRDNORM;
            pollfds_count++;
        }
    }

    return 0;
}
