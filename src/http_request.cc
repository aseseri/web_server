#include "http_request.h"
#include "logger.h"
#include <sstream>

// Parse the raw HTTP request string into an HttpRequest object
// The request line is the first line of the request and contains the method,
// path, and version. The headers are parsed until an empty line is encountered.
// The body is everything after the headers.
HttpRequest HttpRequest::Parse(const std::string &raw_req) {
  HttpRequest req;
  req.raw = raw_req;
  std::istringstream stream(raw_req);
  std::string line;

  // Parse request line: METHOD SP PATH SP VERSION CRLF
  if (!std::getline(stream, line)) {
    BOOST_LOG_TRIVIAL(warning) << "HttpRequest: Failed to read request line.";    
    return req;
  }
  if (!line.empty() && line.back() == '\r')
    line.pop_back();
  {
    std::istringstream rl(line);
    rl >> req.method >> req.path >> req.version;
    if (req.method.empty() || req.path.empty() || req.version.empty()) {
      BOOST_LOG_TRIVIAL(warning) << "HttpRequest: Malformed request line: '" << line << "'";
      return req;
    }
  }

  // Parse headers until empty line
  while (std::getline(stream, line)) {
    // checks if line is empty
    if (!line.empty() && line.back() == '\r')
      line.pop_back();
    if (line.empty())
      break; // end of headers
    auto pos = line.find(':');
    if (pos != std::string::npos) {
      std::string name = line.substr(0, pos);
      std::string value = line.substr(pos + 1);
      // Trim leading whitespace from value
      auto first = value.find_first_not_of(" \t");
      if (first != std::string::npos)
        value.erase(0, first);
      else
        value.clear();
      req.headers[name] = value;
    }
  }

  // The rest is body
  std::string body;
  std::getline(stream, body, '\0');
  req.body = body;
  return req;
}
