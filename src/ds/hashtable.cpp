#include "hashtable.h"

#include <cassert>
#include <utility>

using namespace std;

uint64_t str_hash(const uint8_t *data, size_t len) {
  uint32_t h = 0x811C9DC5;
  for (size_t i = 0; i < len; i++) {
    h = (h + data[i]) * 0x01000193;
  }
  return h;
}

void HTab::init(size_t n) {
  assert(n > 0 && (((n - 1) & n) == 0));
  tab.clear();
  tab.resize(n);
  mask = n - 1;
  size = 0;
}

void HTab::h_insert(HNode *node) {
  size_t pos = node->hcode & mask;
  HNode *next = tab[pos];
  node->next = next;
  tab[pos] = node;
  size++;
}

HNode **HTab::h_lookup(HNode *key, bool (*eq)(HNode *, HNode*)) {
  if (tab.empty()) {
    return nullptr;
  }
  size_t pos = key->hcode & mask;
  HNode **from = &tab[pos];
  for (HNode *cur; (cur = *from) != nullptr; from = &cur->next) {
    if (cur->hcode == key->hcode && eq(cur, key)) {
      return from;
    }
  }
  return nullptr;
}

HNode *HTab::h_detach(HNode **from) {
  HNode *node = *from;
  *from = node->next;
  size--;
  return node;
}

size_t HMap::hm_size() {
  return newer.h_size() + older.h_size();
}

void HMap::hm_help_rehashing() {
  size_t nwork = 0;
  while (nwork < k_rehashing_work && older.size > 0) {
    HNode **from = &older.tab[migrate_pos];
    if (!*from) {
      migrate_pos++;
      continue;
    }
    newer.h_insert(older.h_detach(from));
    nwork++;
  }
  if (older.size == 0 && older.tab.size()) {
    older = HTab{};
  }
}

void HMap::hm_trigger_rehashing() {
  size_t new_mask = (newer.mask + 1) * 2;
  older = move(newer);
  newer = HTab(new_mask);
  migrate_pos = 0;
}

HNode *HMap::hm_lookup(HNode *key, bool (*eq)(HNode *, HNode*)) {
  hm_help_rehashing();
  HNode **from = newer.h_lookup(key, eq);
  if (!from) {
    from = older.h_lookup(key, eq);
  }
  return from ? *from : nullptr;
}

HNode *HMap::hm_delete(HNode *key, bool (*eq)(HNode *, HNode*)) {
  hm_help_rehashing();
  if (HNode **from = newer.h_lookup(key, eq)) {
    return newer.h_detach(from);
  }
  if (HNode **from = older.h_lookup(key, eq)) {
    return older.h_detach(from);
  }
  return nullptr;
}

void HMap::hm_insert(HNode *node) {
  if (newer.tab.empty()) {
    newer.init(4);
  }
  newer.h_insert(node);
  if (older.tab.empty()) {
    size_t threshold = (newer.mask + 1) * k_max_load_factor;
    if (newer.size >= threshold) {
      hm_trigger_rehashing();
    }
  }
  hm_help_rehashing();
}

void HMap::hm_foreach(bool (*cb_keys)(HNode *, void*), void* arg) {
  auto scan = [&](HTab &ht) {
    if (ht.tab.empty()) return;
    size_t size = ht.mask + 1;
    for (size_t i = 0; i < size; i++) {
      for (HNode *cur = ht.tab[i]; cur != nullptr; cur = cur->next) {
        cb_keys(cur, arg);
      }
    }
  };
  scan(newer);
  scan(older);
}
