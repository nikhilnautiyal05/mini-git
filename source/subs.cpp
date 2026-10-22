#include "header/subs.hpp"
#include<sys/stat.h>
#include<fstream>
#include<sstream>
#include<cstdint>
#include<iomanip>
using namespace std;
bool subfunc::pathExists(const string &path){
        struct stat st;
        return stat(path.c_str(),&st)==0;
    }

bool subfunc::readFile(std::string &file,std::string &out){
    ifstream in(file.c_str(),ios::binary);
    if(!in){
        return false;
    }
    ostringstream ss;
    ss<<in.rdbuf();
    out=ss.str();
    return true;
}

bool subfunc::writeFile(const char *path,const string &content){
    ofstream out(path,ios::binary);
    if(!out){return false;}
    out<<content;
    return true;
}

string subfunc::hashString(string &data){
    uint64_t h=14695981039346656037ULL;
    for(size_t i=0;i<data.size();i++){
        h^=(unsigned char)data[i];
        h*=1099511628211ULL;
    }
    ostringstream oss;
    oss<<hex<<setw(16)<<setfill('0')<<h;
    return oss.str();
}
