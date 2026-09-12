#include "pipeline.h"
#include "batch_reader.h"

#include <algorithm>
#include <condition_variable>
#include <deque>
#include <exception>
#include <memory>
#include <mutex>
#include <ostream>
#include <stdexcept>
#include <thread>
#include <vector>

namespace grep {
namespace {
struct Result {
    std::size_t line;
    std::size_t offset;
    std::size_t length;
    reg::ExecutionResult status;
};
struct Batch {
    std::size_t sequence = 0;
    std::size_t first_line = 1;
    std::string bytes;
    std::vector<Result> results;
};
struct State {
    std::mutex mutex;
    std::condition_variable capacity_available, work_available, result_available;

    std::deque<std::unique_ptr<Batch>> pending;
    std::vector<std::unique_ptr<Batch>> completed;

    std::size_t outstanding = 0, submitted = 0;
    bool reader_finished = false, cancelled = false;
    std::exception_ptr fatal_error;

    explicit State(std::size_t capacity) : completed(capacity) {}
    void cancel(std::exception_ptr error) {
        {
            std::lock_guard lock(mutex);
            if (!fatal_error)
                fatal_error = error;
            cancelled = true;
        }
        capacity_available.notify_all();
        work_available.notify_all();
        result_available.notify_all();
    }
};

void reader_loop(State &state, std::istream &input, const Options &options) {
    try {
        detail::BatchReader reader(input, options.batch_bytes, options.read_bytes);
        std::size_t first_line = 1;
        while (true) {
            {
                std::unique_lock lock(state.mutex);
                state.capacity_available.wait(lock, [&] {
                    return state.cancelled || state.outstanding < state.completed.size();
                });
                if (state.cancelled)
                    return;
                ++state.outstanding;
            }
            auto batch = std::make_unique<Batch>();
            const bool present = reader.next(batch->bytes);
            batch->first_line = first_line;
            first_line += std::count(batch->bytes.begin(), batch->bytes.end(), '\n');
            {
                std::lock_guard lock(state.mutex);
                if (state.cancelled)
                    return;
                if (!present) {
                    --state.outstanding;
                    state.reader_finished = true;
                } else {
                    batch->sequence = state.submitted++;
                    state.pending.push_back(std::move(batch));
                }
            }
            if (!present) {
                state.work_available.notify_all();
                state.result_available.notify_all();
                return;
            }
            state.work_available.notify_one();
        }
    } catch (...) {
        state.cancel(std::current_exception());
    }
}

void worker_loop(State &state, const reg::bytecode::Program &program,
                 const Options &options) {
    try {
        while (true) {
            std::unique_ptr<Batch> batch;
            {
                std::unique_lock lock(state.mutex);
                state.work_available.wait(lock, [&] {
                    return state.cancelled || !state.pending.empty() || state.reader_finished;
                });
                if (state.cancelled || state.pending.empty())
                    return;
                batch = std::move(state.pending.front());
                state.pending.pop_front();
            }
            std::size_t offset = 0, line = batch->first_line;
            while (offset < batch->bytes.size()) {
                auto end = batch->bytes.find('\n', offset);
                if (end == std::string::npos)
                    end = batch->bytes.size();
                auto length = end - offset;
                if (end < batch->bytes.size() && length && batch->bytes[end - 1] == '\r')
                    --length;
                auto status = reg::execute(
                    program, std::string_view(batch->bytes).substr(offset, length),
                    options.execution_limits);
                if (!status || *status)
                    batch->results.push_back({line, offset, length, status});
                offset = end == batch->bytes.size() ? end : end + 1;
                ++line;
            }
            {
                std::lock_guard lock(state.mutex);
                if (state.cancelled)
                    return;
                const auto slot = batch->sequence % state.completed.size();
                state.completed[slot] = std::move(batch);
            }
            state.result_available.notify_one();
        }
    } catch (...) {
        state.cancel(std::current_exception());
    }
}

bool writer_loop(State &state, std::ostream &output, std::ostream &errors) {
    bool failed = false;
    for (std::size_t next = 0;; ++next) {
        std::unique_ptr<Batch> batch;
        {
            std::unique_lock lock(state.mutex);
            const auto slot = next % state.completed.size();
            state.result_available.wait(lock, [&] {
                return state.cancelled || state.completed[slot] ||
                       (state.reader_finished && next == state.submitted);
            });
            if (state.cancelled || (state.reader_finished && next == state.submitted))
                return failed;
            batch = std::move(state.completed[slot]);
        }
        for (const auto &result : batch->results) {
            if (result.status) {
                output << result.line << ':';
                output.write(batch->bytes.data() + result.offset,
                             static_cast<std::streamsize>(result.length));
                output.put('\n');
            } else {
                failed = true;
                errors << "line " << result.line << ": "
                       << (result.status.error().kind ==
                                   reg::ExecutionErrorKind::StateLimitExceeded
                               ? "visited-state limit exceeded"
                               : "backtracking stack limit exceeded")
                       << '\n';
            }
        }
        if (!output || !errors)
            throw std::runtime_error("failed to write output");
        batch.reset();
        {
            std::lock_guard lock(state.mutex);
            --state.outstanding;
        }
        state.capacity_available.notify_one();
    }
}
} // namespace

int run(std::istream &input, std::ostream &output, std::ostream &errors,
        const reg::bytecode::Program &program, const Options &options) {
    if (!options.worker_count || !options.max_outstanding_batches || !options.batch_bytes ||
        !options.read_bytes) {
        errors << "pipeline sizes and worker count must be positive\n";
        return 1;
    }
    State state(options.max_outstanding_batches);
    std::vector<std::jthread> workers;
    std::jthread reader;
    bool failed = false;
    try {
        for (std::size_t i = 0; i < options.worker_count; ++i)
            workers.emplace_back([&] { worker_loop(state, program, options); });
        reader = std::jthread([&] { reader_loop(state, input, options); });
        failed = writer_loop(state, output, errors);
        output.flush();
        errors.flush();
        if (!output || !errors)
            throw std::runtime_error("failed to write output");
    } catch (...) {
        state.cancel(std::current_exception());
    }
    if (reader.joinable())
        reader.join();
    for (auto &worker : workers)
        worker.join();
    if (state.fatal_error) {
        try {
            std::rethrow_exception(state.fatal_error);
        } catch (const std::exception &error) {
            errors << error.what() << '\n';
        } catch (...) {
            errors << "pipeline failed\n";
        }
        return 1;
    }
    return failed ? 1 : 0;
}
} // namespace grep
