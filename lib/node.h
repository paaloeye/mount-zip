// Copyright 2021 Google LLC
// Copyright 2008-2021 Alexander Galanin <al@galanin.nnov.ru>
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

#ifndef NODE_H
#define NODE_H

#include <cassert>
#include <functional>
#include <iterator>
#include <memory>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

#include <sys/stat.h>
#include <unistd.h>
#include <zip.h>

#include <boost/intrusive/slist.hpp>
#include <boost/intrusive/unordered_set.hpp>

#include "path.h"
#include "reader.h"

namespace bi = boost::intrusive;

struct HashedStringView {
  std::string_view string;
  size_t hash;

  HashedStringView(std::string_view s);
  HashedStringView(std::string_view s, size_t h) : string(s), hash(h) {}
};

enum class FileType : mode_t {
  Unknown = 0,            // Unknown
  BlockDevice = S_IFBLK,  // Block-oriented device
  CharDevice = S_IFCHR,   // Character-oriented device
  Directory = S_IFDIR,    // Directory
  Fifo = S_IFIFO,         // FIFO or pipe
  File = S_IFREG,         // Regular file
  Socket = S_IFSOCK,      // Socket
  Symlink = S_IFLNK,      // Symbolic link
};

inline FileType GetFileType(const mode_t mode) {
  return FileType(mode & S_IFMT);
}

inline void SetFileType(mode_t* const mode, const FileType type) {
  assert(mode);
  *mode &= ~static_cast<mode_t>(S_IFMT);
  *mode |= static_cast<mode_t>(type);
}

std::ostream& operator<<(std::ostream& out, FileType t);

// Type aliases for shorter code.
using Stat = struct stat;

// A node of the virtual file system: either a directory or a file.
struct Node {
  // Nodes are dynamically allocated and passed around by unique_ptr when
  // the ownership is transferred.
  using Ptr = std::unique_ptr<Node>;

  // Constants and settings shared by all nodes.
  static const blksize_t block_size = 512;
  static const timespec g_now;
  static const uid_t g_uid;
  static const gid_t g_gid;
  static mode_t fmask;
  static mode_t dmask;
  static bool original_permissions;
  static ino_t ino_count;

#ifdef NDEBUG
  using LinkMode = bi::link_mode<bi::normal_link>;
#else
  using LinkMode = bi::link_mode<bi::safe_link>;
#endif

  // --- 16-byte members (Highest alignment) ---

  timespec mtime = g_now;
  timespec atime = g_now;
  timespec ctime = g_now;

  // --- 8-byte members (Fixed size) ---

  // Index of the entry represented by this node in the ZIP archive, or -1 if it
  // is not directly represented in the ZIP archive (like the root directory, or
  // any intermediate directory).
  i64 id = -1;

  // Inode-specific data.
  ino_t ino = ++ino_count;

  // Number of bytes of this file.
  i64 size = 0;

  // Device number for special files.
  dev_t dev = 0;

  // --- Architecture-dependent members (8 bytes on 64-bit, 4 bytes on 32-bit)

  // Reference to the ZIP archive.
  zip_t* zip = nullptr;

  // If this Node is a hardlink, this points to the target node.
  Node* hardlink_target = nullptr;

  // Pointer to the parent node. Should be non null. The only exception is the
  // root directory which has a null parent pointer.
  Node* parent = nullptr;

  Reader::Ptr reader;

  size_t path_length = 0;
  size_t path_hash = 0;

  // --- Intrusive Hooks (8-24 bytes on 64-bit, 4-12 bytes on 32-bit) ---

  // Hook used to index Nodes by parent.
  using ByParent = bi::slist_member_hook<LinkMode>;
  ByParent by_parent;

  // Children of this node. The children are not sorted and their order is not
  // relevant. This collection doesn't own the children nodes. The |parent|
  // pointer of every child in |children| should point back to this node.
  using Children = bi::slist<Node,
                             bi::member_hook<Node, ByParent, &Node::by_parent>,
                             bi::constant_time_size<false>,
                             bi::linear<true>,
                             bi::cache_last<true>>;
  Children children;

  // Hook used to index Nodes by full path.
  using ByPath = bi::unordered_set_member_hook<LinkMode, bi::store_hash<false>>;
  ByPath by_path;

  // --- Remaining members (Strings and 4-byte types) ---

  // Name of this node in the context of its parent. This name should be a valid
  // and non-empty filename, and it shouldn't contain any '/' separator. The
  // only exception is the root directory, which is just named "/".
  std::string name;

  // Link target (e.g. for symlinks or hardlinks).
  std::string target;

  uid_t uid = g_uid;
  gid_t gid = g_gid;
  mode_t mode = 0;
  nlink_t nlink = 1;

  // Number of entries whose name have initially collided with this node.
  int collision_count = 0;

  // Returns true if this node is the root directory of the virtual file system.
  bool IsRoot() const { return !parent; }

  // Returns the FileType (e.g. Regular file, Directory, Symlink) of this node.
  FileType GetType() const { return GetFileType(mode); }

  // Returns true if this node is a directory.
  bool IsDir() const { return S_ISDIR(mode); }

  // Initialize from ZIP entry.
  void Init();

  // Adds a child node to this directory node.
  // This directory becomes the owner of the child's parent pointer.
  // Precondition: IsDir() is true.
  void AddChild(Node* child);

  // Recomputes this Node's path length and hash based on its name and its
  // parent's path length and hash. This is used for fast lookup by path.
  void ComputePathHash();

  // Returns the number of blocks used by this node in the virtual file system.
  // This accounts for the file size and any holes (sparse regions).
  i64 GetBlockCount() const { return (size + block_size - 1) / block_size; }

  // Returns a pointer to the actual data node. If this node is a hardlink,
  // returns the target node; otherwise returns 'this'.
  const Node* GetTarget() const { return hardlink_target ?: this; }
  Node* GetTarget() { return hardlink_target ?: this; }

  // Returns the POSIX 'struct stat' representation of this node's metadata.
  Stat GetStat() const;

  // Returns the full absolute path of this node within the virtual file system.
  std::string GetPath() const;

  // Returns true if this node's full absolute path matches the given string.
  bool HasPath(std::string_view path) const;

  // If this node is a directory which only has one child which is also a
  // directory, returns a pointer to that child. Otherwise returns nullptr.
  // This is used for tree trimming/optimization.
  Node* GetUniqueChildDirectory();

  bool CacheAll(std::function<void(ssize_t)> progress = {});

  // Gets a Reader to read file contents.
  Reader::Ptr GetReader();
};

// Formats a Node for logging output (e.g. "File /path/to/file").
std::ostream& operator<<(std::ostream& out, const Node& n);

#endif  // NODE_H
