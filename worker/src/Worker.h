#pragma once
#include <string>

class Worker {
public:
    Worker(const std::string &coordUrl, const std::string &name);
    void runLoop();

private:
    std::string _coordUrl;
    std::string _name;

    void registerSelf();
    bool requestAndExecute();
    void reportCompletion(int taskId, const std::string &status);
};
