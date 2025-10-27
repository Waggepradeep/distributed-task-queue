#include "DBManager.h"
#include <Poco/Data/SessionFactory.h>
#include <Poco/Data/PostgreSQL/Connector.h>
#include <Poco/Data/Statement.h>
#include <Poco/Data/RecordSet.h>
#include <Poco/Data/Transaction.h>
#include <Poco/ScopedLock.h>
#include <Poco/Mutex.h>
#include <iostream>

using namespace Poco::Data::Keywords;
using Poco::Data::Statement;

DBManager::DBManager(const std::string &connStr)
{
    try
    {
        // Register PostgreSQL connector once (thread-safe)
        Poco::Data::PostgreSQL::Connector::registerConnector();

        // Print the connection string you're using
        std::cout << "Connecting with string: [" << connStr << "]" << std::endl;

        _session = std::make_unique<Poco::Data::Session>(
            Poco::Data::SessionFactory::instance().create("PostgreSQL", connStr));

        std::cout << "✅ DBManager connected successfully!" << std::endl;
    }
    catch (const Poco::Exception &ex)
    {
        std::cerr << "❌ POCO Exception in DBManager: " << ex.displayText() << std::endl;
        throw;
    }
    catch (const std::exception &ex)
    {
        std::cerr << "❌ STD Exception in DBManager: " << ex.what() << std::endl;
        throw;
    }
}
DBManager::~DBManager() = default;

std::vector<Task> DBManager::fetchPendingTasks(int limit)
{
    Poco::ScopedLock<Poco::Mutex> lock(_mutex);
    std::vector<Task> out;

    try
    {
        std::ostringstream query;
        query << "SELECT id, payload, status, COALESCE(worker_id, '') "
              << "FROM tasks WHERE status='pending' "
              << "ORDER BY created_at LIMIT " << limit;

        // Use vector-based extraction to avoid RecordSet dynamic-cast issues
        Poco::Data::Statement stmt(*_session);

        std::vector<int> ids;
        std::vector<std::string> payloads;
        std::vector<std::string> statuses;
        std::vector<std::string> workers;

        stmt << query.str(),
            Poco::Data::Keywords::into(ids),
            Poco::Data::Keywords::into(payloads),
            Poco::Data::Keywords::into(statuses),
            Poco::Data::Keywords::into(workers),
            now;

    const std::size_t rows = std::min({ids.size(), payloads.size(), statuses.size(), workers.size()});
    for (std::size_t i = 0; i < rows; ++i)
        {
            Task t;
            t.id = ids[i];
            t.payload = payloads[i];
            t.status = statuses[i];
            t.worker_id = workers[i];

            std::cout << "[fetchPendingTasks] id=" << t.id
                      << ", status=" << t.status
                      << ", worker_id=" << t.worker_id << std::endl;

            out.push_back(std::move(t));
        }
    }
    catch (const Poco::Exception& e)
    {
        std::cerr << "fetchPendingTasks error: " << e.displayText() << std::endl;
    }
    catch (const std::exception& e)
    {
        std::cerr << "fetchPendingTasks std error: " << e.what() << std::endl;
    }

    return out;
}

bool DBManager::assignTaskToWorker(int taskId, const std::string &workerId)
{
    Poco::ScopedLock<Poco::Mutex> lock(_mutex);

    try
    {
        // Build a safe-ish SQL string (escape single quotes in workerId)
        auto escape = [](const std::string &s){
            std::string out; out.reserve(s.size()*2);
            for (char c: s) {
                if (c == '\'') {
                    // double single quotes for SQL literal
                    out.push_back('\'');
                    out.push_back('\'');
                }
                else {
                    out.push_back(c);
                }
            }
            return out;
        };

        std::string w = escape(workerId);
        std::ostringstream sql;
        sql << "UPDATE tasks SET status='in_progress', worker_id='" << w
            << "', updated_at=now() WHERE id=" << taskId << " AND status='pending'";

    Statement stmt(*_session);
    std::string sqlStr = sql.str();
    std::cout << "[assignTaskToWorker] SQL: " << sqlStr << std::endl;
    stmt << sqlStr, now;

    std::size_t affected = stmt.execute();
    std::cout << "[assignTaskToWorker] affected=" << affected << std::endl;
    return affected > 0;
    }
    catch (const Poco::Exception &e)
    {
        std::cerr << "assignTaskToWorker POCO error: " << e.displayText() << std::endl;
        return false;
    }
    catch (const std::exception &e)
    {
        std::cerr << "assignTaskToWorker error: " << e.what() << std::endl;
        return false;
    }
}

