#include "parser.h"

#include <iostream>
#include <string>
#include <type_traits>

namespace {

using namespace reg;

std::string escape_literal(char value) {
    switch (value) {
    case '\\':
        return "\\\\";
    case '\'':
        return "\\'";
    case '\n':
        return "\\n";
    case '\r':
        return "\\r";
    case '\t':
        return "\\t";
    default:
        const auto byte = static_cast<unsigned char>(value);
        if (byte >= 0x20 && byte <= 0x7e)
            return std::string(1, value);
        constexpr char hex[] = "0123456789ABCDEF";
        std::string escaped = "\\x";
        escaped += hex[byte >> 4];
        escaped += hex[byte & 0x0f];
        return escaped;
    }
}

void print_tree(const Node &node, std::size_t depth = 0) {
    std::cout << std::string(depth * 2, ' ');
    std::visit(
        [depth](const auto &value) {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<T, Empty>) {
                std::cout << "Empty\n";
            } else if constexpr (std::is_same_v<T, Literal>) {
                std::cout << "Literal('" << escape_literal(value.value) << "')\n";
            } else if constexpr (std::is_same_v<T, Any>) {
                std::cout << "Any\n";
            } else if constexpr (std::is_same_v<T, AnchorStart>) {
                std::cout << "AnchorStart\n";
            } else if constexpr (std::is_same_v<T, AnchorEnd>) {
                std::cout << "AnchorEnd\n";
            } else if constexpr (std::is_same_v<T, Repeat>) {
                const char *name = value.quantifier == Quantifier::Star   ? "Star"
                                   : value.quantifier == Quantifier::Plus ? "Plus"
                                                                          : "Question";
                std::cout << "Repeat(" << name << ")\n";
                print_tree(*value.child, depth + 1);
            } else if constexpr (std::is_same_v<T, Concat>) {
                std::cout << "Concat\n";
                for (const auto &child : value.children)
                    print_tree(*child, depth + 1);
            } else if constexpr (std::is_same_v<T, Alt>) {
                std::cout << "Alt\n";
                for (const auto &branch : value.branches)
                    print_tree(*branch, depth + 1);
            }
        },
        node);
}

} // namespace

int main() {
    std::string pattern;
    bool failed = false;
    std::size_t line = 0;
    while (std::getline(std::cin, pattern)) {
        ++line;
        auto result = reg::parse(pattern);
        if (!result) {
            std::cerr << "line " << line << ", byte " << result.error().offset << ": "
                      << result.error().message << '\n';
            failed = true;
            continue;
        }
        print_tree(**result);
        std::cout << '\n';
    }
    if (std::cin.bad()) {
        std::cerr << "failed to read stdin\n";
        failed = true;
    }
    return failed ? 1 : 0;
}
