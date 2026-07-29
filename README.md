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

## Diagnostic Build Options

- `ENABLE_STRICT_WARNINGS=ON` enables supported project warning flags.
- `ENABLE_WARNINGS_AS_ERRORS=ON` promotes project warnings to errors and can be
  disabled independently.
- `ENABLE_ASAN_UBSAN=ON` enables AddressSanitizer and
  UndefinedBehaviorSanitizer.
- `ENABLE_TSAN=ON` enables ThreadSanitizer.

ASan/UBSan and TSan are intentionally mutually exclusive. These options apply
only to project targets; GoogleTest is built without project warning or
Sanitizer flags.

### Strict GCC

```bash
cmake -S . -B build-gcc-strict \
    -DCMAKE_BUILD_TYPE=Debug \
    -DBUILD_TESTING=ON \
    -DENABLE_STRICT_WARNINGS=ON \
    -DENABLE_WARNINGS_AS_ERRORS=ON \
    -DCMAKE_CXX_COMPILER=g++
cmake --build build-gcc-strict --parallel
ctest --test-dir build-gcc-strict --output-on-failure
```

### Strict Clang

```bash
cmake -S . -B build-clang-strict \
    -DCMAKE_BUILD_TYPE=Debug \
    -DBUILD_TESTING=ON \
    -DENABLE_STRICT_WARNINGS=ON \
    -DENABLE_WARNINGS_AS_ERRORS=ON \
    -DCMAKE_CXX_COMPILER=clang++
cmake --build build-clang-strict --parallel
ctest --test-dir build-clang-strict --output-on-failure
```

### AddressSanitizer and UndefinedBehaviorSanitizer

```bash
cmake -S . -B build-asan-ubsan \
    -DCMAKE_BUILD_TYPE=Debug \
    -DBUILD_TESTING=ON \
    -DENABLE_ASAN_UBSAN=ON \
    -DENABLE_TSAN=OFF
cmake --build build-asan-ubsan --parallel
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
ctest --test-dir build-asan-ubsan --output-on-failure
```

### ThreadSanitizer

```bash
cmake -S . -B build-tsan \
    -DCMAKE_BUILD_TYPE=Debug \
    -DBUILD_TESTING=ON \
    -DENABLE_ASAN_UBSAN=OFF \
    -DENABLE_TSAN=ON
cmake --build build-tsan --parallel
TSAN_OPTIONS=halt_on_error=1:history_size=7 \
ctest --test-dir build-tsan --output-on-failure
```

## Local Validation Status

- GCC 13.3.0 strict build completed with warnings-as-errors and all 15 tests.
- ASan/UBSan completed with leak detection, including 20 repeated CTest runs.
- Clang was not available in the validation environment.
- The GCC ThreadSanitizer build completed, but its runtime aborted with
  `FATAL: ThreadSanitizer: unexpected memory mapping`; the TSan result is
  therefore inconclusive and is not evidence that the program is race-free.

Sanitizer runs improve defect detection but do not constitute formal proof of
memory safety, undefined-behavior freedom, or data-race freedom. Continuous
integration remains future work.
