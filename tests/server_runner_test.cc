#define UNIT_TEST
#include "server_runner.h"
#include "gtest/gtest.h"
#include <cstdio>
#include <fstream>

class ServerRunnerTest : public ::testing::Test {
protected:
  std::string filename = "tmp_sr_test.conf";

  void TearDown() override { std::remove(filename.c_str()); }
};

// setup_server should fail if parsing the config fails
TEST_F(ServerRunnerTest, SetupServer_ParseFails) {
  std::ofstream ofs(filename);
  ofs << "just some bad config }";
  ofs.close();
  int port = 0;
  EXPECT_FALSE(ServerRunner::setup_server(filename, port));
}

// setup_server should fail if port is zero or negative
TEST_F(ServerRunnerTest, SetupServer_InvalidPortZero) {
  std::ofstream ofs(filename);
  ofs << "port 0;";
  ofs.close();
  int port = -1;
  EXPECT_FALSE(ServerRunner::setup_server(filename, port));
}

// setup_server should fail if port is out of range (>65535)
TEST_F(ServerRunnerTest, SetupServer_InvalidPortHigh) {
  std::ofstream ofs(filename);
  ofs << "port 70000;";
  ofs.close();
  int port = -1;
  EXPECT_FALSE(ServerRunner::setup_server(filename, port));
}

// setup_server should succeed for a valid port directive
TEST_F(ServerRunnerTest, SetupServer_Succeeds) {
  std::ofstream ofs(filename);
  ofs << "port 12345;";
  ofs.close();
  int port = -1;
  EXPECT_TRUE(ServerRunner::setup_server(filename, port));
  EXPECT_EQ(port, 12345);
}

// run_server should catch exceptions from server ctor and return false
TEST(ServerRunnerRunTest, RunServer_OnException) {
  // assume negative port causes the acceptor to throw in constructor
  EXPECT_FALSE(ServerRunner::run_server(-1));
}

// An unknown handler should fail on server setup
TEST_F(ServerRunnerTest, SetupServer_UnregisteredHandlerFails) {
  std::ofstream ofs(filename);
  ofs << "port 8080;\n"
         "location /foo UnregisteredHandler {\n"
         "}";
  ofs.close();
  int port = -1;
  EXPECT_FALSE(ServerRunner::setup_server(filename, port));
}

// Server starts up on known handler (e.g. EchoHandler)
TEST_F(ServerRunnerTest, SetupServer_ValidHandlerSucceeds) {
  std::ofstream ofs(filename);
  ofs << "port 8080;\n"
         "location /echo EchoHandler {\n"
         "}";
  ofs.close();
  int port = -1;
  EXPECT_TRUE(ServerRunner::setup_server(filename, port));
}

// All handlers are saved in routes according to their path
TEST_F(ServerRunnerTest, SetupServer_RoutesAreRegistered) {
  std::ofstream ofs(filename);
   ofs << "port 12345;\n"
        "location /static StaticHandler { root ./files; }\n"
        "location /echo EchoHandler {}";
  ofs.close();
  int port;
  ASSERT_TRUE(ServerRunner::setup_server(filename, port));

  auto routes = ServerRunner::GetRoutesForTest();
  ASSERT_EQ(routes.size(), 2);
  ASSERT_NE(routes.find("/echo"), routes.end());
  ASSERT_NE(routes.find("/static"), routes.end());
}
