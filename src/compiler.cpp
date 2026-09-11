#include "compiler.h"

#include <ranges>
#include <type_traits>
#include <utility>

namespace reg {
namespace {

// structs

/// Label for an address - used for jumps
using Label = std::size_t;

/// compile node
struct Visit {
    const Node *node;
};
/// emit an instruction
struct Emit {
    bytecode::Instruction instruction;
};
/// record current instruction as label address
struct Bind {
    Label label;
};
struct JumpTo {
    Label label;
};

/// go to preferred, but if match fails, go to fallback
struct SplitTo {
    Label preferred;
    Label fallback;
};
using Task = std::variant<Visit, Emit, Bind, JumpTo, SplitTo>;
enum class Operand { JumpTarget, SplitPreferred, SplitFallback };
struct Fixup {
    /// Instruction address
    std::size_t address;
    /// Label to set
    Label label;
    /// Instruction
    Operand operand;
};

// compiler

class Compilation {
    bytecode::Program mProgram;
    /// Stack of work to do when compiling. Push backwards to process in the correct order.
    std::vector<Task> mWork;
    /// Jump labels
    std::vector<std::size_t> mLabels;
    /// Jump addresses to set after initial compilation
    std::vector<Fixup> mFixups;

  public:
    bytecode::Program run(const Node &root) {
        // Try the pattern at the current input position before consuming a character and
        // trying again. This is the bytecode equivalent of a non-greedy leading `.*`.
        const auto scan = new_label();
        const auto consume = new_label();
        const auto pattern = new_label();
        std::vector<Task> sequence{Bind{scan},
                                   SplitTo{pattern, consume},
                                   Bind{consume},
                                   Emit{bytecode::Any{}},
                                   JumpTo{scan},
                                   Bind{pattern},
                                   Visit{&root}};
        for (auto &&task : sequence | std::views::reverse) {
            mWork.push_back(std::move(task));
        }

        // pop and process tasks until empty
        while (!mWork.empty()) {
            auto task = std::move(mWork.back());
            mWork.pop_back();
            std::visit([this](const auto &value) { process(value); }, task);
        }
        // it didn't fail, so match
        emit(bytecode::Match{});

        // now that we have all the jump labels, fill in the jump address placeholders
        for (const auto &fixup : mFixups) {
            auto &instruction = mProgram.instructions[fixup.address];
            const auto target = mLabels[fixup.label];
            switch (fixup.operand) {
            case Operand::JumpTarget:
                std::get<bytecode::Jump>(instruction).target = target;
                break;
            case Operand::SplitPreferred:
                std::get<bytecode::Split>(instruction).preferred = target;
                break;
            case Operand::SplitFallback:
                std::get<bytecode::Split>(instruction).fallback = target;
                break;
            }
        }
        return std::move(mProgram);
    }

  private:
    Label new_label() {
        mLabels.push_back(0);
        return mLabels.size() - 1;
    }
    void emit(bytecode::Instruction instruction) {
        mProgram.instructions.push_back(std::move(instruction));
    }
    void process(const Emit &task) { emit(task.instruction); }
    void process(const Bind &task) { mLabels[task.label] = mProgram.instructions.size(); }
    void process(const JumpTo &task) {
        mFixups.push_back({mProgram.instructions.size(), task.label, Operand::JumpTarget});
        emit(bytecode::Jump{0});
    }
    void process(const SplitTo &task) {
        const auto address = mProgram.instructions.size();
        mFixups.push_back({address, task.preferred, Operand::SplitPreferred});
        mFixups.push_back({address, task.fallback, Operand::SplitFallback});
        // address placeholders to be fixed up later
        emit(bytecode::Split{0, 0});
    }

    void process(const Visit &task) {
        std::visit(
            [this](const auto &value) {
                using T = std::decay_t<decltype(value)>;
                if constexpr (std::is_same_v<T, Empty>) {
                    return;
                } else if constexpr (std::is_same_v<T, Literal>) {
                    emit(bytecode::Char{value.value});
                } else if constexpr (std::is_same_v<T, Any>) {
                    emit(bytecode::Any{});
                } else if constexpr (std::is_same_v<T, AnchorStart>) {
                    emit(bytecode::AssertStart{});
                } else if constexpr (std::is_same_v<T, AnchorEnd>) {
                    emit(bytecode::AssertEnd{});
                } else if constexpr (std::is_same_v<T, Concat>) {
                    process_visit_concat(value);
                } else if constexpr (std::is_same_v<T, Alt>) {
                    process_visit_alt(value);
                } else if constexpr (std::is_same_v<T, Repeat>) {
                    process_visit_repeat(value);
                }
            },
            *task.node);
    }

    void process_visit_concat(const Concat &value) {
        // push backwards so first char is on top
        for (const auto &child : value.children | std::views::reverse) {
            mWork.push_back(Visit{child.get()});
        }
    }

    void process_visit_alt(const Alt &value) {
        const auto end = new_label();
        // push forwards here, then push to work stack in reverse
        std::vector<Task> sequence;
        for (std::size_t i = 0; i < value.branches.size(); ++i) {
            if (i + 1 == value.branches.size()) {
                // for last one, there is no next choice
                sequence.push_back(Visit{value.branches[i].get()});
                break;
            }
            // chain of: try body, fallback to next
            const auto body = new_label();
            const auto next = new_label();
            sequence.push_back(SplitTo{body, next});
            sequence.push_back(Bind{body});
            sequence.push_back(Visit{value.branches[i].get()});
            sequence.push_back(JumpTo{end});
            sequence.push_back(Bind{next});
        }
        sequence.push_back(Bind{end});
        for (auto &&task : sequence | std::views::reverse) {
            mWork.push_back(std::move(task));
        }
    }

    void process_visit_repeat(const Repeat &value) {
        const auto body = new_label();
        const auto end = new_label();
        mWork.push_back(Bind{end});
        switch (value.quantifier) {
        case Quantifier::Star: {
            // loop: run body, fallback end
            const auto loop = new_label();
            mWork.push_back(JumpTo{loop});
            mWork.push_back(Visit{value.child.get()});
            mWork.push_back(Bind{body});
            mWork.push_back(SplitTo{body, end});
            mWork.push_back(Bind{loop});
            break;
        }
        case Quantifier::Plus:
            // 'do-while' body, fallback end
            mWork.push_back(SplitTo{body, end});
            mWork.push_back(Visit{value.child.get()});
            mWork.push_back(Bind{body});
            break;
        case Quantifier::Question:
            // try body
            mWork.push_back(Visit{value.child.get()});
            mWork.push_back(Bind{body});
            mWork.push_back(SplitTo{body, end});
            break;
        }
    }
};

} // namespace

bytecode::Program compile(const Node &root) { return Compilation{}.run(root); }

} // namespace reg
