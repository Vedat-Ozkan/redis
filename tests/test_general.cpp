#include "buffer.h"
#include "hashtable.h"
#include "kvstore.h"

#include <cassert>
#include <iostream>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static std::vector<std::string> cmd(std::initializer_list<std::string> args) {
  return std::vector<std::string>(args);
}

// ---------------------------------------------------------------------------
// Hashtable tests
// ---------------------------------------------------------------------------

struct TestNode : public HNode {
  std::string key;
  explicit TestNode(const std::string& k) : key(k) {
    hcode = str_hash(reinterpret_cast<const uint8_t*>(k.data()), k.size());
  }
};

static bool node_eq(HNode* a, HNode* b) {
  return static_cast<TestNode*>(a)->key == static_cast<TestNode*>(b)->key;
}

static void test_hashtable_insert_lookup() {
  HMap map;
  TestNode n1("hello"), n2("world"), n3("foo");

  map.hm_insert(&n1);
  map.hm_insert(&n2);
  map.hm_insert(&n3);
  assert(map.hm_size() == 3);

  TestNode lookup_key("hello");
  HNode* found = map.hm_lookup(&lookup_key, node_eq);
  assert(found != nullptr);
  assert(static_cast<TestNode*>(found)->key == "hello");

  TestNode missing("bar");
  assert(map.hm_lookup(&missing, node_eq) == nullptr);
}

static void test_hashtable_delete() {
  HMap map;
  TestNode n1("a"), n2("b");
  map.hm_insert(&n1);
  map.hm_insert(&n2);

  TestNode del_key("a");
  HNode* removed = map.hm_delete(&del_key, node_eq);
  assert(removed != nullptr);
  assert(map.hm_size() == 1);

  assert(map.hm_lookup(&del_key, node_eq) == nullptr);
}

// ---------------------------------------------------------------------------
// KVStore tests
// ---------------------------------------------------------------------------

static void test_kvstore_set_get() {
  KVStore store;
  Buffer out;

  auto set_cmd = cmd({"set", "mykey", "myval"});
  store.do_set(set_cmd, out);
  out.consume(out.size());

  auto get_cmd = cmd({"get", "mykey"});
  store.do_get(get_cmd, out);
  // TODO(human): inspect the response buffer and assert the correct value is present
}

static void test_kvstore_del() {
  KVStore store;
  Buffer out;

  auto set_cmd = cmd({"set", "k", "v"});
  store.do_set(set_cmd, out);
  out.consume(out.size());

  auto del_cmd = cmd({"del", "k"});
  store.do_del(del_cmd, out);
  out.consume(out.size());

  auto get_cmd = cmd({"get", "k"});
  store.do_get(get_cmd, out);
  // Response should indicate key not found — add assertion after inspecting buffer format
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main() {
  test_hashtable_insert_lookup();
  test_hashtable_delete();

  test_kvstore_set_get();
  test_kvstore_del();

  std::cout << "general tests passed\n";
  return 0;
}
