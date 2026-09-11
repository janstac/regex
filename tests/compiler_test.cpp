#include "test.h"
#include "compiler.h"
#include "parser.h"

#include <string>
#include <type_traits>

using namespace reg;

namespace {
std::string render(const bytecode::Program &program) {
    std::string result;
    for (const auto &instruction : program.instructions) {
        if (!result.empty())
            result += ' ';
        result += std::visit(
            [](const auto &op) -> std::string {
                using T = std::decay_t<decltype(op)>;
                if constexpr (std::is_same_v<T, bytecode::Char>)
                    return std::string("CHAR:") + op.value;
                else if constexpr (std::is_same_v<T, bytecode::Any>)
                    return "ANY";
                else if constexpr (std::is_same_v<T, bytecode::Split>)
                    return "SPLIT:" + std::to_string(op.preferred) + "," +
                           std::to_string(op.fallback);
                else if constexpr (std::is_same_v<T, bytecode::Jump>)
                    return "JUMP:" + std::to_string(op.target);
                else if constexpr (std::is_same_v<T, bytecode::AssertStart>)
                    return "START";
                else if constexpr (std::is_same_v<T, bytecode::AssertEnd>)
                    return "END";
                else
                    return "MATCH";
            },
            instruction);
    }
    return result;
}
} // namespace

void compiler_tests(Tests &tests) {
    struct Case {
        std::string_view pattern;
        std::string_view expected;
    };
    for (const auto &[pattern, expected] :
         {Case{"", "SPLIT:3,1 ANY JUMP:0 MATCH"},
          {"ab", "SPLIT:3,1 ANY JUMP:0 CHAR:a CHAR:b MATCH"},
          {"^.$", "SPLIT:3,1 ANY JUMP:0 START ANY END MATCH"},
          {"a|b", "SPLIT:3,1 ANY JUMP:0 SPLIT:4,6 CHAR:a JUMP:7 CHAR:b MATCH"},
          {"a|b|c",
           "SPLIT:3,1 ANY JUMP:0 SPLIT:4,6 CHAR:a JUMP:10 SPLIT:7,9 CHAR:b JUMP:10 "
           "CHAR:c MATCH"},
          {"a*", "SPLIT:3,1 ANY JUMP:0 SPLIT:4,6 CHAR:a JUMP:3 MATCH"},
          {"a+", "SPLIT:3,1 ANY JUMP:0 CHAR:a SPLIT:3,5 MATCH"},
          {"a?", "SPLIT:3,1 ANY JUMP:0 SPLIT:4,5 CHAR:a MATCH"},
          {"()*", "SPLIT:3,1 ANY JUMP:0 SPLIT:4,5 JUMP:3 MATCH"},
          {"|", "SPLIT:3,1 ANY JUMP:0 SPLIT:4,5 JUMP:5 MATCH"}}) {
        auto ast = parse(pattern);
        CHECK(tests, ast.has_value());
        if (!ast)
            continue;
        auto program = compile(**ast);
        tests.check(render(program) == expected, pattern);
    }
}
