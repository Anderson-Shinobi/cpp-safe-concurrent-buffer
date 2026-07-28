#include "safe_concurrent_buffer.hpp"

#include <stdexcept>
#include <utility>

namespace elite::concurrency {

SafeConcurrentBuffer::SafeConcurrentBuffer(const std::size_t capacity)
    : capacity_{capacity},
      mutex_{},
      notEmptyCondition_{},
      notFullCondition_{},
      queue_{},
      closed_{false},
      pushedCount_{0},
      poppedCount_{0} {
    if (capacity_ == 0) {
        throw std::invalid_argument{"SafeConcurrentBuffer capacity must be greater than zero"};
    }
}

bool SafeConcurrentBuffer::push(ValueType value) {
    std::unique_lock lock{mutex_};
    notFullCondition_.wait(lock, [this] {
        return queue_.size() < capacity_ || closed_;
    });

    if (closed_) {
        return false;
    }

    queue_.push_back(std::move(value));

    // These counters are diagnostic-only and do not publish queue state.
    // The mutex establishes queue ordering, so relaxed atomic ordering is sufficient.
    pushedCount_.fetch_add(1, std::memory_order_relaxed);

    lock.unlock();
    notEmptyCondition_.notify_one();
    return true;
}

std::optional<SafeConcurrentBuffer::ValueType> SafeConcurrentBuffer::pop() {
    std::unique_lock lock{mutex_};
    notEmptyCondition_.wait(lock, [this] {
        return !queue_.empty() || closed_;
    });

    // A closed buffer remains readable until every item queued before close is drained.
    if (queue_.empty()) {
        return std::nullopt;
    }

    ValueType value{std::move(queue_.front())};
    queue_.pop_front();

    // This metric is independent of synchronization and therefore needs no ordering
    // stronger than relaxed; queue visibility remains protected by the mutex.
    poppedCount_.fetch_add(1, std::memory_order_relaxed);

    lock.unlock();
    notFullCondition_.notify_one();
    return value;
}

void SafeConcurrentBuffer::close() noexcept {
    {
        std::lock_guard lock{mutex_};
        closed_ = true;
    }

    // All waiters must re-evaluate their predicates so producers can reject writes
    // and consumers can either drain remaining items or observe completion.
    notEmptyCondition_.notify_all();
    notFullCondition_.notify_all();
}

bool SafeConcurrentBuffer::isClosed() const {
    std::lock_guard lock{mutex_};
    return closed_;
}

std::size_t SafeConcurrentBuffer::size() const {
    std::lock_guard lock{mutex_};
    return queue_.size();
}

std::size_t SafeConcurrentBuffer::capacity() const noexcept {
    return capacity_;
}

std::uint64_t SafeConcurrentBuffer::pushedCount() const noexcept {
    return pushedCount_.load(std::memory_order_relaxed);
}

std::uint64_t SafeConcurrentBuffer::poppedCount() const noexcept {
    return poppedCount_.load(std::memory_order_relaxed);
}

}  // namespace elite::concurrency
