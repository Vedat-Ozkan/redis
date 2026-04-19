#include "avl.h"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <random>
#include <set>
#include <vector>

using namespace std;

struct Node : public AVLNode {
  uint32_t key = 0;
  explicit Node(uint32_t k) : AVLNode(), key(k) {}
};

static Node *as_node(AVLNode *n) { return (Node *)n; }

static void avl_verify(AVLNode *node, AVLNode *parent, uint32_t &prev_key, bool &has_prev) {
  if (!node) {
    return;
  }
  assert(node->parent == parent);

  avl_verify(node->left, node, prev_key, has_prev);

  uint32_t k = as_node(node)->key;
  if (has_prev) {
    assert(prev_key < k);
  }
  prev_key = k;
  has_prev = true;

  uint32_t hl = avl_height(node->left);
  uint32_t hr = avl_height(node->right);
  uint32_t hc = 1 + (hl > hr ? hl : hr);
  assert(node->height == hc);

  uint32_t cl = avl_cnt(node->left);
  uint32_t cr = avl_cnt(node->right);
  assert(node->cnt == 1 + cl + cr);

  uint32_t diff = (hl > hr) ? (hl - hr) : (hr - hl);
  assert(diff <= 1);

  avl_verify(node->right, node, prev_key, has_prev);
}

static AVLNode *bst_insert(AVLNode *root, AVLNode *node) {
  node->left = nullptr;
  node->right = nullptr;
  node->parent = nullptr;
  node->height = 1;
  node->cnt = 1;

  if (!root) {
    return node;
  }

  AVLNode *cur = root;
  while (true) {
    if (as_node(node)->key < as_node(cur)->key) {
      if (!cur->left) {
        cur->left = node;
        node->parent = cur;
        break;
      }
      cur = cur->left;
    } else {
      if (!cur->right) {
        cur->right = node;
        node->parent = cur;
        break;
      }
      cur = cur->right;
    }
  }
  return node->avl_fix();
}

static AVLNode *bst_lookup(AVLNode *root, uint32_t key) {
  AVLNode *cur = root;
  while (cur) {
    uint32_t ck = as_node(cur)->key;
    if (key == ck) return cur;
    cur = (key < ck) ? cur->left : cur->right;
  }
  return nullptr;
}

static AVLNode *bst_delete(AVLNode *root, uint32_t key) {
  AVLNode *node = bst_lookup(root, key);
  assert(node);
  AVLNode *new_root = node->avl_del();
  delete as_node(node);
  return new_root;
}

static void verify_tree(AVLNode *root, size_t expected_cnt) {
  if (!root) {
    assert(expected_cnt == 0);
    return;
  }
  uint32_t prev = 0;
  bool has_prev = false;
  avl_verify(root, nullptr, prev, has_prev);
  assert(avl_cnt(root) == expected_cnt);
}

static void test_sorted_inserts() {
  AVLNode *root = nullptr;
  vector<Node *> nodes;
  const int n = 2000;
  nodes.reserve(n);
  for (int i = 0; i < n; ++i) {
    nodes.push_back(new Node((uint32_t)i));
    root = bst_insert(root, nodes.back());
  }
  uint32_t prev = 0;
  bool has_prev = false;
  avl_verify(root, nullptr, prev, has_prev);
  assert(avl_cnt(root) == (uint32_t)n);

  for (Node *p : nodes) delete p;
}

static void test_random_inserts() {
  mt19937_64 rng(123456);
  uniform_int_distribution<uint32_t> dist(1, 1000000);

  AVLNode *root = nullptr;
  vector<Node *> nodes;
  set<uint32_t> seen;

  const int n = 5000;
  nodes.reserve(n);
  while ((int)nodes.size() < n) {
    uint32_t k = dist(rng);
    if (!seen.insert(k).second) continue;
    nodes.push_back(new Node(k));
    root = bst_insert(root, nodes.back());
  }

  uint32_t prev = 0;
  bool has_prev = false;
  avl_verify(root, nullptr, prev, has_prev);
  assert(avl_cnt(root) == (uint32_t)n);

  for (Node *p : nodes) delete p;
}

static void test_sorted_deletes() {
  AVLNode *root = nullptr;
  const int n = 2000;
  for (int i = 0; i < n; ++i) {
    root = bst_insert(root, new Node((uint32_t)i));
  }
  verify_tree(root, (size_t)n);

  for (int i = 0; i < n; ++i) {
    root = bst_delete(root, (uint32_t)i);
    if (i % 50 == 0 || i == n - 1) {
      verify_tree(root, (size_t)(n - i - 1));
    }
  }
  assert(root == nullptr);
}

static void test_random_deletes() {
  mt19937_64 rng(7890);
  uniform_int_distribution<uint32_t> dist(1, 2000000);

  AVLNode *root = nullptr;
  vector<uint32_t> keys;
  keys.reserve(5000);
  set<uint32_t> seen;
  while (keys.size() < 5000) {
    uint32_t k = dist(rng);
    if (!seen.insert(k).second) continue;
    keys.push_back(k);
    root = bst_insert(root, new Node(k));
  }
  verify_tree(root, keys.size());

  for (size_t i = 0; i < keys.size(); ++i) {
    uniform_int_distribution<size_t> pick(i, keys.size() - 1);
    size_t j = pick(rng);
    swap(keys[i], keys[j]);
  }

  for (size_t i = 0; i < keys.size(); ++i) {
    root = bst_delete(root, keys[i]);
    if (i % 50 == 0 || i + 1 == keys.size()) {
      verify_tree(root, keys.size() - i - 1);
    }
  }
  assert(root == nullptr);
}

int main() {
  test_sorted_inserts();
  test_random_inserts();
  test_sorted_deletes();
  test_random_deletes();
  cout << "AVL tests passed\n";
  return 0;
}
