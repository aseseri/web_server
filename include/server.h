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
#include <boost/asio.hpp>
#include <functional>
#include <memory>
#define private public
#include "session.h"
#undef private
using tcp = boost::asio::ip::tcp;

class server {
public:
  using SessionPtr = std::shared_ptr<ISession>;
  using SessionFactory = std::function<SessionPtr(boost::asio::io_service &)>;

  // default factory
  static SessionPtr default_factory(boost::asio::io_service &ios) {
    return std::make_shared<session>(ios, std::make_shared<session::ThreadSafeRouteMap>());
  }

  server(boost::asio::io_service &ios, short port,
         SessionFactory factory = default_factory,
         size_t thread_pool_size = std::thread::hardware_concurrency());
  
  void run();
  void stop();

  // --- TEST HOOK: invoke the private accept handler directly ---
  void on_accept_complete(SessionPtr new_sess,
                          const boost::system::error_code &ec) {
    handle_accept(std::move(new_sess), ec);
  }
  size_t get_thread_pool_size() const { return thread_pool_.size(); }
  bool is_thread_joined(size_t index) const {
    if (index >= thread_pool_.size()) return false;
    return !thread_pool_[index].joinable();
  }

private:
  void start_accept();
  void handle_accept(SessionPtr new_sess, const boost::system::error_code &ec);

  boost::asio::io_service &io_service_;
  tcp::acceptor acceptor_;
  SessionFactory session_factory_;
  std::vector<std::thread> thread_pool_;
  size_t thread_pool_size_;
  std::shared_ptr<boost::asio::io_service::work> work_;
};
