#pragma once
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>

using Clock = std::chrono::steady_clock;

#define container_of(ptr, type, member) \
  ((type *)((char *)(ptr) - offsetof(type, member)))

enum {
  TAG_NIL = 0,
  TAG_ERR = 1,
  TAG_STR = 2,
  TAG_INT = 3,
  TAG_DBL = 4,
  TAG_ARR = 5,
};

enum {
  ERR_UNKNOWN = 1,
  ERR_TOO_BIG = 2,
};


inline constexpr size_t k_max_msg = 32 << 20;
inline constexpr size_t k_max_args = 200 * 1000;

void die(std::string a);
void msg(std::string a);
int32_t read_full(int fd, char *buf, size_t n);
int32_t write_all(int fd, const char *buf, size_t n);
bool read_u32(const uint8_t *&cur, const uint8_t *end, uint32_t &out);
bool read_str(const uint8_t *&cur, const uint8_t *end, size_t n, std::string &out);