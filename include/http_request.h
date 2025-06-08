#pragma once
#include <map>
#include <string>
class HttpRequest {
public:
  // HTTP request line: METHOD SP PATH SP VERSION CRLF
  // e.g. GET /index.html HTTP/1.1
  std::string method, path, version;
  std::map<std::string, std::string> headers;
  std::string body;
  std::string raw; // entire request for echo

  // Parse the raw HTTP request string into an HttpRequest object
  static HttpRequest Parse(const std::string &raw_req);

  // Check if the request is malformed
  bool is_malformed() const {
    return method.empty() || path.empty() || version.empty();
  }
};
