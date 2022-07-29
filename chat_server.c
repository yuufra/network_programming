#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#define MAX_EVENTS 10

static void setnonblocking(int sock){
    int flag = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flag | O_NONBLOCK);
}

int main(int argc, char* argv[]){

    if (argc != 1){
        printf("usage: ./chat_server\n");
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

    // 接続用ソケット作成
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1){
        perror("socket");
        return 1;
    }

    // アドレスとポートの再利用
    int yes = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&yes, sizeof(yes));

    // サーバ側の情報
    int ret = bind(fd, (struct sockaddr*)&server4, sizeof(server4));
    if (ret == -1){
        perror("bind");
        return 1;
    }

    // 待ち受け
    ret = listen(fd, 10);
    if (ret == -1){
        perror("listen");
        return 1;
    }

    // 読み込み用epollインスタンスの作成
    int epfd = epoll_create(1);
    if (epfd < 0){
        perror("epoll_create");
        return 1;
    }

    struct epoll_event event;
    memset(&event, 0, sizeof(event));
    event.events = EPOLLIN;
    event.data.fd = fd;

    // epollにファイルディスクリプタを追加
    ret = epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &event);
    if (ret == -1){
        perror("epoll_ctl: epfd");
        return 1;
    }

    // 書き込み用epollインスタンスの作成
    int epoutfd = epoll_create(1);
    if (epoutfd < 0){
        perror("epoll_create");
        return 1;
    }

    int ndfs, noutdfs, cli;
    char buf[500],obuf[520];
    struct epoll_event ev, events[MAX_EVENTS], outevents[MAX_EVENTS];

    while(1){
        ndfs = epoll_wait(epfd, events, MAX_EVENTS, -1);
        if (ndfs == -1){
            perror("epoll_wait");
            return 1;
        }

        for (int i = 0; i < ndfs; i++){
            if (events[i].data.fd == fd) { // 新規接続
                // 接続開始
                cli = accept(fd, (struct sockaddr*)&client, &client_len);
                if (cli == -1){
                    perror("accept");
                    return 1;
                }

                snprintf(obuf, sizeof(obuf),"welcome to the chat app!\n"
                           "you are user%d\n"
                           "if you want to exit, type \"exit\"\n\n", cli);
                ret = write(cli, obuf, strlen(obuf));
                if (ret == -1){
                    perror("write");
                    return 1;
                }

                // 読み込み用epollにファイルディスクリプタを追加
                setnonblocking(cli);
                memset(&ev, 0, sizeof(ev));
                ev.events = EPOLLIN | EPOLLET;
                ev.data.fd = cli;
                if (epoll_ctl(epfd, EPOLL_CTL_ADD, cli, &ev) == -1) {
                    perror("epoll_ctl: cli");
                    return 1;
                }

                // 書き込み用epollにファイルディスクリプタを追加
                memset(&ev, 0, sizeof(ev));
                ev.events = EPOLLOUT;
                ev.data.fd = cli;
                if (epoll_ctl(epoutfd, EPOLL_CTL_ADD, cli, &ev) == -1) {
                    perror("epoll_ctl: cli");
                    return 1;
                }

                // ログイン通知
                noutdfs = epoll_wait(epoutfd, outevents, MAX_EVENTS, -1);
                if (noutdfs == -1){
                    perror("epoll_wait");
                    return 1;
                }

                for (int j = 0; j < noutdfs; j++){
                    if (outevents[j].data.fd != cli){
                        snprintf(obuf, sizeof(obuf),"[user%d entered]\n", cli);
                        ret = write(outevents[j].data.fd, obuf, strlen(obuf));
                        if (ret == -1){
                            perror("write");
                            return 1;
                        }
                    }
                }
            } else { // チャット処理
                // 読み込み
                cli = events[i].data.fd;
                memset(buf, 0, sizeof(buf));
                ret = read(cli, buf, sizeof(buf));
                if (ret == -1){
                    perror("read");
                    return 1;
                }
                
                if (strcmp(buf, "exit\n") == 0) {
                    // 退出処理
                    ret = epoll_ctl(epfd, EPOLL_CTL_DEL, cli, NULL);
                    if (ret == -1) {
                        perror("epoll_ctl: cli");
                        return 1;
                    }

                    snprintf(obuf, sizeof(obuf),"[see you, user%d!]\n", cli);
                    ret = write(cli, obuf, strlen(obuf));
                    if (ret == -1){
                        perror("write");
                        return 1;
                    }

                    ret = close(cli);
                    if (ret == -1){
                        perror("close");
                        return 1;
                    }

                    // ログアウト通知
                    noutdfs = epoll_wait(epoutfd, outevents, MAX_EVENTS, -1);
                    if (noutdfs == -1){
                        perror("epoll_wait");
                        return 1;
                    }

                    for (int j = 0; j < noutdfs; j++){
                        if (outevents[j].data.fd != cli){
                            snprintf(obuf, sizeof(obuf),"[user%d exited]\n", cli);
                            ret = write(outevents[j].data.fd, obuf, strlen(obuf));
                            if (ret == -1){
                                perror("write");
                                return 1;
                            }
                        }
                    }
                } else if (strcmp(buf, "\n") == 0){ // 空行の時は送らない
                    continue;
                } else {
                    // 書き込み
                    noutdfs = epoll_wait(epoutfd, outevents, MAX_EVENTS, -1);
                    if (noutdfs == -1){
                        perror("epoll_wait");
                        return 1;
                    }

                    for (int j = 0; j < noutdfs; j++){
                        if (outevents[j].data.fd != cli){
                            snprintf(obuf, sizeof(obuf),"(user%d):%s", cli, buf);
                            ret = write(outevents[j].data.fd, obuf, strlen(obuf));
                            if (ret == -1){
                                perror("write");
                                return 1;
                            }
                        }
                    }
                }
            }
        }
    }

    // 終了処理
    ret = close(fd);
    if (ret == -1){
        perror("close");
        return 1;
    }

    return 0;
}