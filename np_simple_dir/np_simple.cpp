#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <cstdlib>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "../utils/netio.h"
#include "npshell.h"

int main(int argc , char *argv[]){
    int pid;
    int ss = 0, cs = 0, port;

    port = atoi(argv[1]);

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

    while(1){
        if((cs = accept(ss,(struct sockaddr*) &clientInfo, &addrlen))< 0){
            perror("Accept()");
            exit(1);
        }

        if((pid = fork()) < 0){
            perror("Fork()");
            exit(1);
        }else if(pid == 0){
            close(ss);
            npshell_run(cs);
            exit(0);
        }else{
            close(cs);
            wait(NULL);
        }
    }
    return 0;
}