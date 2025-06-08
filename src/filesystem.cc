#include "filesystem.h"
#include "logger.h"
#include <filesystem>
#include <fstream>
#include <iostream>

// Takes a absolute or relative path and canonicalizes it
std::string Filesystem::build_path(const std::string &path) const {
  std::filesystem::path path_ = path;
  path_ = std::filesystem::weakly_canonical(path_);
  return path_.string();
}

// Checks if path exists (and is regular file if check_file == true)
bool Filesystem::exists_path(const std::string &path, bool check_file) const {
  BOOST_LOG_TRIVIAL(debug) << "Checking if path " << path << " exists";
  std::filesystem::path path_ = path;
  if (check_file) {
    return std::filesystem::exists(path) &&
           std::filesystem::is_regular_file(path); 
  }
  return std::filesystem::exists(path);
}

// Create directory and parent directories if they do not exist
bool Filesystem::create_dir(const std::string &path) const {
  std::filesystem::path path_ = path;
  return std::filesystem::create_directories(path_);
}

// Write contents to a file
bool Filesystem::write_file(const std::string &path,
                            const std::string &contents) const {
  try {
    // Create folder if doesn't exist
    std::filesystem::path path_ = path;
    std::filesystem::create_directories(path_.parent_path());

    // Open an output file stream for given file
    std::ofstream writer(path_);
    writer << contents;
    writer.close();
    return true;
  } catch (std::filesystem::filesystem_error const &e) {
    BOOST_LOG_TRIVIAL(error) << "Could not write contents to file at path "
                             << path << ": " << e.what();
  }
  return false;
}

// Read contents from a file
bool Filesystem::read_file(const std::string &path,
                           std::string &contents) const {
  try {
    // Open an input file stream for the given file
    std::ifstream reader(path);
    if (!reader.is_open()) {
      BOOST_LOG_TRIVIAL(error) << "Could not open file for reading at path: " << path;
      return false;
    }

    // Read entire file into a buffer
    std::ostringstream buffer;
    buffer << reader.rdbuf();
    contents = buffer.str();

    // Close the stream and return success
    reader.close();
    return true;
  } catch (std::filesystem::filesystem_error const &e) {
    BOOST_LOG_TRIVIAL(error) << "Filesystem error when reading file at path "
                             << path << ": " << e.what();
  } catch (std::exception const &e) {
    BOOST_LOG_TRIVIAL(error) << "General error when reading file at path "
                             << path << ": " << e.what();
  }
  return false;
}

// List filenames in a directory
bool Filesystem::list_files(const std::string &path,
                          std::vector<std::string> &filenames) const {
  try {
    // Canonicalize the path to ensure consistent behavior
    std::filesystem::path dir_path = std::filesystem::weakly_canonical(path);

    // Check if the path exists and is a directory
    if (!std::filesystem::exists(dir_path) || !std::filesystem::is_directory(dir_path)) {
      BOOST_LOG_TRIVIAL(error) << "Path is not a directory or does not exist: " << path;
      return false;
    }

    // Iterate through directory entries
    for (const auto &entry : std::filesystem::directory_iterator(dir_path)) {
      // Only include regular files (skip directories, symlinks, etc.)
      if (std::filesystem::is_regular_file(entry.status())) {
        // Add the filename (not full path) to the result vector
        filenames.push_back(entry.path().filename().string());
      }
    }

    return true; // Successfully listed files
  } catch (std::filesystem::filesystem_error const &e) {
    BOOST_LOG_TRIVIAL(error) << "Filesystem error when listing directory " << path
                             << ": " << e.what();
  } catch (std::exception const &e) {
    BOOST_LOG_TRIVIAL(error) << "General error when listing directory " << path
                             << ": " << e.what();
  }
  return false; // Return false on any exception
}

// Delete a file
bool Filesystem::delete_file(const std::string &path) const {
  try {
    std::filesystem::path path_ = std::filesystem::weakly_canonical(path);

    // Check if file exists and is a regular file
    if (!std::filesystem::exists(path_) || !std::filesystem::is_regular_file(path_)) {
      BOOST_LOG_TRIVIAL(warning) << "File not found or is not a regular file: " << path;
      return false;
    }

    // Attempt to delete the file
    if (std::filesystem::remove(path_)) {
      BOOST_LOG_TRIVIAL(info) << "Successfully deleted file: " << path_;
      return true;
    } else {
      BOOST_LOG_TRIVIAL(error) << "Failed to delete file (no error thrown): " << path_;
      return false;
    }
  } catch (const std::filesystem::filesystem_error &e) {
    BOOST_LOG_TRIVIAL(error) << "Filesystem error when deleting file at path "
                             << path << ": " << e.what();
  } catch (const std::exception &e) {
    BOOST_LOG_TRIVIAL(error) << "General error when deleting file at path "
                             << path << ": " << e.what();
  }
  return false;
}
