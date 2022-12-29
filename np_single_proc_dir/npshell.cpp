#include <iostream>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <vector>
#include <queue>

#include "npshell.h"
#include "../utils/netio.h"
#include "../utils/constant.h"
#include "../utils/parser.h"

using namespace std;

void linTablePop(User *user){
    pair<int, string> top;
    
    if(!(user->lineTable).empty()){
        top = (user->lineTable).top();
        // there is a pipe string needs to be piped to the next command
        if(top.first == user->line){
            (user->lineTable).pop();
            // the flag that imply there is a pipe string
            user->nextPipeFlag = 1;
            // lastOutput is the pipe string
            user->lastOutput = top.second;

            // there are more than one commands piping to same line
            while(!(user->lineTable).empty()){
                top = (user->lineTable).top();
                if(top.first == user->line){
                    (user->lineTable).pop();
                    user->lastOutput += top.second;
                }else break;
            }
        }
        // there is no stored pipe string
        else user->nextPipeFlag = 0;
    }

    return;
}

int forkChild(vector<string>& tokens, User *user, vector<User*>& userList, int* idList, string input){
    int pid, status, n, fds, c, errFlag = 0, redFlag = 0, appFlag = 0, pipeFlag = 0, nPipeFlag = 0, userPipeSendFlag = 0, userPipeRecvFlag = 0;
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
    string catResult, token, dirName="", redDes="", err = "", recvMsg = "", pipeMsg = "";
    vector<string> arg1;
    User *targetUser;
    User *fromUser;
    User *toUser;
    int idForFromUser;
    int idForToUser;

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
            if(idForFromUser > 30 || idList[idForFromUser - 1] == 0){
                string errMsg = "*** Error: user #"+to_string(idForFromUser)+" does not exist yet. ***\n";
                netwrite(user->sockfd, errMsg);
                return -1;
            }
            fromUser = user_find_by_id(userList, idForFromUser);
            recvMsg = recv_user_pipe(user, idForFromUser, pipeMsg);
            if(recvMsg.empty()){
                string errMsg = "*** Error: the pipe #"+to_string(idForFromUser)+"->#"+to_string(user->id)+" does not exist yet. ***\n";
                netwrite(user->sockfd, errMsg);
                return -1;
            }
            string broadMsg = "*** "+user->userName+" (#"+to_string(user->id)+") just received from "+fromUser->userName+" (#"+to_string(fromUser->id)+") by '"+input+"' ***\n";
            broadcast(userList, broadMsg);
            userPipeRecvFlag = 1;
        }
        else if(token.compare("yell") == 0){
            string yellMsg = "*** " + user->userName + " yelled ***: ";
            yellMsg += input.substr(5, input.length()) + "\n";
            broadcast(userList, yellMsg);
            tokens.clear();
            return 0;
        }
        else if(token.compare("tell") == 0){
            int id = stoi(tokens[0]);
            string tellMsg = "*** " + user->userName + " told you ***: ";
            if(idList[id - 1] == 1){
                targetUser = user_find_by_id(userList, id);
                tellMsg += input.substr(5 + tokens[0].length() + 1, input.length()) + "\n";
                netwrite(targetUser->sockfd, tellMsg);
                tokens.clear();
                return 0;
            }else{
                tellMsg = "*** Error: user #" + to_string(id) + " does not exist yet. ***\n";
                netwrite(user->sockfd, tellMsg);
                tokens.clear();
                return -1;
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

    if(arg1[0].compare("printenv") == 0){
        if(getenv(arg2[1]) != NULL){
	        string env = string(getenv(arg2[1])) + "\n";
            netwrite(user->sockfd, env);
	    }   
        return 0;
    }
    else if(arg1[0].compare("setenv") == 0){
        // setenv(arg2[1], arg2[2], 1);
        user->env = arg1[2];
        return 0;
    }
    else if(arg1[0].compare("who") == 0){
        string whoMsg = "<ID>\t<nickname>\t<IP:port>\t<indicate me>\n";
        for(User* u: userList){
            whoMsg += (to_string(u->id) + "\t" + u->userName + "\t" + u->ip + ":" + to_string(u->port));
            if(u->sockfd == user->sockfd) whoMsg += "\t<-me\n";
            else whoMsg += "\n";
        }
        netwrite(user->sockfd, whoMsg);
        return 0;
    }
    else if(arg1[0].compare("name") == 0){
        int ret = check_name(userList, arg1[1], user->sockfd);
        string broadMsg = "";
        if(ret < 0){
            broadMsg = "*** User '" + arg1[1] + "' already exists. ***\n";
            netwrite(user->sockfd, broadMsg);
        }else{
            broadMsg = "*** User from " + user->ip + ":" + to_string(user->port) + " is named '" + arg1[1] + "'. ***\n";
            broadcast(userList, broadMsg);
        }
        user->userName = arg1[1];
        
        return 0;
    }

    if(pipe(pipefd1) < 0) perror("pipe");
    if(pipe(pipefd2) < 0) perror("pipe");
    if(pipe(pipefd3) < 0) perror("pipe");
    pid = fork();
    if(pid < 0) perror("fork");
    else if(pid == 0){ // child
        // there is a pipe string from previous command
        if(user->nextPipeFlag || userPipeRecvFlag) dup2(*rd1, STDIN_FILENO);
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
            return -1;
        }
    }
    else{ // parent
        // there is a pipe string needed to be piped to the current command
        close(*rd1);
        if(user->nextPipeFlag){
            write(*wt1, (user->lastOutput).c_str(), strlen((user->lastOutput).c_str()));
            user->nextPipeFlag = 0;
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
        user->lastOutput = "";
        while((n = read(*rd2, buffer, BUFFERSIZE - 1)) > 0){
            buffer[n] = '\0';
            catResult = string(buffer);
            user->lastOutput += catResult;
        }
        close(*rd2);
        while(waitpid(pid, &status, 0) == -1);
    }

    if(pipeFlag){
        user->nextPipeFlag = 1;
        user->lastOutput = err + user->lastOutput;
        return 0;
    }
    else if(nPipeFlag){
        string a = err + user->lastOutput;
        (user->lineTable).push(make_pair((user->line) + c, a));
        return 0;
    }
    else{
        string output = err + user->lastOutput;
        if(userPipeSendFlag){
            if(idForToUser > 30 || idList[idForToUser - 1] == 0){
                string errMsg = "*** Error: user #"+to_string(idForToUser)+" does not exist yet. ***\n";
                netwrite(user->sockfd, errMsg);
                return -1;
            }
            toUser = user_find_by_id(userList, idForToUser);
            if(check_pipe_exist(userList, user->id, idForToUser)){
                string errMsg = "*** Error: the pipe #"+to_string(user->id)+"->#"+to_string(idForToUser)+" already exists. ***\n";
                netwrite(user->sockfd, errMsg);
                return -1;
            }
            string broadMsg = "*** "+user->userName+" (#"+to_string(user->id)+") just piped '" +input+ "' to "+toUser->userName+" (#"+to_string(toUser->id)+") ***\n";
            broadcast(userList, broadMsg);
            (toUser->pipeMsgs).push_back(to_string(user->id) + "@" + output);
            (toUser->pipeCmds).push_back(input);
        }
        else netwrite(user->sockfd, output);
        return 0;
    }

    return 0;
}

int npshell_run_single(vector<User*>& userList, User *user, int* idList){
    vector<vector<string>> multiTokens;
    vector<string> tmp;
    char buffer[BUFFERSIZE];
    string input = "", token;
    int n, pos, ret;

    input = netread(user->sockfd);
    if(input.compare("exit") == 0) return -1;

    multiTokens = parse(input);

    while(!multiTokens.empty()){ 
        setenv("PATH", (user->env).c_str(), 1);
        tmp = multiTokens[0];
        multiTokens.erase(multiTokens.begin());

        while(!tmp.empty()) ret = forkChild(tmp, user, userList, idList, input);
        (user->line)++;

        linTablePop(user);
    }

    netwrite(user->sockfd, "% ");
    return 0;
}