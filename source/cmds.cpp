#include <iostream>
#include<direct.h>
#include "header/cmds.hpp"
#include "header/subs.hpp"
void repo::init(){
    if(subfunc::pathExists(REPO_DIR)){
        std::cout<<"folder already exists";
    }
    else{
    if (_mkdir("D:\\.minigit") == 0) {
        std::cout << "Folder created successfully!\n";
        initialized_=true;
    }
    else {
        std::cerr << "Error creating folder.\n";
    }}
}
void repo::add(std::string filename){
    
}