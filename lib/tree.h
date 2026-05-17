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

#ifndef TREE_H
#define TREE_H

#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "node.h"

// Holds the ZIP filesystem tree.
class Tree {
 public:
  // Extraction options.
  struct Options {
    // Filename encoding in ZIP.
    // Null, empty or "auto" for automatic detection.
    const char* encoding = nullptr;

    // Include directories?
    bool include_directories = true;

    // Include symbolic links?
    bool include_symlinks = true;

    // Include hard links?
    bool include_hard_links = true;

    // Include special file types (block and character devices, FIFOs and
    // sockets)?
    bool include_special_files = true;

    // Checks the password validity on the first encrypted file found in the
    // ZIP?
    bool check_password = true;

    // Check if all the files use supported compression and encryption methods?
    bool check_compression = true;

    // Pre-cache data?
    bool pre_cache = false;

    // Merge multiple ZIPs at the root level?
    bool merge = true;

    // Trim top level if possible?
    bool trim = true;
  };

  // Opens ZIP archives and constructs the internal tree structure.
  // Throws an std::runtime_error in case of error.
  Tree(std::span<const std::string> paths, Options opts);

  // For tests.
  Tree(const std::string& path) : Tree(std::span(&path, 1), Options{}) {}

  // Finds a node in the tree using a pre-computed path hash.
  Node* FindNode(const HashedStringView& path);

  // Finds a node in the tree by its full absolute path.
  Node* FindNode(std::string_view const path) {
    return FindNode(HashedStringView(Path(path).WithoutTrailingSeparator()));
  }

  blkcnt_t GetBlockCount() const { return total_block_count_; }
  fsfilcnt_t GetNodeCount() const { return nodes_by_path_.size(); }

 private:
  struct CloseZip {
    void operator()(zip_t* z) const { zip_close(z); }
  };

  using Zip = std::unique_ptr<zip_t, CloseZip>;
  using Zips = std::vector<std::pair<Zip, std::string>>;

  // Opens the given ZIP archives.
  static Zips OpenZips(std::span<const std::string> paths);

  // Constructor.
  Tree(Zips zips, Options opts)
      : zips_(std::move(zips)), opts_(std::move(opts)) {}

  // Returned by GetEntryAttributes.
  struct EntryAttributes {
    mode_t mode;        // Unix mode
    bool is_hard_link;  // PkWare hard link flag
  };

  // Gets the UNIX mode and the PkWare hard link flag from the entry external
  // attributes field.
  EntryAttributes GetEntryAttributes(zip_t* z,
                                     i64 id,
                                     std::string_view original_path);

  // Returns a directory node for the given path, creating it and its parents if
  // missing.
  Node* GetOrCreateDirNode(std::string_view path);

  // String conversion function.
  using ToUtf8 = std::function<std::string_view(std::string_view)>;

  // Creates and attaches a node for an existing file or dir entry.
  Node* CreateFile(zip_t* z,
                   i64 id,
                   Node* parent,
                   std::string_view name,
                   mode_t mode);

  // Creates and attaches a hard link node.
  void CreateHardLink(Node* node);

  // Attaches the given |node|, renaming it if necessary to prevent name
  // collisions.
  Node* RenameIfCollision(Node::Ptr node);

  // Reads the password from the standard input.
  // Returns true if a non-empty password was read.
  bool ReadPasswordFromStdIn();

  // If necessary, ask for a password and check it on the given entry.
  // Throws ZipError if the password doesn't match.
  void CheckPassword(const Node* node);

  void Trim(Node& dest);
  void Deindex(Node& node);
  void Reindex(Node& node);

  void RehashIfNecessary();

  // Computes the optimal number of buckets for the hash tables indexing the
  // given ZIP archives.
  static size_t GetBucketCount(const Zips& zips);

  // ZIP archives.
  const Zips zips_;

  // Extraction options.
  const Options opts_;

  // Hash extractor for Node.
  struct GetHash {
    size_t operator()(const HashedStringView& hsv) const { return hsv.hash; }
    size_t operator()(const Node& node) const { return node.path_hash; }
  };

  struct HasSamePath {
    bool operator()(const Node& a, const Node& b) const {
      return a.path_hash == b.path_hash && a.parent == b.parent &&
             a.name == b.name;
    }

    bool operator()(const HashedStringView& hsv, const Node& n) const {
      return hsv.hash == n.path_hash && n.HasPath(hsv.string);
    }
  };

  using NodesByPathBase =
      bi::unordered_set<Node,
                        bi::member_hook<Node, Node::ByPath, &Node::by_path>,
                        bi::constant_time_size<true>,
                        bi::power_2_buckets<true>,
                        bi::compare_hash<false>,
                        bi::hash<GetHash>,
                        bi::equal<HasSamePath>>;

  struct NodesByPath : NodesByPathBase {
    using NodesByPathBase::NodesByPathBase;
    ~NodesByPath();
  };

  // Hash table buckets.
  std::vector<NodesByPath::bucket_type> buckets_by_path_{1 << 4};

  // Collection of all Nodes indexed by full path.
  // Owns the nodes it references.
  NodesByPath nodes_by_path_{
      {buckets_by_path_.data(), buckets_by_path_.size()}};

  std::unordered_map<std::string_view, Node*> files_by_original_path_;

  // Root node.
  Node* const root_ = RenameIfCollision(Node::Ptr(
      new Node{.name = "/", .mode = mode_t(S_IFDIR | 0777), .nlink = 2}));

  blkcnt_t total_block_count_ = 1;

  // Has the password been verified?
  bool checked_password_ = false;
};

#endif  // TREE_H
