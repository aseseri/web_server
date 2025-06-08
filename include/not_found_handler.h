#pragma once
#include "request_handler.h"
#include "http_response.h"

class NotFoundHandler : public RequestHandler {
public:
  // always returns a 404 response
  NotFoundHandler() = default;
  std::unique_ptr<HttpResponse> handle_request(const HttpRequest& req) override;
};
