#include "sleep_handler.h"
#include "registry.h"
#include "logger.h"

std::unique_ptr<HttpResponse> SleepHandler::handle_request(const HttpRequest& req) {
  auto start = std::chrono::steady_clock::now();
  
  BOOST_LOG_TRIVIAL(info) << "SleepHandler: Sleeping for " << sleep_seconds_ << " seconds";
  std::this_thread::sleep_for(std::chrono::seconds(sleep_seconds_));
  
  auto resp = std::make_unique<HttpResponse>();
  resp->version = "HTTP/1.1";
  resp->status_code = 200;
  resp->reason = "OK";
  resp->headers["Content-Type"] = "text/plain";
  
  auto end = std::chrono::steady_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
  resp->body = "Slept for " + std::to_string(duration.count()) + " ms";
  
  return resp;
}

std::unique_ptr<RequestHandler> CreateSleepHandler(
    const std::string& path,
    const std::unordered_map<std::string, std::string>& args) {
  int sleep_seconds = 1; // default
  if (args.count("sleep_seconds")) {
    sleep_seconds = std::stoi(args.at("sleep_seconds"));
  }
  return std::make_unique<SleepHandler>(path, sleep_seconds);
}

REGISTER_HANDLER(SleepHandler, CreateSleepHandler);

void TouchSleepHandler() {}
