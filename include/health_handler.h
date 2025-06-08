#pragma once
#include "request_handler.h"

class HealthHandler : public RequestHandler {
public:
    std::unique_ptr<HttpResponse> handle_request(const HttpRequest& req) override;
}; 