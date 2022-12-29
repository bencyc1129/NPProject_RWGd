#include <iostream>
#include "user.h"
#include "../utils/netio.h"

using namespace std;

int getID(int* idList){
    for(int i = 0; i < 30; i++){
        if(idList[i] == 0){
            idList[i] = 1;
            return i + 1;
        }
    }
    return 0;
}

User* user_insert(vector<User*>& userList, int cs, struct sockaddr_in clientInfo, int* idList){
    User *user = new User;

    user->env = "bin:.";
    user->id = getID(idList);
    user->ip = string(inet_ntoa(clientInfo.sin_addr));
    user->lastOutput = "";
    user->line = 0;
    user->nextPipeFlag = 0;
    user->sockfd = cs;
    user->userName = "(no name)";
    user->port = (int)ntohs(clientInfo.sin_port);

    userList.insert(userList.begin() + user->id - 1, user);

    return user;
}

User* user_find(vector<User*>& userList, int fd){
    User *user = new User;

    for(User *u : userList){
        if(u->sockfd == fd){
            user = u;
            break;
        }
    }
    return user;
}

User* user_find_by_id(vector<User*>& userList, int id){
    User *user = new User;

    for(User *u : userList){
        if(u->id == id){
            user = u;
            break;
        }
    }
    return user;
}

void user_delete(vector<User*>& userList, int fd, int* idList){
    for(int i = 0; i < userList.size(); i++){
        if(userList[i]->sockfd == fd){
            idList[userList[i]->id - 1] = 0;
            userList.erase(userList.begin() + i);
            break;
        }
    }
    return;
}

string recv_user_pipe(User *user, int id, string& pipeMsg){
    string ret = "";

    if(user->pipeMsgs.empty()) return ret;

    for(int i = 0; i < (user->pipeMsgs).size(); i++){
        string fromID = user->pipeMsgs[i].substr(0, user->pipeMsgs[i].find_first_not_of("123456789"));
        if(stoi(fromID) == id){
            ret = user->pipeMsgs[i].substr(fromID.length() + 1, user->pipeMsgs[i].length());
            user->pipeMsgs.erase(user->pipeMsgs.begin() + i);
            pipeMsg = user->pipeCmds[i];
            user->pipeCmds.erase(user->pipeCmds.begin() + i);
            break;
        }
    }
    return ret;
}

int check_name(vector<User*>& userList, string name, int fd){
    for(User *u: userList){
        if(u->sockfd != fd && (u->userName).compare(name) == 0) return -1;
    }
    return 0;
}

int check_pipe_exist(vector<User*>&userList, int id, int idForToUser){
    for(User *u: userList){
        if(u->id == idForToUser){
            for(string s: u->pipeMsgs){
                int fromID = stoi(s.substr(0, s.find_first_not_of("123456789")));
                if(fromID == id) return 1;
            }
        }
    }
    return 0;
}

void broadcast(vector<User*>& userList, string broadMsg){
    for(User *u : userList){
        netwrite(u->sockfd, broadMsg);
    }

    return;
}