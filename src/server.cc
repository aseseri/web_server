#include "server.h"
#include <boost/bind.hpp>

/**
 * @brief Constructs a server object that listens for incoming connections on
 * the specified port.
 *
 * @param ios Reference to the Boost.Asio io_service object used for
 * asynchronous I/O operations.
 * @param port The port number on which the server will listen for incoming
 * connections.
 * @param factory A SessionFactory object used to create new session instances
 * for handling client connections.
 * @param thread_pool_size The number of threads to use in the thread pool.
 *
 * This constructor initializes the server by setting up the acceptor to listen
 * on the specified port and starts the asynchronous accept operation to handle
 * incoming client connections.
 */
server::server(boost::asio::io_service &ios, short port,
               SessionFactory factory, size_t thread_pool_size)
    : io_service_(ios),
      acceptor_(ios, tcp::endpoint(tcp::v4(), port)),
      session_factory_(std::move(factory)),
      work_(new boost::asio::io_service::work(io_service_)),
      thread_pool_size_(thread_pool_size) {
  start_accept();
}

/**
 * @brief Runs the server by launching the I/O service on multiple threads.
 *
 * This function creates and starts threads that run the Boost.Asio io_service
 * to handle asynchronous operations.
 */
void server::run() {
  for (size_t i = 0; i < thread_pool_size_; ++i) {
    thread_pool_.emplace_back([this]() {
      io_service_.run();
    });
  }
}

/**
 * @brief Stops the server and joins all worker threads.
 *
 * This function stops the I/O service and waits for all worker threads to
 * complete before returning.
 */
void server::stop() {
  work_.reset();
  io_service_.stop();
  for (auto &thread : thread_pool_) {
    if (thread.joinable()) {
      thread.join();
    }
  }
}

/**
 * @brief Starts the asynchronous accept operation to handle incoming client
 * connections.
 *
 * This function creates a new session using the session factory and initiates
 * an asynchronous accept operation. When a client connects, the `handle_accept`
 * function is called to process the connection. This ensures the server is
 * always ready to accept new connections.
 */
void server::start_accept() {
  auto new_sess = session_factory_(io_service_);
  acceptor_.async_accept(new_sess->socket(),
                         boost::bind(&server::handle_accept, this, new_sess,
                                     boost::asio::placeholders::error));
}

/**
 * @brief Handles the completion of an asynchronous accept operation.
 *
 * @param new_sess A shared pointer to the newly accepted session.
 * @param ec The error code indicating the result of the accept operation.
 *
 * If the accept operation was successful, this function starts the session to
 * handle client communication. Regardless of success or failure, it continues
 * to accept new connections.
 */
void server::handle_accept(SessionPtr new_sess,
                           const boost::system::error_code &ec) {
  if (!ec)
    new_sess->start();
  // either way, keep accepting
  start_accept();
}
