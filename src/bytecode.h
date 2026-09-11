#pragma once

#include <cstddef>
#include <variant>
#include <vector>

namespace reg::bytecode {

/// Match a char
struct Char {
    char value;
};
/// Match any char
struct Any {};

/// If preferred (address) fails, try fallback
struct Split {
    std::size_t preferred;
    std::size_t fallback;
};

/// Jump to address
struct Jump {
    std::size_t target;
};
struct AssertStart {};
struct AssertEnd {};
struct Match {};

using Instruction = std::variant<Char, Any, Split, Jump, AssertStart, AssertEnd, Match>;

struct Program {
    /// Bytecode instructions. Index is address.
    std::vector<Instruction> instructions;
};

} // namespace reg::bytecode
