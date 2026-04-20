#include <sys/types.h>
#include <sys/socket.h>
#include <string>
#include <iostream>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <cstring>
#include <vector>
#include <cstdint>
#include <unordered_map>
#include <random>
#include <chrono>
#include <sys/time.h>
#include "common.h"

using namespace std;

struct Value {
  uint8_t tag = TAG_NIL;
  uint32_t err_code = 0;
  string err_msg;
  string str;
  int64_t i64 = 0;
  double dbl = 0.0;
  vector<Value> arr;
};

static string tag_name(uint8_t tag) {
  switch (tag) {
    case TAG_NIL: return "nil";
    case TAG_ERR: return "err";
    case TAG_STR: return "str";
    case TAG_INT: return "int";
    case TAG_DBL: return "dbl";
    case TAG_ARR: return "arr";
    default: return "unknown";
  }
}

static string preview(const string &s, size_t max_len = 120) {
  if (s.size() <= max_len) return s;
  return s.substr(0, max_len) + "...(" + to_string(s.size()) + " bytes)";
}

static bool read_u8(const uint8_t *&cur, const uint8_t *end, uint8_t &out) {
  if (cur + 1 > end) return false;
  out = *cur++;
  return true;
}

static bool read_i64(const uint8_t *&cur, const uint8_t *end, int64_t &out) {
  if (cur + 8 > end) return false;
  memcpy(&out, cur, 8);
  cur += 8;
  return true;
}

static bool read_dbl(const uint8_t *&cur, const uint8_t *end, double &out) {
  if (cur + 8 > end) return false;
  memcpy(&out, cur, 8);
  cur += 8;
  return true;
}

static bool read_bytes(const uint8_t *&cur, const uint8_t *end, uint32_t n, string &out) {
  if (cur + n > end) return false;
  out.assign((const char *)cur, (const char *)(cur + n));
  cur += n;
  return true;
}

static bool parse_value(const uint8_t *&cur, const uint8_t *end, Value &out) {
  out = Value{};
  uint8_t tag = 0;
  if (!read_u8(cur, end, tag)) return false;
  out.tag = tag;

  switch (tag) {
    case TAG_NIL:
      return true;
    case TAG_ERR: {
      uint32_t code = 0, len = 0;
      if (!read_u32(cur, end, code)) return false;
      if (!read_u32(cur, end, len)) return false;
      out.err_code = code;
      return read_bytes(cur, end, len, out.err_msg);
    }
    case TAG_STR: {
      uint32_t len = 0;
      if (!read_u32(cur, end, len)) return false;
      return read_bytes(cur, end, len, out.str);
    }
    case TAG_INT:
      return read_i64(cur, end, out.i64);
    case TAG_DBL:
      return read_dbl(cur, end, out.dbl);
    case TAG_ARR: {
      uint32_t n = 0;
      if (!read_u32(cur, end, n)) return false;
      out.arr.reserve(n);
      for (uint32_t i = 0; i < n; ++i) {
        Value v;
        if (!parse_value(cur, end, v)) return false;
        out.arr.push_back(move(v));
      }
      return true;
    }
    default:
      return false;
  }
}

static void append_u32(vector<uint8_t> &out, uint32_t v) {
  uint8_t buf[4];
  memcpy(buf, &v, 4);
  out.insert(out.end(), buf, buf + 4);
}

static void append_bytes(vector<uint8_t> &out, const uint8_t *data, size_t len) {
  out.insert(out.end(), data, data + len);
}

static int32_t send_req(int fd, const vector<string> &cmd) {
  vector<uint8_t> payload;
  payload.reserve(128);

  if (cmd.size() > k_max_args) {
    return -1;
  }

  append_u32(payload, (uint32_t)cmd.size());
  for (const string &s : cmd) {
    if (s.size() > k_max_msg) {
      return -1;
    }
    append_u32(payload, (uint32_t)s.size());
    append_bytes(payload, (const uint8_t *)s.data(), s.size());
  }

  if (payload.size() > k_max_msg) {
    return -1;
  }

  uint32_t ulen = (uint32_t)payload.size();
  int32_t err = write_all(fd, (char *)&ulen, 4);
  if (err) return err;
  if (ulen == 0) return 0;
  return write_all(fd, (const char *)payload.data(), payload.size());
}

static int32_t read_res(int fd, Value &out) {
  uint32_t resp_len = 0;
  int32_t err = read_full(fd, (char *)&resp_len, 4);
  if (err) return err;

  if (resp_len > k_max_msg) return -1;

  vector<uint8_t> buf;
  buf.resize(resp_len);
  err = read_full(fd, (char *)buf.data(), resp_len);
  if (err) return err;

  const uint8_t *cur = buf.data();
  const uint8_t *end = buf.data() + buf.size();
  if (!parse_value(cur, end, out)) {
    return -1;
  }
  if (cur != end) return -1;
  return 0;
}

