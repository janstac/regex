#include "parser.h"

#include <utility>

namespace reg {
namespace {

class Parser {
  public:
    explicit Parser(std::string_view pattern, std::size_t nesting_limit);
    ParseResult parse();

  private:
    ParseResult parse_union();
    ParseResult parse_concat();
    ParseResult parse_repetition();
    ParseResult parse_atom();
    ParseResult parse_group();
    ParseResult parse_escape();

    bool at_end() const;
    char peek() const;
    char advance();
    bool consume(char c);

    std::string_view mPattern;
    std::size_t mPosition = 0;
    std::size_t mNestingDepth = 0;
    std::size_t mNestingLimit;
};

constexpr std::string_view kEscapableCharacters = R"(\.^$|()*+?)";

std::string_view error_message(ParseErrorKind kind) {
    switch (kind) {
    case ParseErrorKind::UnexpectedCharacter:
        return "unexpected character";
    case ParseErrorKind::ExpectedExpression:
        return "expected expression";
    case ParseErrorKind::ExpectedAtom:
        return "expected atom";
    case ParseErrorKind::QuantifierWithoutOperand:
        return "quantifier has no operand";
    case ParseErrorKind::RepeatedQuantifier:
        return "repeated quantifier";
    case ParseErrorKind::QuantifiedAnchor:
        return "cannot quantify an anchor";
    case ParseErrorKind::NestingLimitExceeded:
        return "group nesting limit exceeded";
    case ParseErrorKind::UnclosedGroup:
        return "unclosed group";
    case ParseErrorKind::TrailingBackslash:
        return "trailing backslash";
    case ParseErrorKind::UnsupportedEscape:
        return "unsupported escape";
    }
    return "unknown parse error";
}

template <class T> NodePtr node(T value) { return std::make_unique<Node>(Node{std::move(value)}); }

bool is_quantifier(char c) { return c == '*' || c == '+' || c == '?'; }

} // namespace

ParseError::ParseError(ParseErrorKind kind, std::size_t offset)
    : kind(kind), offset(offset), message(error_message(kind)) {}

Parser::Parser(std::string_view pattern, std::size_t nesting_limit)
    : mPattern(pattern), mNestingLimit(nesting_limit) {}

ParseResult Parser::parse() {
    mPosition = 0;
    mNestingDepth = 0;
    auto root = parse_union();
    if (!root)
        return root;
    if (!at_end())
        return std::unexpected(ParseError{ParseErrorKind::UnexpectedCharacter, mPosition});
    return root;
}

ParseResult Parser::parse_union() {
    std::vector<NodePtr> branches;
    do {
        auto branch = parse_concat();
        if (!branch)
            return branch;
        branches.push_back(std::move(*branch));
    } while (consume('|'));

    if (branches.size() == 1)
        return std::move(branches.front());
    return node(Alt{std::move(branches)});
}

ParseResult Parser::parse_concat() {
    std::vector<NodePtr> children;
    while (!at_end() && peek() != '|' && peek() != ')') {
        auto child = parse_repetition();
        if (!child)
            return child;
        children.push_back(std::move(*child));
    }
    if (children.empty())
        return node(Empty{});
    if (children.size() == 1)
        return std::move(children.front());
    return node(Concat{std::move(children)});
}

ParseResult Parser::parse_repetition() {
    auto child = parse_atom();
    if (!child) {
        return child;
    }
    auto child_node = child->get();
    if (!at_end() && is_quantifier(peek())) {
        if (std::holds_alternative<AnchorStart>(*child_node) ||
            std::holds_alternative<AnchorEnd>(*child_node)) {
            return std::unexpected(ParseError{ParseErrorKind::QuantifiedAnchor, mPosition});
        }
        const char c = advance();
        const auto quantifier = c == '*'   ? Quantifier::Star
                                : c == '+' ? Quantifier::Plus
                                           : Quantifier::Question;
        if (!at_end() && is_quantifier(peek()))
            return std::unexpected(ParseError{ParseErrorKind::RepeatedQuantifier, mPosition});
        return node(Repeat{std::move(*child), quantifier});
    }
    return child;
}

ParseResult Parser::parse_atom() {
    if (at_end())
        return std::unexpected(ParseError{ParseErrorKind::ExpectedExpression, mPosition});
    if (peek() == '(')
        return parse_group();
    if (peek() == '\\')
        return parse_escape();

    const auto offset = mPosition;
    const char c = advance();
    switch (c) {
    case '.':
        return node(Any{});
    case '^':
        return node(AnchorStart{});
    case '$':
        return node(AnchorEnd{});
    case '*':
    case '+':
    case '?':
        return std::unexpected(ParseError{ParseErrorKind::QuantifierWithoutOperand, offset});
    case ')':
    case '|':
        return std::unexpected(ParseError{ParseErrorKind::ExpectedAtom, offset});
    default:
        return node(Literal{c});
    }
}

/// parse (...)
ParseResult Parser::parse_group() {
    const auto opening = mPosition;
    advance();
    if (mNestingDepth >= mNestingLimit)
        return std::unexpected(ParseError{ParseErrorKind::NestingLimitExceeded, opening});

    ++mNestingDepth;
    auto inner = parse_union();
    --mNestingDepth;
    if (!inner)
        return inner;
    if (!consume(')'))
        return std::unexpected(ParseError{ParseErrorKind::UnclosedGroup, opening});
    return inner;
}

ParseResult Parser::parse_escape() {
    const auto opening = mPosition;
    advance();
    if (at_end())
        return std::unexpected(ParseError{ParseErrorKind::TrailingBackslash, opening});
    const char c = advance();
    if (kEscapableCharacters.find(c) == std::string_view::npos)
        return std::unexpected(ParseError{ParseErrorKind::UnsupportedEscape, opening});
    return node(Literal{c});
}

bool Parser::at_end() const { return mPosition == mPattern.size(); }
char Parser::peek() const { return mPattern[mPosition]; }
char Parser::advance() {
    auto current = mPattern[mPosition];
    ++mPosition;
    return current;
}
bool Parser::consume(char c) {
    if (at_end() || peek() != c) {
        return false;
    }
    ++mPosition;
    return true;
}

ParseResult parse(std::string_view pattern, std::size_t nesting_limit) {
    return Parser{pattern, nesting_limit}.parse();
}

} // namespace reg
