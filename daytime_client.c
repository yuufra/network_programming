#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>

int main(int argc, char* argv[]){

    if (argc != 2){
        printf("usage: ./daytime_client ip_addr\n");
        return 1;
    }
    
    // 引数の処理 (IPv4)
    struct sockaddr_in server4;
    memset(&server4, 0, sizeof(server4));
    int fd = -1;
    int ret = inet_pton(AF_INET, argv[1], &server4.sin_addr);
    if (ret == 1){
        server4.sin_family = AF_INET;
        server4.sin_port = htons(13);

        // ネットワーク接続
        fd = socket(AF_INET, SOCK_STREAM, 0);
        if (fd == -1){
            perror("socket");
            return 1;
        }

        ret = connect(fd, (struct sockaddr*)&server4, sizeof(server4));
        if (ret == -1){
            perror("connect");
            return 1;
        }
    } else { // 引数の処理 (IPv6)
        struct sockaddr_in6 server6;
        memset(&server6, 0, sizeof(server6));
        int ret = inet_pton(AF_INET6, argv[1], &server6.sin6_addr);
        if (ret != 1){
            perror("inet_pton");
            return 1;
        }
        server6.sin6_family = AF_INET6;
        server6.sin6_port = htons(13);

        // ネットワーク接続
        fd = socket(AF_INET6, SOCK_STREAM, 0);
        if (fd == -1){
            perror("socket");
            return 1;
        }

        ret = connect(fd, (struct sockaddr*)&server6, sizeof(server6));
        if (ret == -1){
            perror("connect");
            return 1;
        }
    }

    // 受信・表示
    char buf[100];
    memset(buf, 0, sizeof(buf));
    read(fd, buf, sizeof(buf));
    if (ret == -1){
        perror("read");
        return 1;
    }
    printf("%s\n", buf);
    fflush(stdin);

    // 終了処理
    ret = close(fd);
    if (ret == -1){
        perror("close");
        return 1;
    }

    return 0;
}