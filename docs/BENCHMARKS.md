# Lightweight Benchmark Results

## Objective

The benchmark supplies a basic, dependency-free engineering comparison for the
bounded `SafeConcurrentBuffer`. It measures finite transfers across different
producer, consumer, capacity, and payload configurations while validating that
every unique message is consumed exactly once.

It is intentionally not a scientific or statistically rigorous benchmark.

## Methodology

Each scenario:

- creates a fresh buffer and a finite set of producer and consumer threads;
- assigns every message a unique 64-bit ID;
- starts timing with `std::chrono::steady_clock` immediately before worker
  creation;
- includes worker creation, message transfer, deterministic shutdown, joins,
  and integrity observation in the timed interval;
- closes the buffer only after all producers finish;
- records each consumed ID atomically;
- fails if a worker throws, a push is rejected, an ID is missing or duplicated,
  counters disagree, or the final buffer is not empty.

The reported throughput is `consumed messages / total elapsed seconds`.

## Local Environment

- Execution date: 2026-07-29
- CPU: AMD FX(tm)-8350 Eight-Core Processor (8 cores, 1 thread per core)
- Operating system: Linux Mint 22.3
- Kernel: Linux 7.0.0-28-generic
- Compiler: GCC 13.3.0 (`g++ (Ubuntu 13.3.0-6ubuntu2~24.04.1)`)
- C++ mode: C++20
- Build type: Release
- Strict warnings: enabled
- Warnings as errors: enabled

## Commands

```bash
cmake -S . -B build-v020-gcc \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_TESTING=ON \
    -DBUILD_BENCHMARKS=ON \
    -DENABLE_STRICT_WARNINGS=ON \
    -DENABLE_WARNINGS_AS_ERRORS=ON \
    -DENABLE_ASAN_UBSAN=OFF \
    -DENABLE_TSAN=OFF \
    -DCMAKE_CXX_COMPILER=g++
cmake --build build-v020-gcc --parallel
./build-v020-gcc/concurrent_buffer_benchmark
```

## Measured Results

All scenarios completed with equal produced and consumed counts, no missing
IDs, no duplicate IDs, consistent buffer metrics, and an empty final buffer.

- `small-1p-1c-cap64`: 1 producer, 1 consumer, capacity 64, 16-byte payload,
  25,000 messages; 19.437 ms; approximately 1,286,237 messages/second.
- `small-2p-2c-cap64`: 2 producers, 2 consumers, capacity 64, 16-byte payload,
  50,000 messages; 55.660 ms; approximately 898,309 messages/second.
- `small-4p-4c-cap64`: 4 producers, 4 consumers, capacity 64, 16-byte payload,
  100,000 messages; 182.658 ms; approximately 547,470 messages/second.
- `small-4p-4c-cap1`: 4 producers, 4 consumers, capacity 1, 16-byte payload,
  100,000 messages; 2,618.000 ms; approximately 38,197 messages/second.
- `moderate-1p-1c-cap64`: 1 producer, 1 consumer, capacity 64, 1,024-byte
  payload, 5,000 messages; 11.738 ms; approximately 425,965 messages/second.
- `moderate-4p-4c-cap64`: 4 producers, 4 consumers, capacity 64, 1,024-byte
  payload, 20,000 messages; 38.262 ms; approximately 522,708 messages/second.

## Limitations

These values come from one execution on one machine. The harness does not
perform warm-up phases, repeated sampling, confidence intervals, CPU affinity,
frequency control, scheduler isolation, or background-load control. It also
includes thread creation and shutdown overhead. Results will vary with
hardware, compiler, optimization, operating system, scheduler activity, and
system load.

The results are environment-specific and are intended for comparative
engineering analysis, not as universal performance guarantees.
