#include <iostream>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <cstdlib>
#include <vector>
#include <semaphore.h>
#include <fcntl.h> 
#include <sys/stat.h>
#include <signal.h>
#include <sys/shm.h> 
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "../utils/netio.h"
#include "../utils/constant.h"
#include "npshell.h"

using namespace std;

User *userList;
int *idList;
char *broadMsg;
sem_t *ready[MAXCLIENT];
sem_t *sem;
sem_t *test;
int *curID;

int getID(int *idList){
    for(int i = 0; i < MAXCLIENT; i++){
        if(idList[i] == 0){
            idList[i] = 1;
            return i + 1;
        }
    }
    return 0;
}

void broadcast_handler(int signum){
    string tmp = string(broadMsg);
    if(tmp.find_first_not_of("123456789") != 0){
        cout << "broadcast 1\n";
        int targetID = stoi(tmp.substr(0, tmp.find_first_not_of("123456789")));
        netwrite(userList[targetID - 1].sockfd, tmp.substr(1, tmp.size()-1));
    }
    else{
        cout << "broadcast 2 by " << *curID << endl;
        for(int i = 0; i < MAXCLIENT; i++){
            if(idList[i] == 1) netwrite(userList[i].sockfd, string(broadMsg));
        }
    }
    sem_post(ready[*curID - 1]);
    return;
}

void child_exit_handler(int signum){
    close(userList[*curID - 1].sockfd);
    sem_post(ready[*curID - 1]);
    return;
}

int main(int argc , char *argv[]){
    int pid;
    int ss = 0, cs = 0, port;
    string welcome = "****************************************\n** Welcome to the information server. **\n****************************************\n";

    struct stat st = {0};
    if (stat("user_pipe", &st) == -1) {
        mkdir("user_pipe", 0700);
    }

    signal(SIGUSR1, broadcast_handler);
    signal(SIGUSR2, child_exit_handler);

    // shared memory for user list
    int shmid_UL;
    key_t shmkey_UL;
    int size_UL = sizeof(userList) * MAXCLIENT;
    shmkey_UL = ftok ("/dev/null", 5);
    cout << "shmkey for userList = " << shmkey_UL << endl;
    shmctl (shmid_UL, IPC_RMID, 0);
    shmid_UL = shmget(shmkey_UL, size_UL, 0644 | IPC_CREAT);
    if (shmid_UL < 0){                           
        perror ("shmget\n");
        exit (1);
    }
    userList = (User *) shmat(shmid_UL, 0, 0);
    if (userList == (User *)(-1)) {
        perror("shmat");
        exit(1); 
    }

    // shared memory for id list
    int shmid_IL;
    key_t shmkey_IL;
    int size_IL = sizeof(int) * MAXCLIENT;
    shmkey_IL = ftok ("/dev/null", 6);
    cout << "shmkey for idList = " << shmkey_IL << endl;
    shmctl(shmid_IL, IPC_RMID, 0);
    shmid_IL = shmget(shmkey_IL, size_IL, 0644 | IPC_CREAT);
    if (shmid_IL < 0){                           
        perror ("shmget\n");
        exit (1);
    }
    idList = (int *) shmat(shmid_IL, 0, 0);
    if (idList == (int *)(-1)) {
        perror("shmat");
        exit(1); 
    }
    for(int i = 0; i < MAXCLIENT; i++) idList[i] = 0;


    int shmid_BM;
    key_t shmkey_BM;
    int size_BM = 65536;
    shmkey_BM = ftok ("/dev/null", 7);
    cout << "shmkey for broadMsg = " << shmkey_BM << endl;
    shmctl(shmid_BM, IPC_RMID, 0);
    shmid_BM = shmget(shmkey_BM, size_BM, 0644 | IPC_CREAT);
    if (shmid_BM < 0){                           
        perror ("shmget\n");
        exit (1);
    }
    broadMsg = (char *) shmat(shmid_BM, 0, 0);
    if (broadMsg == (char *)(-1)) {
        perror("shmat");
        exit(1); 
    }
    strcpy(broadMsg, "");

    int shmid_CU;
    key_t shmkey_CU;
    int size_CU = sizeof(int);
    shmkey_CU = ftok ("/dev/null", 8);
    cout << "shmkey for curID = " << shmkey_CU << endl;
    shmctl(shmid_CU, IPC_RMID, 0);
    shmid_CU = shmget(shmkey_CU, size_CU, 0644 | IPC_CREAT);
    if (shmid_CU < 0){                           
        perror ("shmget\n");
        exit (1);
    }
    curID = (int *) shmat(shmid_CU, 0, 0);
    if (curID == (int *)(-1)) {
        perror("shmat");
        exit(1); 
    }

    sem_unlink ("pSem");   
    sem = sem_open ("pSem", O_CREAT | O_EXCL, 0644, 1); 
    sem_unlink ("test");   
    test = sem_open ("test", O_CREAT | O_EXCL, 0644, 1); 
    for(int i = 0; i < MAXCLIENT; i++){
        char buf[6];
        sprintf(buf, "sem%02d", i);
        buf[5] = '\0';
        cout << buf << endl;
        sem_unlink (buf);   
        ready[i] = sem_open (buf, O_CREAT | O_EXCL, 0644, 0); 
    }

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
    if(listen(ss, MAXCLIENT) < 0){
        perror("Listen");
        exit(1);
    }

    while(1){
        if((cs = accept(ss,(struct sockaddr*) &clientInfo, &addrlen)) < 0){
            perror("Accept()");
            exit(1);
        }
        cout << "current cs: " << cs << endl;
        int id = getID(idList);
        userList[id - 1].id = id;
        strcpy(userList[id - 1].ip, inet_ntoa(clientInfo.sin_addr));
        userList[id - 1].port = (int)ntohs(clientInfo.sin_port);
        userList[id - 1].sockfd = cs;
        strcpy(userList[id - 1].userName, "(no name)");
        netwrite(cs, welcome);
        string msg = "*** User '" + string(userList[id - 1].userName) + "' entered from " + string(userList[id - 1].ip) + ":" + to_string(userList[id - 1].port) +". ***\n";
        strcpy(broadMsg, msg.c_str());
        *curID = id;
        kill(getpid(), SIGUSR1);
        sem_wait(ready[id -1]);

        if((pid = fork()) < 0){
            perror("Fork()");
            exit(1);
        }else if(pid == 0){
            close(ss);
            npshell_run(userList, &(userList[id - 1]), idList, broadMsg, sem, ready[id - 1], curID);
            exit(0);
        }else{
            // close(cs);
        }
    }
    return 0;
}