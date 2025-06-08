#include "session.h"
#include "logger.h"
#include <boost/bind.hpp>
#include <chrono> 
#include <sstream>
#include "not_found_handler.h"
#include "registry.h"
#include "auth_handler.h"

std::string find_longest_prefix_match(
  const session::RouteMap& routes,
  const std::string& request_path)
{
  std::string best_match;
  for (const auto& [route_path, _] : routes) {
    if (request_path.compare(0, route_path.size(), route_path) == 0) {
      // Ensure the match is exact or followed by a '/'
      if (request_path.size() == route_path.size() || request_path[route_path.size()] == '/') {
        if (route_path.size() > best_match.size()) {
            best_match = route_path;
        }
      }
    }
  }
  return best_match;
}

/**
 * Asynchronously read data from the socket
 *
 * This function sets up an async read operation on the socket
 * and binds the handle_read function to be called when the read
 * operation completes
 */
void session::do_read() {
  auto self = shared_from_this();
  socket_.async_read_some(
      boost::asio::buffer(data_),
      [self](auto ec, auto bytes) { self->handle_read(ec, bytes); });
}

/**
 * Processes a single, complete HTTP request from the buffer
 * This function handles parsing, routing, and authentication
 */
void session::process_request(const std::string& request_str) {
    auto start_time = std::chrono::steady_clock::now();
    HttpRequest req = HttpRequest::Parse(request_str);

    std::string client_ip = "unknown";
    try {
        client_ip = socket_.remote_endpoint().address().to_string();
    } catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(warning) << "Could not get client IP: " << e.what();
    }

    // Immediately handle a malformed request
    if (req.is_malformed()) {
        BOOST_LOG_TRIVIAL(warning) << "[ResponseMetrics] "
            << "response_code=400 request_path=\"" << req.path
            << "\" client_ip=\"" << client_ip << "\" handler=\"MalformedRequestHandler\"";
        auto resp = std::make_unique<HttpResponse>(HttpResponse::Stock400());
        do_write(resp->ToString());
        return;
    }

    BOOST_LOG_TRIVIAL(info) << "Processing request from " << client_ip << ": "
                              << req.method << " " << req.path;

    std::unique_ptr<RequestHandler> handler;
    std::unique_ptr<HttpResponse> resp;
    std::string handler_name;

    const std::string matched_path = find_longest_prefix_match(routes_->routes, req.path);

    if (!matched_path.empty()) {
        auto& [factory, args] = routes_->routes.at(matched_path);

        // Check for authentication requirement
        if (args["requires_auth"] == "true") {
            if (!AuthHandler::AuthenticateRequest(req)) {
                BOOST_LOG_TRIVIAL(warning) << "[AuthFailure] path=" << req.path << " ip=" << client_ip;
                resp = std::make_unique<HttpResponse>(AuthHandler::UnauthorizedResponse());
                handler_name = "AuthFailureHandler";
            }
        }
        // If authentication passed (or wasn't required), create the designated handler
        if (!resp) {
            handler_name = matched_path;
            handler = factory(matched_path, args);
        }
    } else {
        handler_name = "NotFoundHandler";
        handler = std::make_unique<NotFoundHandler>();
    }

    if (!resp) {
        if (handler) {
            resp = handler->handle_request(req);
        } else {
            // This case should not be reached with proper configuration
            BOOST_LOG_TRIVIAL(error) << "Critical: No handler or response generated for " << req.path;
            resp = std::make_unique<HttpResponse>(HttpResponse::Stock500());
            handler_name = "ServerErrorHandler";
        }
    }

    // Log response metrics
    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start_time).count();

    BOOST_LOG_TRIVIAL(info) << "[ResponseMetrics] "
        << "response_code=" << resp->status_code << " "
        << "request_path=\"" << req.path << "\" "
        << "client_ip=\"" << client_ip << "\" "
        << "handler=\"" << handler_name << "\" "
        << "duration_ms=" << duration_ms;

    do_write(resp->ToString());
}

/**
 * Handles completion of an async read
 *
 * Appends data to the buffer and processes any complete HTTP requests
 * This supports requests with bodies (Content-Length) and pipelining
 */
void session::handle_read(const boost::system::error_code &ec,
                          std::size_t bytes) {
    if (ec) {
        if (ec != boost::asio::error::eof) {
            BOOST_LOG_TRIVIAL(error) << "Session handle_read error: " << ec.message();
        }
        return;
    }

    buffer_.append(data_.data(), bytes);

    // Loop to process all complete requests in the buffer
    while (true) {
        size_t header_end_pos = buffer_.find("\r\n\r\n");
        if (header_end_pos == std::string::npos) {
            // Incomplete headers, read more data
            do_read();
            return;
        }

        // Parse headers to find Content-Length
        HttpRequest temp_req = HttpRequest::Parse(buffer_.substr(0, header_end_pos + 4));
        size_t content_length = 0;
        if (auto it = temp_req.headers.find("Content-Length"); it != temp_req.headers.end()) {
            try {
                content_length = std::stoul(it->second);
            } catch (const std::exception&) {
                BOOST_LOG_TRIVIAL(error) << "Invalid Content-Length. Sending 400 Bad Request.";
                auto resp = std::make_unique<HttpResponse>(HttpResponse::Stock400());
                do_write(resp->ToString());
                buffer_.clear();    // Clear buffer to prevent reprocessing bad request
                return;
            }
        }

        size_t total_request_size = header_end_pos + 4 + content_length;
        if (buffer_.length() < total_request_size) {
            // Incomplete body, read more data
            do_read();
            return;
        }

        std::string complete_request = buffer_.substr(0, total_request_size);
        process_request(complete_request);
        buffer_.erase(0, total_request_size);   // Remove processed request from the buffer

        if (buffer_.empty()) {
            break;  // Done with buffer, wait for next read
        }
        // Loop again to check for a pipelined request
    }
}

/**
 * Asynchronously write data to the socket
 *
 * This function sets up an async write operation on the socket
 * and binds the handle_write function to be called when the write
 * operation completes
 */
void session::do_write(std::string resp) {
  response_ = std::move(resp); // Store in member to keep alive
  boost::asio::async_write(
      socket_, boost::asio::buffer(response_),
      [self = shared_from_this()](auto ec, auto) { self->handle_write(ec); });
}

/**
 * Handle the completion of an async write operation
 *
 * After a response is sent, this function starts a new read to listen for
 * the next request. It does not clear the main buffer to support pipelining
 */
void session::handle_write(const boost::system::error_code &ec) {
  if (ec) {
    BOOST_LOG_TRIVIAL(error) << "Session handle_write error: " << ec.message();
    return;
  }

  BOOST_LOG_TRIVIAL(debug) << "Response sent successfully.";
  response_.clear(); // Clear the sent response buffer

  // Start reading again to listen for the next request
  do_read();
}
