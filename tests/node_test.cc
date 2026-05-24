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

#include "node.h"

#include <gtest/gtest.h>
#include <sys/stat.h>
#include <iostream>
#include <sstream>

namespace {

TEST(NodeTest, StaticSize) {
  EXPECT_LE(sizeof(Node), 256);
}

TEST(NodeTest, Basic) {
  Node root{};
  root.name = "/";
  root.mode = S_IFDIR | 0755;
  root.ComputePathHash();

  EXPECT_TRUE(root.IsRoot());
  EXPECT_TRUE(root.IsDir());
  EXPECT_EQ(root.GetType(), FileType::Directory);
  EXPECT_EQ(root.GetPath(), "/");
  EXPECT_TRUE(root.HasPath("/"));

  Node child{};
  child.name = "foo";
  child.mode = S_IFREG | 0644;
  child.parent = &root;
  child.ComputePathHash();

  EXPECT_FALSE(child.IsRoot());
  EXPECT_FALSE(child.IsDir());
  EXPECT_EQ(child.GetType(), FileType::File);
  EXPECT_EQ(child.GetPath(), "/foo");
  EXPECT_TRUE(child.HasPath("/foo"));
  EXPECT_FALSE(child.HasPath("/bar"));
}

TEST(NodeTest, GetFileType) {
  EXPECT_EQ(GetFileType(S_IFREG | 0644), FileType::File);
  EXPECT_EQ(GetFileType(S_IFDIR | 0755), FileType::Directory);
  EXPECT_EQ(GetFileType(S_IFLNK), FileType::Symlink);
  EXPECT_EQ(GetFileType(S_IFIFO), FileType::Fifo);
  EXPECT_EQ(GetFileType(S_IFSOCK), FileType::Socket);
  EXPECT_EQ(GetFileType(S_IFBLK), FileType::BlockDevice);
  EXPECT_EQ(GetFileType(S_IFCHR), FileType::CharDevice);
}

TEST(NodeTest, OutputOperator) {
  Node n{};
  n.name = "/";
  n.mode = S_IFDIR | 0755;
  n.parent = nullptr;

  std::ostringstream oss;
  oss << n;
  EXPECT_TRUE(oss.str().find("Directory") != std::string::npos);
  EXPECT_TRUE(oss.str().find("/") != std::string::npos);
}

TEST(NodeTest, GetUniqueChildDirectory) {
  Node root{};
  root.name = "/";
  root.mode = S_IFDIR | 0755;

  EXPECT_EQ(root.GetUniqueChildDirectory(), nullptr);

  {
    Node child{};
    child.name = "dir";
    child.mode = S_IFDIR | 0755;
    child.parent = &root;
    root.children.push_front(child);

    EXPECT_EQ(root.GetUniqueChildDirectory(), &child);

    {
      Node file{};
      file.name = "file";
      file.mode = S_IFREG | 0644;
      file.parent = &root;
      root.children.push_front(file);

      EXPECT_EQ(root.GetUniqueChildDirectory(), nullptr);
      root.children.pop_front();  // remove file
    }

    EXPECT_EQ(root.GetUniqueChildDirectory(), &child);
    root.children.pop_front();  // remove child
  }

  EXPECT_EQ(root.GetUniqueChildDirectory(), nullptr);
}

TEST(NodeTest, GetStat) {
  Node n{};
  n.name = "test";
  n.mode = S_IFREG | 0777;
  n.size = 1024;
  n.uid = 1000;
  n.gid = 1000;

  Node::enforce_permissions = true;
  Node::fmask = 0022;
  Stat st = n.GetStat();
  EXPECT_EQ(st.st_mode, static_cast<mode_t>(S_IFREG | 0755));
  EXPECT_EQ(st.st_uid, 1000);

  Node::enforce_permissions = false;
  st = n.GetStat();
  EXPECT_EQ(st.st_mode, static_cast<mode_t>(S_IFREG | 0755));
}

}  // namespace
