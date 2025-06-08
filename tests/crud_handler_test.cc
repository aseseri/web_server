#include "crud_handler.h"
#include "filesystem.h"
#include "http_request.h"
#include "http_response.h"
#include "gtest/gtest.h"
#include <gmock/gmock.h>
#include <thread>
#include <chrono>

using namespace ::testing;
class MockFilesystem : public IFilesystem {
public:
  MOCK_METHOD(std::string, build_path, (const std::string &path),
              (const, override));

  MOCK_METHOD(bool, exists_path, (const std::string &path, bool check_file),
              (const, override));

  MOCK_METHOD(bool, create_dir, (const std::string &path), (const, override));

  MOCK_METHOD(bool, write_file,
              (const std::string &path, const std::string &contents),
              (const, override));

  MOCK_METHOD(bool, read_file, (const std::string &path, std::string &contents),
              (const, override));

  MOCK_METHOD(bool, list_files,
              (const std::string &path, std::vector<std::string> &filenames),
              (const, override));

  MOCK_METHOD(bool, delete_file, (const std::string &path), (const, override));

  // File system state
  // All paths
  mutable std::unordered_set<std::string> paths_;
  // Files in a directory
  mutable std::unordered_map<std::string, std::vector<std::string>>
      directories_;
  // Contents in a file
  mutable std::unordered_map<std::string, std::string> files_;

  MockFilesystem() {
    // Quiet warnings
    EXPECT_CALL(*this, build_path).Times(AnyNumber());
    EXPECT_CALL(*this, exists_path).Times(AnyNumber());
    EXPECT_CALL(*this, create_dir).Times(AnyNumber());
    EXPECT_CALL(*this, write_file).Times(AnyNumber());
    EXPECT_CALL(*this, read_file).Times(AnyNumber());

    // Just return the path (i.e. simulate as if all paths relative)
    ON_CALL(*this, build_path(_)).WillByDefault([](const std::string &path) {
      return path;
    });

    // Check our set of paths if it exists
    ON_CALL(*this, exists_path(_, _))
        .WillByDefault([this](const std::string &path, bool check_file) {
          if (check_file)
            return files_.count(path);
          return paths_.count(path);
        });

    // Create a path and return if it existed or not
    ON_CALL(*this, create_dir(_))
        .WillByDefault([this](const std::string &path) {
          return paths_.insert(path).second;
        });

    // Create file, add to paths, and register its contents
    ON_CALL(*this, write_file(_, _))
        .WillByDefault([this](const std::string &path,
                              const std::string &contents) {
          files_[path] = contents;
          paths_.insert(path);
          if (auto slash = path.find_last_of('/'); slash != std::string::npos) {
            std::string parent = path.substr(0, slash);
            std::string filename = path.substr(slash + 1);
            directories_[parent].push_back(filename);
          }
          return true;
        });

    // Read contents if file exists
    ON_CALL(*this, read_file(_, _))
        .WillByDefault([this](const std::string &path, std::string &contents) {
          auto it = files_.find(path);
          if (it != files_.end()) {
            contents = it->second;
            return true;
          }
          return false;
        });

    // Get all files in directory
    ON_CALL(*this, list_files(_, _))
        .WillByDefault([this](const std::string &path,
                              std::vector<std::string> &filenames) {
          auto it = directories_.find(path);
          if (it != directories_.end()) {
            filenames = it->second;
            return true;
          }
          return false;
        });

    // Delete file from paths
    ON_CALL(*this, delete_file(_))
        .WillByDefault([this](const std::string &path) {
          bool removed = (paths_.erase(path) > 0);
          files_.erase(path);
          if (auto slash = path.find_last_of('/'); slash != std::string::npos) {
            std::string parent = path.substr(0, slash);
            std::string filename = path.substr(slash + 1);
            auto it = directories_.find(parent);
            if (it != directories_.end()) {
              auto &files = it->second;
              files.erase(std::remove(files.begin(), files.end(), filename),
                          files.end());
            }
          }
          return removed;
        });
  }
};

// test fixture for CRUDHandler
// empty because need to create new handler and reset mock filesystem each time
class CRUDHandlerTest : public ::testing::Test {
protected:
};

// Repeated create requests
TEST_F(CRUDHandlerTest, HandlesPostRequest) {
  CRUDHandler h{"/api", "/mnt/crud", std::make_unique<MockFilesystem>()};
  std::string raw = "POST /api/Shoes HTTP/1.1\r\nHost: example\r\n\r\nBODY";
  HttpRequest req = HttpRequest::Parse(raw);
  auto resp = h.handle_request(req);

  EXPECT_EQ(resp->version, "HTTP/1.1");
  EXPECT_EQ(resp->status_code, 201);
  EXPECT_EQ(resp->reason, "Created");
  auto it = resp->headers.find("Content-Type");
  ASSERT_NE(it, resp->headers.end());
  EXPECT_EQ(it->second, "application/json");
  EXPECT_THAT(resp->body, MatchesRegex(R"(\{"id": [0-9]+\})"));

  resp = h.handle_request(req);
  EXPECT_EQ(resp->version, "HTTP/1.1");
  EXPECT_EQ(resp->status_code, 201);
  EXPECT_EQ(resp->reason, "Created");
  it = resp->headers.find("Content-Type");
  ASSERT_NE(it, resp->headers.end());
  EXPECT_EQ(it->second, "application/json");
  EXPECT_THAT(resp->body, MatchesRegex(R"(\{"id": [0-9]+\})"));
}

