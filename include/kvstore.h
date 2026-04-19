#pragma once

#include "buffer.h"
#include "common.h"
#include "entry.h"
#include "hashtable.h"
#include "thread_pool.h"
#include "ttl_heap.h"

#include <chrono>
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

class KVStore {
  HMap db;
  TtlHeap ttl_;
  ThreadPool pool_{4};

  void set_ttl(Entry& e, std::chrono::milliseconds ms);
  void persist(Entry& e) { ttl_.remove(e); }
  void schedule_entry_delete(std::unique_ptr<Entry> owned);
  Entry* find(std::string_view key);
  ZSet* ensure_zset(std::string& key);

 public:
  KVStore();
  ~KVStore();

  KVStore(const KVStore&) = delete;
  KVStore& operator=(const KVStore&) = delete;

  std::optional<Clock::time_point> ttl_next_deadline() const {
    return ttl_.peek_deadline();
  }
  size_t reap_expired(size_t cap, Clock::time_point now);

  void do_get(std::vector<std::string>& cmd, Buffer& out);
  void do_set(std::vector<std::string>& cmd, Buffer& out);
  void do_del(std::vector<std::string>& cmd, Buffer& out);
  void do_keys(std::vector<std::string>& cmd, Buffer& out);
  void do_zadd(std::vector<std::string>& cmd, Buffer& out);
  void do_zrem(std::vector<std::string>& cmd, Buffer& out);
  void do_zscore(std::vector<std::string>& cmd, Buffer& out);
  void do_zquery(std::vector<std::string>& cmd, Buffer& out);
  void do_expire(std::vector<std::string>& cmd, Buffer& out);
  void do_ttl(std::vector<std::string>& cmd, Buffer& out);
  void do_persist(std::vector<std::string>& cmd, Buffer& out);

  void do_request(std::vector<std::string>& cmd, Buffer& out);
};
