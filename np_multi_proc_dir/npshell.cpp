#include <iostream>
#include <string>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <vector>
#include <queue>
#include <semaphore.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

#include "npshell.h"
#include "../utils/netio.h"
#include "../utils/constant.h"
#include "../utils/parser.h"

using namespace std;

// the compare function for pcTable
// the pair with smaller line value will be int front of queue
class cmp
{
public:
    bool operator()(pair<int,string> n1,pair<int,string> n2) {
        return n1.first > n2.first;
    }
};

string linTablePop(priority_queue<pair<int,string>,vector<pair<int,string>>, cmp>& lineTable, 
    int line, int *nextPipeFlag, string lastOutput){
    pair<int, string> top;
    
    if(!lineTable.empty()){
        top = lineTable.top();
        // there is a pipe string needs to be piped to the next command
        if(top.first == line){
            lineTable.pop();
            // the flag that imply there is a pipe string
            *nextPipeFlag = 1;
            // lastOutput is the pipe string
            lastOutput = top.second;

            // there are more than one commands piping to same line
            while(!lineTable.empty()){
                top = lineTable.top();
                if(top.first == line){
                    lineTable.pop();
                    lastOutput += top.second;
                }else break;
            }
        }
        // there is no stored pipe string
        else *nextPipeFlag = 0;
    }

    return lastOutput;
}

