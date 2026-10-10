#include<iostream>
#include<sstream>
#include "header/init.hpp"
using namespace std;
int main(){
    repo git;
    string command;
    cin>>command;
    if(command=="init"){
        git.init();
        cout<<endl<<"Initialised successfully";
    }
    else if(git.isInitialized()){
        cout<<"Create a directory first";
    }
    else{
        cout<<"good to go";
    }
    return 0;
}