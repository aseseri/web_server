#pragma once

#include <string>
#include <vector>

class IFilesystem {
public:
  virtual ~IFilesystem() = default;

  // Construct a path (e.g. resolve relative path)
  virtual std::string build_path(const std::string &path) const = 0;

  // Checks if path exists (and is regular file if check_file == true)
  virtual bool exists_path(const std::string &path, bool check_file) const = 0;

  // Create a directory at a given path
  virtual bool create_dir(const std::string &path) const = 0;

  // Write contents to a file at path
  virtual bool write_file(const std::string &path,
                          const std::string &contents) const = 0;

  // Read contents of a file at path
  virtual bool read_file(const std::string &path,
                         std::string &contents) const = 0;
  
  // List filenames in a directory
  virtual bool list_files(const std::string &path,
                          std::vector<std::string> &filenames) const = 0;

  // Delete file at path
  virtual bool delete_file(const std::string &path) const = 0;
};

class Filesystem : public IFilesystem {
public:
  std::string build_path(const std::string &path) const override;

  bool exists_path(const std::string &path, bool check_file) const override;

  bool create_dir(const std::string &path) const override;

  bool write_file(const std::string &path,
                  const std::string &contents) const override;

  bool read_file(const std::string &path, std::string &contents) const override;

  bool list_files(const std::string &path,
                std::vector<std::string> &filenames) const override;

  bool delete_file(const std::string &path) const override;
};
