#include "protocol.h"

#include "common.h"

using namespace std;

int32_t parse_req(const uint8_t* data, size_t size, vector<string>& out) {
  const uint8_t* end = data + size;
  uint32_t nstr = 0;
  if (!read_u32(data, end, nstr)) return -1;
  if (nstr > k_max_args) return -1;

  while (out.size() < nstr) {
    uint32_t len = 0;
    if (!read_u32(data, end, len)) return -1;
    out.push_back(string());
    if (!read_str(data, end, len, out.back())) return -1;
  }
  if (data != end) return -1;
  return 0;
}
