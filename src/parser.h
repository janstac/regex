#pragma once

#include "ast.h"

#include <cstddef>
#include <expected>
#include <string_view>

namespace reg {

enum class ParseErrorKind {
    UnexpectedCharacter,
    ExpectedExpression,
    ExpectedAtom,
    QuantifierWithoutOperand,
    RepeatedQuantifier,
    QuantifiedAnchor,
    NestingLimitExceeded,
    UnclosedGroup,
    TrailingBackslash,
    UnsupportedEscape
};

struct ParseError {
    ParseError(ParseErrorKind kind, std::size_t offset);

    ParseErrorKind kind;
    /// character index in pattern
    std::size_t offset;
    std::string_view message;
};
using ParseResult = std::expected<NodePtr, ParseError>;

ParseResult parse(std::string_view pattern, std::size_t nesting_limit = 128);

} // namespace reg
