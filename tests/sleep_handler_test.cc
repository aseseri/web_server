#include "sleep_handler.h"
#include "http_request.h"
#include "http_response.h"
#include "gtest/gtest.h"

class SleepHandlerTest : public ::testing::Test {
protected:
  // No special setup needed for these tests
};

TEST_F(SleepHandlerTest, SleepOneSecond) {
  SleepHandler handler("/sleep", 1);
  HttpRequest req;
  auto resp = handler.handle_request(req);

  EXPECT_EQ(resp->status_code, 200);
  EXPECT_EQ(resp->reason, "OK");
  EXPECT_EQ(resp->headers["Content-Type"], "text/plain");

  // Parse the duration from the response body
  std::string body = resp->body;
  size_t prefix_len = std::string("Slept for ").length();
  size_t suffix_pos = body.find(" ms");
  ASSERT_NE(suffix_pos, std::string::npos) << "Response body format incorrect";

  std::string ms_str = body.substr(prefix_len, suffix_pos - prefix_len);
  int ms = 0;
  ASSERT_NO_THROW({
    ms = std::stoi(ms_str);
  }) << "Failed to convert duration to integer";

  EXPECT_GE(ms, 1000) << "Sleep duration should be at least 1000 ms";
}

TEST_F(SleepHandlerTest, SleepZeroSeconds) {
  SleepHandler handler("/sleep", 0);
  HttpRequest req;
  auto resp = handler.handle_request(req);

  EXPECT_EQ(resp->status_code, 200);
  EXPECT_EQ(resp->reason, "OK");
  EXPECT_EQ(resp->headers["Content-Type"], "text/plain");

  std::string body = resp->body;
  size_t prefix_len = std::string("Slept for ").length();
  size_t suffix_pos = body.find(" ms");
  ASSERT_NE(suffix_pos, std::string::npos) << "Response body format incorrect";

  std::string ms_str = body.substr(prefix_len, suffix_pos - prefix_len);
  int ms = 0;
  ASSERT_NO_THROW({
    ms = std::stoi(ms_str);
  }) << "Failed to convert duration to integer";

  EXPECT_GE(ms, 0) << "Sleep duration should be non-negative";
}