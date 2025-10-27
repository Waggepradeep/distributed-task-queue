#include "CoordinatorServer.h"
#include "DBManager.h"

#include <Poco/Net/ServerSocket.h>
#include <Poco/Net/HTTPServer.h>
#include <Poco/Net/HTTPServerRequest.h>
#include <Poco/Net/HTTPServerResponse.h>
#include <Poco/Net/HTTPRequestHandlerFactory.h>
#include <Poco/Net/HTTPRequestHandler.h>
#include <Poco/Net/HTTPServerParams.h>
#include <Poco/Util/ServerApplication.h>

#include <Poco/JSON/Object.h>
#include <Poco/JSON/Parser.h>
#include <Poco/JSON/Stringifier.h>
#include <Poco/Dynamic/Var.h>
#include <iostream>

using namespace Poco::Net;
using namespace Poco::Util;
using namespace Poco::JSON;
using Poco::Dynamic::Var;

// ==========================
// REGISTER HANDLER
// ==========================
class RegisterHandler : public HTTPRequestHandler {
public:
    explicit RegisterHandler(std::shared_ptr<DBManager> db) : _db(std::move(db)) {}

    void handleRequest(HTTPServerRequest& request, HTTPServerResponse& response) override {
        response.setStatus(HTTPResponse::HTTP_OK);
        response.setContentType("application/json");

        try {
            std::istream& is = request.stream();
            std::string body((std::istreambuf_iterator<char>(is)), std::istreambuf_iterator<char>());

            Parser parser;
            Var result = parser.parse(body);
            Object::Ptr obj = result.extract<Object::Ptr>();
            std::string workerId = obj->getValue<std::string>("id");

            std::cout << "[coordinator] Worker registered: " << workerId << std::endl;

            Object::Ptr json = new Object();
            json->set("status", "ok");
            json->set("message", "Worker registered successfully");

            std::ostream& ostr = response.send();
            Stringifier::stringify(json, ostr);
        }
        catch (const std::exception& e) {
            std::cerr << "[coordinator] RegisterHandler error: " << e.what() << std::endl;
        }
    }

private:
    std::shared_ptr<DBManager> _db;
};

// ==========================
// REQUEST TASK HANDLER
// ==========================
class RequestTaskHandler : public HTTPRequestHandler {
public:
    explicit RequestTaskHandler(std::shared_ptr<DBManager> db) : _db(std::move(db)) {}

    void handleRequest(HTTPServerRequest& request, HTTPServerResponse& response) override {
        // Parse worker id from request body
        try {
            std::istream& is = request.stream();
            std::string body((std::istreambuf_iterator<char>(is)), std::istreambuf_iterator<char>());
            Parser parser;
            Var result = parser.parse(body);
            Object::Ptr obj = result.extract<Object::Ptr>();
            std::string workerId = obj->getValue<std::string>("id");

            auto assigned = _db->fetchAndAssignTask(workerId);
            if (!assigned.has_value()) {
                // No task available
                response.setStatus(HTTPResponse::HTTP_NO_CONTENT);
                response.send();
                return;
            }

            Task t = assigned.value();
            Object::Ptr json = new Object();
            json->set("id", t.id);
            json->set("payload", t.payload);

            response.setStatus(HTTPResponse::HTTP_OK);
            response.setContentType("application/json");
            std::ostream& ostr = response.send();
            Stringifier::stringify(json, ostr);

            std::cout << "[coordinator] Assigned task id=" << t.id << " to worker=" << workerId << std::endl;
            return;
        }
        catch (const std::exception& e) {
            std::cerr << "[coordinator] RequestTaskHandler error: " << e.what() << std::endl;
            response.setStatus(HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
            response.setContentType("application/json");
            Object::Ptr err = new Object();
            err->set("status", "error");
            err->set("message", std::string("internal error: ") + e.what());
            std::ostream& ostr = response.send();
            Stringifier::stringify(err, ostr);
            return;
        }
}


private:
    std::shared_ptr<DBManager> _db;
};

// ==========================
// COMPLETE TASK HANDLER
// ==========================
class CompleteTaskHandler : public HTTPRequestHandler {
public:
    explicit CompleteTaskHandler(std::shared_ptr<DBManager> db) : _db(std::move(db)) {}

