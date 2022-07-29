#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>

int main(int argc, char* argv[]){

    if (argc != 1){
        printf("usage: ./http_server\n");
        return 1;
    }
    
    // サーバ情報の処理
    struct sockaddr_in server4;
    memset(&server4, 0, sizeof(server4));
    server4.sin_family = AF_INET;
    server4.sin_port = htons(22752);   //id(10052)+12700
    server4.sin_addr.s_addr = INADDR_ANY;

    struct sockaddr_in client;
    int client_len = (int)sizeof(client);

    // ソケット作成
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1){
        perror("socket");
        return 1;
    }

    // アドレスとポートの再利用
    int yes = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&yes, sizeof(yes));

    // 通信元(サーバ側)の情報
    int ret = bind(fd, (struct sockaddr*)&server4, sizeof(server4));
    if (ret == -1){
        perror("bind");
        return 1;
    }

    // 待ち受け
    ret = listen(fd, 5);
    if (ret == -1){
        perror("listen");
        return 1;
    }

    int cli;
    char obuf[500];
    while(1){
        // 接続開始
        cli = accept(fd, (struct sockaddr*)&client, &client_len);
        if (cli == -1){
            perror("accept");
            return 1;
        }

        // 応答
        memset(obuf, 0, sizeof(obuf));
        snprintf(obuf, sizeof(obuf),
            "HTTP/1.0 200 OK\r\n"
            "Content-Type: text/html\r\n"
            "\r\n"
            "<h1>Hello HTTP!</h1>\r\n"
        );
        ret = send(cli, obuf, (int)strlen(obuf), 0);
        if (ret == -1){
            perror("send");
            return 1;
        }
        close(cli);
    }

    // 終了処理
    ret = close(fd);
    if (ret == -1){
        perror("close");
        return 1;
    }

    return 0;
}