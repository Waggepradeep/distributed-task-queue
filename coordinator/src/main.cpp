#include "CoordinatorServer.h"
#include "DBManager.h"
#include <iostream>
#include <csignal>
#include <atomic>
#include <future>
#include <cstdlib>

using namespace std;

static atomic<bool> g_stopRequested{false};
static promise<void> g_exitPromise;

void signalHandler(int)
{
    g_stopRequested = true;
    g_exitPromise.set_value();
}

int main()
{
    try
    {
        // Get database credentials from environment variables
        const char* dbHost = std::getenv("DB_HOST");
        const char* dbPort = std::getenv("DB_PORT");
        const char* dbName = std::getenv("DB_NAME");
        const char* dbUser = std::getenv("DB_USER");
        const char* dbPassword = std::getenv("DB_PASSWORD");
        
        // Use defaults if environment variables are not set
        std::string host = dbHost ? dbHost : "localhost";
        std::string port = dbPort ? dbPort : "5433";
        std::string name = dbName ? dbName : "distributed_task_queue";
        std::string user = dbUser ? dbUser : "yugabyte";
        std::string password = dbPassword ? dbPassword : "yugabyte";
        
        std::string connStr = "host=" + host + " port=" + port + " user=" + user + " password=" + password + " dbname=" + name;
        auto dbManager = std::make_shared<DBManager>(connStr);

        CoordinatorServer server(8080, dbManager);
        server.start();

        cout << "Coordinator is running..." << endl;

        signal(SIGINT, signalHandler);
        g_exitPromise.get_future().wait();
        server.stop();
    }
    catch (const std::exception& e)
    {
        cerr << "Fatal error starting coordinator: " << e.what() << endl;
        return 1;
    }

    return 0;
}
