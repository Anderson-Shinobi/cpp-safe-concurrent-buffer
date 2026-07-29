# API Reference

## Namespace

All public symbols are in:

```cpp
elite::concurrency
```

## Class

```cpp
SafeConcurrentBuffer
```

`SafeConcurrentBuffer` is a bounded FIFO buffer for its fixed payload type. The
class is non-copyable and non-movable.

## ValueType

```cpp
using ValueType = std::vector<std::byte>;
```

## Constructor

```cpp
explicit SafeConcurrentBuffer(std::size_t capacity);
```

The capacity is fixed for the lifetime of the object. A zero capacity is
invalid and causes `std::invalid_argument` to be thrown. Copy construction,
copy assignment, move construction, and move assignment are deleted.

## push

```cpp
[[nodiscard]] bool push(ValueType value);
```

The payload is accepted by value and moved into internal storage. If the buffer
is full, the call blocks until capacity becomes available or the buffer closes.
An accepted payload returns `true`, increments the pushed metric, and notifies a
waiting consumer. Once closing begins, a blocked or later push returns `false`
without adding the payload or incrementing the metric.

## pop

```cpp
[[nodiscard]] std::optional<ValueType> pop();
```

If the buffer is empty and open, the call blocks until a payload is available
or the buffer closes. Available payloads are returned in FIFO acceptance order,
including payloads remaining after `close()`. Removing a payload increments the
popped metric and notifies a waiting producer.

`std::nullopt` is returned only when the buffer is both closed and empty.

## close

```cpp
void close() noexcept;
```

Closing is idempotent. It rejects new pushes and wakes blocked producers and
consumers. Already accepted payloads remain available for draining.

The existing `noexcept` policy means an exceptional failure while acquiring the
internal mutex cannot propagate and results in program termination.

## Observers

```cpp
[[nodiscard]] bool isClosed() const;
[[nodiscard]] std::size_t size() const;
[[nodiscard]] std::size_t capacity() const noexcept;
[[nodiscard]] std::uint64_t pushedCount() const noexcept;
[[nodiscard]] std::uint64_t poppedCount() const noexcept;
```

- `isClosed()` returns a synchronized snapshot of the closed state.
- `size()` returns a synchronized snapshot of the queued payload count.
- `capacity()` returns the immutable configured capacity.
- `pushedCount()` returns the number of accepted pushes.
- `poppedCount()` returns the number of payloads removed by `pop()`.

Snapshots do not make a later check-then-act sequence atomic. The counters are
diagnostic metrics and do not publish queue state.

## Thread-Safety Guarantees

Public operations may be called from multiple threads according to their
documented contract. Queue contents and the closed state are protected by a
mutex. Diagnostic metrics are atomic. Condition-variable waits use predicates
that account for queue state and closure.

The implementation does not claim lock-free or wait-free progress, fairness,
hard real-time behavior, priority handling, or formal freedom from races in
arbitrary external usage.

## Lifetime Requirements

The buffer object must remain alive until every thread that can access it has
finished all member calls. Destruction concurrent with access is unsupported
and is not made safe by the class.

## Exceptions

- Construction with zero capacity throws `std::invalid_argument`.
- Creating or copying the by-value payload before or during a call can propagate
  allocation-related exceptions.
- Synchronization operations in `push()`, `pop()`, `size()`, and `isClosed()`
  can propagate applicable standard-library exceptions such as
  `std::system_error`.
- `close()` is `noexcept`; it cannot propagate an exception.
- `capacity()`, `pushedCount()`, and `poppedCount()` are `noexcept`.

No broader exception guarantee is asserted beyond the declared interface and
the standard-library operations it uses.

## Limitations

- FIFO ordering reflects payload acceptance under synchronization; producer
  scheduling fairness is not guaranteed.
- Operations have no timeout support.
- There is no `std::stop_token` cancellation.
- There is no priority mechanism.
- Capacity is fixed at construction.
- The payload type is specifically `std::vector<std::byte>`.
- Diagnostic 64-bit metrics can wrap after 2^64 increments.
