#include <vector>
#include <string>

using namespace std;

vector<vector<string>> parse(string input){
    int pos;
    string token;
    vector<string> tmp;
    vector<vector<string>> mutiTokens;

    while((pos = input.find(" ")) != string::npos){
        token = input.substr(0, pos);
        tmp.push_back(token);
        if(token.compare("|") != 0 && (token[0] == '|' || token[0] == '!')){
            mutiTokens.push_back(tmp);
            tmp.clear();
        }
        input.erase(0, pos + 1);
        input.erase(0, input.find_first_not_of(" "));
    }
    if(!input.empty()){
        tmp.push_back(input);
        mutiTokens.push_back(tmp);
    }else{
        mutiTokens.push_back(tmp);
    }

    return mutiTokens;
}