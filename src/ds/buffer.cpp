#include "buffer.h"

#include <algorithm>
#include <cassert>
#include <cstring>

using namespace std;

void Buffer::consume(size_t n) {
  assert(n <= size());
  begin_ += n;
  if (begin_ == end_) {
    begin_ = end_ = 0;
  }
}

void Buffer::append(const uint8_t *data, size_t len) {
  if (len == 0) return;
  if (buf_.size() < end_ + len) {
    if (buf_.size() >= size() + len) {
      memmove(buf_.data(), buf_.data() + begin_, size());
      end_ = size();
      begin_ = 0;
    } else {
      size_t cur = size();
      size_t new_cap = max(buf_.size() ? buf_.size() * 2 : size_t{16}, cur + len);
      vector<uint8_t> nb(new_cap);
      memcpy(nb.data(), buf_.data() + begin_, cur);
      buf_.swap(nb);
      begin_ = 0;
      end_ = cur;
    }
  }
  memcpy(buf_.data() + end_, data, len);
  end_ += len;
}

void Buffer::append_str(const char *s, size_t size) {
  append_8(TAG_STR);
  append_32((uint32_t)size);
  append((const uint8_t *)s, size);
}

void Buffer::append_int(int64_t val) {
  append_8(TAG_INT);
  append_64(val);
}

void Buffer::append_double(double val) {
  append_8(TAG_DBL);
  append_dbl(val);
}

void Buffer::append_arr(uint32_t n) {
  append_8(TAG_ARR);
  append_32(n);
}

void Buffer::append_err(uint32_t code, const string &msg) {
  append_8(TAG_ERR);
  append_32(code);
  append_32((uint32_t)msg.size());
  append((const uint8_t *)msg.data(), msg.size());
}

void Buffer::response_begin(size_t *header) {
  *header = size();
  append_32(0);
}

void Buffer::response_end(size_t header) {
  size_t msg_size = response_size(header);
  if (msg_size > k_max_msg) {
    end_ = begin_ + header + 4;
    buf_.resize(end_);
    append_err(ERR_TOO_BIG, "response is too big");
    msg_size = response_size(header);
  }
  uint32_t len = (uint32_t)msg_size;
  memcpy(&buf_[header], &len, 4);
}

void Buffer::patch_array_count(size_t header_pos, uint32_t count) {
  memcpy(&buf_[header_pos + 1], &count, 4);
}