string forkChild(vector<string>& tokens, 
                priority_queue<pair<int,string>,vector<pair<int,string>>, cmp>& lineTable,
                int* line, int* nextPipeFlag, string lastOutput, int *idList, User *userList, User *user, 
                char *shm_broadMsg, sem_t *sem, sem_t *ready, int *curID, string input)
{
    int pid, status, n, fds, c, errFlag = 0, redFlag = 0, appFlag = 0, pipeFlag = 0, nPipeFlag = 0, lineIncFlag = 0;
    // for receiving child's stdout
    int pipefd1[2];
    int *rd1 = &pipefd1[0];
    int *wt1 = &pipefd1[1];
    // for parent to write pipe string as child's stdin
    int pipefd2[2];
    int *rd2 = &pipefd2[0];
    int *wt2 = &pipefd2[1];
    // for receiving child's stderr
    int pipefd3[2];
    int *rd3 = &pipefd3[0];
    int *wt3 = &pipefd3[1];
    char buffer[BUFFERSIZE];
    string catResult, token, dirName="", redDes="", err = "";
    vector<string> arg1;
    int userPipeSendFlag = 0, userPipeRecvFlag = 0;
    int idForToUser, idForFromUser;
    string recvMsg = "";

    while(!tokens.empty()){
        // identify the current command
        token = string(tokens[0]);
        tokens.erase(tokens.begin());
        
        /*
        construct argument vector for the later system call
        example: % ls -al bin > test1.txt
        arg1: {"ls", "-al", "bin"}
        redFlag = 1, redDes = "test1.txt"
        */
        if(token.compare("|") == 0){ //ordinary pipe
            pipeFlag = 1;
            break;
        }
        else if(token[0] == '|'){ // numbered-pipe
            nPipeFlag = 1;
            int exp = (token.substr(1, token.length()).find_first_not_of("0123456789") == string::npos);
            if(exp) c = stoi(token.substr(1, token.length()));
            else{
                // cout << "!";
                int a = token[1] - '0';
                char op = token[2];
                int b = token[3] - '0';
                
                if(op == '+')c = a + b;
                else if(op == '-')c = a - b;
                else if(op == '*')c = a * b;
                else c = a / b;
            }
            // cout << c;
            break;
        }
        else if(token[0] == '!'){ // numbered-pipe with stderr 
            nPipeFlag = 1;
            errFlag = 1;
            c = stoi(token.substr(1, token.length()));
            break;
        } 
        else if(token.compare(">") == 0){ // redirection
            redFlag = 1;
            redDes = string(tokens[0]);
            tokens.erase(tokens.begin());
            break;
        }
        else if(token[0] == '>'){ // >N
            idForToUser = stoi(token.substr(1, token.length()));
            userPipeSendFlag = 1;
        }
        else if(token[0] == '<'){ // <N
            idForFromUser = stoi(token.substr(1, token.length()));
            if(idForFromUser > MAXCLIENT || idList[idForFromUser - 1] == 0){
                string errMsg = "*** Error: user #"+to_string(idForFromUser)+" does not exist yet. ***\n";
                netwrite(user->sockfd, errMsg);
                return "";
            }
            char fifoName[21];
            int fifoRD, n;
            sprintf(fifoName, "./user_pipe/pipe%02d%02d", idForFromUser, user->id);
            fifoName[20] = '\0';
            if((fifoRD = open(fifoName, O_RDONLY)) < 0){
                string errMsg = "*** Error: the pipe #"+to_string(idForFromUser)+"->#"+to_string(user->id)+" does not exist yet. ***\n";
                netwrite(user->sockfd, errMsg);
                return "";
            }
            // recvMsg = netread(fifoRD);
            n = read(fifoRD, buffer, BUFFERSIZE - 1);	
            if(n < 0) perror("Read()");
            else{
                if(n == BUFFERSIZE - 1){
                    while((n = read(fifoRD, buffer, BUFFERSIZE - 1)) > 0){
                        buffer[n] = '\0';
                        recvMsg += string(buffer);
                    }
                }
                else{
                    buffer[n] = '\0';
                    recvMsg = string(buffer);
                }
            }

            close(fifoRD);
            remove(fifoName);
            string msg = "*** "+string(user->userName)+" (#"+to_string(user->id)+") just received from "+string(userList[idForFromUser - 1].userName)+" (#"+to_string(userList[idForFromUser - 1].id)+") by '"+input+"' ***\n";
            
            strcpy(shm_broadMsg, msg.c_str());
            *curID = user->id;
            
            kill(getppid(), SIGUSR1);
            sem_wait(ready);
            // netwrite(user->sockfd, recvMsg);
            userPipeRecvFlag = 1;
        }
        else if(token.compare("yell") == 0){
            string yellMsg = "*** " + string(user->userName) + " yelled ***: ";
            yellMsg += input.substr(5, input.length()) + "\n";
            
            strcpy(shm_broadMsg, yellMsg.c_str());
            *curID = user->id;
            
            kill(getppid(), SIGUSR1);
            sem_wait(ready);
            tokens.clear();
            return "";
        }
        else if(token.compare("tell") == 0){
            int id = stoi(tokens[0]);
            string tellMsg = "*** " + string(user->userName) + " told you ***: ";
            if(idList[id - 1] == 1){
                tellMsg = to_string(id) + tellMsg + input.substr(5 + tokens[0].length() + 1, input.length()) + "\n";
                
                strcpy(shm_broadMsg, tellMsg.c_str());
                *curID = user->id;
                
                kill(getppid(), SIGUSR1);
                sem_wait(ready);
                tokens.clear();
                return "";
            }else{
                tellMsg = "*** Error: user #" + to_string(id) + " does not exist yet. ***\n";
                netwrite(user->sockfd, tellMsg);
                tokens.clear();
                return "";
            }
        }
        else{ 
            arg1.push_back(token);
        }
    }

    // copy vector<string> into vector<char*>, because it's more friendly to system call
    vector<char*> arg2(arg1.size(), nullptr);
    for (int i=0; i<arg1.size();i++) {
        arg2[i]= const_cast<char*>(arg1[i].c_str());
    }
    arg2.push_back(NULL);

    // there is no need of forking process for build-in commands 
    if(arg1[0].compare("exit") == 0){
        for(int i = 1; i <= MAXCLIENT; i++){
            char buf[21];
            sprintf(buf, "./user_pipe/pipe%02d%02d", i, user->id);
            buf[20] = '\0';
            remove(buf);
        }
        string msg = "*** User '" + string(user->userName) + "' left. ***\n";
        
        idList[user->id - 1] = 0;
        strcpy(shm_broadMsg, msg.c_str());
        *curID = user->id;
        
        kill(getppid(), SIGUSR1);
        sem_wait(ready);
        
        kill(getppid(), SIGUSR2);
        sem_wait(ready);
        sem_post(sem);
        exit(0);
    }
    else if(arg1[0].compare("printenv") == 0){
        if(getenv(arg2[1]) != NULL){
	        string env = string(getenv(arg2[1])) + "\n";
            netwrite(user->sockfd, env);
	    }   
        return "";
    }
    else if(arg1[0].compare("setenv") == 0){
        setenv(arg2[1], arg2[2], 1);
        return "";
    }
    else if(arg1[0].compare("who") == 0){
        string whoMsg = "<ID>\t<nickname>\t<IP:port>\t<indicate me>\n";
        for(int i = 0; i < MAXCLIENT; i++){
            if(idList[i] == 1){
                whoMsg += (to_string(userList[i].id) + "\t" + string(userList[i].userName) + "\t" + string(userList[i].ip) + ":" + to_string(userList[i].port));
                if(i + 1 == user->id) whoMsg += "\t<-me\n";
                else whoMsg += "\n";
            }
        }
        netwrite(user->sockfd, whoMsg);
        return "";
    }
    else if(arg1[0].compare("name") == 0){
        int ret = check_name(userList, idList, arg1[1], user->id);
        string broadMsg = "";
        if(ret < 0){
            broadMsg = "*** User '" + arg1[1] + "' already exists. ***\n";
            netwrite(user->sockfd, broadMsg);
            return "";
        }else{
            broadMsg = "*** User from " + string(user->ip) + ":" + to_string(user->port) + " is named '" + arg1[1] + "'. ***\n";
            strcpy(user->userName, arg2[1]);
            strcpy(shm_broadMsg, broadMsg.c_str());
            *curID = user->id;
            kill(getppid(), SIGUSR1);
            sem_wait(ready);
            return "";
        }        
    }

    if(pipe(pipefd1) < 0) perror("pipe");
    if(pipe(pipefd2) < 0) perror("pipe");
    if(pipe(pipefd3) < 0) perror("pipe");
    pid = fork();
    if(pid < 0) perror("fork");
    else if(pid == 0){ // child
        // there is a pipe string from previous command
        if(*nextPipeFlag || userPipeRecvFlag) dup2(*rd1, STDIN_FILENO);
        else close(*rd1);
        // there is a redirection
        if(redFlag){
            fds = open(redDes.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0666);
            dup2(fds, STDOUT_FILENO);
            close(*wt2);
        }
        else dup2(*wt2, STDOUT_FILENO); // else receive child process stdout
        
        // !N, parent will use pipefds3 to receive the stderr of child process
        if(errFlag) dup2(*wt3, STDERR_FILENO);
        else dup2(user->sockfd, STDERR_FILENO);
        // closing the redundant pipe heads
        close(*rd2);
        close(*wt1);
        close(*rd3);
        
        if(execvp(arg2[0], arg2.data()) < 0){
            cerr << "Unknown command: [" << arg2[0] << "]." << endl;
            exit(1);
        }
    }
    else{ // parent
        // there is a pipe string needed to be piped to the current command
        close(*rd1);
        if(*nextPipeFlag){
            write(*wt1, lastOutput.c_str(), strlen(lastOutput.c_str()));
            *nextPipeFlag = 0;
        }
        else if(userPipeRecvFlag){
            write(*wt1, recvMsg.c_str(), strlen(recvMsg.c_str()));
        }
        close(*wt1);

        close(*wt3);
        // receiving stderr of child process
        if(errFlag){
            while((n = read(*rd3, buffer, BUFFERSIZE - 1)) > 0){
                buffer[n] = '\0';
                catResult = string(buffer);
                err += catResult;
            }
        }
        // cout << err << err.find("Unknown command");
        close(*rd3);

        // receiving stdout of child process
        close(*wt2);
        lastOutput = "";
        while((n = read(*rd2, buffer, BUFFERSIZE - 1)) > 0){
            buffer[n] = '\0';
            catResult = string(buffer);
            lastOutput += catResult;
        }
        close(*rd2);
        while(waitpid(pid, &status, 0) == -1);
    }

    if(pipeFlag){
        *nextPipeFlag = 1;
        return err + lastOutput;
    }
    else if(nPipeFlag){
        string a = err + lastOutput;
        lineTable.push(make_pair((*line) + c, a));
        return "";
    }
    else{
        string output = err + lastOutput;
        if(userPipeSendFlag){
            if(idForToUser > MAXCLIENT || idList[idForToUser - 1] == 0){
                string errMsg = "*** Error: user #"+to_string(idForToUser)+" does not exist yet. ***\n";
                netwrite(user->sockfd, errMsg);
                return "";
            }
            char fifoName[21];
            int fifoRD;
            int fifoWT;
            sprintf(fifoName, "./user_pipe/pipe%02d%02d", user->id, idForToUser);
            fifoName[20] = '\0';
            if(mkfifo(fifoName, 0777) != 0){
                string errMsg = "*** Error: the pipe #"+to_string(user->id)+"->#"+to_string(idForToUser)+" already exists. ***\n";
                netwrite(user->sockfd, errMsg);
                return "";
            }
            if((fifoRD = open(fifoName, O_RDONLY | O_NONBLOCK)) < 0){
                // netwrite(user->sockfd, to_string(errno));
                return "";
            }
            if((fifoWT = open(fifoName, O_WRONLY)) < 0){
                // netwrite(user->sockfd, to_string(errno));
                return "";
            }
            netwrite(fifoWT, output);
            
            string msg = "*** "+string(user->userName)+" (#"+to_string(user->id)+") just piped '" +input+ "' to "+string(userList[idForToUser - 1].userName)+" (#"+to_string(idForToUser)+") ***\n";
            strcpy(shm_broadMsg, msg.c_str());
            *curID = user->id;
            kill(getppid(), SIGUSR1);
            sem_wait(ready);
        }
        else netwrite(user->sockfd, output);
        return "";
    }

    return lastOutput;
}

