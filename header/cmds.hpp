#pragma once
static std::string REPO_DIR="D:\\.minigit";
class repo{
    private:
    bool initialized_;
    public:
    void init();
    void add(std::string filename);
    bool isInitialized(){return initialized_;}
};