    void handleRequest(HTTPServerRequest& request, HTTPServerResponse& response) override {
        response.setStatus(HTTPResponse::HTTP_OK);
        response.setContentType("application/json");

        try {
            std::istream& is = request.stream();
            std::string body((std::istreambuf_iterator<char>(is)), std::istreambuf_iterator<char>());

            Parser parser;
            Var result = parser.parse(body);
            Object::Ptr obj = result.extract<Object::Ptr>();
            int taskId = obj->getValue<int>("task_id");
            std::string status = obj->getValue<std::string>("status");

            std::cout << "[coordinator] Task " << taskId << " marked as " << status << std::endl;

            bool updated = _db ? _db->updateTaskStatus(taskId, status) : false;
            Object::Ptr json = new Object();
            if (updated) {
                json->set("status", "ok");
                json->set("message", "Task completion recorded");
                response.setStatus(HTTPResponse::HTTP_OK);
            } else {
                json->set("status", "error");
                json->set("message", "failed to update task status");
                response.setStatus(HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
            }

            std::ostream& ostr = response.send();
            Stringifier::stringify(json, ostr);
        }
        catch (const std::exception& e) {
            std::cerr << "[coordinator] CompleteTaskHandler error: " << e.what() << std::endl;
        }
    }

private:
    std::shared_ptr<DBManager> _db;
};

// ==========================
// DEFAULT HANDLER
// ==========================
class DefaultHandler : public HTTPRequestHandler {
public:
    void handleRequest(HTTPServerRequest& request, HTTPServerResponse& response) override {
        response.setStatus(HTTPResponse::HTTP_OK);
        response.setContentType("application/json");

        Object::Ptr json = new Object();
        json->set("message", "Coordinator server is running");
        json->set("status", "ok");

        std::ostream& ostr = response.send();
        Stringifier::stringify(json, ostr);
    }
};

// ==========================
// HANDLER FACTORY
// ==========================
class CoordinatorRequestHandlerFactory : public HTTPRequestHandlerFactory {
public:
    explicit CoordinatorRequestHandlerFactory(std::shared_ptr<DBManager> db)
        : _db(std::move(db)) {}

    HTTPRequestHandler* createRequestHandler(const HTTPServerRequest& request) override {
        if (request.getURI() == "/register") return new RegisterHandler(_db);
        if (request.getURI() == "/request_task") return new RequestTaskHandler(_db);
        if (request.getURI() == "/complete_task") return new CompleteTaskHandler(_db);
        return new DefaultHandler();
    }

private:
    std::shared_ptr<DBManager> _db;
};

// ==========================
// COORDINATOR SERVER
// ==========================
CoordinatorServer::CoordinatorServer(unsigned short port, std::shared_ptr<DBManager> db)
    : _port(port), _db(std::move(db)), _server(nullptr)
{
    std::cout << "✅ CoordinatorServer instantiated with DB connection.\n";
}

CoordinatorServer::~CoordinatorServer() {
    stop();
}

void CoordinatorServer::start() {
    try {
        HTTPServerParams* pParams = new HTTPServerParams;
        const std::string bindAddr = "0.0.0.0"; // bind all interfaces

        ServerSocket svs(SocketAddress(bindAddr, _port));
        _server = std::make_unique<HTTPServer>(
            new CoordinatorRequestHandlerFactory(_db), svs, pParams);
        _server->start();

        std::cout << "Coordinator running on " << bindAddr << ":" << _port << std::endl;
    }
    catch (const std::exception& e) {
        std::cerr << "❌ Error starting CoordinatorServer: " << e.what() << std::endl;
    }
}

void CoordinatorServer::stop() {
    if (_server) {
        _server->stop();
        _server.reset();
        std::cout << "Coordinator stopped." << std::endl;
    }
}
