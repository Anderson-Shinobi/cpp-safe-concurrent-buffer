#include "safe_concurrent_buffer.hpp"

#include <cstddef>
#include <cstdlib>
#include <exception>
#include <iostream>

int main() {
    using Buffer = elite::concurrency::SafeConcurrentBuffer;

    try {
        Buffer buffer{2};
        const Buffer::ValueType expected{
            std::byte{0x10},
            std::byte{0x20},
            std::byte{0x30},
            std::byte{0x40},
        };

        if (!buffer.push(expected)) {
            std::cerr << "Installed consumer failed: initial push was rejected\n";
            return EXIT_FAILURE;
        }

        const auto received = buffer.pop();
        if (!received || *received != expected) {
            std::cerr << "Installed consumer failed: payload mismatch\n";
            return EXIT_FAILURE;
        }

        buffer.close();
        if (!buffer.isClosed() ||
            buffer.pop().has_value() ||
            buffer.size() != 0U ||
            buffer.capacity() != 2U ||
            buffer.pushedCount() != 1U ||
            buffer.poppedCount() != 1U) {
            std::cerr << "Installed consumer failed: final state mismatch\n";
            return EXIT_FAILURE;
        }

        std::cout << "Installed package consumer completed successfully\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& exception) {
        std::cerr << "Installed consumer failed: " << exception.what() << '\n';
        return EXIT_FAILURE;
    }
}
