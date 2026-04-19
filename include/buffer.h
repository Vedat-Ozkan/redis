#pragma once

#include "common.h"
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

class Buffer {
  std::vector<uint8_t> buf_;
  size_t begin_ = 0;
  size_t end_ = 0;

 public:
  Buffer(size_t cap_initial = 0) { buf_.resize(cap_initial); }

  size_t size() const { return end_ - begin_; }
  bool empty() const { return size() == 0; }
  uint8_t *data() { return buf_.data() + begin_; }
  const uint8_t *data() const { return buf_.data() + begin_; }
  void clear() { begin_ = end_ = 0; }

  void consume(size_t n);
  void append(const uint8_t *data, size_t len);

  void append_8(uint8_t data) { append(&data, 1); }
  void append_32(uint32_t data) { append((const uint8_t *)&data, 4); }
  void append_64(int64_t data) { append((const uint8_t *)&data, 8); }
  void append_dbl(double data) { append((const uint8_t *)&data, 8); }

  void append_nil() { append_8(TAG_NIL); }
  void append_str(const char *s, size_t size);
  void append_str(std::string_view sv) { append_str(sv.data(), sv.size()); }
  void append_int(int64_t val);
  void append_double(double val);
  void append_arr(uint32_t n);
  void append_err(uint32_t code, const std::string &msg);

  void response_begin(size_t *header);
  size_t response_size(size_t header) { return size() - header - 4; }
  void response_end(size_t header);

  void patch_array_count(size_t header_pos, uint32_t count);
};