// Retrieve request
TEST_F(CRUDHandlerTest, HandlesGetRequest) {
  CRUDHandler h{"/api", "/mnt/crud", std::make_unique<MockFilesystem>()};

  // First, create the resource
  std::string put_raw = "PUT /api/Shoes/42 HTTP/1.1\r\nHost: example\r\n\r\nSneakers";
  HttpRequest put_req = HttpRequest::Parse(put_raw);
  auto resp = h.handle_request(put_req);
  EXPECT_EQ(resp->status_code, 201);

  // GET request
  std::string get_raw = "GET /api/Shoes/42 HTTP/1.1\r\nHost: example\r\n\r\n";
  HttpRequest get_req = HttpRequest::Parse(get_raw);
  resp = h.handle_request(get_req);
  EXPECT_EQ(resp->version, "HTTP/1.1");
  EXPECT_EQ(resp->status_code, 200);
  EXPECT_EQ(resp->reason, "OK");
  EXPECT_EQ(resp->headers.at("Content-Type"), "application/json");
  EXPECT_EQ(resp->body, "Sneakers");
}

// Retrieve non-existent resource
TEST_F(CRUDHandlerTest, HandlesGetNonexistentResource) {
  CRUDHandler h{"/api", "/mnt/crud", std::make_unique<MockFilesystem>()};

  std::string get_raw = "GET /api/Shoes/123 HTTP/1.1\r\nHost: example\r\n\r\n";
  HttpRequest get_req = HttpRequest::Parse(get_raw);
  auto resp = h.handle_request(get_req);

  EXPECT_EQ(resp->version, "HTTP/1.1");
  EXPECT_EQ(resp->status_code, 404);
  EXPECT_EQ(resp->reason, "Not Found");
  EXPECT_EQ(resp->headers.at("Content-Type"), "text/plain");
  EXPECT_EQ(resp->body, "404 Not Found");
}

// Repeated update requests
TEST_F(CRUDHandlerTest, HandlesPutRequest) {
  CRUDHandler h{"/api", "/mnt/crud", std::make_unique<MockFilesystem>()};

  // Verify doesn't exist with GET
  std::string raw = "GET /api/Shoes/1 HTTP/1.1\r\nHost: example\r\n\r\n";
  HttpRequest req = HttpRequest::Parse(raw);
  auto resp = h.handle_request(req);
  EXPECT_EQ(resp->version, "HTTP/1.1");
  EXPECT_EQ(resp->status_code, 404);
  EXPECT_EQ(resp->reason, "Not Found");

  raw = "PUT /api/Shoes/1 HTTP/1.1\r\nHost: example\r\n\r\nBODY";
  req = HttpRequest::Parse(raw);
  resp = h.handle_request(req);

  EXPECT_EQ(resp->version, "HTTP/1.1");
  EXPECT_EQ(resp->status_code, 201);
  EXPECT_EQ(resp->reason, "Created");

  // Next call to same object should return 204 to indicate already existed
  resp = h.handle_request(req);

  EXPECT_EQ(resp->version, "HTTP/1.1");
  EXPECT_EQ(resp->status_code, 204);
  EXPECT_EQ(resp->reason, "No Content");

  // Verify body with GET
  raw = "GET /api/Shoes/1 HTTP/1.1\r\nHost: example\r\n\r\n";
  req = HttpRequest::Parse(raw);
  resp = h.handle_request(req);
  EXPECT_EQ(resp->version, "HTTP/1.1");
  EXPECT_EQ(resp->status_code, 200);
  EXPECT_EQ(resp->reason, "OK");
  EXPECT_EQ(resp->body, "BODY");
}

