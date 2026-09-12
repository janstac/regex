#pragma once

#include <algorithm>
#include <istream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace grep::detail {
// Internal: finish each batch at the first newline at or beyond the target.
class BatchReader {
    std::istream &input;
    std::vector<char> buffer;
    std::size_t position = 0;
    std::size_t available = 0;
    std::size_t target;

  public:
    BatchReader(std::istream &input, std::size_t target, std::size_t read_bytes)
        : input(input), buffer(read_bytes), target(target) {
        if (!target || !read_bytes ||
            read_bytes > static_cast<std::size_t>(std::numeric_limits<std::streamsize>::max()))
            throw std::invalid_argument("invalid batch/read size");
    }

    bool next(std::string &bytes) {
        bytes.clear();
        while (true) {
            if (position == available) {
                if (input.bad() || (input.fail() && !input.eof()))
                    throw std::runtime_error("failed to read input");
                if (input.eof())
                    return !bytes.empty();
                input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
                if (input.bad() || (input.fail() && !input.eof()))
                    throw std::runtime_error("failed to read input");
                available = static_cast<std::size_t>(input.gcount());
                position = 0;
                if (!available)
                    return !bytes.empty();
            }
            // Skip directly to the target, then look for a complete-line boundary.
            const auto needed = bytes.size() < target ? target - bytes.size() - 1 : 0;
            const auto start = position + std::min(needed, available - position);
            auto newline = std::find(buffer.begin() + start, buffer.begin() + available, '\n');
            const bool found = newline != buffer.begin() + available;
            const auto end =
                found ? static_cast<std::size_t>(newline - buffer.begin()) + 1 : available;
            bytes.append(buffer.data() + position, end - position);
            position = end;
            if (found)
                return true;
        }
    }
};
} // namespace grep::detail
