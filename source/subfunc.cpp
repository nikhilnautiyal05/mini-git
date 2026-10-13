#include "header/subfunc.hpp"
#include<sys/stat.h>
    bool subfunc::pathExists(std::string &path){
        struct stat st;
        return stat(path.c_str(),&st)==0;
    }