// Delete request
TEST_F(CRUDHandlerTest, HandlesDeleteRequest) {
  auto mock_fs = std::make_unique<MockFilesystem>();
  mock_fs->create_dir("/mnt/crud/Shoes");  // Ensure the directory exists
  CRUDHandler h{"/api", "/mnt/crud", std::move(mock_fs)};

  // First, create the resource
  std::string put_raw = "PUT /api/Shoes/99 HTTP/1.1\r\nHost: example\r\n\r\nLoafers";
  HttpRequest put_req = HttpRequest::Parse(put_raw);
  auto resp = h.handle_request(put_req);
  EXPECT_EQ(resp->status_code, 201);

  // Confirm resource exists via GET
  std::string get_raw = "GET /api/Shoes/99 HTTP/1.1\r\nHost: example\r\n\r\n";
  HttpRequest get_req = HttpRequest::Parse(get_raw);
  resp = h.handle_request(get_req);
  EXPECT_EQ(resp->status_code, 200);
  EXPECT_EQ(resp->body, "Loafers");

  // DELETE the resource
  std::string delete_raw = "DELETE /api/Shoes/99 HTTP/1.1\r\nHost: example\r\n\r\n";
  HttpRequest delete_req = HttpRequest::Parse(delete_raw);
  resp = h.handle_request(delete_req);
  EXPECT_EQ(resp->version, "HTTP/1.1");
  EXPECT_EQ(resp->status_code, 204);  // No Content
  EXPECT_EQ(resp->reason, "No Content");
  EXPECT_TRUE(resp->body.empty());  // Body should be empty

  // Confirm resource is gone via GET
  resp = h.handle_request(get_req);
  EXPECT_EQ(resp->status_code, 404);
  EXPECT_EQ(resp->reason, "Not Found");
  EXPECT_EQ(resp->headers.at("Content-Type"), "text/plain");
  EXPECT_EQ(resp->body, "404 Not Found");
}

// List request
TEST_F(CRUDHandlerTest, HandlesRepeatedGetCollectionRequest) {
  auto mock_fs = std::make_unique<MockFilesystem>();
  mock_fs->create_dir("/mnt/crud/Shoes");  // Ensure the directory exists
  CRUDHandler h{"/api", "/mnt/crud", std::move(mock_fs)};

  // Create two resources
  std::string put1_raw = "PUT /api/Shoes/1 HTTP/1.1\r\nHost: example\r\n\r\nRed shoes";
  std::string put2_raw = "PUT /api/Shoes/2 HTTP/1.1\r\nHost: example\r\n\r\nBlue shoes";
  HttpRequest put1_req = HttpRequest::Parse(put1_raw);
  HttpRequest put2_req = HttpRequest::Parse(put2_raw);
  auto resp = h.handle_request(put1_req);
  EXPECT_EQ(resp->status_code, 201);
  resp = h.handle_request(put2_req);
  EXPECT_EQ(resp->status_code, 201);

  // First GET-all request
  std::string get_raw = "GET /api/Shoes HTTP/1.1\r\nHost: example\r\n\r\n";
  HttpRequest get_req = HttpRequest::Parse(get_raw);
  resp = h.handle_request(get_req);
  EXPECT_EQ(resp->version, "HTTP/1.1");
  EXPECT_EQ(resp->status_code, 200);
  EXPECT_EQ(resp->reason, "OK");
  EXPECT_EQ(resp->headers.at("Content-Type"), "application/json");
  EXPECT_THAT(resp->body, HasSubstr("\"1\""));
  EXPECT_THAT(resp->body, HasSubstr("\"2\""));
}

// Delete non-existent resource
TEST_F(CRUDHandlerTest, HandlesDeleteNonexistentResource) {
  CRUDHandler h{"/api", "/mnt/crud", std::make_unique<MockFilesystem>()};
  
  std::string delete_raw = "DELETE /api/Shoes/404 HTTP/1.1\r\nHost: example\r\n\r\n";
  HttpRequest delete_req = HttpRequest::Parse(delete_raw);
  auto resp = h.handle_request(delete_req);

  EXPECT_EQ(resp->status_code, 404);
  EXPECT_EQ(resp->reason, "Not Found");
  EXPECT_EQ(resp->body, "404 Not Found");
}

// Concurrent entity put requests
TEST_F(CRUDHandlerTest, ConcurrentEntityUpdates) {
    const int num_threads = 10;
    std::vector<std::thread> threads;
    std::atomic<int> successes(0);
    CRUDHandler h{"/api", "/mnt/crud", std::make_unique<MockFilesystem>()};

    // Initial create
    std::string put_raw = "PUT /api/Shoes/1 HTTP/1.1\r\nHost: example\r\n\r\nInitial";
    HttpRequest put_req = HttpRequest::Parse(put_raw);
    auto resp = h.handle_request(put_req);
    EXPECT_EQ(resp->status_code, 201);

    // Concurrent updates
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&, i]() {
            std::string update_raw = "PUT /api/Shoes/1 HTTP/1.1\r\nHost: example\r\n\r\nThread" + std::to_string(i);
            HttpRequest update_req = HttpRequest::Parse(update_raw);
            auto update_resp = h.handle_request(update_req);
            if (update_resp->status_code == 204) successes++;
        });
    }

    for (auto& t : threads) t.join();

    // Verify
    std::string get_raw = "GET /api/Shoes/1 HTTP/1.1\r\nHost: example\r\n\r\n";
    HttpRequest get_req = HttpRequest::Parse(get_raw);
    auto get_resp = h.handle_request(get_req);

    EXPECT_EQ(successes, num_threads);
    EXPECT_EQ(get_resp->status_code, 200);
    EXPECT_TRUE(get_resp->body.find("Thread") != std::string::npos);
}
