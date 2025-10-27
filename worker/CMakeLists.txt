cmake_minimum_required(VERSION 3.15)

set(WORKER_SOURCES
    src/main.cpp
    src/Worker.cpp
)

add_executable(worker ${WORKER_SOURCES})

find_package(Poco REQUIRED COMPONENTS
    Foundation
    Util
    Net
    NetSSL
    JSON
    XML
)

target_link_libraries(worker PRIVATE
    Poco::Foundation
    Poco::Util
    Poco::Net
    Poco::NetSSL
    Poco::JSON
    Poco::XML
)
