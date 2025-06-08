#pragma once
#include "mime_types.h"
#include "request_handler.h"
#include <fstream>

class StaticHandler : public RequestHandler {
public:
  StaticHandler(const std::string& path, const std::string& root_dir)
    : prefix_(std::move(path)), root_(std::move(root_dir)) {}
  std::unique_ptr<HttpResponse> handle_request(const HttpRequest& req) override;

private:
  std::string prefix_, root_;
};
