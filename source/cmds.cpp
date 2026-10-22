#include <iostream>
#include<direct.h>
#include<map>
#include "header/cmds.hpp"
#include "header/subs.hpp"
using namespace std;
using namespace subfunc;

typedef map<string, string>::const_iterator MapIt;

repo::repo() : initialized_(false) {
}
void repo::save(){
    writeFile((REPO_DIR + "/HEAD").c_str(), currentBranch_);
    string names;
    for (MapIt it = branches_.begin(); it != branches_.end(); ++it) {
        writeFile((REPO_DIR + "/refs/" + it->first).c_str(), it->second);
        names += it->first + "\n";
    }
    writeFile((REPO_DIR + "/BRANCHES").c_str(), names);
    string index;
    for (MapIt it = staging_.begin(); it != staging_.end(); ++it)
        index += it->first + "\t" + it->second + "\n";
    writeFile((REPO_DIR + "/INDEX").c_str(), index);
}

void repo::init(){
    if(pathExists(REPO_DIR)){
        cout<<"folder already exists";
    }
    else{
    if (_mkdir(REPO_DIR.c_str()) == 0) {
        cout << "Folder created successfully!\n";
        initialized_=true;
    }
    else {
        cerr << "Error creating folder.\n";
    }
    }
}
void repo::add(std::string filename){
    string content;
    if(!readFile(filename,content)){
        cout<<"Cannot read file:"<<filename<<endl;
        return;
    }
    string hash=hashString(content);
    writeFile((REPO_DIR + "/object/"+hash).c_str(),content);
    staging_[filename]=hash;
    save();
    cout << "Staged " << filename << " (" << hash.substr(0, 7) << ")"<<endl;
}
