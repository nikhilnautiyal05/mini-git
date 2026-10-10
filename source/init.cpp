#include <iostream>
#include<direct.h>
#include "header/init.hpp"
#include "header/subfunc.hpp"
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

