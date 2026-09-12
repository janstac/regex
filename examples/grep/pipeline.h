#pragma once

#include "vm.h"

#include <cstddef>
#include <iosfwd>

namespace grep {
struct Options {
    std::size_t worker_count = 1;
    std::size_t batch_bytes = 1 * 1024;
    std::size_t read_bytes = 1024 * 1024;
    std::size_t max_outstanding_batches = 3;
    reg::ExecutionLimits execution_limits{};
};

// Returns 1 on I/O failure or execution errors, otherwise 0.
int run(std::istream &input, std::ostream &output, std::ostream &errors,
        const reg::bytecode::Program &program, const Options &options);
} // namespace grep
