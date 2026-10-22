#pragma once
#include<map>
static  std::string REPO_DIR="D://.minigit";
class repo{
    private:
    bool initialized_;
    std::string currentBranch_;
    std::map<std::string,std::string>staging_;
    std::map<std::string, std::string> branches_; 
    public:
    repo();
    void init();
    void add(std::string filename);
    bool isInitialized(){return initialized_;}
    private:
    void save();
};