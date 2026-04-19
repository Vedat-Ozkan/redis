#include "zset.h"

#include "common.h"

#include <algorithm>
#include <cassert>
#include <cstring>

using namespace std;

ZNode::ZNode(string name_, double score_)
    : score(score_), name(move(name_)) {
  hmap.hcode = str_hash((const uint8_t *)name.data(), name.size());
}

static bool zless(const AVLNode *lhs, double score, string_view name) {
  const ZNode *zl = container_of(lhs, ZNode, tree);
  if (zl->score != score) {
    return zl->score < score;
  }
  int rv = memcmp(zl->name.data(), name.data(), min(zl->name.size(), name.size()));
  if (rv != 0) {
    return rv < 0;
  }
  return zl->name.size() < name.size();
}

static bool zless(const AVLNode *lhs, const AVLNode *rhs) {
  const ZNode *zr = container_of(rhs, ZNode, tree);
  return zless(lhs, zr->score, zr->name);
}


static void tree_insert(ZSet *zs, ZNode *node) {
  AVLNode *parent = nullptr;
  AVLNode **from = &zs->root;
  while (*from) {
    parent = *from;
    from = zless(&node->tree, parent) ? &parent->left : &parent->right;
  }
  *from = &node->tree;
  node->tree.parent = parent;
  zs->root = node->tree.avl_fix();
}

static void tree_dispose(AVLNode *node) {
  if (!node) {
    return;
  }
  tree_dispose(node->left);
  tree_dispose(node->right);
  delete container_of(node, ZNode, tree);
}

static void update_score(ZSet *zs, ZNode *node, double score) {
  if (node->score == score) {
    return;
  }
  zs->root = node->tree.avl_del();
  node->tree = AVLNode{};
  node->score = score;
  tree_insert(zs, node);
}


struct HKey {
  HNode node;
  string_view name;
};

static bool hcmp(HNode *node, HNode *key) {
  ZNode *zn = container_of(node, ZNode, hmap);
  HKey *k = container_of(key, HKey, node);
  if (zn->name.size() != k->name.size()) {
    return false;
  }
  return 0 == memcmp(zn->name.data(), k->name.data(), k->name.size());
}


ZNode *ZSet::lookup(string_view name) {
  if (!root) {
    return nullptr;
  }
  HKey k;
  k.node.hcode = str_hash((const uint8_t *)name.data(), name.size());
  k.name = name;
  HNode *found = hmap.hm_lookup(&k.node, &hcmp);
  return found ? container_of(found, ZNode, hmap) : nullptr;
}

bool ZSet::insert(string name, double score) {
  if (ZNode *existing = lookup(name)) {
    update_score(this, existing, score);
    return false;
  }
  ZNode *node = new ZNode(move(name), score);
  hmap.hm_insert(&node->hmap);
  tree_insert(this, node);
  return true;
}

bool ZSet::remove(string_view name) {
  HKey k;
  k.node.hcode = str_hash((const uint8_t *)name.data(), name.size());
  k.name = name;
  HNode *found = hmap.hm_delete(&k.node, &hcmp);
  if (!found) {
    return false;
  }
  ZNode *node = container_of(found, ZNode, hmap);
  root = node->tree.avl_del();
  delete node;
  return true;
}

ZNode *ZSet::seekge(double score, string_view name) {
  AVLNode *cur = root;
  AVLNode *found = nullptr;
  while (cur) {
    if (zless(cur, score, name)) {
      cur = cur->right;
    } else {
      found = cur;
      cur = cur->left;
    }
  }
  return found ? container_of(found, ZNode, tree) : nullptr;
}

ZSet::~ZSet() {
  tree_dispose(root);
  root = nullptr;
}

ZNode *znode_offset(ZNode *node, int64_t offset) {
  if (!node) {
    return nullptr;
  }
  AVLNode *tnode = node->tree.avl_offset(offset);
  return tnode ? container_of(tnode, ZNode, tree) : nullptr;
}
