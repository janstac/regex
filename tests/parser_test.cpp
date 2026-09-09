#include "test.h"
#include "parser.h"

#include <string>
#include <type_traits>

using namespace reg;

namespace {

std::string render(const Node &node) {
    return std::visit(
        [](const auto &value) -> std::string {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<T, Empty>)
                return "empty";
            else if constexpr (std::is_same_v<T, Literal>)
                return "lit(" + std::string(1, value.value) + ")";
            else if constexpr (std::is_same_v<T, Any>)
                return "any";
            else if constexpr (std::is_same_v<T, AnchorStart>)
                return "start";
            else if constexpr (std::is_same_v<T, AnchorEnd>)
                return "end";
            else if constexpr (std::is_same_v<T, Repeat>) {
                const auto name = value.quantifier == Quantifier::Star   ? "star"
                                  : value.quantifier == Quantifier::Plus ? "plus"
                                                                         : "question";
                return std::string(name) + "(" + render(*value.child) + ")";
            } else {
                const auto &children = [&]() -> const auto & {
                    if constexpr (std::is_same_v<T, Concat>)
                        return value.children;
                    else
                        return value.branches;
                }();
                std::string result = std::is_same_v<T, Concat> ? "concat(" : "alt(";
                for (std::size_t i = 0; i < children.size(); ++i) {
                    if (i)
                        result += ',';
                    result += render(*children[i]);
                }
                return result + ')';
            }
        },
        node);
}

} // namespace

void parser_tests(Tests &tests) {
    struct ErrorMessage {
        ParseErrorKind kind;
        std::string_view message;
    };
    for (const auto &[kind, message] :
         {ErrorMessage{ParseErrorKind::UnexpectedCharacter, "unexpected character"},
          {ParseErrorKind::ExpectedExpression, "expected expression"},
          {ParseErrorKind::ExpectedAtom, "expected atom"},
          {ParseErrorKind::QuantifierWithoutOperand, "quantifier has no operand"},
          {ParseErrorKind::RepeatedQuantifier, "repeated quantifier"},
          {ParseErrorKind::QuantifiedAnchor, "cannot quantify an anchor"},
          {ParseErrorKind::NestingLimitExceeded, "group nesting limit exceeded"},
          {ParseErrorKind::UnclosedGroup, "unclosed group"},
          {ParseErrorKind::TrailingBackslash, "trailing backslash"},
          {ParseErrorKind::UnsupportedEscape, "unsupported escape"}}) {
        const ParseError error{kind, 7};
        CHECK(tests, error.kind == kind);
        CHECK(tests, error.offset == 7);
        tests.check(error.message == message, message);
    }

    struct Valid {
        std::string_view pattern;
        std::string_view expected;
    };
    for (const auto &[pattern, expected] :
         {Valid{"", "empty"},
          {"()", "empty"},
          {"a", "lit(a)"},
          {"ab|c*d", "alt(concat(lit(a),lit(b)),concat(star(lit(c)),lit(d)))"},
          {"a|b|c", "alt(lit(a),lit(b),lit(c))"},
          {"(a|b)c", "concat(alt(lit(a),lit(b)),lit(c))"},
          {"^a.$", "concat(start,lit(a),any,end)"},
          {"a+b?", "concat(plus(lit(a)),question(lit(b)))"},
          {"|a|", "alt(empty,lit(a),empty)"},
          {"(a?)*", "star(question(lit(a)))"},
          {"()*", "star(empty)"},
          {"((a))", "lit(a)"},
          {"[a]", "concat(lit([),lit(a),lit(]))"}}) {
        auto result = parse(pattern);
        tests.check(result.has_value(), pattern);
        if (result)
            tests.check(render(**result) == expected, pattern);
    }

    for (char c : std::string{R"(\.^$|()*+?)"}) {
        const std::string pattern = std::string{"\\"} + c;
        auto result = parse(pattern);
        CHECK(tests, result.has_value());
        if (result) {
            const auto *literal = std::get_if<Literal>(result->get());
            CHECK(tests, literal != nullptr);
            if (literal)
                CHECK(tests, literal->value == c);
        }
    }

    struct Invalid {
        std::string_view pattern;
        std::size_t offset;
        ParseErrorKind kind;
    };
    for (const auto &[pattern, offset, kind] :
         {Invalid{"*a", 0, ParseErrorKind::QuantifierWithoutOperand},
          {"a|+", 2, ParseErrorKind::QuantifierWithoutOperand},
          {"a**", 2, ParseErrorKind::RepeatedQuantifier},
          {"a+?", 2, ParseErrorKind::RepeatedQuantifier},
          {"^*", 1, ParseErrorKind::QuantifiedAnchor},
          {"($)+", 3, ParseErrorKind::QuantifiedAnchor},
          {"a(", 1, ParseErrorKind::UnclosedGroup},
          {"a)", 1, ParseErrorKind::UnexpectedCharacter},
          {"\\", 0, ParseErrorKind::TrailingBackslash},
          {"a\\n", 1, ParseErrorKind::UnsupportedEscape}}) {
        auto result = parse(pattern);
        tests.check(!result.has_value(), pattern);
        if (!result) {
            tests.check(result.error().offset == offset, pattern);
            tests.check(result.error().kind == kind, pattern);
        }
    }

    CHECK(tests, (parse("((a))", 2).has_value()));
    auto too_deep = parse("(((a)))", 2);
    CHECK(tests, !too_deep);
    if (!too_deep) {
        CHECK(tests, too_deep.error().offset == 2);
        CHECK(tests, too_deep.error().kind == ParseErrorKind::NestingLimitExceeded);
    }
    CHECK(tests, (parse("a", 0).has_value()));
    CHECK(tests, !(parse("()", 0).has_value()));
    const std::string deep = std::string(10000, '(') + "a" + std::string(10000, ')');
    CHECK(tests, !parse(deep));

    const std::string flat(10000, 'a');
    auto long_result = parse(flat);
    CHECK(tests, long_result.has_value());
    if (long_result) {
        const auto *concat = std::get_if<Concat>(long_result->get());
        CHECK(tests, concat != nullptr);
        if (concat)
            CHECK(tests, concat->children.size() == flat.size());
    }

    const std::string nul(1, '\0');
    auto nul_result = parse(nul);
    CHECK(tests, nul_result.has_value());
    if (nul_result) {
        const auto *literal = std::get_if<Literal>(nul_result->get());
        CHECK(tests, literal != nullptr);
        if (literal)
            CHECK(tests, literal->value == '\0');
    }
    CHECK(tests, parse("(a|b)*").has_value());
    CHECK(tests, parse("(a|b)*").has_value());
}
