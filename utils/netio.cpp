#include <iostream>
#include <string>
#include <string.h>
#include <unistd.h>

#include "constant.h"

using namespace std;

int netwrite(int sockfd, string output){
    char buffer[BUFFERSIZE];
    int i, j, cnt;
    if(output.length() > BUFFERSIZE - 1){
        for(i = 0, j = 0, cnt = BUFFERSIZE - 1; j < output.length();){
            buffer[i++] = output[j++];
            cnt--;
            if(cnt == 0){
                i = 0;
                cnt = BUFFERSIZE - 1;
                buffer[BUFFERSIZE - 1] = '\0';
                write(sockfd, buffer, strlen(buffer));
            }
        }
        if(i != 0){
            buffer[i + 1] = '\0';
            write(sockfd, buffer, strlen(buffer));
        }
    }else{
        for(int i = 0; i < output.length(); i++){
            buffer[i] = output[i];
        }
        buffer[output.length()] = '\0';
        write(sockfd, buffer, strlen(buffer));
    }

    return 0;
}

string remove(string s){
	s.pop_back();
	if(s[s.length()-1] == '\r') s.pop_back();
	return s;
}

string netread(int sockfd){
    int n;
    string input = "";
    char buffer[BUFFERSIZE];

    n = read(sockfd, buffer, BUFFERSIZE - 1);	
    if(n < 0) perror("Read()");
    if(n == 1 || n == 2) return "";
    else{
        if(n == BUFFERSIZE - 1){
            while((n = read(sockfd, buffer, BUFFERSIZE - 1)) > 0){
                buffer[n] = '\0';
                input += string(buffer);
            }

            input = remove(input);
        }
        else{
            buffer[n] = '\0';
            input = string(buffer);
            input = remove(input);
        }
    }

    return input;
}
