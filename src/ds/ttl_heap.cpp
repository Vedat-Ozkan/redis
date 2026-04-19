#include "ttl_heap.h"

#include <algorithm>

using namespace std;

void TtlHeap::swap_items(size_t i, size_t j) {
  swap(a_[i], a_[j]);
  a_[i].owner->heap_idx = i;
  a_[j].owner->heap_idx = j;
}

void TtlHeap::heap_up(size_t i) {
  while (i > 0 && a_[parent_of(i)].deadline > a_[i].deadline) {
    swap_items(i, parent_of(i));
    i = parent_of(i);
  }
}

void TtlHeap::heap_down(size_t i) {
  while (true) {
    size_t l = left_of(i);
    size_t r = right_of(i);
    size_t min_pos = i;
    if (l < size() && a_[l].deadline < a_[min_pos].deadline) min_pos = l;
    if (r < size() && a_[r].deadline < a_[min_pos].deadline) min_pos = r;
    if (min_pos == i) break;
    swap_items(i, min_pos);
    i = min_pos;
  }
}

void TtlHeap::heap_update(size_t i) {
  if (i > 0 && a_[parent_of(i)].deadline > a_[i].deadline) {
    heap_up(i);
  } else {
    heap_down(i);
  }
}

void TtlHeap::upsert(Entry& item, Clock::time_point deadline) {
  size_t idx = item.heap_idx;
  if (idx == kNotInHeap) {
    idx = a_.size();
    a_.push_back({deadline, &item});
    item.heap_idx = idx;
    heap_up(idx);
  } else {
    a_[idx].deadline = deadline;
    heap_update(idx);
  }
}

void TtlHeap::remove(Entry& item) {
  size_t idx = item.heap_idx;
  if (idx == kNotInHeap) return;
  size_t last = a_.size() - 1;
  if (idx != last) {
    swap_items(idx, last);
  }
  a_.pop_back();
  item.heap_idx = kNotInHeap;
  if (idx < a_.size()) {
    heap_update(idx);
  }
}
