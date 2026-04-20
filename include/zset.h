#pragma once

#include "avl.h"
#include "hashtable.h"
#include <cstdint>
#include <cstddef>
#include <string>
#include <string_view>

struct ZNode {
  AVLNode avlnode;
  HNode hnode;
  double score = 0;
  std::string name;

  ZNode(std::string name_, double score_);

  ZNode(const ZNode &) = delete;
  ZNode &operator=(const ZNode &) = delete;
  ZNode(ZNode &&) = delete;
  ZNode &operator=(ZNode &&) = delete;
};

class ZSet {
 public:
  AVLNode *root = nullptr;
  HMap hmap;

  ZSet() = default;
  ~ZSet();

  ZSet(const ZSet &) = delete;
  ZSet &operator=(const ZSet &) = delete;
  ZSet(ZSet &&) = delete;
  ZSet &operator=(ZSet &&) = delete;

  bool insert(std::string name, double score);

  ZNode *lookup(std::string_view name);

  bool remove(std::string_view name);

  ZNode *seekge(double score, std::string_view name);
};

ZNode *znode_offset(ZNode *node, int64_t offset);
