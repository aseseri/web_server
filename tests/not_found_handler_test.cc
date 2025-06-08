#include "not_found_handler.h"
#include "http_request.h"
#include "http_response.h"
#include "gtest/gtest.h"

// test fixture
class NotFoundHandlerTest : public ::testing::Test {
  protected:
  NotFoundHandler h;
  HttpRequest req;
};

TEST_F(NotFoundHandlerTest, HandleRequestReturns404) {
  req.path = "/no-such-path";
  auto resp = std::make_unique<HttpResponse>();
  // prefill resp to ensure HandleRequest overwrites it
  resp->version = "bad";
  resp->status_code = 123;
  resp->reason = "Bad";
  resp->headers["X-Test"] = "value";
  resp->body = "old";

  resp = h.handle_request(req);

  // Should be a standard 404
  EXPECT_EQ(resp->version, "HTTP/1.1");
  EXPECT_EQ(resp->status_code, 404);
  EXPECT_EQ(resp->reason, "Not Found");
  // Stock404 sets Content-Type and body
  auto it = resp->headers.find("Content-Type");
  ASSERT_NE(it, resp->headers.end());
  EXPECT_EQ(it->second, "text/plain");
  EXPECT_EQ(resp->body, "404 Not Found");
}
