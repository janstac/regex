#pragma once

#include "bytecode.h"

#include <expected>
#include <string_view>

namespace reg {

struct ExecutionLimits {
    std::size_t max_visited_states = 1'000'000;
    std::size_t max_backtrack_frames = 1'000'000;
};
enum class ExecutionErrorKind { StateLimitExceeded, BacktrackLimitExceeded };
struct ExecutionError {
    ExecutionErrorKind kind;
};
using ExecutionResult = std::expected<bool, ExecutionError>;

/**
 * @brief Searches \p input for a match of \p program.
 * @return Match status, or a resource-limit error distinct from rejection.
 */
ExecutionResult execute(const bytecode::Program &program, std::string_view input,
                        ExecutionLimits limits = {});

} // namespace reg
