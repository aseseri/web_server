#include "session.h"
#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include <boost/asio.hpp>

using ::testing::HasSubstr;
using ::testing::StartsWith;

// A fake handler for isolated testing
class FakeHandler : public RequestHandler {
public:
  std::unique_ptr<HttpResponse> handle_request(const HttpRequest& req) override {
    auto resp = std::make_unique<HttpResponse>();
    resp->status_code = 200;
    resp->reason = "OK";
    resp->version = "HTTP/1.1";
    resp->body = "fake response";
    return resp;
  }
};

// Fixture for testing session
class SessionTest : public ::testing::Test {
protected:
  boost::asio::io_context ioc;
  std::shared_ptr<session> sut;

  // Sets up a minimal session with routes
  void SetUp() override {
    auto test_routes = std::make_shared<session::ThreadSafeRouteMap>();
    test_routes->routes["/fake"] = {
      [](const std::string&, const std::unordered_map<std::string, std::string>&) {
        return std::make_unique<FakeHandler>();
      },
      {}
    };
    sut = std::make_shared<session>(ioc, test_routes);
  }

  // Helper to simulate a client sending data
  void simulate_client_send(const std::string& data) {
    std::copy(data.begin(), data.end(), sut->data_for_test().begin());
    sut->on_read_complete({}, data.size());
  }
};

TEST_F(SessionTest, SocketInitiallyClosed) {
  EXPECT_FALSE(sut->socket().is_open());
}

TEST_F(SessionTest, StartDoesNotThrow) {
  EXPECT_NO_THROW(sut->start());
}

TEST_F(SessionTest, HandleReadOnErrorDoesNothing) {
  sut->set_buffer_for_test("PRESERVED_DATA");

  boost::system::error_code ec = boost::asio::error::operation_aborted;
  sut->on_read_complete(ec, 123); // bytes read is irrelevant on error

  EXPECT_EQ(sut->buffer_for_test(), "PRESERVED_DATA");
  EXPECT_EQ(sut->response_for_test(), "");
}

TEST_F(SessionTest, HandleWriteOnErrorKeepsBuffer) {
  sut->set_buffer_for_test("keep me");
  boost::system::error_code ec = boost::asio::error::connection_reset;
  sut->on_write_complete(ec);

  EXPECT_EQ(sut->buffer_for_test(), "keep me");
}

TEST_F(SessionTest, HandleReadAppendsIncompleteRequest) {
  simulate_client_send("GET /fake HTTP/1.1\r\n"); // No final \r\n\r\n
  EXPECT_EQ(sut->buffer_for_test(), "GET /fake HTTP/1.1\r\n");
  EXPECT_EQ(sut->response_for_test(), ""); // No response should be generated
}

TEST_F(SessionTest, HandleReadProcessesCompleteRequestWithoutBody) {
  std::string request = "GET /fake HTTP/1.1\r\n\r\n";
  simulate_client_send(request);

  EXPECT_TRUE(sut->buffer_for_test().empty());
  EXPECT_THAT(sut->response_for_test(), StartsWith("HTTP/1.1 200 OK"));
}

TEST_F(SessionTest, HandleReadProcessesCompleteRequestWithBody) {
  std::string request = "POST /fake HTTP/1.1\r\nContent-Length: 4\r\n\r\nDATA";
  simulate_client_send(request);

  EXPECT_TRUE(sut->buffer_for_test().empty());
  EXPECT_THAT(sut->response_for_test(), StartsWith("HTTP/1.1 200 OK"));
}

TEST_F(SessionTest, HandleReadWaitsForFullBodyBeforeProcessing) {
  simulate_client_send("POST /fake HTTP/1.1\r\nContent-Length: 10\r\n\r\n");
  EXPECT_FALSE(sut->buffer_for_test().empty()); // Should not process yet

  simulate_client_send("0123456789"); // Send the body
  EXPECT_TRUE(sut->buffer_for_test().empty());
  EXPECT_THAT(sut->response_for_test(), StartsWith("HTTP/1.1 200 OK"));
}

TEST_F(SessionTest, HandleReadProcessesPipelinedRequests) {
  std::string request1 = "GET /fake HTTP/1.1\r\n\r\n";
  std::string request2 = "POST /fake HTTP/1.1\r\nContent-Length: 4\r\n\r\nDATA";

  simulate_client_send(request1 + request2);

  EXPECT_TRUE(sut->buffer_for_test().empty());
  EXPECT_THAT(sut->response_for_test(), StartsWith("HTTP/1.1 200 OK"));
}

TEST_F(SessionTest, HandleReadGenerates404ForUnmatchedRoute) {
  simulate_client_send("GET /unconfigured-path HTTP/1.1\r\n\r\n");

  EXPECT_TRUE(sut->buffer_for_test().empty());
  EXPECT_THAT(sut->response_for_test(), StartsWith("HTTP/1.1 404 Not Found"));
}

TEST_F(SessionTest, HandleReadGenerates400ForInvalidContentLength) {
  simulate_client_send("POST /fake HTTP/1.1\r\nContent-Length: abc\r\n\r\n");

  EXPECT_TRUE(sut->buffer_for_test().empty());
  EXPECT_THAT(sut->response_for_test(), StartsWith("HTTP/1.1 400 Bad Request"));
}
