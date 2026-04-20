#include "avl.h"
#include "zset.h"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <random>
#include <set>
#include <vector>

using namespace std;

// ---------------------------------------------------------------------------
// AVL structural verification (via ZSet's exposed root)
// ---------------------------------------------------------------------------

static void avl_verify(AVLNode *node, AVLNode *parent) {
  if (!node) return;
  assert(node->parent == parent);

  avl_verify(node->left, node);
  avl_verify(node->right, node);

  uint32_t hl = avl_height(node->left);
  uint32_t hr = avl_height(node->right);
  assert(node->height == 1 + (hl > hr ? hl : hr));
  assert(node->cnt == 1 + avl_cnt(node->left) + avl_cnt(node->right));
  assert((hl > hr ? hl - hr : hr - hl) <= 1);
}

static void verify_zset(ZSet &zs, size_t expected) {
  avl_verify(zs.root, nullptr);
  assert(avl_cnt(zs.root) == expected);
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

static void test_insert_and_lookup() {
  ZSet zs;
  assert(zs.insert("alice", 1.0));
  assert(zs.insert("bob", 2.0));
  assert(zs.insert("carol", 3.0));
  verify_zset(zs, 3);

  ZNode *n = zs.lookup("bob");
  assert(n && n->score == 2.0 && n->name == "bob");

  assert(zs.lookup("nobody") == nullptr);
}

static void test_update_score() {
  ZSet zs;
  zs.insert("x", 10.0);
  // insert with same name returns false (update path)
  assert(!zs.insert("x", 99.0));
  verify_zset(zs, 1);

  ZNode *n = zs.lookup("x");
  assert(n && n->score == 99.0);
}

static void test_remove() {
  ZSet zs;
  zs.insert("a", 1.0);
  zs.insert("b", 2.0);
  zs.insert("c", 3.0);

  assert(zs.remove("b"));
  verify_zset(zs, 2);
  assert(zs.lookup("b") == nullptr);
  assert(!zs.remove("b"));  // already gone
}

static void test_seekge_ordering() {
  ZSet zs;
  // Insert out of order
  zs.insert("d", 4.0);
  zs.insert("a", 1.0);
  zs.insert("c", 3.0);
  zs.insert("b", 2.0);
  verify_zset(zs, 4);

  // seekge(2.0, "b") should land on "b"
  ZNode *n = zs.seekge(2.0, "b");
  assert(n && n->name == "b");

  // Walk forward through all nodes by rank
  vector<string> order;
  for (ZNode *cur = n; cur; cur = znode_offset(cur, 1)) {
    order.push_back(cur->name);
  }
  assert((order == vector<string>{"b", "c", "d"}));
}

static void test_random_inserts_and_deletes() {
  mt19937_64 rng(42);
  uniform_real_distribution<double> score_dist(0.0, 1e6);

  ZSet zs;
  vector<string> names;
  set<string> seen;
  const int n = 1000;

  for (int i = 0; i < n; ++i) {
    string name = "key" + to_string(i);
    zs.insert(name, score_dist(rng));
    names.push_back(name);
  }
  verify_zset(zs, (size_t)n);

  // Delete half in shuffled order
  shuffle(names.begin(), names.end(), rng);
  for (int i = 0; i < n / 2; ++i) {
    assert(zs.remove(names[i]));
    if (i % 100 == 0) verify_zset(zs, (size_t)(n - i - 1));
  }
  verify_zset(zs, (size_t)(n / 2));
}

static void test_rank_navigation() {
  ZSet zs;
  const int n = 20;
  for (int i = 0; i < n; ++i) {
    zs.insert("k" + to_string(i), (double)i);
  }
  verify_zset(zs, (size_t)n);

  // Start at rank 0, walk forward n steps
  ZNode *first = zs.seekge(0.0, "");
  assert(first);
  int count = 0;
  for (ZNode *cur = first; cur; cur = znode_offset(cur, 1)) {
    ++count;
  }
  assert(count == n);

  // avl_offset backward from last should reach first
  ZNode *last = znode_offset(first, n - 1);
  assert(last);
  assert(znode_offset(last, -(n - 1)) == first);
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main() {
  test_insert_and_lookup();
  test_update_score();
  test_remove();
  test_seekge_ordering();
  test_random_inserts_and_deletes();
  test_rank_navigation();
  cout << "ZSet/AVL tests passed\n";
  return 0;
}
