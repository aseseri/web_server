#include "server_runner.h"
#include "echo_handler.h"
#include "logger.h"
#include "not_found_handler.h"
#include "request_handler.h"
#include "server.h"
#include "static_handler.h"
#include "health_handler.h"

// Define the static member
std::map<std::string, std::pair<Registry::RequestHandlerFactory,
                                std::unordered_map<std::string, std::string>>>
    ServerRunner::routes_;

bool ServerRunner::validate_port(int port) {
  // Check if the port is within the valid range
  return (port > 0 && port <= 65535);
}

extern void TouchEchoHandler();
extern void TouchStaticHandler();
extern void TouchNotFoundHandler();
extern void TouchCRUDHandler();
extern void TouchHealthHandler();
extern void TouchSleepHandler();

void ServerRunner::initialize_handlers() {
  TouchEchoHandler();
  TouchStaticHandler();
  TouchNotFoundHandler();
  TouchCRUDHandler();
  TouchHealthHandler();
  TouchSleepHandler();
}

bool ServerRunner::parse_config(const std::string &config_path,
                                NginxConfig &config) {
  NginxConfigParser config_parser;
  if (!config_parser.Parse(config_path.c_str(), &config)) {
    BOOST_LOG_TRIVIAL(error) << "Failed to parse config file: " << config_path;
    return false;
  }
  BOOST_LOG_TRIVIAL(info) << "Successfully parsed config file: " << config_path;
  return true;
}

bool ServerRunner::validate_and_extract_port(const NginxConfig &config,
                                             int &port) {
  port = config.GetPortNumber();
  if (!validate_port(port)) {
    BOOST_LOG_TRIVIAL(error) << "Invalid port number: " << port;
    return false;
  }
  BOOST_LOG_TRIVIAL(info) << "Using port number: " << port;
  return true;
}

bool ServerRunner::extract_routes(
    const std::vector<NginxConfig::HandlerConfig> &handlers_conf) {
  for (const auto &hc : handlers_conf) {
    BOOST_LOG_TRIVIAL(info)
        << "Mapping path to handler: " << hc.path << " -> type: " << hc.type;

    auto factory = Registry::GetHandlerFactory(hc.type);
    if (!factory) {
      BOOST_LOG_TRIVIAL(error) << "Handler type not registered: " << hc.type;
      return false;
    }
    routes_[hc.path] = {factory, hc.args};
  }
  return true;
}

bool ServerRunner::setup_server(const std::string &config_path, int &port) {
  initialize_handlers();  // Initialize handlers first
  
  NginxConfig config;

  if (!parse_config(config_path, config)) {
    return false;
  }

  if (!validate_and_extract_port(config, port)) {
    return false;
  }

  std::vector<NginxConfig::HandlerConfig> handlers_conf;
  if (!config.GetHandlerConfigs(handlers_conf)) {
    BOOST_LOG_TRIVIAL(error) << "Invalid handler configs";
    return false;
  }

  if (!extract_routes(handlers_conf)) {
    return false;
  }

  BOOST_LOG_TRIVIAL(info) << "Server setup complete with " << routes_.size()
                          << " route(s).";
  return true;
}

// Function to run the server
bool ServerRunner::run_server(int port) {
  if (!validate_port(port)) {
    BOOST_LOG_TRIVIAL(error) << "Invalid port: " << port;
    return false;
  }
  try {
    boost::asio::io_service ios;

    auto routes = std::make_shared<session::ThreadSafeRouteMap>();
    {
      std::lock_guard<std::mutex> lock(routes->mutex);
      routes->routes = routes_;
    }

    server srv(ios, port, [routes](boost::asio::io_service &ios_ref) {
      return std::make_shared<session>(ios_ref, routes);
    });
    BOOST_LOG_TRIVIAL(info) << "Server started with "
                           << std::thread::hardware_concurrency()
                           << " threads";

    ios.run();
    return true;
  } catch (const std::exception &e) {
    BOOST_LOG_TRIVIAL(fatal) << "Exception: " << e.what();
    return false;
  }
}
