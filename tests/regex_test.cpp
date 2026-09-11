#include "test.h"
#include "compiler.h"
#include "parser.h"
#include "vm.h"

#include <string>

using namespace reg;

void regex_tests(Tests &tests) {
    struct Case {
        std::string_view pattern;
        std::string_view input;
        bool expected;
    };
    for (const auto &[pattern, input, expected] :
         {Case{"", "", true},       {"", "a", true},          {"ab|c*d", "ab", true},
          {"ab|c*d", "cccd", true}, {"ab|c*d", "abc", true},  {"a|ab", "ab", true},
          {"a*ab", "aaab", true},   {"a+", "", false},        {"a+", "aaa", true},
          {"a?", "", true},         {"a?", "aa", true},       {"^a.$", "a\n", true},
          {"^a.$", "xab", false},   {R"(\*\.)", "*.", true},  {"(a?)*", "", true},
          {"(a?)*", "aaa", true},   {"(a?)*", "b", true},     {"(a*)*b", "aaab", true},
          {"(a*)*b", "aaa", false}, {"()*", "", true},        {"()+", "", true},
          {"(|a)*b", "aaab", true}, {"a|", "", true},         {"($|a)*", "aaa", true},
          {"bc", "abcd", true},     {"^bc", "abcd", false},   {"cd$", "abcd", true},
          {"bc$", "abcd", false},   {"^abcd$", "abcd", true}}) {
        auto ast = parse(pattern);
        CHECK(tests, ast.has_value());
        if (!ast)
            continue;
        auto program = compile(**ast);
        auto result = execute(program, input);
        tests.check(result.has_value(), pattern);
        if (result)
            tests.check(*result == expected, pattern);
    }

    auto ast = parse("(a|aa)*b");
    CHECK(tests, ast.has_value());
    if (!ast)
        return;
    auto program = compile(**ast);
    const std::string input(100, 'a');
    const auto bound = program.instructions.size() * (input.size() + 1);
    auto result = execute(program, input, {bound, bound});
    CHECK(tests, result.has_value());
    if (result)
        CHECK(tests, !*result);
    result = execute(program, "aab");
    CHECK(tests, result.has_value());
    if (result)
        CHECK(tests, *result);
}
