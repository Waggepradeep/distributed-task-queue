#pragma once
#include <string>
#include <chrono>


struct Task {
int id{-1};
std::string payload;
std::string status; // pending, in_progress, done, failed
std::string worker_id;
};