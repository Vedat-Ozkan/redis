# Redis-Like In-Memory Database

Small C++ project that implements a Redis-inspired in-memory database over a custom TCP protocol.

The server uses non-blocking sockets and a single-threaded `poll()` event loop to handle client connections.

Requests are read into per-connection buffers, parsed from a framed binary format, executed against an in-memory store, and written back as typed responses.

Current commands: `set`, `get`, `del`, `keys`, and sorted set operations via `zadd`, `zquery`, `zdel`, `zscore`.

### Project Structure

```
src/
  server/   — server.cpp, conn.cpp, protocol.cpp, kvstore.cpp
  client/   — client.cpp
  ds/       — avl.cpp, hashtable.cpp, zset.cpp, buffer.cpp, thread_pool.cpp, ttl_heap.cpp
include/    — header files
tests/      — test_avl.cpp
```

Storage uses a custom hash table with incremental rehashing. The AVL tree backs the sorted set (`zset`) implementation. A TTL heap handles key expiry, and a thread pool supports background tasks.

### Build

```bash
cmake -S . -B build
cmake --build build
./build/server
./build/client
```

### Test

```bash
./build/test_avl
```

### Acknowledgements

This project was built while working through [Build Your Own Redis with C/C++ by James Smith](https://build-your-own.org/redis/#table-of-contents), with my own implementation of the completed chapters.