int npshell_run(User *userList, User *user, int *idList, char *shm_broadMsg, sem_t *sem, sem_t *ready, int *curID){
    vector<vector<string>> multiTokens;
    vector<string> tmp;
    string input, token, lastOutput = "";
    int n, pos, nextPipeFlag = 0, line = 0;
    priority_queue<pair<int,string>,vector<pair<int,string>>, cmp> lineTable;

    setenv("PATH", "bin:.", 1);
    
    while(1){
        netwrite(user->sockfd, "% ");
        input = netread(user->sockfd);
        if(input.empty()) continue;
	    
        multiTokens = parse(input);

        // consume the input
        while(!multiTokens.empty()){ 
            tmp = multiTokens[0];
            multiTokens.erase(multiTokens.begin());

            // consume the command
            while(!tmp.empty()){
                sem_wait(sem);
                lastOutput = forkChild(tmp, lineTable, &line, &nextPipeFlag, lastOutput, idList, userList, user, shm_broadMsg, sem, ready, curID, input);
                sem_post(sem);
            }
            line++;

            /*
            lineTable is a vector of pair<int, string>, also a priority queue, which compare key is pair[0]
            it records the strings needed to be piped, and which line it should be piped to
            */
            lastOutput = linTablePop(lineTable, line, &nextPipeFlag, lastOutput);
        }
    }
    return 0;
}