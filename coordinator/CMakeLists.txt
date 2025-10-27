cmake_minimum_required(VERSION 3.15)

# Sources for coordinator
set(COORDINATOR_SOURCES
    src/main.cpp
    src/CoordinatorServer.cpp
    src/DBManager.cpp
)

add_executable(coordinator ${COORDINATOR_SOURCES})

find_package(PostgreSQL REQUIRED)

find_package(Poco REQUIRED COMPONENTS
    Foundation
    Util
    Net
    NetSSL
    JSON
    XML
    Data
    DataPostgreSQL
)

target_link_libraries(coordinator PRIVATE
    Poco::Foundation
    Poco::Util
    Poco::Net
    Poco::NetSSL
    Poco::JSON
    Poco::XML
    Poco::Data
    Poco::DataPostgreSQL
)

add_executable(test_connection src/test_connection.cpp)
target_link_libraries(test_connection PRIVATE Poco::Data Poco::DataPostgreSQL Poco::Foundation)
