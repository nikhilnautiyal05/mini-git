#pragma once
static std::string REPO_DIR="D:\\.minigit";
class repo{
    private:
    bool initialized_;
    public:
    void init();
    bool isInitialized(){return initialized_;}
};