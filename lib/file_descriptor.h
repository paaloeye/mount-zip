// Copyright 2021 Google LLC
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#ifndef FILE_DESCRIPTOR_H
#define FILE_DESCRIPTOR_H

#include <utility>

#include <sys/types.h>

// A scoped file descriptor.
class FileDescriptor {
 public:
  // Closes the file descriptor if it is valid.
  ~FileDescriptor();

  explicit FileDescriptor(int fd) noexcept : fd_(fd) {}
  FileDescriptor(FileDescriptor&& other) noexcept
      : fd_(std::exchange(other.fd_, -1)) {}

  FileDescriptor& operator=(FileDescriptor other) noexcept {
    std::swap(fd_, other.fd_);
    return *this;
  }

  bool IsValid() const { return fd_ >= 0; }
  int GetDescriptor() const { return fd_; }

 private:
  // File descriptor.
  int fd_;
};

// A file mapping to memory.
class FileMapping {
 public:
  // Removes the memory mapping.
  ~FileMapping();

  // Maps a file to memory in read-only mode.
  // Throws a system_error in case of error.
  explicit FileMapping(const char* path);

  // No copy.
  FileMapping(const FileMapping&) = delete;
  FileMapping& operator=(const FileMapping&) = delete;

  // Start of the memory mapping.
  const void* data() const { return data_; }

 private:
  void* data_;
  size_t size_;
};

#endif  // SCOPED_FILE_H
