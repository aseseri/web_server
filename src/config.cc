#include "config.h"
#include "config_statement.h"
#include "logger.h"

std::string NginxConfig::ToString(int depth) {
  std::string serialized_config;
  for (const auto &statement : statements_) {
    serialized_config.append(statement->ToString(depth));
  }
  return serialized_config;
}

int NginxConfig::GetPortNumber() const {
  for (const auto &statement : statements_) {
    // Direct port declaration
    if (statement->tokens_.size() == 2 && statement->tokens_[0] == "port") {
      try {
        return std::stoi(statement->tokens_[1]);
      } catch (...) {
        return -1;
      }
    }
    // Check child blocks recursively for the port
    if (statement->child_block_) {
      int port = statement->child_block_->GetPortNumber();
      if (port != -1) {
        return port;
      }
    }
  }
  return -1;
}

bool NginxConfig::GetHandlerConfigs(std::vector<HandlerConfig>& out_configs) const {
  std::unordered_set<std::string> seen_paths;
  out_configs.clear();

  for (const auto &stmt : statements_) {
    // Skips non-location statements
    if (stmt->tokens_.empty() || stmt->tokens_[0] != "location") {
      continue;
    }
    if (!IsValidLocationStatement(*stmt)) return false;

    const std::string& path = stmt->tokens_[1];
    const std::string& type = stmt->tokens_[2];

    if (!IsValidPath(path, seen_paths)) return false;

    HandlerConfig config;
    config.path = path;
    config.type = type;

    if (!IsValidHandlerConfig(config)) return false;

    ParseHandlerArgs(*stmt->child_block_, config);

    out_configs.push_back(std::move(config));
    seen_paths.insert(path);
  }
  return true;
}

bool NginxConfig::IsValidLocationStatement(const NginxConfigStatement& stmt) const {
  if(!(stmt.tokens_.size() == 3 && stmt.child_block_)){
    BOOST_LOG_TRIVIAL(error) << "Invalid location statement: Expected 3 tokens (location, path, handler) and a child block";
    return false;
  }
  return true;
}

bool NginxConfig::IsValidPath(const std::string& path, const std::unordered_set<std::string>& seen_paths) const {
  if (path.empty()) {
    BOOST_LOG_TRIVIAL(error) << "Empty path in location block";
    return false;
  }

  if (path.back() == '/') {
    BOOST_LOG_TRIVIAL(error) << "Trailing slash prohibited in path: " << path;
    return false;
  }

  if (seen_paths.count(path)) {
    BOOST_LOG_TRIVIAL(error) << "Duplicate location path: " << path;
    return false;
  }
  return true;
}

bool NginxConfig::IsValidHandlerConfig(const HandlerConfig& config) const {
  if (config.type.empty()) {
    BOOST_LOG_TRIVIAL(error) << "Missing handler type for path: " << config.path;
    return false;
  }
  return true;
}

void NginxConfig::ParseHandlerArgs(const NginxConfig& block, HandlerConfig& config) const {
  for (const auto& inner : block.statements_) {
    if (inner->tokens_.size() == 2) {
      const std::string& key = inner->tokens_[0];
      const std::string& value = inner->tokens_[1];

      if (key == "requires_auth") {
        if (value == "true") {
          BOOST_LOG_TRIVIAL(debug) << "Path's requires_auth value: " << value;
          config.requires_auth = true;
        } else if (value == "false") {
          BOOST_LOG_TRIVIAL(debug) << "Path's requires_auth value: " << value;
          config.requires_auth = false;
        } else {
          BOOST_LOG_TRIVIAL(warning) << "Invalid requires_auth value: " << value;
        }
      }

      config.args[key] = value;
    }
    else if (inner->tokens_.size() != 0) {
      BOOST_LOG_TRIVIAL(warning) << "Invalid argument format in handler block";
    }
  }
}
