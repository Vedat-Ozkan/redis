#pragma once
#include <string>
#include <cstdint>
#include <cstddef>

enum {
  TAG_NIL = 0,    // nil
  TAG_ERR = 1,    // error code + msg
  TAG_STR = 2,    // string
  TAG_INT = 3,    // int64
  TAG_DBL = 4,    // double
  TAG_ARR = 5,    // array
};

enum {
  ERR_UNKNOWN = 1,    // unknown command
  ERR_TOO_BIG = 2,    // response too big
};


enum class Status : uint32_t {
  RES_OK = 0,
  RES_ERR = 1,
  RES_NX = 2
};

inline constexpr size_t k_max_msg = 32 << 20;
inline constexpr size_t k_max_args = 200 * 1000;

void die(std::string a);
void msg(std::string a);
int32_t read_full(int fd, char *buf, size_t n);
int32_t write_all(int fd, const char *buf, size_t n);
bool read_u32(const uint8_t *&cur, const uint8_t *end, uint32_t &out);
bool read_str(const uint8_t *&cur, const uint8_t *end, size_t n, std::string &out);