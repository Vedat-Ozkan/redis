#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "buffer.h"
#include "common.h"
#include "conn.h"
#include "kvstore.h"
#include "protocol.h"

using namespace std;

constexpr auto k_idle_timeout = chrono::seconds{5};
constexpr size_t k_max_ttl_evictions_per_tick = 2000;

class Server {
  int listen_fd_ = -1;
  vector<unique_ptr<Conn>> fd2conn_;
  IdleList idle_;
  KVStore store_;

  void touch(Conn* c) {
    c->last_active = Clock::now();
    if (c->idle_hook.is_linked()) {
      idle_.erase(idle_.iterator_to(*c));
    }
    idle_.push_back(*c);
  }

  void destroy_conn(int fd) {
    Conn* c = fd2conn_[fd].get();
    if (c->idle_hook.is_linked()) {
      idle_.erase(idle_.iterator_to(*c));
    }
    (void)close(fd);
    fd2conn_[fd].reset();
  }

  int next_timeout_ms() {
    optional<Clock::time_point> idle_deadline;
    if (!idle_.empty()) {
      idle_deadline = idle_.front().last_active + k_idle_timeout;
    }
    auto ttl_deadline = store_.ttl_next_deadline();

    optional<Clock::time_point> deadline;
    if (idle_deadline && ttl_deadline) {
      deadline = min(*idle_deadline, *ttl_deadline);
    } else {
      deadline = idle_deadline ? idle_deadline : ttl_deadline;
    }

    if (!deadline) return -1;
    auto now = Clock::now();
    if (*deadline <= now) return 0;
    return chrono::duration_cast<chrono::milliseconds>(*deadline - now).count();
  }

  void reap_idle() {
    auto now = Clock::now();
    while (!idle_.empty()) {
      Conn& c = idle_.front();
      if (c.last_active + k_idle_timeout > now) break;
      destroy_conn(c.fd);
    }
  }

  bool try_one_request(Conn* conn) {
    if (conn->incoming.size() < 4) return false;
    uint32_t len = 0;
    memcpy(&len, conn->incoming.data(), 4);
    if (len > k_max_msg) {
      conn->want_close = true;
      return false;
    }
    if (4 + len > conn->incoming.size()) return false;

    const uint8_t* request = conn->incoming.data() + 4;
    vector<string> cmd;
    if (parse_req(request, len, cmd) < 0) {
      conn->want_close = true;
      return false;
    }
    size_t header_pos = 0;
    conn->outgoing.response_begin(&header_pos);
    store_.do_request(cmd, conn->outgoing);
    conn->outgoing.response_end(header_pos);

    conn->incoming.consume(4 + len);
    return true;
  }

  unique_ptr<Conn> handle_accept(int fd) {
    struct sockaddr_in client_addr = {};
    socklen_t addrlen = sizeof(client_addr);
    int connfd = accept(fd, (struct sockaddr*)&client_addr, &addrlen);
    if (connfd < 0) return nullptr;

    fd_set_nb(connfd);
    int on = 1;
    setsockopt(connfd, IPPROTO_TCP, TCP_NODELAY, &on, sizeof(on));

    auto conn = make_unique<Conn>();
    conn->fd = connfd;
    conn->want_read = true;
    conn->last_active = Clock::now();
    return conn;
  }

  void handle_write(Conn* conn) {
    assert(conn->outgoing.size() > 0);
    ssize_t rv = write(conn->fd, conn->outgoing.data(), conn->outgoing.size());
    if (rv < 0 && errno == EAGAIN) return;
    if (rv < 0) {
      conn->want_close = true;
      return;
    }
    conn->outgoing.consume((size_t)rv);
    touch(conn);
    if (conn->outgoing.size() == 0) {
      conn->want_read = true;
      conn->want_write = false;
    } else {
      conn->want_write = true;
    }
  }

  void handle_read(Conn* conn) {
    uint8_t buf[64 * 1024];
    ssize_t rv = read(conn->fd, buf, sizeof(buf));
    if (rv < 0 && errno == EAGAIN) return;
    if (rv <= 0) {
      conn->want_close = true;
      return;
    }
    conn->incoming.append(buf, (size_t)rv);
    while (try_one_request(conn)) {
    }
    touch(conn);
    if (conn->outgoing.size() > 0) {
      conn->want_read = false;
      conn->want_write = true;
      return handle_write(conn);
    } else {
      conn->want_read = true;
    }
  }

 public:
  void run() {
    listen_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    int val = 1;
    setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));

    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(1234);
    addr.sin_addr.s_addr = htonl(0);

    int rv = bind(listen_fd_, (const struct sockaddr*)&addr, sizeof(addr));
    if (rv) die("bind()");

    rv = listen(listen_fd_, SOMAXCONN);
    if (rv) die("listen");

    fd_set_nb(listen_fd_);

    vector<struct pollfd> poll_args;

    while (true) {
      poll_args.clear();
      struct pollfd pfd {listen_fd_, POLLIN, 0};
      poll_args.push_back(pfd);

      for (const auto& conn : fd2conn_) {
        if (!conn) continue;
        struct pollfd pfd = {conn->fd, POLLERR, 0};
        if (conn->want_read) pfd.events |= POLLIN;
        if (conn->want_write) pfd.events |= POLLOUT;
        poll_args.push_back(pfd);
      }

      int rv = poll(poll_args.data(), (nfds_t)poll_args.size(), next_timeout_ms());
      reap_idle();
      store_.reap_expired(k_max_ttl_evictions_per_tick, Clock::now());
      if (rv < 0 && errno == EINTR) continue;
      if (rv < 0) die("poll");

      if (poll_args[0].revents) {
        if (auto conn = handle_accept(listen_fd_)) {
          touch(conn.get());
          int cfd = conn->fd;
          if (fd2conn_.size() <= (size_t)cfd) fd2conn_.resize(cfd + 1);
          fd2conn_[cfd] = move(conn);
        }
      }

      for (size_t i = 1; i < poll_args.size(); i++) {
        uint32_t ready = poll_args[i].revents;
        Conn* conn = fd2conn_[poll_args[i].fd].get();
        if (ready & POLLIN) handle_read(conn);
        if (ready & POLLOUT) handle_write(conn);
        if ((ready & POLLERR) || conn->want_close) destroy_conn(conn->fd);
      }
    }
  }
};

int main() {
  Server s;
  s.run();
  return 0;
}
