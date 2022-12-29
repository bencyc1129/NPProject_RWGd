#include <vector>
#include <queue>
#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

using namespace std;

class cmp
{
public:
    bool operator()(pair<int,string> n1,pair<int,string> n2) {
        return n1.first > n2.first;
    }
};

struct User{
    priority_queue<pair<int,string>,vector<pair<int,string>>, cmp> lineTable;
    string ip;
    string userName;
    string lastOutput;
    string env;
    vector<string> pipeMsgs;
    vector<string> pipeCmds;
    int port;
    int line;
    int nextPipeFlag;
    int id;
    int sockfd;
};

extern User* user_insert(vector<User*>& userList, int cs, struct sockaddr_in clientInfo, int* idList);
extern void user_delete(vector<User*>& userList, int cs, int* idList);
extern User* user_find(vector<User*>& userList, int cs);
extern User* user_find_by_id(vector<User*>& userList, int id);
extern string recv_user_pipe(User *user, int id, string& pipMsg);
extern int check_name(vector<User*>& userlist, string name, int cs);
extern int check_pipe_exist(vector<User*>&userList, int id, int idForToUser);
extern void broadcast(vector<User*>& userList, string broadMsg);
int getID(int* idList);