#pragma once

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <optional>
#include <vector>

namespace elite::concurrency {

class SafeConcurrentBuffer final {
public:
    using ValueType = std::vector<std::byte>;

    explicit SafeConcurrentBuffer(std::size_t capacity);

    SafeConcurrentBuffer(const SafeConcurrentBuffer&) = delete;
    SafeConcurrentBuffer& operator=(const SafeConcurrentBuffer&) = delete;
    SafeConcurrentBuffer(SafeConcurrentBuffer&&) = delete;
    SafeConcurrentBuffer& operator=(SafeConcurrentBuffer&&) = delete;

    ~SafeConcurrentBuffer() = default;

    // Blocks while the buffer is full. Returns false without insertion after close.
    [[nodiscard]] bool push(ValueType value);

    // Blocks while empty. After close, drains queued values before returning nullopt.
    [[nodiscard]] std::optional<ValueType> pop();

    // Idempotent. Wakes every blocked producer and consumer. Failure to acquire the
    // internal mutex terminates because close is also used during exception recovery.
    void close() noexcept;

    // These synchronized observations are snapshots and must not be used for
    // check-then-act decisions with push or pop.
    [[nodiscard]] bool isClosed() const;
    [[nodiscard]] std::size_t size() const;
    [[nodiscard]] std::size_t capacity() const noexcept;
    [[nodiscard]] std::uint64_t pushedCount() const noexcept;
    [[nodiscard]] std::uint64_t poppedCount() const noexcept;

private:
    const std::size_t capacity_;
    mutable std::mutex mutex_;
    std::condition_variable notEmptyCondition_;
    std::condition_variable notFullCondition_;
    std::deque<ValueType> queue_;
    bool closed_{false};
    std::atomic<std::uint64_t> pushedCount_{0};
    std::atomic<std::uint64_t> poppedCount_{0};
};

}  // namespace elite::concurrency