bool DBManager::updateTaskStatus(int taskId, const std::string &newStatus)
{
    Poco::ScopedLock<Poco::Mutex> lock(_mutex);

    try
    {
        auto escape = [](const std::string &s){
            std::string out; out.reserve(s.size()*2);
            for (char c: s) {
                if (c == '\'') {
                    // double single quotes for SQL literal
                    out.push_back('\'');
                    out.push_back('\'');
                }
                else {
                    out.push_back(c);
                }
            }
            return out;
        };

        std::string s = escape(newStatus);
        std::ostringstream sql;
        sql << "UPDATE tasks SET status='" << s << "', updated_at=now() WHERE id=" << taskId;

    Statement stmt(*_session);
    std::string sqlStr = sql.str();
    std::cout << "[updateTaskStatus] SQL: " << sqlStr << std::endl;
    stmt << sqlStr, now;

    std::size_t affected = stmt.execute();
    std::cout << "[updateTaskStatus] affected=" << affected << std::endl;
    return affected > 0;
    }
    catch (const Poco::Exception &e)
    {
        std::cerr << "updateTaskStatus POCO error: " << e.displayText() << std::endl;
        return false;
    }
    catch (const std::exception &e)
    {
        std::cerr << "updateTaskStatus error: " << e.what() << std::endl;
        return false;
    }
}

std::optional<Task> DBManager::fetchAndAssignTask(const std::string &workerId)
{
    Poco::ScopedLock<Poco::Mutex> lock(_mutex);

    try
    {
        // Use a single UPDATE ... RETURNING statement to atomically claim one pending task.
        // This avoids syntax/order issues with FOR UPDATE placement and is a single-statement atomic claim.

        // escape single quotes in workerId
        auto escape = [](const std::string &s){ std::string out; out.reserve(s.size()*2); for (char c: s) { if (c == '\'') { out.push_back('\''); out.push_back('\''); } else out.push_back(c); } return out; };
        std::string w = escape(workerId);

        std::ostringstream sql;
        sql << "WITH candidate AS (SELECT id FROM tasks WHERE status='pending' ORDER BY created_at LIMIT 1) "
            << "UPDATE tasks t SET status='in_progress', worker_id='" << w << "', updated_at=now() "
            << "FROM candidate c WHERE t.id = c.id AND t.status='pending' "
            << "RETURNING t.id, t.payload, t.status, COALESCE(t.worker_id, '');";

        Poco::Data::Statement stmt(*_session);

        std::vector<int> ids;
        std::vector<std::string> payloads;
        std::vector<std::string> statuses;
        std::vector<std::string> workers;

        stmt << sql.str(),
            Poco::Data::Keywords::into(ids),
            Poco::Data::Keywords::into(payloads),
            Poco::Data::Keywords::into(statuses),
            Poco::Data::Keywords::into(workers),
            now;

        if (ids.empty())
        {
            // no task claimed
            return std::nullopt;
        }

        Task t;
        t.id = ids[0];
        t.payload = payloads[0];
        t.status = statuses[0];
        // Return the clean workerId (not the escaped version)
        t.worker_id = workerId;

        std::cout << "[fetchAndAssignTask] claimed id=" << t.id << " for worker=" << workerId << std::endl;
        return t;
    }
    catch (const Poco::Exception &e)
    {
        std::cerr << "fetchAndAssignTask POCO error: " << e.displayText() << std::endl;
        return std::nullopt;
    }
    catch (const std::exception &e)
    {
        std::cerr << "fetchAndAssignTask error: " << e.what() << std::endl;
        return std::nullopt;
    }
}
