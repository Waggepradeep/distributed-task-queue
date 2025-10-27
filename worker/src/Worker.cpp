#include "Worker.h"
#include <Poco/Net/HTTPClientSession.h>
#include <Poco/Net/HTTPRequest.h>
#include <Poco/Net/HTTPResponse.h>
#include <Poco/URI.h>
#include <Poco/StreamCopier.h>
#include <Poco/JSON/Object.h>
#include <Poco/JSON/Parser.h>
#include <Poco/JSON/Stringifier.h>
#include <Poco/Dynamic/Var.h>
#include <iostream>
#include <thread>
#include <chrono>
#include <sstream>

using namespace Poco::Net;
using namespace Poco::JSON;
using namespace Poco;
using Poco::Dynamic::Var;

Worker::Worker(const std::string &coordUrl, const std::string &name)
: _coordUrl(coordUrl), _name(name) {}

void Worker::registerSelf() {
    try {
        URI uri(_coordUrl + "/register");
        HTTPClientSession session(uri.getHost(), uri.getPort());
        HTTPRequest req(HTTPRequest::HTTP_POST, uri.getPathAndQuery(), HTTPMessage::HTTP_1_1);

        Object obj;
        obj.set("id", _name);
        obj.set("meta", "{\"capabilities\":[]}"); // stringified metadata

        std::ostringstream oss;
        Stringifier::stringify(obj, oss);
        std::string body = oss.str();

        req.setContentType("application/json");
        req.setContentLength(body.size());

        std::ostream &os = session.sendRequest(req);
        os << body;

        HTTPResponse res;
        std::istream &is = session.receiveResponse(res);
        std::string respBody;
        StreamCopier::copyToString(is, respBody);

        std::cout << "[worker] register response: " << res.getStatus() << " body=" << respBody << std::endl;
    } catch (const std::exception &e) {
        std::cerr << "[worker] registerSelf error: " << e.what() << std::endl;
    }
}

bool Worker::requestAndExecute() {
    try {
        URI uri(_coordUrl + "/request_task");
        HTTPClientSession session(uri.getHost(), uri.getPort());
        HTTPRequest req(HTTPRequest::HTTP_POST, uri.getPathAndQuery(), HTTPMessage::HTTP_1_1);

        Object obj;
        obj.set("id", _name);
        std::ostringstream oss;
        Stringifier::stringify(obj, oss);
        std::string body = oss.str();

        req.setContentType("application/json");
        req.setContentLength(body.size());
        std::ostream &os = session.sendRequest(req);
        os << body;

        HTTPResponse res;
        std::istream &is = session.receiveResponse(res);

        if (res.getStatus() == HTTPResponse::HTTP_NO_CONTENT) {
            // no task available
            std::this_thread::sleep_for(std::chrono::seconds(2));
            return true; // keep polling
        }

        if (res.getStatus() != HTTPResponse::HTTP_OK) {
            std::string resp;
            StreamCopier::copyToString(is, resp);
            std::cerr << "[worker] request_task error status=" << res.getStatus() << " body=" << resp << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(2));
            return true;
        }

        std::string respBody;
        StreamCopier::copyToString(is, respBody);
        Parser parser;
        Var parsed = parser.parse(respBody);
        Object::Ptr pObj = parsed.extract<Object::Ptr>();

        int taskId = pObj->getValue<int>("id");
        std::string payload = pObj->getValue<std::string>("payload");

        std::cout << "[worker] got task id=" << taskId << " payload=" << payload << std::endl;

        // Execute task (demo: simulate work)
        // NOTE: DO NOT call system() on untrusted payloads in production.
        std::this_thread::sleep_for(std::chrono::seconds(1));

        reportCompletion(taskId, "done");
        return true;
    } catch (const std::exception &e) {
        std::cerr << "[worker] requestAndExecute exception: " << e.what() << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(3));
        return true;
    }
}

void Worker::reportCompletion(int taskId, const std::string &status) {
    try {
        URI uri(_coordUrl + "/complete_task");
        HTTPClientSession session(uri.getHost(), uri.getPort());
        HTTPRequest req(HTTPRequest::HTTP_POST, uri.getPathAndQuery(), HTTPMessage::HTTP_1_1);

        Object obj;
        obj.set("task_id", taskId);
        obj.set("status", status);

        std::ostringstream oss;
        Stringifier::stringify(obj, oss);
        std::string body = oss.str();

        req.setContentType("application/json");
        req.setContentLength(body.size());

        std::ostream &os = session.sendRequest(req);
        os << body;

        HTTPResponse res;
        std::istream &is = session.receiveResponse(res);
        std::string resp;
        StreamCopier::copyToString(is, resp);
        std::cout << "[worker] reportCompletion response: " << res.getStatus() << " body=" << resp << std::endl;
    } catch (const std::exception &e) {
        std::cerr << "[worker] reportCompletion error: " << e.what() << std::endl;
    }
}

void Worker::runLoop() {
    registerSelf();
    while (true) {
        if (!requestAndExecute()) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}
