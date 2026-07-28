#include "safe_concurrent_buffer.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <vector>

namespace {

using Buffer = elite::concurrency::SafeConcurrentBuffer;

[[nodiscard]] Buffer::ValueType makePayload(const std::uint64_t value) {
    Buffer::ValueType payload;
    payload.reserve(sizeof(value));

    // Encode in network byte order so payload contents do not depend on host endianness.
    for (std::size_t byteIndex{0}; byteIndex < sizeof(value); ++byteIndex) {
        const auto shift = static_cast<unsigned int>(
            (sizeof(value) - 1U - byteIndex) * 8U);
        const auto byteValue = static_cast<unsigned char>((value >> shift) & 0xFFU);
        payload.push_back(static_cast<std::byte>(byteValue));
    }

    return payload;
}

void joinAll(std::vector<std::jthread>& threads) {
    for (auto& thread : threads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
}

}  // namespace

int main() {
    try {
        constexpr std::size_t bufferCapacity{32};
        constexpr std::uint64_t producerCount{2};
        constexpr std::uint64_t consumerCount{2};
        constexpr std::uint64_t messagesPerProducer{1'000};
        constexpr std::uint64_t expectedMessages{
            producerCount * messagesPerProducer};

        // Each worker owns a shared reference to the buffer. Explicit joins ensure
        // every worker releases that ownership before main examines final state.
        const auto buffer = std::make_shared<Buffer>(bufferCapacity);

        std::atomic<std::uint64_t> producedCount{0};
        std::atomic<std::uint64_t> consumedCount{0};
        std::mutex exceptionMutex;
        std::exception_ptr firstException;
        std::vector<std::jthread> consumers;
        std::vector<std::jthread> producers;

        consumers.reserve(static_cast<std::size_t>(consumerCount));
        producers.reserve(static_cast<std::size_t>(producerCount));

        try {
            for (std::uint64_t consumerIndex{0}; consumerIndex < consumerCount;
                 ++consumerIndex) {
                consumers.emplace_back(
                    [buffer, &consumedCount, &exceptionMutex, &firstException] {
                        try {
                            while (auto payload = buffer->pop()) {
                                if (payload->size() != sizeof(std::uint64_t)) {
                                    throw std::runtime_error{"Invalid payload size"};
                                }
                                consumedCount.fetch_add(1, std::memory_order_relaxed);
                            }
                        } catch (...) {
                            {
                                std::lock_guard lock{exceptionMutex};
                                if (!firstException) {
                                    firstException = std::current_exception();
                                }
                            }
                            buffer->close();
                        }
                    });
            }

            for (std::uint64_t producerIndex{0}; producerIndex < producerCount;
                 ++producerIndex) {
                producers.emplace_back(
                    [buffer, producerIndex, &producedCount, &exceptionMutex,
                     &firstException] {
                        try {
                            for (std::uint64_t messageIndex{0};
                                 messageIndex < messagesPerProducer; ++messageIndex) {
                                const std::uint64_t messageId{
                                    producerIndex * messagesPerProducer + messageIndex};
                                if (!buffer->push(makePayload(messageId))) {
                                    return;
                                }
                                producedCount.fetch_add(1, std::memory_order_relaxed);
                            }
                        } catch (...) {
                            {
                                std::lock_guard lock{exceptionMutex};
                                if (!firstException) {
                                    firstException = std::current_exception();
                                }
                            }
                            buffer->close();
                        }
                    });
            }

            // Joining producers before close guarantees that every accepted message
            // is visible to consumers and that no producer can write after shutdown.
            joinAll(producers);
            buffer->close();
            joinAll(consumers);
        } catch (...) {
            // A thread-construction or join failure must also release blocked workers.
            buffer->close();
            joinAll(producers);
            joinAll(consumers);
            throw;
        }

        std::exception_ptr threadException;
        {
            std::lock_guard lock{exceptionMutex};
            threadException = firstException;
        }
        if (threadException) {
            std::rethrow_exception(threadException);
        }

        // A second close verifies idempotence, while the rejected push confirms
        // that shutdown is permanent and does not alter successful-operation metrics.
        buffer->close();
        const bool postClosePushRejected{
            !buffer->push(makePayload(expectedMessages))};

        const std::uint64_t produced{
            producedCount.load(std::memory_order_relaxed)};
        const std::uint64_t consumed{
            consumedCount.load(std::memory_order_relaxed)};
        const std::uint64_t pushed{buffer->pushedCount()};
        const std::uint64_t popped{buffer->poppedCount()};
        const std::size_t finalSize{buffer->size()};
        const bool closed{buffer->isClosed()};

        const bool invariantsHold{
            produced == expectedMessages &&
            consumed == expectedMessages &&
            pushed == expectedMessages &&
            popped == expectedMessages &&
            finalSize == 0 &&
            closed &&
            postClosePushRejected};

        if (!invariantsHold) {
            std::cerr << "Concurrent buffer invariant validation failed\n"
                      << "Expected: " << expectedMessages << '\n'
                      << "Produced: " << produced << '\n'
                      << "Consumed: " << consumed << '\n'
                      << "Pushed metric: " << pushed << '\n'
                      << "Popped metric: " << popped << '\n'
                      << "Final size: " << finalSize << '\n'
                      << "Closed: " << std::boolalpha << closed << '\n'
                      << "Post-close push rejected: " << postClosePushRejected << '\n';
            return EXIT_FAILURE;
        }

        std::cout << "Concurrent buffer demo completed successfully\n"
                  << "Expected: " << expectedMessages << '\n'
                  << "Produced: " << produced << '\n'
                  << "Consumed: " << consumed << '\n'
                  << "Pushed metric: " << pushed << '\n'
                  << "Popped metric: " << popped << '\n'
                  << "Final size: " << finalSize << '\n'
                  << "Closed: " << std::boolalpha << closed << '\n'
                  << "Post-close push rejected: " << postClosePushRejected << '\n';
        return EXIT_SUCCESS;
    } catch (const std::exception& exception) {
        std::cerr << "Concurrent buffer demo failed: " << exception.what() << '\n';
    } catch (...) {
        std::cerr << "Concurrent buffer demo failed with an unknown exception\n";
    }

    return EXIT_FAILURE;
}
