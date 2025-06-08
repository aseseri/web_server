#pragma once
#include "filesystem.h"
#include "http_response.h"
#include "request_handler.h"
#include <unordered_set>

using namespace std;

// This handler handles CRUD operations on entities
class CRUDHandler : public RequestHandler {
public:
  CRUDHandler(const string &path, const string &data_dir,
              unique_ptr<IFilesystem> fs);
  unique_ptr<HttpResponse> handle_request(const HttpRequest &req) override;

private:
  // Retrieve entity (GET request)
  // ```
  // GET /api/Shoes/1
  // ```
  unique_ptr<HttpResponse> handle_get(const string &entity, unsigned int id);
  // List all valid entity IDs (GET request)
  // ```
  // GET /api/Shoes
  // ```
  unique_ptr<HttpResponse> handle_get_all(const string &entity);
  // Create entity (POST request)
  // ```
  // POST /api/Shoes
  // ```
  unique_ptr<HttpResponse> handle_post(const string &entity,
                                       const string &body);
  // Update entity (PUT request)
  // ```
  // PUT /api/Shoes/1
  // ```
  unique_ptr<HttpResponse> handle_put(const string &entity, unsigned int id,
                                      const string &body);
  // Delete entity (DELETE request)
  // ```
  // DELETE /api/Shoes/1
  // ```
  unique_ptr<HttpResponse> handle_delete(const string &entity, unsigned int id);

  // Prefix of CRUD handler (e.g. /api)
  // and data path to save and serve JSON data
  string prefix_, data_path_;

  unique_ptr<IFilesystem> fs_;
};
