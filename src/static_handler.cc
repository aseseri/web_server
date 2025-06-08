#include "static_handler.h"
#include "mime_types.h"
#include "logger.h"
#include "registry.h"
#include <fstream>
#include <sstream>
#include "not_found_handler.h"
#include <filesystem>

std::unique_ptr<HttpResponse> StaticHandler::handle_request(const HttpRequest& req) {
  // strip off the prefix and any leading '/'
  auto rel = req.path.substr(prefix_.size());
  if (!rel.empty() && rel[0] == '/')
    rel.erase(0, 1);

  // build full filesystem path
  std::string full = root_;
  if (!full.empty() && full.back() != '/')
    full.push_back('/');
  full += rel;

  // try opening
  std::ifstream ifs(full, std::ios::binary);
  if (!ifs) {
    BOOST_LOG_TRIVIAL(error) << "StaticHandler: File not found: " << full;
    return std::make_unique<HttpResponse>(HttpResponse::Stock404());
  }

  // slurp file
  std::ostringstream oss;
  oss << ifs.rdbuf();
  auto body = oss.str();

  // build response
  auto resp = std::make_unique<HttpResponse>();
  resp->version = "HTTP/1.1";
  resp->status_code = 200;
  resp->reason = "OK";
  resp->headers["Content-Type"] = MimeType::LookupByExtension(full);
  resp->body = std::move(body);

  return resp;
}

std::unique_ptr<RequestHandler> CreateStaticHandler(const std::string& path,
  const std::unordered_map<std::string, std::string>& args) {
  auto it = args.find("root");
  if(it == args.end()) {
    BOOST_LOG_TRIVIAL(error) << "StaticHandler missing 'root'";
    return std::unique_ptr<RequestHandler>(nullptr);
  }
  std::string root_dir = std::filesystem::absolute(it->second).string();
  return std::make_unique<StaticHandler>(path, root_dir);
}

REGISTER_HANDLER(StaticHandler, CreateStaticHandler);

void TouchStaticHandler() {}