static int32_t expect_nil(const vector<string> &cmd, const Value &v) {
  if (v.tag != TAG_NIL) {
    msg("FAIL cmd=[" + (cmd.empty() ? string() : cmd[0]) + "] expected nil, got " + tag_name(v.tag));
    return -1;
  }
  return 0;
}

static int32_t expect_int(const vector<string> &cmd, const Value &v, int64_t expected) {
  if (v.tag != TAG_INT || v.i64 != expected) {
    msg("FAIL cmd=[" + (cmd.empty() ? string() : cmd[0]) + "] expected int=" + to_string(expected) +
        ", got " + tag_name(v.tag) + (v.tag == TAG_INT ? ("=" + to_string(v.i64)) : string()));
    return -1;
  }
  return 0;
}

static int32_t expect_str(const vector<string> &cmd, const Value &v, const string &expected) {
  if (v.tag != TAG_STR || v.str != expected) {
    msg("FAIL cmd=[" + (cmd.empty() ? string() : cmd[0]) + "] expected str=" + preview(expected) +
        ", got " + tag_name(v.tag) + (v.tag == TAG_STR ? ("=" + preview(v.str)) : string()));
    return -1;
  }
  return 0;
}

static int32_t send_and_expect(int fd, const vector<string> &cmd, int32_t (*check)(const vector<string>&, const Value&)) {
  if (int32_t err = send_req(fd, cmd)) return err;
  Value res;
  if (int32_t err = read_res(fd, res)) return err;
  return check(cmd, res);
}

static int32_t send_and_expect_nil(int fd, const vector<string> &cmd) {
  return send_and_expect(fd, cmd, expect_nil);
}

static int32_t send_and_expect_int(int fd, const vector<string> &cmd, int64_t expected) {
  if (int32_t err = send_req(fd, cmd)) return err;
  Value res;
  if (int32_t err = read_res(fd, res)) return err;
  return expect_int(cmd, res, expected);
}

static int32_t send_and_expect_str(int fd, const vector<string> &cmd, const string &expected) {
  if (int32_t err = send_req(fd, cmd)) return err;
  Value res;
  if (int32_t err = read_res(fd, res)) return err;
  return expect_str(cmd, res, expected);
}

static int32_t test_basic(int fd) {
  msg("[test] basic");
  if (send_and_expect_nil(fd, {"set", "k", "v"})) return -1;
  if (send_and_expect_str(fd, {"get", "k"}, "v")) return -1;
  if (send_and_expect_nil(fd, {"get", "missing"})) return -1;
  if (send_and_expect_int(fd, {"del", "k"}, 1)) return -1;
  if (send_and_expect_nil(fd, {"get", "k"})) return -1;
  return 0;
}

static int32_t test_update(int fd) {
  msg("[test] update");
  if (send_and_expect_nil(fd, {"set", "u", "v1"})) return -1;
  if (send_and_expect_nil(fd, {"set", "u", "v2"})) return -1;
  if (send_and_expect_str(fd, {"get", "u"}, "v2")) return -1;
  if (send_and_expect_int(fd, {"del", "u"}, 1)) return -1;
  return 0;
}

static int32_t test_many_keys(int fd) {
  msg("[test] many-keys");
  const int n = 200;
  for (int i = 0; i < n; ++i) {
    string k = "k" + to_string(i);
    string v = "v" + to_string(i);
    if (send_and_expect_nil(fd, {"set", k, v})) return -1;
  }
  for (int i = 0; i < n; ++i) {
    string k = "k" + to_string(i);
    string v = "v" + to_string(i);
    if (send_and_expect_str(fd, {"get", k}, v)) return -1;
  }
  for (int i = 0; i < n; i += 2) {
    string k = "k" + to_string(i);
    if (send_and_expect_int(fd, {"del", k}, 1)) return -1;
  }
  for (int i = 0; i < n; ++i) {
    string k = "k" + to_string(i);
    if (i % 2 == 0) {
      if (send_and_expect_nil(fd, {"get", k})) return -1;
    } else {
      string v = "v" + to_string(i);
      if (send_and_expect_str(fd, {"get", k}, v)) return -1;
    }
  }
  return 0;
}

static string rand_string(mt19937_64 &rng, size_t len) {
  static const char alphabet[] =
      "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-";
  uniform_int_distribution<size_t> pick(0, sizeof(alphabet) - 2);
  string s;
  s.resize(len);
  for (size_t i = 0; i < len; ++i) {
    s[i] = alphabet[pick(rng)];
  }
  return s;
}

