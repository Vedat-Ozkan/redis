#pragma once

#include "common.h"
#include "entry.h"

#include <cstddef>
#include <optional>
#include <vector>

class TtlHeap {
  struct Item {
    Clock::time_point deadline;
    Entry* owner;
  };
  std::vector<Item> a_;

  static size_t parent_of(size_t i) { return (i + 1) / 2 - 1; }
  static size_t left_of(size_t i) { return 2 * i + 1; }
  static size_t right_of(size_t i) { return 2 * i + 2; }

  void swap_items(size_t i, size_t j);
  void heap_up(size_t i);
  void heap_down(size_t i);
  void heap_update(size_t i);

 public:
  TtlHeap() = default;
  TtlHeap(const TtlHeap&) = delete;
  TtlHeap& operator=(const TtlHeap&) = delete;

  bool empty() const { return a_.empty(); }
  size_t size() const { return a_.size(); }

  std::optional<Clock::time_point> peek_deadline() const {
    if (a_.empty()) return std::nullopt;
    return a_.front().deadline;
  }

  void upsert(Entry& item, Clock::time_point deadline);
  void remove(Entry& item);

  Clock::time_point deadline_of(const Entry& e) const {
    return a_[e.heap_idx].deadline;
  }

  template <typename F>
  size_t pop_expired(size_t cap, Clock::time_point now, F&& on_expired) {
    size_t popped = 0;
    while (popped < cap && !a_.empty() && a_.front().deadline <= now) {
      Entry* victim = a_.front().owner;
      remove(*victim);
      on_expired(*victim);
      popped++;
    }
    return popped;
  }
};
