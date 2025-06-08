#include "echo_handler.h"
#include "logger.h"
#include "registry.h"

// fills in resp with the request details
// including the raw request string
std::unique_ptr<HttpResponse> EchoHandler::handle_request(const HttpRequest& req) {
  auto resp = std::make_unique<HttpResponse>();
  BOOST_LOG_TRIVIAL(info) << "EchoHandler: Echoed request of length " << req.raw.size();
  resp->version = "HTTP/1.1";
  resp->status_code = 200;
  resp->reason = "OK";
  resp->headers["Content-Type"] = "text/plain";
  resp->body = req.raw; // echo entire raw request

  return resp;
}

std::unique_ptr<RequestHandler> CreateEchoHandler(const std::string& path,
    const std::unordered_map<std::string, std::string>& args) {
    return std::make_unique<EchoHandler>(path);
}

REGISTER_HANDLER(EchoHandler, CreateEchoHandler);

void TouchEchoHandler() {}

