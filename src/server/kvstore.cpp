#include "kvstore.h"

#include "common.h"

#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <utility>

using namespace std;

static bool entry_eq(HNode* lhs, HNode* rhs) {
  Entry* le = container_of(lhs, Entry, node);
  Entry* re = container_of(rhs, Entry, node);
  return le->key == re->key;
}

static bool parse_dbl(const string& s, double& out) {
  char* endp = nullptr;
  out = strtod(s.c_str(), &endp);
  return endp != s.c_str() && *endp == '\0';
}

static bool parse_i64(const string& s, int64_t& out) {
  char* endp = nullptr;
  out = strtoll(s.c_str(), &endp, 10);
  return endp != s.c_str() && *endp == '\0';
}

static bool cb_keys(HNode* node, void* arg) {
  Buffer& out = *(Buffer*)arg;
  const string& key = container_of(node, Entry, node)->key;
  out.append_str(key.data(), key.size());
  return true;
}

KVStore::KVStore() = default;

KVStore::~KVStore() {
  db.hm_foreach([](HNode* node, void*) -> bool {
    delete container_of(node, Entry, node);
    return true;
  }, nullptr);
}

void KVStore::set_ttl(Entry& e, chrono::milliseconds ms) {
  ttl_.upsert(e, Clock::now() + ms);
}

void KVStore::schedule_entry_delete(unique_ptr<Entry> owned) {
  pool_.submit([p = owned.release()] { delete p; });
}

size_t KVStore::reap_expired(size_t cap, Clock::time_point now) {
  return ttl_.pop_expired(cap, now, [this](Entry& e) {
    db.hm_delete(&e.node, &entry_eq);
    schedule_entry_delete(unique_ptr<Entry>(&e));
  });
}

Entry* KVStore::find(string_view key) {
  Entry probe;
  probe.key.assign(key);
  probe.node.hcode = str_hash((const uint8_t*)key.data(), key.size());
  HNode* node = db.hm_lookup(&probe.node, &entry_eq);
  return node ? container_of(node, Entry, node) : nullptr;
}

ZSet* KVStore::ensure_zset(string& key) {
  if (Entry* e = find(key)) {
    if (!holds_alternative<ZSet>(e->value)) return nullptr;
    return &get<ZSet>(e->value);
  }
  auto ent = make_unique<Entry>();
  ent->key.swap(key);
  ent->node.hcode = str_hash((const uint8_t*)ent->key.data(), ent->key.size());
  ent->value.emplace<ZSet>();
  Entry* raw = ent.release();
  db.hm_insert(&raw->node);
  return &get<ZSet>(raw->value);
}

void KVStore::do_get(vector<string>& cmd, Buffer& out) {
  Entry* e = find(cmd[1]);
  if (!e) return out.append_nil();
  if (!holds_alternative<string>(e->value)) {
    return out.append_err(ERR_UNKNOWN, "wrong type");
  }
  const string& val = get<string>(e->value);
  out.append_str(val.data(), val.size());
}

void KVStore::do_set(vector<string>& cmd, Buffer& out) {
  if (Entry* e = find(cmd[1])) {
    if (!holds_alternative<string>(e->value)) {
      return out.append_err(ERR_UNKNOWN, "wrong type");
    }
    get<string>(e->value).swap(cmd[2]);
    return out.append_nil();
  }
  auto ent = make_unique<Entry>();
  ent->key.swap(cmd[1]);
  ent->node.hcode = str_hash((const uint8_t*)ent->key.data(), ent->key.size());
  ent->value.emplace<string>(move(cmd[2]));
  db.hm_insert(&ent->node);
  (void)ent.release();
  out.append_nil();
}

void KVStore::do_del(vector<string>& cmd, Buffer& out) {
  Entry probe;
  probe.key.swap(cmd[1]);
  probe.node.hcode =
      str_hash((const uint8_t*)probe.key.data(), probe.key.size());
  HNode* node = db.hm_delete(&probe.node, &entry_eq);
  if (node) {
    Entry* e = container_of(node, Entry, node);
    ttl_.remove(*e);
    schedule_entry_delete(unique_ptr<Entry>(e));
  }
  out.append_int(node ? 1 : 0);
}

void KVStore::do_keys(vector<string>&, Buffer& out) {
  out.append_arr((uint32_t)db.hm_size());
  db.hm_foreach(&cb_keys, (void*)&out);
}

void KVStore::do_zadd(vector<string>& cmd, Buffer& out) {
  double score;
  if (!parse_dbl(cmd[2], score)) {
    return out.append_err(ERR_UNKNOWN, "expected float");
  }
  ZSet* zs = ensure_zset(cmd[1]);
  if (!zs) return out.append_err(ERR_UNKNOWN, "wrong type");
  bool added = zs->insert(move(cmd[3]), score);
  out.append_int(added ? 1 : 0);
}

