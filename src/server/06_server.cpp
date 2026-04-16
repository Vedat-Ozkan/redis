#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <iostream>
#include <stdio.h>
#include <netinet/in.h>
#include <unistd.h>
#include <vector>
#include <poll.h>
#include <cassert>
#include <errno.h>
#include <fcntl.h>
#include <map>
#include <algorithm>
#include "common.h"

using namespace std;

static map<string, string> g_data;

class Buffer {
  vector<uint8_t> buf_;
  size_t begin_ = 0;
  size_t end_ = 0;

  public:
    explicit Buffer(size_t cap_inital = 0) {
      buf_.resize(cap_inital);
    }

    size_t size() const {
      return end_ - begin_;
    }

    bool empty() const {
      return size() == 0;
    }

    uint8_t* data() {
      return buf_.data() + begin_;
    }

    const uint8_t* data() const {
      return buf_.data() + begin_;
    }

    void clear() {
      begin_ = end_ = 0;
    }

    void consume(size_t n) {
      assert(n <= size());
      begin_ += n;
      if (begin_ == end_) {
        begin_ = end_ = 0;
      }
    }

    void append(const uint8_t *data, size_t len) {
      if (len == 0) {
        return;
      }

      if (buf_.size() < end_ + len) {
        if (buf_.size() >= size() + len) {
          memmove(buf_.data(), buf_.data() + begin_, size());
          end_ = size();
          begin_ = 0;
        } else {
          size_t cur = size();
          size_t new_cap = max(buf_.size() ? buf_.size() * 2: 16, cur + len);
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

};

struct Conn {
  int fd = -1;

  bool want_read = false;
  bool want_write = false;
  bool want_close = false;

  Buffer incoming;
  Buffer outgoing;
};

static void make_response(Buffer &out, Status status, const uint8_t *data, size_t len) {
  uint32_t resp_len = 4 + (uint32_t)len;
  uint32_t stat = (uint32_t)status;

  out.append((const uint8_t *)&resp_len, 4);
  out.append((const uint8_t *)&stat, 4);
  if (len > 0) {
    out.append(data, len);
  }
}

static void do_request(vector<string> &cmd, Buffer &out) {
  if (cmd.size() == 2 && cmd[0] == "get") {
    auto it = g_data.find(cmd[1]);
    if (it == g_data.end()) {
      make_response(out, Status::RES_NX, nullptr, 0);
      return;
    }
    const string &val = it->second;
    make_response(out, Status::RES_OK, (const uint8_t*)val.data(), val.size());
  } else if (cmd.size() == 3 && cmd[0] == "set") {
    g_data[cmd[1]].swap(cmd[2]);
    make_response(out, Status::RES_OK, nullptr, 0);
  } else if (cmd.size() == 2 && cmd[0] == "del") {
    g_data.erase(cmd[1]);
    make_response(out, Status::RES_OK, nullptr, 0);
  } else {
    make_response(out, Status::RES_ERR, nullptr, 0);
  }
}

static void fd_set_nb(int fd) {
  fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
}

static Conn *handle_accept(int fd) {
  struct sockaddr_in client_addr = {};
  socklen_t addrlen = sizeof(client_addr);
  int connfd = accept(fd, (struct sockaddr *)&client_addr, &addrlen);
  if (connfd < 0) {
    return NULL;
  }

  fd_set_nb(connfd);

  Conn *conn = new Conn();
  conn->fd = connfd;
  conn->want_read = true;
  conn->incoming = Buffer(4 + k_max_msg);
  conn->outgoing = Buffer(4 + k_max_msg);
  return conn;
}



static int32_t parse_req(const uint8_t *data, size_t size, vector<string> &out) {
  const uint8_t *end = data + size;
  uint32_t nstr = 0;
  if (!read_u32(data, end, nstr)) {
    return -1;
  }
  if (nstr > k_max_args) {
    return -1;  
  } 

  while (out.size() < nstr) {
    uint32_t len = 0;
    if (!read_u32(data, end, len)) {
      return -1;
    }

    out.push_back(string());
    if (!read_str(data, end, len, out.back())) {
      return -1;
    }
  }

  if (data != end) {
    return -1;
  }
  return 0;
}

static bool try_one_request(Conn *conn) {
  if (conn->incoming.size() < 4) {
    return false;
  }
  
  uint32_t len = 0;
  memcpy(&len, conn->incoming.data(), 4);
  if (len > k_max_msg) {
    conn->want_close = true;
    return false;
  }

  if (4 + len > conn->incoming.size()) {
    return false;
  }

  const uint8_t *request = conn->incoming.data() + 4;

  vector<string> cmd;
  if (parse_req(request, len, cmd) < 0) {
    conn->want_close = true;
    return false;
  }
  do_request(cmd, conn->outgoing);
  

  // conn->outgoing.append((const uint8_t *)&len, 4);
  // conn->outgoing.append(request, len);

  conn->incoming.consume(4 + len);

  return true;
}

static void handle_write(Conn *conn) {
  assert(conn->outgoing.size() > 0);
  ssize_t rv = write(conn->fd, conn->outgoing.data(), conn->outgoing.size());
  if (rv < 0 && errno == EAGAIN) {
    return;
  }
  if (rv < 0) {
    conn->want_close = true;
    return;
  }
  conn->outgoing.consume((size_t)rv);

  if (conn->outgoing.size() == 0) {
    conn->want_read = true;
    conn->want_write = false;
  } else {
    conn->want_write = true;
  }
}

static void handle_read(Conn *conn) {
  uint8_t buf[64 * 1024];
  ssize_t rv = read(conn->fd, buf, sizeof(buf));

  if (rv < 0 && errno == EAGAIN) {
    return;
  }
  if (rv <= 0) {
    conn->want_close = true;
    return;
  }
  conn->incoming.append(buf, (size_t)rv);

  while(try_one_request(conn)) {}

  if (conn->outgoing.size() > 0) {
    conn->want_read = false;
    conn->want_write = true;
    return handle_write(conn);
  } else {
    conn->want_read = true;
  }

}


int main() {
  int fd = socket(AF_INET, SOCK_STREAM, 0); 
  int val = 1;
  setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));

  struct sockaddr_in addr = {};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(1234);
  addr.sin_addr.s_addr = htonl(0);

  int rv = bind(fd, (const struct sockaddr *)&addr, sizeof(addr));
  if (rv) { die("bind()"); }

  rv = listen(fd, SOMAXCONN);
  if (rv) { die("listen"); }

  fd_set_nb(fd);

  vector<Conn *> fd2conn;

  vector<struct pollfd> poll_args;
  
  
  while (true) {
    poll_args.clear();
    
    struct pollfd pfd = {fd, POLLIN, 0};
    poll_args.push_back(pfd);

    for (Conn *conn : fd2conn) {
      if (!conn) {
        continue;
      }

      struct pollfd pfd = {conn->fd, POLLERR, 0};

      if (conn->want_read) {
        pfd.events |= POLLIN;
      }
      if (conn->want_write) {
        pfd.events |= POLLOUT;
      }
      poll_args.push_back(pfd);
    }

    int rv = poll(poll_args.data(), (nfds_t)poll_args.size(), -1);
    if (rv < 0 && errno == EINTR) {
      continue;
    }
    if (rv < 0) {
      die("poll");
    }

    if (poll_args[0].revents) {
      if (Conn *conn = handle_accept(fd)) {
        if (fd2conn.size() <= (size_t)conn->fd) {
          fd2conn.resize(conn->fd + 1);
        }
        fd2conn[conn->fd] = conn;
      }
    }

    for (size_t i = 1; i < poll_args.size(); i++) {
      uint32_t ready = poll_args[i].revents;
      Conn *conn = fd2conn[poll_args[i].fd];
      if (ready & POLLIN) {
        handle_read(conn);
      }
      if (ready & POLLOUT) {
        handle_write(conn);
      }

      if ((ready & POLLERR) || conn->want_close) {
        (void)close(conn->fd);
        fd2conn[conn->fd] = NULL;
        delete conn;
      }
    }
  }
}




