#include <vector>
#include <queue>
#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "../utils/netio.h"
#include "../utils/constant.h"

using namespace std;

struct User{
    char ip[100];
    char userName[100];
    int port;
    int id;
    int sockfd;
};

extern int check_name(User *userlist, int *idList, string name, int id);
extern User* user_find_by_id(User *userList, int *idList, int id);
