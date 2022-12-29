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
                int* line, int* nextPipeFlag, string lastOutput, int sockfd)
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
        else if(token.compare(">>") == 0){
            appFlag = 1;
            redDes = string(tokens[0]);
            tokens.erase(tokens.begin());
            break;
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
    if(arg1[0].compare("exit") == 0) exit(0);
    else if(arg1[0].compare("printenv") == 0){
        if(getenv(arg2[1]) != NULL){
	        string env = string(getenv(arg2[1])) + "\n";
            netwrite(sockfd, env);
	    }   
        return "";
    }
    else if(arg1[0].compare("setenv") == 0){
        setenv(arg2[1], arg2[2], 1);
        return "";
    }

    if(pipe(pipefd1) < 0) perror("pipe");
    if(pipe(pipefd2) < 0) perror("pipe");
    if(pipe(pipefd3) < 0) perror("pipe");
    pid = fork();
    if(pid < 0) perror("fork");
    else if(pid == 0){ // child
        // there is a pipe string from previous command
        if(*nextPipeFlag) dup2(*rd1, STDIN_FILENO);
        else close(*rd1);
        // there is a redirection
        if(redFlag){
            fds = open(redDes.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0666);
            dup2(fds, STDOUT_FILENO);
            close(*wt2);
        }
        else if(appFlag){
            fds = open(redDes.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0666);
            dup2(fds, STDOUT_FILENO);
            close(*wt2);
        }
        else dup2(*wt2, STDOUT_FILENO); // else receive child process stdout
        
        // !N, parent will use pipefds3 to receive the stderr of child process
        if(errFlag) dup2(*wt3, STDERR_FILENO);
        else dup2(sockfd, STDERR_FILENO);
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
        netwrite(sockfd, output);
        return "";
    }

    return lastOutput;
}

int npshell_run(int sockfd){
    vector<vector<string>> multiTokens;
    vector<string> tmp;
    char buffer[BUFFERSIZE];
    string input, token, lastOutput = "";
    int n, pos, nextPipeFlag = 0, line = 0;
    priority_queue<pair<int,string>,vector<pair<int,string>>, cmp> lineTable;

    setenv("PATH", "bin:.", 1);

    while(1){
        netwrite(sockfd, "% ");
        input = netread(sockfd);
        if(input.empty()) continue;
	    
        multiTokens = parse(input);

        // consume the input
        while(!multiTokens.empty()){ 
            tmp = multiTokens[0];
            multiTokens.erase(multiTokens.begin());

            // consume the command
            while(!tmp.empty()) lastOutput = forkChild(tmp, lineTable, &line, &nextPipeFlag, lastOutput, sockfd);
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