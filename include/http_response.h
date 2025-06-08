#pragma once
#include <map>
#include <sstream>
#include <string>
class HttpResponse {
public:
  std::string version;
  int status_code;
  std::string reason;
  std::map<std::string, std::string> headers;
  std::string body;

  // returns a string representation of the response
  std::string ToString() const;

  // returns a response with a 404 status code
  // and a body of "404 Not Found"
  static HttpResponse Stock404();

  // returns a response with a 500 status code
  // and a body of "500 Internal Server Error"
  static HttpResponse Stock500();

  // returns a response with a 400 status code
  // and a body of "400 Bad Request"
  static HttpResponse Stock400();
};
