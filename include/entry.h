#pragma once

#include "hashtable.h"
#include "zset.h"

#include <cstddef>
#include <string>
#include <variant>

inline constexpr size_t kNotInHeap = static_cast<size_t>(-1);

struct Entry {
  HNode node;
  std::string key;
  std::variant<std::string, ZSet> value;
  size_t heap_idx = kNotInHeap;

  Entry() = default;
};
