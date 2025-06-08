#pragma once
#include "http_request.h"
#include "http_response.h"
#include <memory>
#include <string>

struct RequestHandler {
  virtual ~RequestHandler() = default;
  virtual std::unique_ptr<HttpResponse> handle_request(const HttpRequest& req) = 0;
};
using RequestHandlerPtr = std::shared_ptr<RequestHandler>;
