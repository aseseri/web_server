#include "auth_handler.h"
#include "http_request.h"
#include "http_response.h"
#include "gtest/gtest.h"
#include <string>
#include <sstream>
#include <boost/archive/iterators/base64_from_binary.hpp>
#include <boost/archive/iterators/transform_width.hpp>
#include <thread>

class AuthHandlerTest : public ::testing::Test {
protected:
    // Helper to encode username:password to base64
    std::string EncodeBase64(const std::string& input) {
        using namespace boost::archive::iterators;
        using base64_enc = base64_from_binary<transform_width<std::string::const_iterator, 6, 8>>;
        std::stringstream ss;
        std::copy(base64_enc(input.begin()), base64_enc(input.end()), std::ostream_iterator<char>(ss));
        size_t padding = (3 - input.size() % 3) % 3;
        for (size_t i = 0; i < padding; ++i) ss << '=';
        return ss.str();
    }

    HttpRequest CreateRequestWithAuth(const std::string& credentials) {
        HttpRequest req;
        req.headers["Authorization"] = "Basic " + EncodeBase64(credentials);
        return req;
    }
};

// Valid credentials test
TEST_F(AuthHandlerTest, ValidCredentialsAuthenticate) {
    HttpRequest req = CreateRequestWithAuth("admin:password123");
    EXPECT_TRUE(AuthHandler::AuthenticateRequest(req));
}

// Invalid username
TEST_F(AuthHandlerTest, InvalidUsernameFails) {
    HttpRequest req = CreateRequestWithAuth("notarealuser:password123");
    EXPECT_FALSE(AuthHandler::AuthenticateRequest(req));
}

// Invalid password
TEST_F(AuthHandlerTest, InvalidPasswordFails) {
    HttpRequest req = CreateRequestWithAuth("admin:wrongpass");
    EXPECT_FALSE(AuthHandler::AuthenticateRequest(req));
}

// Missing Authorization header
TEST_F(AuthHandlerTest, MissingAuthorizationHeaderFails) {
    HttpRequest req;
    EXPECT_FALSE(AuthHandler::AuthenticateRequest(req));
}

// Authorization header not using Basic scheme
TEST_F(AuthHandlerTest, InvalidAuthorizationSchemeFails) {
    HttpRequest req;
    req.headers["Authorization"] = "Bearer sometoken";
    EXPECT_FALSE(AuthHandler::AuthenticateRequest(req));
}

// Malformed base64 (invalid characters or length)
TEST_F(AuthHandlerTest, MalformedBase64FailsGracefully) {
    HttpRequest req;
    req.headers["Authorization"] = "Basic $$$invalid$$$";
    EXPECT_FALSE(AuthHandler::AuthenticateRequest(req));
}

// UnauthorizedResponse has correct headers and status
TEST_F(AuthHandlerTest, UnauthorizedResponseCorrect) {
    HttpResponse resp = AuthHandler::UnauthorizedResponse();
    EXPECT_EQ(resp.status_code, 401);
    EXPECT_EQ(resp.reason, "Unauthorized");
    EXPECT_EQ(resp.headers["WWW-Authenticate"], "Basic realm=\"Restricted Area\"");
    EXPECT_EQ(resp.headers["Content-Type"], "text/plain");
    EXPECT_EQ(resp.body, "401 Unauthorized");
}

// No colon in decoded credentials
TEST_F(AuthHandlerTest, NoColonFails) {
    std::string badCreds = "adminpassword123"; // no colon
    HttpRequest req;
    req.headers["Authorization"] = "Basic " + EncodeBase64(badCreds);
    EXPECT_FALSE(AuthHandler::AuthenticateRequest(req));
}

// Encoded but empty string
TEST_F(AuthHandlerTest, EncodedEmptyStringFails) {
    HttpRequest req;
    req.headers["Authorization"] = "Basic " + EncodeBase64("");
    EXPECT_FALSE(AuthHandler::AuthenticateRequest(req));
}

// Base64-encoded garbage string
TEST_F(AuthHandlerTest, Base64DecodedGarbageFails) {
    // This is valid base64 but decodes to unreadable data
    HttpRequest req;
    req.headers["Authorization"] = "Basic SGVsbG8hISE=";  // "Hello!!!"
    EXPECT_FALSE(AuthHandler::AuthenticateRequest(req));
}

// Test concurrent authentication
TEST_F(AuthHandlerTest, ConcurrentAuthentication) {
    const int num_threads = 10;
    std::vector<std::thread> threads;
    std::atomic<int> successes(0);
    
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&]() {
            HttpRequest req = CreateRequestWithAuth("admin:password123");
            if (AuthHandler::AuthenticateRequest(req)) {
                successes++;
            }
        });
    }
    
    for (auto& t : threads) t.join();
    EXPECT_EQ(successes, num_threads);
}

TEST_F(AuthHandlerTest, EmptyCredentialsFail) {
    HttpRequest req = CreateRequestWithAuth(":");
    EXPECT_FALSE(AuthHandler::AuthenticateRequest(req));
}

TEST_F(AuthHandlerTest, MissingPasswordFails) {
    HttpRequest req = CreateRequestWithAuth("admin:");
    EXPECT_FALSE(AuthHandler::AuthenticateRequest(req));
}

TEST_F(AuthHandlerTest, MissingUsernameFails) {
    HttpRequest req = CreateRequestWithAuth(":password123");
    EXPECT_FALSE(AuthHandler::AuthenticateRequest(req));
}

TEST_F(AuthHandlerTest, MultipleColonsFails) {
    HttpRequest req = CreateRequestWithAuth("admin:password123:extra");
    EXPECT_FALSE(AuthHandler::AuthenticateRequest(req));
}