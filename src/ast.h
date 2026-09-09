#pragma once

#include <memory>
#include <variant>
#include <vector>

namespace reg {


struct Empty {};

struct Literal {
    char value;
};
struct Any {};
struct AnchorStart {};
struct AnchorEnd {};

enum class Quantifier { Star, Plus, Question };

struct Concat;
struct Alt;
struct Repeat;

using Node = std::variant<Empty, Literal, Any, AnchorStart, AnchorEnd, Concat, Alt, Repeat>;
using NodePtr = std::unique_ptr<Node>;

struct Concat {
    std::vector<NodePtr> children;
};
struct Alt {
    std::vector<NodePtr> branches;
};

struct Repeat {
    NodePtr child;
    Quantifier quantifier;
};

} // namespace reg