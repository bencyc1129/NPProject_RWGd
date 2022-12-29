#include <string>

using namespace std;

extern string netread(int sockfd);
extern int netwrite(int sockfd, string output);