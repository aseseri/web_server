#include "http_request.h"
#include "http_response.h"
#include "gtest/gtest.h"


class HttpRequestTest : public ::testing::Test { 
};
class HttpResponseTest : public ::testing::Test {
};
// Test that HttpRequest::Parse correctly parses a request line
TEST_F(HttpRequestTest, ParseRequestLineAndHeadersAndBody) {
  std::string raw =
    "POST /path/resource?x=1 HTTP/1.1\r\n"
    "Host: localhost\r\n"
    "X-Test:  42\r\n"
    "\r\n"
    "BODY_DATA";
  HttpRequest req = HttpRequest::Parse(raw);

  EXPECT_EQ(req.method, "POST");
  EXPECT_EQ(req.path, "/path/resource?x=1");
  EXPECT_EQ(req.version, "HTTP/1.1");
  EXPECT_EQ(req.headers.size(), 2u);
  EXPECT_EQ(req.headers["Host"], "localhost");
  EXPECT_EQ(req.headers["X-Test"], "42");
  EXPECT_EQ(req.body, "BODY_DATA");
  EXPECT_EQ(req.raw, raw);
}

// Test that HttpRequest::Parse correctly handles a request with no body
TEST_F(HttpResponseTest, ToStringFormatsHeadersAndBody) {
  HttpResponse r;
  r.version = "HTTP/1.1";
  r.status_code = 201;
  r.reason = "Created";
  r.headers["X-Foo"] = "Bar";
  r.body = "OK";
  std::string out = r.ToString();

  // Should start with status line
  EXPECT_NE(out.find("HTTP/1.1 201 Created\r\n"), std::string::npos);
  // Contains our header
  EXPECT_NE(out.find("X-Foo: Bar\r\n"), std::string::npos);
  // Contains Content-Length
  EXPECT_NE(out.find("Content-Length: 2\r\n"), std::string::npos);
  // Blank line then body
  EXPECT_NE(out.find("\r\n\r\nOK"), std::string::npos);
}

// Test that HttpResponse::Stock404 creates a valid 404 response
TEST_F(HttpResponseTest, Stock404HasBodyAndHeaders) {
  HttpResponse r = HttpResponse::Stock404();
  EXPECT_EQ(r.version, "HTTP/1.1");
  EXPECT_EQ(r.status_code, 404);
  EXPECT_EQ(r.reason, "Not Found");
  EXPECT_EQ(r.headers["Content-Type"], "text/plain");
  // ToString should include 404 body
  std::string out = r.ToString();
  EXPECT_NE(out.find("404 Not Found"), std::string::npos);
  EXPECT_NE(out.find("Content-Length: 13"), std::string::npos);
}