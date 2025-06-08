#include <gtest/gtest.h>
#include "health_handler.h"
#include "http_request.h"
#include "http_response.h"

class HealthHandlerTest : public ::testing::Test {
protected:
    void SetUp() override {
        handler_ = std::make_unique<HealthHandler>();
    }

    std::unique_ptr<HealthHandler> handler_;
};

TEST_F(HealthHandlerTest, ReturnsOK) {
    HttpRequest req;
    auto resp = handler_->handle_request(req);

    EXPECT_EQ(resp->status_code, 200);
    EXPECT_EQ(resp->reason, "OK");
    EXPECT_EQ(resp->version, "HTTP/1.1");
    EXPECT_EQ(resp->headers["Content-Type"], "text/plain");
    EXPECT_EQ(resp->body, "OK");
}