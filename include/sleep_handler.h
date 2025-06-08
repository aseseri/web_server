#pragma once
#include "request_handler.h"
#include <chrono>
#include <thread>
#include <unordered_map>

class SleepHandler : public RequestHandler {
public:
  SleepHandler(const std::string& path, int sleep_seconds)
    : path_(path), sleep_seconds_(sleep_seconds) {}
  
  std::unique_ptr<HttpResponse> handle_request(const HttpRequest& req) override;

private:
  std::string path_;
  int sleep_seconds_;
};
