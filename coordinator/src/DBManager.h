#pragma once
#include <Poco/Data/Session.h>
#include <Poco/Data/SessionFactory.h>
#include <Poco/Data/PostgreSQL/Connector.h>
#include <Poco/Data/Statement.h>
#include <Poco/Data/RecordSet.h>
#include <Poco/Mutex.h>
#include <memory>
#include <vector>
#include <string>
#include <optional>

struct Task
{
    int id;
    std::string payload;
    std::string status;
    std::string worker_id;
};

class DBManager
{
public:
    DBManager(const std::string& connStr);
    ~DBManager();

    std::vector<Task> fetchPendingTasks(int limit);
    bool assignTaskToWorker(int taskId, const std::string& workerId);
    bool updateTaskStatus(int taskId, const std::string& newStatus);
    // Atomically fetch a single pending task and assign it to workerId.
    // Returns a Task if one was assigned, or std::nullopt if none available.
    std::optional<Task> fetchAndAssignTask(const std::string& workerId);

private:
    std::unique_ptr<Poco::Data::Session> _session; // Session pointer
    Poco::Mutex _mutex;                            // Thread safety
};
