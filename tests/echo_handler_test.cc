#include "echo_handler.h"
#include "http_request.h"
#include "http_response.h"
#include "gtest/gtest.h"

// test fixture for EchoHandler
class EchoHandlerTest : public ::testing::Test {
protected:
    static HttpRequest make_request(const std::string& raw) {
        HttpRequest r;
        r.raw = raw;
        r.method = "GET";
        r.path = "/echo/foo";
        r.version = "HTTP/1.1";
        return r;
      }
      EchoHandler h{"/echo"};
};

// expects the response to be 200 OK
// and the body to be the raw request
TEST_F(EchoHandlerTest, HandlesRequestEchoesRaw) {
  std::string raw = "GET /echo/bar HTTP/1.1\r\nHost: example\r\n\r\nBODY";
  HttpRequest req = make_request(raw);
  req.raw = raw;
  auto resp = h.handle_request(req);

  EXPECT_EQ(resp->version, "HTTP/1.1");
  EXPECT_EQ(resp->status_code, 200);
  EXPECT_EQ(resp->reason, "OK");
  auto it = resp->headers.find("Content-Type");
  ASSERT_NE(it, resp->headers.end());
  EXPECT_EQ(it->second, "text/plain");
  EXPECT_EQ(resp->body, raw);
}

TEST_F(EchoHandlerTest, HandlesEmptyRequestGracefully) {
  HttpRequest empty_req;
  auto resp = h.handle_request(empty_req);

  EXPECT_EQ(resp->status_code, 200);
  EXPECT_TRUE(resp->body.empty());
}
