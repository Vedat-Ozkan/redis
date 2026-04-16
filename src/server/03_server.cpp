#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <iostream>
#include <stdio.h>
#include <netinet/in.h>
#include <unistd.h>
#include "common.h"

using namespace std;

// static void do_something(int connfd) {
//   char rbuf[64] = {};
//   ssize_t n = read(connfd, rbuf, sizeof(rbuf) - 1);
//   if (n < 0) {
//     cerr << "read() error\n";
//     exit(1);
//     return;
//   }
//   cout << "client says: " << rbuf << "\n";

//   string wbuf = "world";
//   write(connfd, wbuf.c_str(), wbuf.length());
// }

static int32_t one_request(int connfd) {
  char rbuf[4 + k_max_msg];
  errno = 0;
  int32_t err = read_full(connfd, rbuf, 4);
  if (err) {
    msg(errno == 0 ? "EOF" : "read() error");
    return err;
  }
  uint32_t len = 0;
  memcpy(&len, rbuf, 4);
  if (len > k_max_msg) {
    msg("too long");
    return -1;
  }

  err = read_full(connfd, &rbuf[4], len);
  if (err) {
    msg("read() error");
    return err;
  }

  msg("client says: " + string(&rbuf[4], len));
  const char reply[] = "world";
  char wbuf[4 + sizeof(reply)];
  len = (uint32_t) strlen(reply);
  memcpy(wbuf, &len, 4);
  memcpy(&wbuf[4], reply, len);
  return write_all(connfd, wbuf, 4 + len);
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

  

  while (true) {
    struct sockaddr_in client_addr = {};
    socklen_t addrlen = sizeof(client_addr);
    int connfd = accept(fd, (struct sockaddr *)&client_addr, &addrlen);
    if (connfd < 0) {
      continue;
    }

    while (true) {
      int32_t err = one_request(connfd);
      if (err) {
        break;
      }
    }

    
    close(connfd);
  }
}




