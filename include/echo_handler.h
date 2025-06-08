#pragma once
#include "request_handler.h"

// This handler echoes back the request in the response body.
class EchoHandler : public RequestHandler {
public:
  EchoHandler(const std::string& path) : path_(std::move(path)) {}
  std::unique_ptr<HttpResponse> handle_request(const HttpRequest& req) override;

private:
  std::string path_;
};
