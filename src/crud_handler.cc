#include "crud_handler.h"
#include "filesystem.h"
#include "http_request.h"
#include "http_response.h"
#include "logger.h"
#include "registry.h"
#include "request_handler.h"
#include "path_mutex_provider.h"
#include <filesystem>
#include <memory>
#include <random>
#include <sstream>
#include <unordered_map>

// Initialize CRUD handler with the given API prefix and data directory path
CRUDHandler::CRUDHandler(const std::string &path, const std::string &data_dir,
                         std::unique_ptr<IFilesystem> fs)
    : prefix_(std::move(path)), data_path_(std::move(data_dir)),
      fs_(std::move(fs)) {
  // Create directory and parent directories they do not exist
  fs_->create_dir(data_dir);
}

std::unique_ptr<HttpResponse>
CRUDHandler::handle_request(const HttpRequest &req) {
  // Split request URI into segments by '/' delimiter
  // Remove leading slash from path and parse URL into segments
  std::stringstream ss(req.path.substr(1));

  // Split the path (e.g. /api/Shoes/123) into ["api", "Shoes", "123"]
  std::vector<std::string> segments;
  std::string segment;
  while (std::getline(ss, segment, '/')) {
    segments.push_back(segment);
  }
  // Should be ['api', 'ENTITY', 'ID?']
  // Error if:
  // - not at least two segments
  if (segments.size() >= 2) {
    std::string entity = segments[1];
    // Must have third segment for ID
    unsigned int id = 0;
    // Attempt to parse the third segment as an unsigned integer ID
    if (segments.size() >= 3) {
      BOOST_LOG_TRIVIAL(debug) << "SEGMENT" << segments[2]; // Log raw ID string for debugging purposes
      try {
        id = std::stoul(segments[2]);
      } catch (std::invalid_argument const &e) {  // Log and skip malformed ID that cannot be converted to unsigned int
        BOOST_LOG_TRIVIAL(error) << "Entity id not an integer: " << e.what();
      } catch (std::out_of_range const &e) {
        BOOST_LOG_TRIVIAL(error) << "Entity id out of range: " << e.what();
      }
    }
    // Delegate to subhandler
    if (req.method == "GET" && segments.size() == 2) { // Handle GET with only entity type -> list all IDs for the entity
      BOOST_LOG_TRIVIAL(info) << "CRUD list operation";
      return handle_get_all(entity);
    } else if (req.method == "GET" && segments.size() >= 3) { // Handle GET with entity and ID -> return specific entity instance
      BOOST_LOG_TRIVIAL(info) << "CRUD retrieve operation for entity " << entity;
      return handle_get(entity, id);
    } else if (req.method == "POST") { // Handle POST -> create new instance of the entity and return generated ID
      BOOST_LOG_TRIVIAL(info) << "CRUD create operation for entity " << entity;
      return handle_post(entity, req.body);
    } else if (req.method == "PUT") {  // Handle PUT -> create or update the entity instance with a specific ID
      return handle_put(entity, id, req.body);
    } else if (req.method == "DELETE") {
      BOOST_LOG_TRIVIAL(info) << "CRUD delete operation for entity " << entity << " with ID " << id;
      return handle_delete(entity, id);
    }
  }
  return std::make_unique<HttpResponse>(HttpResponse::Stock404());
}

unique_ptr<HttpResponse> CRUDHandler::handle_get(const string &entity,
                                                 unsigned int id) {
  std::lock_guard<std::mutex> lock(PathMutexProvider::get_mutex_for_path(this->data_path_));

  auto resp = std::make_unique<HttpResponse>();
  // File directory path
  std::string file_path = fs_->build_path(data_path_ + "/" + entity + "/" + std::to_string(id));
  // Check if the file exists
  if (!fs_->exists_path(file_path, true)) {
    BOOST_LOG_TRIVIAL(warning) << "GET request failed: File not found at " << file_path;
    return std::make_unique<HttpResponse>(HttpResponse::Stock404());
  }
  // Read file contents
  std::string contents;
  if (!fs_->read_file(file_path, contents)) {
    BOOST_LOG_TRIVIAL(warning) << "GET request failed: Failed to read file contents " << file_path;
    return std::make_unique<HttpResponse>(HttpResponse::Stock500());
  }
  BOOST_LOG_TRIVIAL(info) << "GET request succeeded: Retrieved file " << file_path;
  // Construct response
  resp->version = "HTTP/1.1";
  resp->status_code = 200;
  resp->reason = "OK";
  resp->headers["Content-Type"] = "application/json";
  resp->body = contents;
  return resp;
}

