#include<iostream>
#include<sstream>
#include "header/cmds.hpp"
using namespace std;
int main(){
    repo git;
    string line;
    cout<<"Mini-Git - help for commands\n";
    while(cout<<">" && getline(cin,line)){
        istringstream iss(line);
        string cmd;
        iss>>cmd;
        if(cmd=="exit" || cmd=="quit")break;
        else if(cmd=="help"){
             cout << "init | add <file> | commit <message> | log | status|"<<endl<<"branch [name] | checkout <name> | exit"<<endl;}
        else if(cmd=="init"){
        git.init();
        cout<<endl<<"Initialised successfully"<<endl;
        }
        else if(!git.isInitialized()){
        cout<<"Create a directory first";
        }
        else if(cmd=="add"){
            string fn;
            iss>>fn;
            git.add(fn);
        }
        else{
        cout<<"good to go";
        }
    }
    return 0;
}