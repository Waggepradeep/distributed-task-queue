cmake_minimum_required(VERSION 3.15)
project(distributed-task-queue)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Ensure vcpkg toolchain can inject correct find_package logic
set(CMAKE_TOOLCHAIN_FILE "C:/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")

# Add subdirectories for the two components
add_subdirectory(coordinator)
add_subdirectory(worker)
