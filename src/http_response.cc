#include "http_response.h"

std::string HttpResponse::ToString() const {
  std::ostringstream oss;
  oss << version << " " << status_code << " " << reason << "\r\n";

  // first print any user‐supplied headers
  for (auto &h : headers) {
    oss << h.first << ": " << h.second << "\r\n";
  }

  // then always send a Content‐Length
  oss << "Content-Length: " << body.size() << "\r\n";

  // end of headers
  oss << "\r\n";

  // body (for 404 this is "404 Not Found")
  oss << body;
  return oss.str();
}

// returns a response with a 404 status code
// and a body of "404 Not Found"
HttpResponse HttpResponse::Stock404() {
  HttpResponse r;
  r.version = "HTTP/1.1";
  r.status_code = 404;
  r.reason = "Not Found";
  r.headers["Content-Type"] = "text/plain";
  r.body = "404 Not Found";
  return r;
}

// returns a response with a 500 status code
// and a body of "500 Internal Server Error"
HttpResponse HttpResponse::Stock500() {
  HttpResponse r;
  r.version = "HTTP/1.1";
  r.status_code = 500;
  r.reason = "Internal Server Error";
  r.headers["Content-Type"] = "text/plain";
  r.body = "500 Internal Server Error";
  return r;
}

// returns a response with a 400 status code
// and a body of "400 Bad Request"
HttpResponse HttpResponse::Stock400() {
  HttpResponse r;
  r.version = "HTTP/1.1";
  r.status_code = 400;
  r.reason = "Bad Request";
  r.headers["Content-Type"] = "text/plain";
  r.body = "400 Bad Request";
  return r;
}
