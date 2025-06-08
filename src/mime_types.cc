#include "mime_types.h"

// Build a map of common extensions to MIME types
const std::unordered_map<std::string, std::string> &MimeType::ExtensionMap() {
  static std::unordered_map<std::string, std::string> m{
      {".html", "text/html"},        {".htm", "text/html"},
      {".css", "text/css"},          {".js", "application/javascript"},
      {".json", "application/json"}, {".png", "image/png"},
      {".jpg", "image/jpeg"},        {".jpeg", "image/jpeg"},
      {".gif", "image/gif"},         {".txt", "text/plain"},
      {".zip", "application/zip"},
  };
  return m;
}

// Given a filename or path, extract extension and look up the MIME type
std::string MimeType::LookupByExtension(const std::string &path) {
  auto pos = path.find_last_of('.');
  if (pos == std::string::npos)
    return "application/octet-stream";
  std::string ext = path.substr(pos);
  auto it = ExtensionMap().find(ext);
  if (it != ExtensionMap().end())
    return it->second;
  return "application/octet-stream";
}
