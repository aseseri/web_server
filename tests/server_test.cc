#include "server.h"
#include "session.h"
#include "gtest/gtest.h"
#include <boost/asio.hpp>
#include <memory>

// Fixture for testing server
class ServerTest : public ::testing::Test {
protected:
  boost::asio::io_service ios;
};

// Test that default_factory returns a non-null shared_ptr<ISession>
// and that it actually points to a concrete session.
TEST_F(ServerTest, DefaultFactoryCreatesSession) {
  auto ptr = server::default_factory(ios);
  ASSERT_NE(ptr, nullptr);
  // dynamic_cast to session should succeed
  auto concrete = std::dynamic_pointer_cast<session>(ptr);
  EXPECT_NE(concrete, nullptr);
}

// Test that constructing a server with the default factory does not throw.
TEST_F(ServerTest, ConstructorWithDefaultFactoryDoesNotThrow) {
  EXPECT_NO_THROW({ server srv(ios, 12345); });
}

// Test that a custom factory is invoked exactly once during server
// construction.
TEST_F(ServerTest, CustomFactoryInvocationCount) {
  int count = 0;
  server::SessionFactory factory = [&](boost::asio::io_service &ios_ref) {
    ++count;
    // return a dummy session
    return std::make_shared<session>(
      ios_ref,
      std::make_shared<session::ThreadSafeRouteMap>()
    );
  };
  server srv(ios, 23456, factory);
  EXPECT_EQ(count, 1);
}

// A minimal ISession stub that counts start() calls
class FakeSession : public ISession {
public:
  explicit FakeSession(boost::asio::io_service &ios) : sock_(ios) {}
  tcp::socket &socket() override { return sock_; }
  void start() override { ++start_count; }
  int start_count{0};

private:
  tcp::socket sock_;
};

// When ec == success, handle_accept should call start() on the session
// and should schedule a new accept (i.e. factory invoked again).
TEST_F(ServerTest, HandleAcceptOnSuccess) {
  int factory_count = 0;
  server::SessionFactory factory = [&](boost::asio::io_service &ios_ref) {
    ++factory_count;
    return std::make_shared<session>(
      ios_ref,
      std::make_shared<session::ThreadSafeRouteMap>()
    );
  };
  server srv(ios, 15001, factory);
  // constructor → one call to start_accept()
  EXPECT_EQ(factory_count, 1);

  auto fake = std::make_shared<FakeSession>(ios);
  boost::system::error_code ec; // success
  srv.on_accept_complete(fake, ec);

  // start() should have been called exactly once
  EXPECT_EQ(fake->start_count, 1);
  // handle_accept → start_accept() again
  EXPECT_EQ(factory_count, 2);
}

// When ec != success, start() should NOT be called, but we still accept again.
TEST_F(ServerTest, HandleAcceptOnError) {
  int factory_count = 0;
  server::SessionFactory factory = [&](boost::asio::io_service &ios_ref) {
    ++factory_count;
    return std::make_shared<session>(
      ios_ref,
      std::make_shared<session::ThreadSafeRouteMap>()
    );
  };
  server srv(ios, 15002, factory);
  EXPECT_EQ(factory_count, 1);

  auto fake = std::make_shared<FakeSession>(ios);
  boost::system::error_code ec = boost::asio::error::operation_aborted;
  srv.on_accept_complete(fake, ec);

  // start() should NOT have been called
  EXPECT_EQ(fake->start_count, 0);
  // but we do start_accept() again
  EXPECT_EQ(factory_count, 2);
}

// Test stop() stops threads
TEST_F(ServerTest, StopJoinsThreads) {
  server srv(ios, 15004, server::default_factory, 2);
  srv.run();
  srv.stop();

  for (size_t i = 0; i < srv.get_thread_pool_size(); ++i) {
    EXPECT_TRUE(srv.is_thread_joined(i));
  }
}

// Test multiple accept cycles (continue creating session)
TEST_F(ServerTest, MultipleAcceptCycles) {
  int factory_count = 0;
  server::SessionFactory factory = [&](boost::asio::io_service &ios_ref) {
    ++factory_count;
    return std::make_shared<FakeSession>(ios_ref);
  };

  server srv(ios, 15005, factory);
  EXPECT_EQ(factory_count, 1);

  for (int i = 0; i < 5; ++i) {
    auto fake = std::make_shared<FakeSession>(ios);
    boost::system::error_code ec;
    srv.on_accept_complete(fake, ec);
    EXPECT_EQ(fake->start_count, 1);
  }

  EXPECT_EQ(factory_count, 6);  // 1 initial + 5 cycles
}

// Server should cleanly stop with zero threads
TEST_F(ServerTest, ZeroThreadsStillWorks) {
  server srv(ios, 15006, server::default_factory, 0);
  EXPECT_NO_THROW(srv.run());  // Shouldn't crash
  srv.stop();                  // Should still cleanly stop
}
