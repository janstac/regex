#pragma once

#include "ast.h"
#include "bytecode.h"

namespace reg {

bytecode::Program compile(const Node &root);

} // namespace reg
