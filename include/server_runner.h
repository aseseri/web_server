#pragma once
#include "config_parser.h"
#include "registry.h"
#include <boost/asio.hpp>
#include <cstdlib>
#include <string>

class ServerRunner {
#ifdef UNIT_TEST
public:
  static const auto& GetRoutesForTest() { return routes_; }
#endif
public:
  static bool validate_port(int port);

  // Function to parse the config and create the server
  static bool setup_server(const std::string &config_path, int &port);

  // Function to run the server
  static bool run_server(int port);

private:
  static void initialize_handlers(); // Ensures all available request handler types are registered with the Registry
  static bool parse_config(const std::string& config_path, NginxConfig& config);
  static bool validate_and_extract_port(const NginxConfig& config, int& port);
  static bool extract_routes(const std::vector<NginxConfig::HandlerConfig>& handlers_conf);

  // Holds the handlers created in setup_server()
  static std::map<std::string, std::pair<
      Registry::RequestHandlerFactory,
      std::unordered_map<std::string, std::string>
  >> routes_;
};
