# Redis-Like In-Memory Database

Small C++ project that implements a Redis-inspired in-memory database over a custom TCP protocol.

The server uses non-blocking sockets and a single-threaded `poll()` event loop to handle client connections.

Requests are read into per-connection buffers, parsed from a framed binary format, executed against an in-memory store, and written back as typed responses.

Current commands: `set`, `get`, `del`, and `keys`.

Main server: `src/server/08_server.cpp`

Main client/test harness: `src/client/03_client.cpp`

Storage uses a custom hash table with incremental rehashing; the AVL tree is included for future ordered operations.

Build:
```bash
cmake -S . -B build
cmake --build build
./build/server_08
./build/client
```