void KVStore::do_zrem(vector<string>& cmd, Buffer& out) {
  Entry* e = find(cmd[1]);
  if (!e) return out.append_int(0);
  if (!holds_alternative<ZSet>(e->value)) {
    return out.append_err(ERR_UNKNOWN, "wrong type");
  }
  bool removed = get<ZSet>(e->value).remove(cmd[2]);
  out.append_int(removed ? 1 : 0);
}

void KVStore::do_zscore(vector<string>& cmd, Buffer& out) {
  Entry* e = find(cmd[1]);
  if (!e) return out.append_nil();
  if (!holds_alternative<ZSet>(e->value)) {
    return out.append_err(ERR_UNKNOWN, "wrong type");
  }
  ZNode* zn = get<ZSet>(e->value).lookup(cmd[2]);
  if (!zn) return out.append_nil();
  out.append_double(zn->score);
}

void KVStore::do_zquery(vector<string>& cmd, Buffer& out) {
  double score;
  int64_t offset, limit;
  if (!parse_dbl(cmd[2], score)) {
    return out.append_err(ERR_UNKNOWN, "expected float");
  }
  if (!parse_i64(cmd[4], offset)) {
    return out.append_err(ERR_UNKNOWN, "expected int");
  }
  if (!parse_i64(cmd[5], limit)) {
    return out.append_err(ERR_UNKNOWN, "expected int");
  }

  Entry* e = find(cmd[1]);
  if (!e || !holds_alternative<ZSet>(e->value)) {
    out.append_arr(0);
    return;
  }
  ZSet& zs = get<ZSet>(e->value);

  size_t count_header = out.size();
  out.append_arr(0);

  ZNode* node = zs.seekge(score, cmd[3]);
  node = znode_offset(node, offset);

  uint32_t n = 0;
  while (node && (int64_t)n < limit) {
    out.append_str(node->name.data(), node->name.size());
    out.append_double(node->score);
    node = znode_offset(node, 1);
    n++;
  }
  out.patch_array_count(count_header, n * 2);
}

void KVStore::do_expire(vector<string>& cmd, Buffer& out) {
  int64_t ms;
  if (!parse_i64(cmd[2], ms)) {
    return out.append_err(ERR_UNKNOWN, "expected int");
  }
  Entry* e = find(cmd[1]);
  if (!e) return out.append_int(0);
  set_ttl(*e, chrono::milliseconds{ms});
  out.append_int(1);
}

void KVStore::do_ttl(vector<string>& cmd, Buffer& out) {
  Entry* e = find(cmd[1]);
  if (!e) return out.append_int(-2);
  if (e->heap_idx == kNotInHeap) return out.append_int(-1);
  auto deadline = ttl_.deadline_of(*e);
  auto now = Clock::now();
  auto ms = chrono::duration_cast<chrono::milliseconds>(deadline - now).count();
  out.append_int(ms < 0 ? 0 : ms);
}

void KVStore::do_persist(vector<string>& cmd, Buffer& out) {
  Entry* e = find(cmd[1]);
  if (!e || e->heap_idx == kNotInHeap) return out.append_int(0);
  persist(*e);
  out.append_int(1);
}

void KVStore::do_request(vector<string>& cmd, Buffer& out) {
  if (cmd.size() == 2 && cmd[0] == "get") {
    return do_get(cmd, out);
  } else if (cmd.size() == 3 && cmd[0] == "set") {
    return do_set(cmd, out);
  } else if (cmd.size() == 2 && cmd[0] == "del") {
    return do_del(cmd, out);
  } else if (cmd.size() == 1 && cmd[0] == "keys") {
    return do_keys(cmd, out);
  } else if (cmd.size() == 4 && cmd[0] == "zadd") {
    return do_zadd(cmd, out);
  } else if (cmd.size() == 3 && cmd[0] == "zrem") {
    return do_zrem(cmd, out);
  } else if (cmd.size() == 3 && cmd[0] == "zscore") {
    return do_zscore(cmd, out);
  } else if (cmd.size() == 6 && cmd[0] == "zquery") {
    return do_zquery(cmd, out);
  } else if (cmd.size() == 3 && cmd[0] == "expire") {
    return do_expire(cmd, out);
  } else if (cmd.size() == 2 && cmd[0] == "ttl") {
    return do_ttl(cmd, out);
  } else if (cmd.size() == 2 && cmd[0] == "persist") {
    return do_persist(cmd, out);
  } else {
    out.append_err(ERR_UNKNOWN, "unknown command.");
  }
}
