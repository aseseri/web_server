#pragma once
#include "config_statement.h"
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>

class NginxConfig {
public:
  std::string ToString(int depth = 0);
  int GetPortNumber() const;

  // new struct and method to extract all top‑level handler blocks
  struct HandlerConfig {
    std::string path;
    std::string type;
    std::unordered_map<std::string, std::string> args; // handler arguments
    bool requires_auth = false; // authentication flag
  };
  bool GetHandlerConfigs(std::vector<HandlerConfig>& out_configs) const;


  std::vector<std::shared_ptr<NginxConfigStatement>> statements_;

private:
  bool IsValidLocationStatement(const NginxConfigStatement& stmt) const;
  bool IsValidPath(const std::string& path, const std::unordered_set<std::string>& seen_paths) const;
  bool IsValidHandlerConfig(const HandlerConfig& config) const;
  void ParseHandlerArgs(const NginxConfig& block, HandlerConfig& config) const;
};
