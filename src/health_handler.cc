#include "health_handler.h"
#include "registry.h"
#include "logger.h"

std::unique_ptr<HttpResponse> HealthHandler::handle_request(const HttpRequest& req) {
    auto resp = std::make_unique<HttpResponse>();
    resp->version = "HTTP/1.1";
    resp->status_code = 200;
    resp->reason = "OK";
    resp->headers["Content-Type"] = "text/plain";
    resp->body = "OK";
    return resp;
}

std::unique_ptr<RequestHandler> CreateHealthHandler(const std::string& path,
    const std::unordered_map<std::string, std::string>& args) {
    BOOST_LOG_TRIVIAL(info) << "Creating HealthHandler for path: " << path;
    return std::make_unique<HealthHandler>();
}

REGISTER_HANDLER(HealthHandler, CreateHealthHandler);

void TouchHealthHandler() {
    BOOST_LOG_TRIVIAL(info) << "Touching HealthHandler";
} 