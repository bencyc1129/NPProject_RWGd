#include <iostream>
#include <string.h>
#include <unistd.h>
#include <cstdlib>
#include <vector>
#include <sys/select.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "npshell.h"
#include "../utils/netio.h"

using namespace std;

int main(int argc , char *argv[]){
    int pid;
    int ss = 0, cs = 0, port;
    int nfds;
    int idList[30] = {0};
    fd_set afds, rfds;
    vector<User*> userList;
    string welcome = "****************************************\n** Welcome to the information server. **\n****************************************\n";

    if(argc != 2){
        cout << "./np_single_proc [port]";
        exit(1);
    }
    else port = atoi(argv[1]);
    //socket的建立
    if ((ss = socket(AF_INET , SOCK_STREAM , 0)) < 0){
        perror("Socket()");
        exit(1);
    }

    int enable = 1;
    if (setsockopt(ss, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
        perror("Setsockopt(SO_REUSEADDR)");
        exit(1);
    }

    //socket的連線
    struct sockaddr_in serverInfo, clientInfo;
    socklen_t addrlen = sizeof(clientInfo);
    bzero(&serverInfo,sizeof(serverInfo));
    serverInfo.sin_family = PF_INET;
    serverInfo.sin_addr.s_addr = INADDR_ANY;
    serverInfo.sin_port = htons(port);
    if(bind(ss,(struct sockaddr *)&serverInfo,sizeof(serverInfo))){
        perror("Bind()");
        exit(1);
    }

    if(listen(ss, 30) < 0){
        perror("Listen");
        exit(1);
    }

    nfds = ss;
    FD_ZERO(&afds);
    FD_SET(ss, &afds);

    while(1){
        memcpy(&rfds, &afds, sizeof(rfds));
        while(select(nfds + 1, &rfds, (fd_set *)0, (fd_set *)0, (struct timeval *)0) < 0);

        if(FD_ISSET(ss, &rfds)){
            if ((cs = accept(ss, (struct sockaddr *) &clientInfo, &addrlen)) == -1) {
                perror("Accept()");
                exit(1);
            }
            FD_SET(cs, &afds);
            nfds = (nfds > cs ? nfds: cs);
            User *user = user_insert(userList, cs, clientInfo, idList);

            netwrite(cs, welcome);
            string broadMsg = "*** User '" + user->userName + "' entered from " + user->ip + ":" + to_string(user->port) +". ***\n";
            broadcast(userList, broadMsg);
            netwrite(cs, "% ");
        }
        for(int fd = 0; fd < nfds + 1; fd++){
            if(fd != ss && FD_ISSET(fd, &rfds)){
                User *user = user_find(userList, fd);
                char *oldenv = getenv("PATH");
                int ret = npshell_run_single(userList, user, idList);
                if(ret < 0){
                    string broadMsg = "*** User '" + user->userName + "' left. ***\n";
                    broadcast(userList, broadMsg);
                    FD_CLR(fd, &afds);
                    close(fd);
                    user_delete(userList, fd, idList);
                }
                setenv("PATH", oldenv, 1);
            }
        }
    }

    return 0;
}