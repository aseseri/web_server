//
// async_tcp_echo_server.cpp
// ~~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2017 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "logger.h"
#include "server_runner.h"
#include <cstdlib>
#include <iostream>
#include <csignal>

/**
 * Main entry point for the echo server
 * argc: number of command-line arguments
 * argv: array of command-line arguments
 * ret: 0 on success, 1 on error
 */
void signal_handler(int signal) {
  BOOST_LOG_TRIVIAL(info) << "Server shutting down (signal " << signal << ")";
  exit(0);
}

int main(int argc, char *argv[]) {
  init_logging();
  std::signal(SIGINT, signal_handler);
  std::signal(SIGTERM, signal_handler);
  
  if (argc < 2) {
    std::cerr << "Usage: " << argv[0] << " <config_path>\n";
    return 1;
  }

  int port = 0;
  const std::string &config_path = argv[1];

  // Setup server with the provided config path
  if (!ServerRunner::setup_server(config_path, port)) {
    BOOST_LOG_TRIVIAL(info) << "Server failed to start on port " << port;
    return 1;
  }

  BOOST_LOG_TRIVIAL(info) << "Server started and listening on port " << port;

  // Run the server
  if (!ServerRunner::run_server(port)) {
    return 1;
  }

  shutdown_logging();

  return 0;
}
