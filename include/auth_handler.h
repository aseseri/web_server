#pragma once
#include "http_request.h"
#include "http_response.h"
#include <string>
#include <unordered_map>

class AuthHandler {
public:
    // Validate credentials against stored pairs
    static bool ValidateCredentials(const std::string& credentials);
    
    // Parse and validate Authorization header
    static bool AuthenticateRequest(const HttpRequest& req);
    
    // Create a 401 Unauthorized response
    static HttpResponse UnauthorizedResponse();
    
private:
    // Hardcoded credentials (username:password)
    static const std::unordered_map<std::string, std::string> valid_credentials_;
    
    // Extract credentials from Authorization header
    static std::string ExtractCredentials(const HttpRequest& req);
    
    // Decode encoded credentials
    static std::string DecodeBase64(const std::string& encoded);
};