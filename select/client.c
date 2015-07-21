#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>

#define PORT 3333

int main() {
    int client_sockfd;
    struct sockaddr_in server_addr;

    client_sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (client_sockfd < 0) {
        printf("create socket failed\r\n");
        return 0;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port   = htons(PORT);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(client_sockfd, (struct sockaddr*)&server_addr, sizeof(struct sockaddr)) < 0) {
        printf("connect error\r\n");
        return 0;
    }

    char buffer[1024];
    for (;;) {
        scanf("%s", buffer);
        int ret = send(client_sockfd, buffer, strlen(buffer), 0);
        if (ret < 0) {
            printf("send error\r\n");
        }
        else {
            printf("send: %s\r\n", buffer);
        }
    }

    return 0;
}
