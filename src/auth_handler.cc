#include "auth_handler.h"
#include "logger.h"
#include <boost/algorithm/string.hpp>
#include <boost/archive/iterators/base64_from_binary.hpp>
#include <boost/archive/iterators/binary_from_base64.hpp>
#include <boost/archive/iterators/transform_width.hpp>

using namespace boost::archive::iterators;

// Hardcoded credentials (in production, these should be configurable)
const std::unordered_map<std::string, std::string> AuthHandler::valid_credentials_ = {
    {"admin", "password123"},
    {"user", "mypassword"}
};

bool AuthHandler::ValidateCredentials(const std::string& credentials) {
    // Split into username:password
    size_t colon_pos = credentials.find(':');
    if (colon_pos == std::string::npos) {
        BOOST_LOG_TRIVIAL(debug) << "Invalid credentials format - missing colon";        
        BOOST_LOG_TRIVIAL(debug) << "correct format: \'username:password\'";
        return false;
    }

    std::string username = credentials.substr(0, colon_pos);
    std::string password = credentials.substr(colon_pos + 1);

    if (username.empty() || password.empty()) {
        BOOST_LOG_TRIVIAL(debug) << "Username or password is empty";
        return false;
    }

    // Check against stored credentials
    auto it = valid_credentials_.find(username);
    if (it == valid_credentials_.end()) {
        BOOST_LOG_TRIVIAL(debug) << "Username not found: " << username;
        return false;
    }

    if (it->second != password) {
        BOOST_LOG_TRIVIAL(debug) << "Password mismatch for user: " << username;
        return false;
    }

    BOOST_LOG_TRIVIAL(debug) << "Authentication successful for user: " << username;
    return true;
}

bool AuthHandler::AuthenticateRequest(const HttpRequest& req) {
    std::string credentials = ExtractCredentials(req);
    if (credentials.empty()) {
        BOOST_LOG_TRIVIAL(debug) << "No credentials provided";
        return false;
    }
    return ValidateCredentials(credentials);
}

HttpResponse AuthHandler::UnauthorizedResponse() {
    BOOST_LOG_TRIVIAL(debug) << "Creating response: 401 Unauthorized";
    HttpResponse resp;
    resp.version = "HTTP/1.1";
    resp.status_code = 401;
    resp.reason = "Unauthorized";
    resp.headers["WWW-Authenticate"] = "Basic realm=\"Restricted Area\"";
    resp.headers["Content-Type"] = "text/plain";
    resp.body = "401 Unauthorized";
    return resp;
}

std::string AuthHandler::ExtractCredentials(const HttpRequest& req) {
    auto it = req.headers.find("Authorization");
    if (it == req.headers.end()) {
        BOOST_LOG_TRIVIAL(debug) << "No Authorization header found";
        return "";
    }

    const std::string& auth_header = it->second;
    if (auth_header.find("Basic ") != 0) {
        BOOST_LOG_TRIVIAL(debug) << "Invalid Authorization header - not Basic auth";
        BOOST_LOG_TRIVIAL(debug) << "Encode Authorization header with proper encoding, for example: echo -n \"username:password\" | base64";

        return "";
    }

    std::string encoded = auth_header.substr(6); // Remove "Basic "
    return DecodeBase64(encoded);
}

std::string AuthHandler::DecodeBase64(const std::string& encoded) {
    try {
        using It = transform_width<binary_from_base64<std::string::const_iterator>, 8, 6>;
        std::string decoded(It(encoded.begin()), It(encoded.end()));
        return boost::algorithm::trim_right_copy_if(decoded, [](char c) { return c == '\0'; });
    } catch (...) {
        BOOST_LOG_TRIVIAL(error) << "Failed to decode Base64 credentials";
        return "";
    }
}
