#include <Poco/Data/SessionFactory.h>
#include <Poco/Data/PostgreSQL/Connector.h>
#include <Poco/Exception.h>
#include <iostream>
#include <cstdlib>

int main()
{
    // Register PostgreSQL Connector
    Poco::Data::PostgreSQL::Connector::registerConnector();

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

    try
    {
        // Create a session to verify the connection
        auto session = Poco::Data::SessionFactory::instance().create("PostgreSQL", connStr);

        if (session.isConnected())
        {
            std::cout << "✅ Database connected successfully!" << std::endl;
        }
        else
        {
            std::cerr << "⚠️ Database session could not be established!" << std::endl;
        }
    }
    catch (const Poco::Exception &ex)
    {
        std::cerr << "❌ POCO Exception: " << ex.displayText() << std::endl;
    }
    catch (const std::exception &ex)
    {
        std::cerr << "❌ STD Exception: " << ex.what() << std::endl;
    }
    catch (...)
    {
        std::cerr << "❌ Unknown Exception occurred!" << std::endl;
    }

    return 0;
}
