# C++ Safe Concurrent Buffer

A focused Modern C++ portfolio project demonstrating a bounded, thread-safe FIFO
buffer with deterministic shutdown, explicit ownership, and auditable concurrency
invariants.

## Features

- C++20 library and demonstration application
- Multiple concurrent producers and consumers
- Blocking bounded-buffer semantics without busy waiting
- Explicit, idempotent close with post-close draining
- Atomic diagnostic metrics
- RAII-based memory and thread lifetime management

## Requirements

- CMake 3.20 or newer
- A C++20 compiler such as GCC or Clang
- A platform threading implementation supported by CMake
- GoogleTest 1.15.2, resolved from an installed package when available or
  downloaded by CMake when tests are enabled

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
cmake --build build --parallel
```

## Run

```bash
./build/concurrent_buffer_demo
```

## Concurrent API Contract

- `push()` blocks while the bounded queue is full. Once `close()` begins, blocked
  producers wake and every later push is rejected without changing the queue or
  successful-push metric.
- `pop()` blocks while the queue is empty and open. Closing does not discard
  accepted values: consumers drain the queue before `pop()` returns
  `std::nullopt`.
- `close()` is idempotent and wakes all blocked producers and consumers. It is
  `noexcept` so it remains usable from worker exception handlers; an exceptional
  failure to lock its internal mutex therefore terminates the process.
- `size()` and `isClosed()` acquire the internal mutex and may propagate
  `std::system_error`. Their return values are snapshots, not synchronization for
  a later check-then-act operation.
- The immutable capacity and atomic metrics are non-blocking observations.
  Metrics use relaxed atomic ordering because they do not publish queue state.
- Callers must keep the buffer alive until every concurrent member call has
  returned. The demonstration enforces this with shared ownership and joins all
  workers before releasing its final reference.

## Test

```bash
ctest --test-dir build --output-on-failure
./build/concurrent_buffer_tests
```

The GoogleTest suite covers construction, FIFO behavior, capacity, metrics,
blocking producers and consumers, close and drain semantics, and concurrent
multi-worker transfer integrity. CMake prefers an existing GoogleTest package
and otherwise fetches the pinned 1.15.2 release.

Sanitizer configurations and continuous integration will be added in later
project stages.
