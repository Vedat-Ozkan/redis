#pragma once
#include <cstdint>
#include <cstdlib>
#include <utility>
#include <cassert>
#include <cstddef>  // offsetof

const size_t k_max_load_factor = 8;
const size_t k_rehashing_work = 128;

using namespace std;

struct HNode {
  HNode *next = NULL;
  uint64_t hcode = 0;
};

class HTab {
  public:
  HNode **tab = NULL;
  size_t mask = 0;
  size_t size = 0;

  void init(size_t n) {
    assert(n > 0 && (((n - 1) & n) == 0));
    free(tab);
    tab = (HNode **)calloc(n, sizeof(HNode *));
    mask = n - 1;
    size = 0;
  }

  HTab() = default;

  HTab(size_t n) { init(n); }

  ~HTab() { free(tab); }
  
  HTab(const HTab&) = delete;
  HTab& operator=(const HTab&) = delete;

  HTab(HTab&& other) noexcept
    : tab(other.tab), mask(other.mask), size(other.size) {
    other.tab = nullptr;
    other.mask = 0;
    other.size = 0;
  }

  HTab& operator=(HTab&& other) noexcept {
    if (this == &other) return *this;

    free(tab);               // release current ownership
    tab = other.tab;
    mask = other.mask;
    size = other.size;

    other.tab = nullptr;     // leave other in destructible state
    other.mask = 0;
    other.size = 0;
    return *this;
  }

  size_t h_size() {
    return size;
  }

  void h_insert(HNode *node) {
    size_t pos = node->hcode & mask;
    HNode *next = tab[pos];
    node->next = next;
    tab[pos] = node;
    size++;
  }

  HNode **h_lookup(HNode *key, bool (*eq)(HNode *, HNode*)) {
    if (!tab) {
      return NULL;
    }

    size_t pos = key->hcode & mask;
    HNode **from = &tab[pos];
    for (HNode *cur; (cur = *from) != NULL; from = &cur->next) {
      if (cur->hcode == key->hcode && eq(cur, key)) {
        return from;
      }
    }

    return NULL;
  }

  HNode *h_detach(HNode **from) {
    HNode *node = *from;
    *from = node->next;
    size--;
    return node;
  }
};

class HMap {
  public:
  HTab newer;
  HTab older;
  size_t migrate_pos = 0;

  HMap(size_t n = 4) : newer(n), older(), migrate_pos(0) {}

  size_t hm_size() {
    return newer.h_size() + older.h_size();
  }

  void hm_help_rehashing() {
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

    if (older.size == 0 && older.tab) {
      older = HTab{};
    }
  }

  void hm_trigger_rehashing() {
    size_t new_mask = (newer.mask + 1) * 2;
    older = move(newer);
    newer = HTab(new_mask);
    migrate_pos = 0;
  }

  HNode *hm_lookup(HNode *key, bool (*eq)(HNode *, HNode*)) {
    hm_help_rehashing();
    HNode **from = newer.h_lookup(key, eq);
    if (!from) {
      from = older.h_lookup(key, eq);
    }
    return from ? *from : NULL;
  }

  HNode *hm_delete(HNode *key, bool (*eq)(HNode *, HNode*)) {
    hm_help_rehashing();
    if (HNode **from = newer.h_lookup(key, eq)) {
      return newer.h_detach(from);
    }
    if (HNode **from = older.h_lookup(key, eq)) {
      return older.h_detach(from);
    }
    return NULL;
  }

  void hm_insert(HNode *node) {
    if (!newer.tab) {
      newer.init(4);
    }

    newer.h_insert(node);

    if (!older.tab) {
      size_t threshold = (newer.mask + 1) * k_max_load_factor;
      if (newer.size >= threshold) {
        hm_trigger_rehashing();
      }
    }

    hm_help_rehashing();
  }

  void hm_foreach(bool (*cb_keys)(HNode *, void*), void* arg) {
    auto scan = [&](HTab &ht) {
      if (!ht.tab) return;
      size_t size = ht.mask + 1;
      for (size_t i = 0; i <= size; i++) {
        for (HNode *cur = ht.tab[i]; cur != NULL; cur = cur->next) {
          cb_keys(cur, arg);
        }
      }
    };
    scan(newer);
    scan(older);
  }
};
