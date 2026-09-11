#include "test.h"
#include "vm.h"

using namespace reg;
namespace bc = reg::bytecode;

void vm_tests(Tests &tests) {
    auto check = [&](bc::Program program, std::string_view input, bool expected,
                     std::source_location where = std::source_location::current()) {
        auto result = execute(program, input);
        tests.check(result.has_value(), "result.has_value()", where);
        if (result)
            tests.check(*result == expected, "*result == expected", where);
    };
    check({{bc::Match{}}}, "", true);
    check({{bc::Match{}}}, "a", true);
    check({{bc::Char{'a'}, bc::Match{}}}, "a", true);
    check({{bc::Char{'a'}, bc::Match{}}}, "b", false);
    check({{bc::Char{'a'}, bc::Match{}}}, "", false);
    check({{bc::Any{}, bc::Match{}}}, "\n", true);
    check({{bc::Any{}, bc::Match{}}}, "", false);
    check({{bc::AssertStart{}, bc::Any{}, bc::AssertEnd{}, bc::Match{}}}, "a", true);
    check({{bc::Any{}, bc::AssertStart{}, bc::Match{}}}, "a", false);
    check({{bc::AssertEnd{}, bc::Any{}, bc::Match{}}}, "a", false);
    check({{bc::Split{1, 2}, bc::Jump{0}, bc::Match{}}}, "", true);
    check({{bc::Jump{0}}}, "", false);
    check({{bc::Split{1, 2}, bc::Char{'x'}, bc::Char{'a'}, bc::Match{}}}, "a", true);

    auto limited = execute({{bc::Match{}}}, "", {0, 10});
    CHECK(tests, !limited);
    if (!limited)
        CHECK(tests, limited.error().kind == ExecutionErrorKind::StateLimitExceeded);
    limited = execute({{bc::Split{1, 1}, bc::Match{}}}, "", {10, 0});
    CHECK(tests, !limited);
    if (!limited)
        CHECK(tests, limited.error().kind == ExecutionErrorKind::BacktrackLimitExceeded);
    auto exact = execute({{bc::Jump{0}}}, "", {1, 0});
    CHECK(tests, exact.has_value());
    if (exact)
        CHECK(tests, !*exact);
    exact = execute({{bc::Match{}}}, "", {1, 0});
    CHECK(tests, exact.has_value());
    if (exact)
        CHECK(tests, *exact);
}
