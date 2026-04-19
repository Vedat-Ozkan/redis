#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

const size_t k_max_load_factor = 8;
const size_t k_rehashing_work = 128;

struct HNode {
  HNode *next = nullptr;
  uint64_t hcode = 0;
};

uint64_t str_hash(const uint8_t *data, size_t len);

class HTab {
 public:
  std::vector<HNode*> tab;
  size_t mask = 0;
  size_t size = 0;

  HTab() = default;
  HTab(size_t n) { init(n); }

  HTab(const HTab&) = delete;
  HTab& operator=(const HTab&) = delete;
  HTab(HTab&&) = default;
  HTab& operator=(HTab&&) = default;
  ~HTab() = default;

  void init(size_t n);

  size_t h_size() { return size; }
  void h_insert(HNode *node);
  HNode **h_lookup(HNode *key, bool (*eq)(HNode *, HNode*));
  HNode *h_detach(HNode **from);
};

class HMap {
 public:
  HTab newer;
  HTab older;
  size_t migrate_pos = 0;

  HMap(size_t n = 4) : newer(n), older(), migrate_pos(0) {}

  size_t hm_size();
  HNode *hm_lookup(HNode *key, bool (*eq)(HNode *, HNode*));
  HNode *hm_delete(HNode *key, bool (*eq)(HNode *, HNode*));
  void hm_insert(HNode *node);
  void hm_foreach(bool (*cb_keys)(HNode *, void*), void* arg);

  void hm_help_rehashing();
  void hm_trigger_rehashing();
};
