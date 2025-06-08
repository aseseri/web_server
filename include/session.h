//
// async_tcp_echo_server.cpp
// ~~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2017 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once
#include "request_handler.h"
#include <array>
#include <boost/asio.hpp>
#include <memory>
#include <string>
#include "request_handler.h"
#include "registry.h"

using tcp = boost::asio::ip::tcp;

// abstract interface
struct ISession {
  virtual tcp::socket &socket() = 0;
  virtual void start() = 0;
  virtual ~ISession() = default;
};

// real session implements it and uses shared_from_this()
class session : public ISession, public std::enable_shared_from_this<session> {
public:
  using RouteEntry = std::pair<
    Registry::RequestHandlerFactory,
    std::unordered_map<std::string, std::string>
  >;
  using RouteMap = std::map<std::string, RouteEntry>;

  struct ThreadSafeRouteMap {
    RouteMap routes;
    std::mutex mutex;
  };

  explicit session(boost::asio::io_service& ios,
                    std::shared_ptr<ThreadSafeRouteMap> routes = std::make_shared<ThreadSafeRouteMap>())
      : socket_(ios), routes_(std::move(routes)) {}

  tcp::socket &socket() override { return socket_; }

  void start() override { do_read(); }
  void on_read_complete(const boost::system::error_code &ec,
                        std::size_t bytes) {
    handle_read(ec, bytes);
  }

  /// Manually invoke handle_write as if an async_write completed.
  void on_write_complete(const boost::system::error_code &ec) {
    handle_write(ec);
  }

  // -- Test accessors --
  void set_buffer_for_test(const std::string &s) { buffer_ = s; }
  const std::string &buffer_for_test() const { return buffer_; }
  auto &data_for_test() { return data_; }
  const std::string &response_for_test() const { return response_; }

private:
  void do_read();
  void do_write(std::string);

  void handle_read(const boost::system::error_code &ec, std::size_t bytes);
  void handle_write(const boost::system::error_code &ec);
  void process_request(const std::string& request_str);

  tcp::socket socket_;
  std::shared_ptr<ThreadSafeRouteMap> routes_;
  enum { max_length = 1024 };
  std::array<char, max_length> data_;
  std::string buffer_;
  std::string response_;
};
