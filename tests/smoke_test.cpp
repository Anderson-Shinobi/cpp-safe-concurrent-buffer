#include "safe_concurrent_buffer.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <future>
#include <latch>
#include <optional>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

namespace {

using Buffer = elite::concurrency::SafeConcurrentBuffer;
using namespace std::chrono_literals;

constexpr auto blockedObservationWindow = 100ms;
constexpr auto completionTimeout = 2s;

[[nodiscard]] Buffer::ValueType makePayload(const std::uint64_t value) {
    Buffer::ValueType payload;
    payload.reserve(sizeof(value));

    // A fixed big-endian representation keeps test IDs portable across hosts.
    for (std::size_t byteIndex{0}; byteIndex < sizeof(value); ++byteIndex) {
        const auto shift = static_cast<unsigned int>(
            (sizeof(value) - 1U - byteIndex) * 8U);
        const auto byteValue = static_cast<unsigned char>((value >> shift) & 0xFFU);
        payload.push_back(static_cast<std::byte>(byteValue));
    }

    return payload;
}

[[nodiscard]] std::uint64_t decodePayload(const Buffer::ValueType& payload) {
    if (payload.size() != sizeof(std::uint64_t)) {
        throw std::invalid_argument{"Payload must contain exactly eight bytes"};
    }

    std::uint64_t value{0};
    for (const std::byte byte : payload) {
        value = (value << 8U) | std::to_integer<std::uint64_t>(byte);
    }
    return value;
}

void joinAll(std::vector<std::jthread>& threads) {
    for (auto& thread : threads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
}

struct TransferResult {
    std::uint64_t expected{0};
    std::uint64_t produced{0};
    std::uint64_t consumed{0};
    std::uint64_t pushed{0};
    std::uint64_t popped{0};
    std::size_t finalSize{0};
    bool closed{false};
    std::size_t missingIds{0};
    std::size_t duplicatedIds{0};
};

[[nodiscard]] TransferResult runConcurrentTransfer(
    const std::size_t producerCount,
    const std::size_t consumerCount,
    const std::size_t itemsPerProducer,
    const std::size_t capacity) {
    const std::size_t expected{producerCount * itemsPerProducer};
    Buffer buffer{capacity};
    std::atomic<std::uint64_t> produced{0};
    std::atomic<std::uint64_t> consumed{0};
    std::vector<std::atomic<std::uint32_t>> observations(expected);
    std::vector<std::exception_ptr> workerExceptions(
        producerCount + consumerCount);
    std::vector<std::jthread> consumers;
    std::vector<std::jthread> producers;

    for (auto& observation : observations) {
        observation.store(0, std::memory_order_relaxed);
    }

    consumers.reserve(consumerCount);
    producers.reserve(producerCount);

    try {
        for (std::size_t consumerIndex{0}; consumerIndex < consumerCount;
             ++consumerIndex) {
            consumers.emplace_back([&, consumerIndex] {
                try {
                    while (auto payload = buffer.pop()) {
                        const std::uint64_t id{decodePayload(*payload)};
                        if (id >= expected) {
                            throw std::out_of_range{"Decoded item ID is outside the test range"};
                        }
                        observations[static_cast<std::size_t>(id)].fetch_add(
                            1, std::memory_order_relaxed);
                        consumed.fetch_add(1, std::memory_order_relaxed);
                    }
                } catch (...) {
                    workerExceptions[consumerIndex] = std::current_exception();
                    buffer.close();
                }
            });
        }

        for (std::size_t producerIndex{0}; producerIndex < producerCount;
             ++producerIndex) {
            producers.emplace_back([&, producerIndex] {
                try {
                    for (std::size_t itemIndex{0}; itemIndex < itemsPerProducer;
                         ++itemIndex) {
                        const std::size_t id{
                            producerIndex * itemsPerProducer + itemIndex};
                        if (!buffer.push(makePayload(static_cast<std::uint64_t>(id)))) {
                            return;
                        }
                        produced.fetch_add(1, std::memory_order_relaxed);
                    }
                } catch (...) {
                    workerExceptions[consumerCount + producerIndex] =
                        std::current_exception();
                    buffer.close();
                }
            });
        }

        joinAll(producers);
        buffer.close();
        joinAll(consumers);
    } catch (...) {
        buffer.close();
        joinAll(producers);
        joinAll(consumers);
        throw;
    }

    for (const auto& workerException : workerExceptions) {
        if (workerException) {
            std::rethrow_exception(workerException);
        }
    }

    std::size_t missingIds{0};
    std::size_t duplicatedIds{0};
    for (const auto& observation : observations) {
        const std::uint32_t count{
            observation.load(std::memory_order_relaxed)};
        if (count == 0) {
            ++missingIds;
        } else if (count > 1) {
            ++duplicatedIds;
        }
    }

    return TransferResult{
        static_cast<std::uint64_t>(expected),
        produced.load(std::memory_order_relaxed),
        consumed.load(std::memory_order_relaxed),
        buffer.pushedCount(),
        buffer.poppedCount(),
        buffer.size(),
        buffer.isClosed(),
        missingIds,
        duplicatedIds,
    };
}

TEST(SafeConcurrentBufferTest, ZeroCapacityThrows) {
    EXPECT_THROW(Buffer{0}, std::invalid_argument);
}

TEST(SafeConcurrentBufferTest, InitialStateIsValid) {
    constexpr std::size_t capacity{7};
    Buffer buffer{capacity};

    EXPECT_EQ(buffer.size(), 0U);
    EXPECT_EQ(buffer.capacity(), capacity);
    EXPECT_FALSE(buffer.isClosed());
    EXPECT_EQ(buffer.pushedCount(), 0U);
    EXPECT_EQ(buffer.poppedCount(), 0U);
}

TEST(SafeConcurrentBufferTest, PushAndPopSinglePayload) {
    Buffer buffer{2};
    const Buffer::ValueType expected{makePayload(42)};

    EXPECT_TRUE(buffer.push(expected));
    EXPECT_EQ(buffer.size(), 1U);

    const auto received = buffer.pop();
    ASSERT_TRUE(received.has_value());
    EXPECT_EQ(*received, expected);
    EXPECT_EQ(buffer.size(), 0U);
    EXPECT_EQ(buffer.pushedCount(), 1U);
    EXPECT_EQ(buffer.poppedCount(), 1U);
}

TEST(SafeConcurrentBufferTest, PreservesFifoOrder) {
    Buffer buffer{4};
    const std::vector<Buffer::ValueType> expected{
        makePayload(11), makePayload(22), makePayload(33), makePayload(44)};

    for (const auto& payload : expected) {
        ASSERT_TRUE(buffer.push(payload));
    }

    for (const auto& payload : expected) {
        const auto received = buffer.pop();
        ASSERT_TRUE(received.has_value());
        EXPECT_EQ(*received, payload);
    }
}

TEST(SafeConcurrentBufferTest, CloseIsIdempotent) {
    Buffer buffer{2};
    ASSERT_TRUE(buffer.push(makePayload(5)));
    const auto pushesBeforeClose = buffer.pushedCount();
    const auto popsBeforeClose = buffer.poppedCount();

    EXPECT_NO_THROW(buffer.close());
    EXPECT_NO_THROW(buffer.close());
    EXPECT_NO_THROW(buffer.close());

    EXPECT_TRUE(buffer.isClosed());
    EXPECT_EQ(buffer.size(), 1U);
    EXPECT_EQ(buffer.pushedCount(), pushesBeforeClose);
    EXPECT_EQ(buffer.poppedCount(), popsBeforeClose);
}

TEST(SafeConcurrentBufferTest, PushAfterCloseIsRejected) {
    Buffer buffer{1};
    buffer.close();

    EXPECT_FALSE(buffer.push(makePayload(1)));
    EXPECT_EQ(buffer.size(), 0U);
    EXPECT_EQ(buffer.pushedCount(), 0U);
}

TEST(SafeConcurrentBufferTest, DrainsItemsAfterClose) {
    Buffer buffer{3};
    const std::vector<Buffer::ValueType> expected{
        makePayload(3), makePayload(6), makePayload(9)};
    for (const auto& payload : expected) {
        ASSERT_TRUE(buffer.push(payload));
    }
    buffer.close();

    for (const auto& payload : expected) {
        const auto received = buffer.pop();
        ASSERT_TRUE(received.has_value());
        EXPECT_EQ(*received, payload);
    }

    EXPECT_FALSE(buffer.pop().has_value());
    EXPECT_EQ(buffer.poppedCount(), expected.size());
    EXPECT_EQ(buffer.size(), 0U);
}

TEST(SafeConcurrentBufferTest, PopOnClosedEmptyBufferReturnsNullopt) {
    Buffer buffer{1};
    buffer.close();

    EXPECT_EQ(buffer.pop(), std::nullopt);
}

TEST(SafeConcurrentBufferTest, ConsumerBlocksUntilPush) {
    Buffer buffer{1};
    const Buffer::ValueType expected{makePayload(101)};
    std::latch started{1};
    std::promise<std::optional<Buffer::ValueType>> resultPromise;
    auto resultFuture = resultPromise.get_future();
    std::jthread consumer{
        [&buffer, &started, promise = std::move(resultPromise)]() mutable noexcept {
            started.count_down();
            try {
                promise.set_value(buffer.pop());
            } catch (...) {
                try {
                    promise.set_exception(std::current_exception());
                } catch (...) {
                    std::terminate();
                }
            }
        }};

    started.wait();
    // This moderate observation window detects premature completion without
    // using a sleep as synchronization.
    EXPECT_EQ(resultFuture.wait_for(blockedObservationWindow),
              std::future_status::timeout);
    EXPECT_TRUE(buffer.push(expected));

    const auto completionStatus = resultFuture.wait_for(completionTimeout);
    if (completionStatus != std::future_status::ready) {
        buffer.close();
    }
    EXPECT_EQ(completionStatus, std::future_status::ready);

    std::optional<Buffer::ValueType> received;
    if (completionStatus == std::future_status::ready) {
        EXPECT_NO_THROW(received = resultFuture.get());
    }
    buffer.close();
    if (consumer.joinable()) {
        consumer.join();
    }

    ASSERT_TRUE(received.has_value());
    EXPECT_EQ(*received, expected);
}

TEST(SafeConcurrentBufferTest, ConsumerUnblocksWhenBufferCloses) {
    Buffer buffer{1};
    std::latch started{1};
    std::promise<std::optional<Buffer::ValueType>> resultPromise;
    auto resultFuture = resultPromise.get_future();
    std::jthread consumer{
        [&buffer, &started, promise = std::move(resultPromise)]() mutable noexcept {
            started.count_down();
            try {
                promise.set_value(buffer.pop());
            } catch (...) {
                try {
                    promise.set_exception(std::current_exception());
                } catch (...) {
                    std::terminate();
                }
            }
        }};

    started.wait();
    EXPECT_EQ(resultFuture.wait_for(blockedObservationWindow),
              std::future_status::timeout);
    buffer.close();

    const auto completionStatus = resultFuture.wait_for(completionTimeout);
    EXPECT_EQ(completionStatus, std::future_status::ready);
    std::optional<Buffer::ValueType> received;
    if (completionStatus == std::future_status::ready) {
        EXPECT_NO_THROW(received = resultFuture.get());
    }
    if (consumer.joinable()) {
        consumer.join();
    }

    EXPECT_FALSE(received.has_value());
}

TEST(SafeConcurrentBufferTest, ProducerBlocksUntilSpaceIsAvailable) {
    Buffer buffer{1};
    const Buffer::ValueType first{makePayload(1)};
    const Buffer::ValueType second{makePayload(2)};
    ASSERT_TRUE(buffer.push(first));

    std::latch started{1};
    std::promise<bool> resultPromise;
    auto resultFuture = resultPromise.get_future();
    std::jthread producer{
        [&buffer, &started, second, promise = std::move(resultPromise)]() mutable noexcept {
            started.count_down();
            try {
                promise.set_value(buffer.push(second));
            } catch (...) {
                try {
                    promise.set_exception(std::current_exception());
                } catch (...) {
                    std::terminate();
                }
            }
        }};

    started.wait();
    EXPECT_EQ(resultFuture.wait_for(blockedObservationWindow),
              std::future_status::timeout);
    const auto firstResult = buffer.pop();

    const auto completionStatus = resultFuture.wait_for(completionTimeout);
    if (completionStatus != std::future_status::ready) {
        buffer.close();
    }
    EXPECT_EQ(completionStatus, std::future_status::ready);
    bool pushAccepted{false};
    if (completionStatus == std::future_status::ready) {
        EXPECT_NO_THROW(pushAccepted = resultFuture.get());
    }
    buffer.close();
    if (producer.joinable()) {
        producer.join();
    }

    ASSERT_TRUE(firstResult.has_value());
    EXPECT_EQ(*firstResult, first);
    EXPECT_TRUE(pushAccepted);
    const auto secondResult = buffer.pop();
    ASSERT_TRUE(secondResult.has_value());
    EXPECT_EQ(*secondResult, second);
}

TEST(SafeConcurrentBufferTest, BlockedProducerIsRejectedWhenBufferCloses) {
    Buffer buffer{1};
    const Buffer::ValueType first{makePayload(7)};
    ASSERT_TRUE(buffer.push(first));

    std::latch started{1};
    std::promise<bool> resultPromise;
    auto resultFuture = resultPromise.get_future();
    std::jthread producer{
        [&buffer, &started, promise = std::move(resultPromise)]() mutable noexcept {
            started.count_down();
            try {
                promise.set_value(buffer.push(makePayload(8)));
            } catch (...) {
                try {
                    promise.set_exception(std::current_exception());
                } catch (...) {
                    std::terminate();
                }
            }
        }};

    started.wait();
    EXPECT_EQ(resultFuture.wait_for(blockedObservationWindow),
              std::future_status::timeout);
    buffer.close();

    const auto completionStatus = resultFuture.wait_for(completionTimeout);
    EXPECT_EQ(completionStatus, std::future_status::ready);
    bool pushAccepted{true};
    if (completionStatus == std::future_status::ready) {
        EXPECT_NO_THROW(pushAccepted = resultFuture.get());
    }
    if (producer.joinable()) {
        producer.join();
    }

    EXPECT_FALSE(pushAccepted);
    EXPECT_EQ(buffer.size(), 1U);
    const auto original = buffer.pop();
    ASSERT_TRUE(original.has_value());
    EXPECT_EQ(*original, first);
    EXPECT_FALSE(buffer.pop().has_value());
}

TEST(SafeConcurrentBufferTest,
     MultipleProducersAndConsumersTransferEveryItemOnce) {
    const TransferResult result{runConcurrentTransfer(4, 4, 300, 16)};

    EXPECT_EQ(result.expected, 1'200U);
    EXPECT_EQ(result.produced, result.expected);
    EXPECT_EQ(result.consumed, result.expected);
    EXPECT_EQ(result.pushed, result.expected);
    EXPECT_EQ(result.popped, result.expected);
    EXPECT_EQ(result.finalSize, 0U);
    EXPECT_TRUE(result.closed);
    EXPECT_EQ(result.missingIds, 0U);
    EXPECT_EQ(result.duplicatedIds, 0U);
}

TEST(SafeConcurrentBufferTest, CapacityOneMaintainsCorrectnessUnderContention) {
    const TransferResult result{runConcurrentTransfer(3, 3, 200, 1)};

    EXPECT_EQ(result.expected, 600U);
    EXPECT_EQ(result.produced, result.expected);
    EXPECT_EQ(result.consumed, result.expected);
    EXPECT_EQ(result.pushed, result.expected);
    EXPECT_EQ(result.popped, result.expected);
    EXPECT_EQ(result.finalSize, 0U);
    EXPECT_TRUE(result.closed);
    EXPECT_EQ(result.missingIds, 0U);
    EXPECT_EQ(result.duplicatedIds, 0U);
}

TEST(SafeConcurrentBufferTest, MetricsRemainConsistent) {
    Buffer buffer{3};
    ASSERT_TRUE(buffer.push(makePayload(1)));
    ASSERT_TRUE(buffer.push(makePayload(2)));
    ASSERT_TRUE(buffer.push(makePayload(3)));
    ASSERT_TRUE(buffer.pop().has_value());

    buffer.close();
    EXPECT_FALSE(buffer.push(makePayload(4)));
    EXPECT_TRUE(buffer.pop().has_value());
    EXPECT_TRUE(buffer.pop().has_value());
    EXPECT_FALSE(buffer.pop().has_value());

    EXPECT_EQ(buffer.pushedCount(), 3U);
    EXPECT_EQ(buffer.poppedCount(), 3U);
    EXPECT_EQ(buffer.size(), 0U);
}

}  // namespace
