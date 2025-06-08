// nginx_config_statement.h
#pragma once
#include <memory>
#include <string>
#include <vector>

class NginxConfig;

class NginxConfigStatement {
public:
  std::string ToString(int depth);
  std::vector<std::string> tokens_;
  std::unique_ptr<NginxConfig> child_block_;
};