static int32_t test_random(int fd) {
  msg("[test] randomized");
  mt19937_64 rng(12345);

  vector<string> keys;
  keys.reserve(200);
  for (int i = 0; i < 200; ++i) keys.push_back("r" + to_string(i));

  unordered_map<string, string> model;

  uniform_int_distribution<int> op_pick(0, 99);
  uniform_int_distribution<int> key_pick(0, (int)keys.size() - 1);
  uniform_int_distribution<int> vlen_pick(0, 80);

  const int ops = 2000;
  for (int i = 0; i < ops; ++i) {
    int op = op_pick(rng);
    const string &k = keys[key_pick(rng)];

    if (op < 50) {
      string v = rand_string(rng, (size_t)vlen_pick(rng));
      model[k] = v;
      if (send_and_expect_nil(fd, {"set", k, v})) return -1;
    } else if (op < 80) {
      auto it = model.find(k);
      if (it == model.end()) {
        if (send_and_expect_nil(fd, {"get", k})) return -1;
      } else {
        const string &v = it->second;
        if (send_and_expect_str(fd, {"get", k}, v)) return -1;
      }
    } else {
      int64_t expected = model.erase(k) ? 1 : 0;
      if (send_and_expect_int(fd, {"del", k}, expected)) return -1;
    }
  }
  return 0;
}

static int32_t test_large_value(int fd) {
  msg("[test] large-value");
  string big(1 << 20, 'x');
  if (send_and_expect_nil(fd, {"set", "big", big})) return -1;
  if (send_and_expect_str(fd, {"get", "big"}, big)) return -1;
  if (send_and_expect_int(fd, {"del", "big"}, 1)) return -1;
  return 0;
}

static int32_t test_bigzset_async_del(int fd) {
  msg("[test] bigzset-async-del");
  const int n = 50000;

  for (int i = 0; i < n; ++i) {
    string score = to_string(i);
    string name = "m" + to_string(i);
    if (send_and_expect_int(fd, {"zadd", "bz", score, name}, 1)) return -1;
  }

  if (send_and_expect_nil(fd, {"set", "probe", "1"})) return -1;

  using clock = chrono::steady_clock;
  auto t0 = clock::now();
  if (send_and_expect_int(fd, {"del", "bz"}, 1)) return -1;
  auto t1 = clock::now();
  int64_t del_ms =
      chrono::duration_cast<chrono::milliseconds>(t1 - t0).count();
  msg("  DEL on " + to_string(n) + "-member zset returned in " +
      to_string(del_ms) + "ms");

  auto t2 = clock::now();
  if (send_and_expect_str(fd, {"get", "probe"}, "1")) return -1;
  auto t3 = clock::now();
  int64_t followup_ms =
      chrono::duration_cast<chrono::milliseconds>(t3 - t2).count();
  msg("  GET right after DEL returned in " + to_string(followup_ms) + "ms");

  if (send_and_expect_int(fd, {"del", "probe"}, 1)) return -1;
  return 0;
}

struct TestTiming {
  string name;
  int64_t ms = 0;
};

template <class Fn>
static int32_t run_test(int fd, const string &name, Fn &&fn, vector<TestTiming> &timings) {
  using clock = chrono::steady_clock;
  auto t0 = clock::now();
  int32_t rc = fn(fd);
  auto t1 = clock::now();
  int64_t ms = chrono::duration_cast<chrono::milliseconds>(t1 - t0).count();
  timings.push_back(TestTiming{name, ms});
  if (rc == 0) {
    msg("[time] " + name + " " + to_string(ms) + "ms");
  }
  return rc;
}

int main() {
  int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) {
    die("socket()");
  }

  struct timeval tv = {};
  tv.tv_sec = 2;
  (void)setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
  (void)setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

  int on = 1;
  (void)setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &on, sizeof(on));

  struct sockaddr_in addr = {};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(1234);
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  int rv = connect(fd, (const struct sockaddr*)&addr, sizeof(addr));
  if (rv) {
    die("connect");
  }

  vector<TestTiming> timings;
  if (run_test(fd, "basic", test_basic, timings)) goto L_DONE;
  if (run_test(fd, "update", test_update, timings)) goto L_DONE;
  if (run_test(fd, "many_keys", test_many_keys, timings)) goto L_DONE;
  if (run_test(fd, "random", test_random, timings)) goto L_DONE;
  if (run_test(fd, "large_value", test_large_value, timings)) goto L_DONE;
  if (run_test(fd, "bigzset_async_del", test_bigzset_async_del, timings)) goto L_DONE;

  {
    int64_t total = 0;
    for (const auto &t : timings) total += t.ms;
    msg("TOTAL " + to_string(total) + "ms");
  }
  msg("ALL TESTS PASSED");

L_DONE:
  close(fd);
  return 0;
}
