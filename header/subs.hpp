#pragma once
#include<iostream>
namespace subfunc{
    std::string hashString(std::string &data);
    bool pathExists(const std::string &path);
    bool readFile(std::string &path,std::string &out);
    bool writeFile(const char *path,const std::string &content);
};