#include "static_handler.h"
#include "http_request.h"
#include "http_response.h"
#include "gtest/gtest.h"
#include <fstream>
#include <cstdio>

class StaticHandlerTest : public ::testing::Test {
protected:
  // Helper to create a temporary file with given content
  static std::string make_temp_file(const std::string& dir,
    const std::string& name,
    const std::string& content) {
        std::string path = dir + "/" + name;
        std::ofstream ofs(path, std::ios::binary);
        ofs << content;
        ofs.close();
        return path;
    }
};

// Test that StaticHandler serves files correctly
TEST_F(StaticHandlerTest, ServeExistingFile) {
  // write temp file in current dir
  auto fname = make_temp_file(".", "test.txt", "HELLO");
  StaticHandler h("/static", ".");
  HttpRequest req;
  req.path = "/static/test.txt";
  auto resp = h.handle_request(req);

  EXPECT_EQ(resp->status_code, 200);
  EXPECT_EQ(resp->headers["Content-Type"], "text/plain");
  EXPECT_EQ(resp->body, "HELLO");
  // clean up
  std::remove(fname.c_str());
}

// Test that StaticHandler returns 404 for non-existing files
TEST_F(StaticHandlerTest, MissingFileReturns404) {
  StaticHandler h("/static", ".");
  HttpRequest req;
  req.path = "/static/no_such_file.bin";
  auto resp = h.handle_request(req);

  EXPECT_EQ(resp->status_code, 404);
  EXPECT_EQ(resp->reason, "Not Found");
  EXPECT_EQ(resp->body, "404 Not Found");
}