#include "vm.h"

#include <functional>
#include <unordered_set>

namespace reg {
namespace {

class VM {

    struct State {
        /// next bytecode instruction
        std::size_t address;
        /// next input character
        std::size_t input_pos;
        bool operator==(const State &) const = default;
    };

    struct StateHash {
        std::size_t operator()(const State &state) const {
            const auto first = std::hash<std::size_t>{}(state.address);
            const auto second = std::hash<std::size_t>{}(state.input_pos);
            // golden ratio thing
            return first ^ (second + 0x9e3779b9U + (first << 6) + (first >> 2));
        }
    };

    enum class Step { Continue, Fail, Accept };
    using StepResult = std::expected<Step, ExecutionError>;

    const bytecode::Program &mProgram;
    std::string_view mInput;
    ExecutionLimits mLimits;
    State mState{0, 0};
    std::vector<State> mStack;

    /// Set of visited states. Checked to prevent cycles.
    std::unordered_set<State, StateHash> mVisited;

  public:
    VM(const bytecode::Program &program, std::string_view input, ExecutionLimits limits)
        : mProgram(program), mInput(input), mLimits(limits) {}

    ExecutionResult run() {
        std::size_t executed_states_count = 0;
        while (true) {
            if (mVisited.contains(mState)) {
                if (!backtrack()) {
                    return false;
                }
                continue;
            }
            if (executed_states_count >= mLimits.max_visited_states) {
                return std::unexpected(ExecutionError{ExecutionErrorKind::StateLimitExceeded});
            }
            mVisited.insert(mState);
            ++executed_states_count;
            auto result =
                std::visit([this](const auto &instruction) { return handle(instruction); },
                           mProgram.instructions[mState.address]);
            if (!result) {
                return std::unexpected(result.error());
            }
            switch (*result) {
            case Step::Continue:
                continue;
            case Step::Accept:
                return true;
            case Step::Fail:
                if (!backtrack()) {
                    return false;
                }
                continue;
            }
        }
    }

  private:
    StepResult handle(const bytecode::Char &instruction) {
        if (mState.input_pos == mInput.size() || mInput[mState.input_pos] != instruction.value)
            return Step::Fail;
        ++mState.input_pos;
        ++mState.address;
        return Step::Continue;
    }

    StepResult handle(const bytecode::Any &) {
        if (mState.input_pos == mInput.size())
            return Step::Fail;
        ++mState.input_pos;
        ++mState.address;
        return Step::Continue;
    }

    StepResult handle(const bytecode::Split &instruction) {
        if (mStack.size() >= mLimits.max_backtrack_frames)
            return std::unexpected(ExecutionError{ExecutionErrorKind::BacktrackLimitExceeded});
        mStack.push_back({instruction.fallback, mState.input_pos});
        mState.address = instruction.preferred;
        return Step::Continue;
    }

    StepResult handle(const bytecode::Jump &instruction) {
        mState.address = instruction.target;
        return Step::Continue;
    }

    StepResult handle(const bytecode::AssertStart &) {
        if (mState.input_pos != 0)
            return Step::Fail;
        ++mState.address;
        return Step::Continue;
    }

    StepResult handle(const bytecode::AssertEnd &) {
        if (mState.input_pos != mInput.size())
            return Step::Fail;
        ++mState.address;
        return Step::Continue;
    }

    StepResult handle(const bytecode::Match &) {
        return Step::Accept;
    }

    /// Attempt to backtrack. Returns false if it can't.
    bool backtrack() {
        if (mStack.empty())
            return false;
        mState = mStack.back();
        mStack.pop_back();
        return true;
    }
};

} // namespace

ExecutionResult execute(const bytecode::Program &program, std::string_view input,
                        ExecutionLimits limits) {
    return VM{program, input, limits}.run();
}

} // namespace reg
