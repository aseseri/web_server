#pragma once
#include <string>
#include <unordered_map>

class MimeType {
public:
  // Build a map of common extensions to MIME types
  static const std::unordered_map<std::string, std::string> &ExtensionMap();

  // Given a filename or path, extract extension and look up the MIME type
  static std::string LookupByExtension(const std::string &path);
};
