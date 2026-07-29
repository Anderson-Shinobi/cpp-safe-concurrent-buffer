#include "safe_concurrent_buffer.hpp"

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

using Buffer = elite::concurrency::SafeConcurrentBuffer;

struct Scenario {
    const char* name;
    std::size_t producers;
    std::size_t consumers;
    std::size_t capacity;
    std::size_t payloadBytes;
    std::size_t messagesPerProducer;
};

struct Result {
    std::uint64_t produced;
    std::uint64_t consumed;
    double durationSeconds;
};

[[nodiscard]] Buffer::ValueType makePayload(
    const std::uint64_t id,
    const std::size_t payloadBytes) {
    if (payloadBytes < sizeof(id)) {
        throw std::invalid_argument{"Payload must have room for its unique ID"};
    }

    Buffer::ValueType payload(payloadBytes, std::byte{0xA5});
    for (std::size_t byteIndex{0}; byteIndex < sizeof(id); ++byteIndex) {
        const auto shift = static_cast<unsigned int>(
            (sizeof(id) - 1U - byteIndex) * 8U);
        payload[byteIndex] =
            static_cast<std::byte>((id >> shift) & std::uint64_t{0xFF});
    }
    return payload;
}

[[nodiscard]] std::uint64_t decodeId(const Buffer::ValueType& payload) {
    if (payload.size() < sizeof(std::uint64_t)) {
        throw std::invalid_argument{"Payload is too small to contain an ID"};
    }

    std::uint64_t id{0};
    for (std::size_t byteIndex{0}; byteIndex < sizeof(id); ++byteIndex) {
        id = (id << 8U) |
             std::to_integer<std::uint64_t>(payload[byteIndex]);
    }
    return id;
}

void joinAll(std::vector<std::jthread>& threads) {
    for (auto& thread : threads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
}

[[nodiscard]] Result runScenario(const Scenario& scenario) {
    const std::size_t expected{
        scenario.producers * scenario.messagesPerProducer};
    Buffer buffer{scenario.capacity};
    std::atomic<std::uint64_t> produced{0};
    std::atomic<std::uint64_t> consumed{0};
    std::vector<std::atomic<std::uint32_t>> observations(expected);
    std::vector<std::exception_ptr> workerExceptions(
        scenario.producers + scenario.consumers);
    std::vector<std::jthread> producers;
    std::vector<std::jthread> consumers;

    for (auto& observation : observations) {
        observation.store(0, std::memory_order_relaxed);
    }
    producers.reserve(scenario.producers);
    consumers.reserve(scenario.consumers);

    const auto startedAt = std::chrono::steady_clock::now();
    try {
        for (std::size_t consumerIndex{0};
             consumerIndex < scenario.consumers;
             ++consumerIndex) {
            consumers.emplace_back([&, consumerIndex] {
                try {
                    while (auto payload = buffer.pop()) {
                        const std::uint64_t id{decodeId(*payload)};
                        if (id >= static_cast<std::uint64_t>(expected)) {
                            throw std::out_of_range{
                                "Consumed ID is outside the scenario range"};
                        }
                        observations[static_cast<std::size_t>(id)].fetch_add(
                            1, std::memory_order_relaxed);
                        consumed.fetch_add(1, std::memory_order_relaxed);
                    }
                } catch (...) {
                    workerExceptions[consumerIndex] =
                        std::current_exception();
                    buffer.close();
                }
            });
        }

        for (std::size_t producerIndex{0};
             producerIndex < scenario.producers;
             ++producerIndex) {
            producers.emplace_back([&, producerIndex] {
                try {
                    for (std::size_t messageIndex{0};
                         messageIndex < scenario.messagesPerProducer;
                         ++messageIndex) {
                        const std::size_t id{
                            producerIndex * scenario.messagesPerProducer +
                            messageIndex};
                        if (!buffer.push(makePayload(
                                static_cast<std::uint64_t>(id),
                                scenario.payloadBytes))) {
                            throw std::runtime_error{
                                "Buffer closed before all messages were produced"};
                        }
                        produced.fetch_add(1, std::memory_order_relaxed);
                    }
                } catch (...) {
                    workerExceptions[scenario.consumers + producerIndex] =
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
    const auto finishedAt = std::chrono::steady_clock::now();

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
        if (count == 0U) {
            ++missingIds;
        } else if (count > 1U) {
            ++duplicatedIds;
        }
    }

    const std::uint64_t producedCount{
        produced.load(std::memory_order_relaxed)};
    const std::uint64_t consumedCount{
        consumed.load(std::memory_order_relaxed)};
    const std::uint64_t expectedCount{
        static_cast<std::uint64_t>(expected)};
    if (producedCount != expectedCount ||
        consumedCount != expectedCount ||
        buffer.pushedCount() != expectedCount ||
        buffer.poppedCount() != expectedCount ||
        buffer.size() != 0U ||
        missingIds != 0U ||
        duplicatedIds != 0U) {
        throw std::runtime_error{
            "Scenario integrity validation failed"};
    }

    return Result{
        producedCount,
        consumedCount,
        std::chrono::duration<double>(finishedAt - startedAt).count(),
    };
}

void printResult(const Scenario& scenario, const Result& result) {
    const double throughput{
        result.durationSeconds > 0.0
            ? static_cast<double>(result.consumed) /
                  result.durationSeconds
            : 0.0};

    std::cout << "scenario=" << scenario.name
              << " producers=" << scenario.producers
              << " consumers=" << scenario.consumers
              << " capacity=" << scenario.capacity
              << " payload_bytes=" << scenario.payloadBytes
              << " produced=" << result.produced
              << " consumed=" << result.consumed
              << " duration_ms=" << std::fixed << std::setprecision(3)
              << (result.durationSeconds * 1'000.0)
              << " throughput_msg_per_sec=" << std::setprecision(0)
              << throughput << '\n';
}

}  // namespace

int main() {
    const std::vector<Scenario> scenarios{
        {"small-1p-1c-cap64", 1, 1, 64, 16, 25'000},
        {"small-2p-2c-cap64", 2, 2, 64, 16, 25'000},
        {"small-4p-4c-cap64", 4, 4, 64, 16, 25'000},
        {"small-4p-4c-cap1", 4, 4, 1, 16, 25'000},
        {"moderate-1p-1c-cap64", 1, 1, 64, 1'024, 5'000},
        {"moderate-4p-4c-cap64", 4, 4, 64, 1'024, 5'000},
    };

    try {
        std::cout << "Lightweight concurrent buffer benchmark\n";
        std::cout << "This is an engineering comparison, not a scientific benchmark.\n";
        for (const Scenario& scenario : scenarios) {
            printResult(scenario, runScenario(scenario));
        }
    } catch (const std::exception& exception) {
        std::cerr << "Benchmark failed: " << exception.what() << '\n';
        return 1;
    }

    return 0;
}
