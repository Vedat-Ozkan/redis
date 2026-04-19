#pragma once

#include "buffer.h"
#include "common.h"

#include <boost/intrusive/list.hpp>
#include <boost/intrusive/list_hook.hpp>

struct Conn {
  int fd = -1;
  bool want_read = false;
  bool want_write = false;
  bool want_close = false;
  Buffer incoming;
  Buffer outgoing;
  Clock::time_point last_active;
  boost::intrusive::list_member_hook<> idle_hook;
};

using IdleList = boost::intrusive::list<Conn, boost::intrusive::member_hook<Conn, boost::intrusive::list_member_hook<>, &Conn::idle_hook>>;

void fd_set_nb(int fd);
