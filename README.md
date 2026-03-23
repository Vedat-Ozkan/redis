# Redis-Like In-Memory Database

A C++ project that implements a small Redis-inspired in-memory database over a custom TCP protocol. The codebase is intentionally kept in an incremental, systems-project style: earlier files show simpler milestones, and the latest server version pulls together non-blocking I/O, request framing, typed responses, and a custom hash table.

## Current Status

This project is still in progress. The current implementation is strong enough to demonstrate core systems programming work, but it is not yet a full Redis clone.

Implemented today:
- TCP client/server communication over a custom binary protocol
- Non-blocking server built around `poll()`
- Request framing and typed response serialization
- In-memory key-value operations: `set`, `get`, `del`, `keys`
- Custom hash table with incremental rehashing
- AVL tree implementation with standalone tests
- Client-side integration tests for database operations

Planned / not yet integrated:
- Sorted-set style operations using the AVL tree
- Key expiration / TTL support
- Background task offloading
- Persistence, replication, and broader Redis command coverage

## Project Structure

The repository keeps milestone files instead of hiding the development path:

- `src/server/03_server.cpp`: blocking single-connection server prototype
- `src/server/06_server.cpp`: event-driven server with basic key-value commands
- `src/server/08_server.cpp`: latest server version with typed responses and custom hash map
- `src/client/03_client.cpp`: client + test harness for the custom wire protocol
- `src/common.cpp`, `include/common.h`: shared protocol and I/O helpers
- `include/hashtable.h`: custom chained hash table with incremental rehashing
- `src/ds/avl.cpp`, `include/avl.h`: AVL tree implementation for future ordered data features
- `tests/test_avl.cpp`: randomized and deterministic AVL tests
- `CMakeLists.txt`: build configuration

If you are reviewing the project for the current state of the system, start with `src/server/08_server.cpp`.

## Architecture Overview

The latest server uses a single-threaded event loop with non-blocking sockets:

1. Accept client connections and mark sockets non-blocking
2. Read framed requests into per-connection buffers
3. Parse commands from a custom binary message format
4. Execute operations against the in-memory store
5. Serialize typed responses back to the client
6. Use `poll()` readiness events to interleave many active connections

The storage layer currently uses a custom hash table rather than STL maps in the latest server path. Rehashing work is spread across operations instead of happening all at once.

## Build

```bash
cmake -S . -B build
cmake --build build
```

## Run

Start the latest server:

```bash
./build/server_08
```

Run the client test harness in a second terminal:

```bash
./build/client
```

The server binds to port `1234` on localhost.

## Verification

Verified locally in this workspace:
- `cmake -S . -B build && cmake --build build`
- `./build/test_avl`

Full client/server runtime verification was not completed in this session because port `1234` was already unavailable in the local environment.

## Resume-Friendly Summary

- Built a Redis-inspired in-memory database in C++ using a custom TCP protocol and a non-blocking event loop
- Implemented request parsing, typed response serialization, and core key-value commands over an in-memory store
- Built custom storage internals including a hash table with incremental rehashing and an AVL tree for future ordered queries
- Wrote integration and data-structure tests to validate protocol behavior and balancing logic
