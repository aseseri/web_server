#pragma once
#include <string>
#include <mutex>
#include <map>
#include <memory>

class PathMutexProvider {
public:
    static std::mutex& get_mutex_for_path(const std::string& resource_path);

private:
    // Stores a map from a resource path string to a unique_ptr holding the mutex for that path
    static std::map<std::string, std::unique_ptr<std::mutex>> path_to_mutex_map_;
    // A mutex to protect concurrent access and modification of the path_to_mutex_map_ itself
    static std::mutex map_access_mutex_;
};