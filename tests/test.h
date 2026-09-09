#pragma once

#include <iostream>
#include <source_location>
#include <string_view>

struct Tests {
    int failures = 0;

    void check(bool passed, std::string_view expression,
               std::source_location where = std::source_location::current()) {
        if (passed)
            return;
        ++failures;
        std::cerr << where.file_name() << ':' << where.line()
                  << ": failed: " << expression << '\n';
    }
};

#define CHECK(tests, expression) \
    (tests).check(static_cast<bool>(expression), #expression)
