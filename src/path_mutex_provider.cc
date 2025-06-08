#include "path_mutex_provider.h"

// Initialize static members
std::map<std::string, std::unique_ptr<std::mutex>> PathMutexProvider::path_to_mutex_map_;
std::mutex PathMutexProvider::map_access_mutex_;

// Gets or creates a mutex for the given resource path to ensure synchronized access across threads
std::mutex& PathMutexProvider::get_mutex_for_path(const std::string& resource_path) {
    std::lock_guard<std::mutex> guard(map_access_mutex_);

    auto it = path_to_mutex_map_.find(resource_path);
    if (it == path_to_mutex_map_.end()) {
        // If no mutex exists for this path, create one and add it to the map
        it = path_to_mutex_map_.emplace(resource_path, std::make_unique<std::mutex>()).first;
    }

    return *(it->second);   // reference to the mutex associated with this resource_path
}