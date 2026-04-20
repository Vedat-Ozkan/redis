#include "common.h"

#ifndef SERVER_BIN
#define SERVER_BIN "./server"
#endif

#include <arpa/inet.h>
#include <cassert>
#include <csignal>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sstream>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

using namespace std;

// ---------------------------------------------------------------------------
// Server lifecycle
// ---------------------------------------------------------------------------

static pid_t g_server_pid = -1;

static void spawn_server() {
  pid_t pid = fork();
  if (pid < 0) { die("fork"); }
  if (pid == 0) {
    execl(SERVER_BIN, SERVER_BIN, nullptr);
    _exit(1);
  }
  g_server_pid = pid;
}

static void kill_server() {
  if (g_server_pid > 0) {
    kill(g_server_pid, SIGTERM);
    waitpid(g_server_pid, nullptr, 0);
    g_server_pid = -1;
  }
}

// ---------------------------------------------------------------------------
// Client connection
// ---------------------------------------------------------------------------

static int connect_with_retry(int port, int max_tries = 20) {
  for (int i = 0; i < max_tries; ++i) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    assert(fd >= 0);
    int on = 1;
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &on, sizeof(on));

    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)port);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (connect(fd, (const struct sockaddr*)&addr, sizeof(addr)) == 0) {
      return fd;
    }
    close(fd);
    usleep(50 * 1000);
  }
  die("connect_with_retry: server did not come up");
  return -1;
}

// ---------------------------------------------------------------------------
// Protocol helpers
// ---------------------------------------------------------------------------

struct Value {
  uint8_t tag = TAG_NIL;
  uint32_t err_code = 0;
  string err_msg;
  string str;
  int64_t i64 = 0;
  double dbl = 0.0;
  vector<Value> arr;
};

static bool read_u8(const uint8_t *&cur, const uint8_t *end, uint8_t &out) {
  if (cur + 1 > end) return false;
  out = *cur++; return true;
}
static bool read_i64(const uint8_t *&cur, const uint8_t *end, int64_t &out) {
  if (cur + 8 > end) return false;
  memcpy(&out, cur, 8); cur += 8; return true;
}
static bool read_dbl(const uint8_t *&cur, const uint8_t *end, double &out) {
  if (cur + 8 > end) return false;
  memcpy(&out, cur, 8); cur += 8; return true;
}
static bool read_bytes(const uint8_t *&cur, const uint8_t *end, uint32_t n, string &out) {
  if (cur + n > end) return false;
  out.assign((const char*)cur, (const char*)(cur + n)); cur += n; return true;
}

static bool parse_value(const uint8_t *&cur, const uint8_t *end, Value &out) {
  out = Value{};
  uint8_t tag = 0;
  if (!read_u8(cur, end, tag)) return false;
  out.tag = tag;
  switch (tag) {
    case TAG_NIL: return true;
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
    case TAG_INT: return read_i64(cur, end, out.i64);
    case TAG_DBL: return read_dbl(cur, end, out.dbl);
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
    default: return false;
  }
}

static void append_u32(vector<uint8_t> &out, uint32_t v) {
  uint8_t buf[4]; memcpy(buf, &v, 4);
  out.insert(out.end(), buf, buf + 4);
}

static int32_t send_req(int fd, const vector<string> &cmd) {
  vector<uint8_t> payload;
  append_u32(payload, (uint32_t)cmd.size());
  for (const string &s : cmd) {
    append_u32(payload, (uint32_t)s.size());
    payload.insert(payload.end(), s.begin(), s.end());
  }
  uint32_t ulen = (uint32_t)payload.size();
  if (write_all(fd, (char*)&ulen, 4)) return -1;
  return write_all(fd, (const char*)payload.data(), payload.size());
}

static int32_t read_res(int fd, Value &out) {
  uint32_t resp_len = 0;
  if (read_full(fd, (char*)&resp_len, 4)) return -1;
  vector<uint8_t> buf(resp_len);
  if (read_full(fd, (char*)buf.data(), resp_len)) return -1;
  const uint8_t *cur = buf.data(), *end = buf.data() + buf.size();
  return parse_value(cur, end, out) ? 0 : -1;
}

// ---------------------------------------------------------------------------
// Value formatter — TODO(human)
// ---------------------------------------------------------------------------

static string format_value(const Value &v) {
  // TODO(human): convert a Value to a human-readable string for the output file.
  // Each tag should produce a distinct format, e.g.:
  //   TAG_NIL  -> "(nil)"
  //   TAG_STR  -> the string itself
  //   TAG_INT  -> "(integer) 42"
  //   TAG_DBL  -> "(double) 1.500000"
  //   TAG_ERR  -> "(error) <code> <msg>"
  //   TAG_ARR  -> one element per line, each recursively formatted
  stringstream ss;
  switch(v.tag) {
    case TAG_NIL: return "(nil)";
    case TAG_STR: return v.str;
    case TAG_INT: return "(integer) " + to_string(v.i64);
    case TAG_DBL: return "(double) " + to_string(v.dbl);
    case TAG_ERR: return "(error) " + to_string(v.err_code) + " " + v.err_msg;
    case TAG_ARR: {
      string out;
      for (const auto &x : v.arr)
        out += format_value(x) + "\n";
      return out;
    }
    default: return "(unknown)";
  }
}

// ---------------------------------------------------------------------------
// File-driven harness
// ---------------------------------------------------------------------------

static vector<string> split_line(const string &line) {
  vector<string> tokens;
  istringstream iss(line);
  string tok;
  while (iss >> tok) tokens.push_back(tok);
  return tokens;
}

static void run_file(int fd, const string &input_path, const string &output_path) {
  ifstream in(input_path);
  if (!in) die("cannot open input: " + input_path);

  ofstream out(output_path);
  if (!out) die("cannot open output: " + output_path);

  string line;
  while (getline(in, line)) {
    if (line.empty() || line[0] == '#') {
      out << line << "\n";
      continue;
    }
    vector<string> cmd = split_line(line);
    if (cmd.empty()) continue;

    assert(send_req(fd, cmd) == 0);
    Value v;
    assert(read_res(fd, v) == 0);

    out << "> " << line << "\n" << format_value(v) << "\n";
  }
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main(int argc, char *argv[]) {
  string input_path  = argc > 1 ? argv[1] : "test_input.txt";
  string output_path = argc > 2 ? argv[2] : "test_output.txt";

  spawn_server();
  atexit(kill_server);

  int fd = connect_with_retry(1234);
  run_file(fd, input_path, output_path);
  close(fd);

  cout << "done -> " << output_path << "\n";
  return 0;
}
