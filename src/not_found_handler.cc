#include "not_found_handler.h"
#include "registry.h"

std::unique_ptr<HttpResponse> NotFoundHandler::handle_request(const HttpRequest& req) {
    return std::make_unique<HttpResponse>(HttpResponse::Stock404());
}