unique_ptr<HttpResponse> CRUDHandler::handle_get_all(const string &entity) {
  std::lock_guard<std::mutex> lock(PathMutexProvider::get_mutex_for_path(this->data_path_));

  auto resp = std::make_unique<HttpResponse>();
  // Entity directory path
  std::string entity_path = fs_->build_path(data_path_ + "/" + entity);
  // Check if the entity directory exists
  if (!fs_->exists_path(entity_path, false)) {
    BOOST_LOG_TRIVIAL(warning) << "GET all failed: Directory not found at " << entity_path;
    return std::make_unique<HttpResponse>(HttpResponse::Stock404());
  }
  // List all files in the directory
  std::vector<std::string> filenames;
  if (!fs_->list_files(entity_path, filenames)) {
    BOOST_LOG_TRIVIAL(error) << "GET all failed: Unable to list directory " << entity_path;
    return std::make_unique<HttpResponse>(HttpResponse::Stock500());
  }
  // Build JSON array of IDs
  std::ostringstream json;
  json << "[";
  for (size_t i = 0; i < filenames.size(); ++i) {
    if (i > 0) json << ",";
    json << "\"" << filenames[i] << "\"";
  }
  json << "]";
  BOOST_LOG_TRIVIAL(info) << "GET all succeeded for entity " << entity;
  resp->version = "HTTP/1.1";
  resp->status_code = 200;
  resp->reason = "OK";
  resp->headers["Content-Type"] = "application/json";
  resp->body = json.str();
  return resp;
}

unique_ptr<HttpResponse> CRUDHandler::handle_post(const string &entity,
                                                  const string &body) {
  std::lock_guard<std::mutex> lock(PathMutexProvider::get_mutex_for_path(this->data_path_));

  // Define the directory for storing new instances of this entity
  std::string path = fs_->build_path(data_path_ + "/" + entity);
  // Create random ID that does not already exist
  std::random_device rd;
  unsigned int id = rd();
  std::string id_path = path + "/" + std::to_string(id);
  while (fs_->exists_path(id_path, true)) {
    id = rd();
    id_path = path + "/" + std::to_string(id);
  }
  BOOST_LOG_TRIVIAL(info) << "Generated new unique entity id " << id;
  // Save file at [data_dir]/[entity]/[id]
  fs_->write_file(id_path, body);
  BOOST_LOG_TRIVIAL(info) << "Wrote file to " << id_path;
  // Write response
  auto resp = std::make_unique<HttpResponse>();
  resp->version = "HTTP/1.1";
  resp->status_code = 201;
  resp->reason = "Created";
  resp->headers["Content-Type"] = "application/json";
  // Return JSON response with newly assigned ID
  resp->body = R"({"id": )" + std::to_string(id) + "}";
  return resp;
}

unique_ptr<HttpResponse> CRUDHandler::handle_put(const string &entity,
                                                 unsigned int id,
                                                 const string &body) {
  std::lock_guard<std::mutex> lock(PathMutexProvider::get_mutex_for_path(this->data_path_));

  // Entity path
  std::string id_path =
      fs_->build_path(data_path_ + "/" + entity + "/" + std::to_string(id));
  // Check if this PUT creates a new resource or updates an existing one
  bool new_resource = !fs_->exists_path(id_path, true);
  // Update file at [data_dir]/[entity]/[id]
  fs_->write_file(id_path, body);
  BOOST_LOG_TRIVIAL(info) << "Wrote file to " << id_path;
  // Write response
  auto resp = std::make_unique<HttpResponse>();
  resp->version = "HTTP/1.1";
  resp->status_code = new_resource ? 201 : 204;  // Return 201 if created, 204 if updated with no additional content
  resp->reason = new_resource ? "Created" : "No Content";
  return resp;
}

unique_ptr<HttpResponse> CRUDHandler::handle_delete(const string &entity,
                                                    unsigned int id) {
  std::lock_guard<std::mutex> lock(PathMutexProvider::get_mutex_for_path(this->data_path_));

  auto resp = std::make_unique<HttpResponse>();
  // Entity path
  std::string file_path = fs_->build_path(data_path_ + "/" + entity + "/" + std::to_string(id));
  // Check if the file exists before deleting
  if (!fs_->exists_path(file_path, true)) {
    BOOST_LOG_TRIVIAL(warning) << "DELETE failed: File not found at " << file_path;
    return std::make_unique<HttpResponse>(HttpResponse::Stock404());
  }
  // Try to delete the file
  if (!fs_->delete_file(file_path)) {
    BOOST_LOG_TRIVIAL(error) << "DELETE failed: Unable to delete file at " << file_path;
    return std::make_unique<HttpResponse>(HttpResponse::Stock500());
  }
  BOOST_LOG_TRIVIAL(info) << "DELETE succeeded: Deleted file " << file_path;
  // Return 204 No Content
  resp->version = "HTTP/1.1";
  resp->status_code = 204;
  resp->reason = "No Content";
  return resp;
}

std::unique_ptr<RequestHandler>
CreateCRUDHandler(const std::string &path,
                  const std::unordered_map<std::string, std::string> &args) {
  auto it = args.find("data_path");
  if (it == args.end()) {
    BOOST_LOG_TRIVIAL(error) << "CRUDHandler missing 'data_path'";
    return std::unique_ptr<RequestHandler>(nullptr);
  }
  std::string data_dir = std::filesystem::absolute(it->second).string();
  return std::make_unique<CRUDHandler>(path, data_dir,
                                       std::make_unique<Filesystem>());
}

REGISTER_HANDLER(CRUDHandler, CreateCRUDHandler);

void TouchCRUDHandler() {}

