#pragma once
#include <string>
#include <memory>
#include "DBManager.h"
#include <Poco/Net/HTTPServer.h>

class CoordinatorServer {
public:
    CoordinatorServer(unsigned short port, std::shared_ptr<DBManager> db);
    ~CoordinatorServer();

    void start();
    void stop();

private:
    unsigned short _port;
    std::shared_ptr<DBManager> _db;
    std::unique_ptr<Poco::Net::HTTPServer> _server;
};
