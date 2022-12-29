#include "user.h"

int check_name(User *userList, int *idList, string name, int id){
    for(int i = 0; i < MAXCLIENT; i++){
        if(idList[i] == 0) continue;
        if(userList[i].id != id && string(userList[i].userName).compare(name) == 0){
            return -1;
        }
    }
    return 0;
}

User *user_find_by_id(User *userList, int *idList, int id){
    for(int i = 0; i < MAXCLIENT; i++){
        if(idList[i] == 1 && userList[i].id == id) return &(userList[i]);
    }
    return (User *)0;
}