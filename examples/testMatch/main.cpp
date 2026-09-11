#include "compiler.h"
#include "parser.h"
#include "vm.h"

#include <iostream>
#include <string>

int main() {
    std::string pattern;
    if (!std::getline(std::cin, pattern)) {
        std::cerr << "failed to read regex from first line\n";
        return 1;
    }

    auto ast = reg::parse(pattern);
    if (!ast) {
        std::cerr << "byte " << ast.error().offset << ": " << ast.error().message << '\n';
        return 1;
    }
    auto program = reg::compile(**ast);

    std::string input;
    std::size_t line = 1;
    bool failed = false;
    while (std::getline(std::cin, input)) {
        ++line;
        auto result = reg::execute(program, input);
        if (!result) {
            std::cerr << "line " << line << ": ";
            switch (result.error().kind) {
            case reg::ExecutionErrorKind::StateLimitExceeded:
                std::cerr << "visited-state limit exceeded\n";
                break;
            case reg::ExecutionErrorKind::BacktrackLimitExceeded:
                std::cerr << "backtracking stack limit exceeded\n";
                break;
            }
            failed = true;
            continue;
        }
        std::cout << (*result ? "match\n" : "no match\n");
    }
    if (std::cin.bad()) {
        std::cerr << "failed to read stdin\n";
        failed = true;
    }
    return failed ? 1 : 0;
}
