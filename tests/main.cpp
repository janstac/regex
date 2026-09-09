#include "test.h"

void parser_tests(Tests& tests);

int main() {
    Tests tests;
    parser_tests(tests);
    if (tests.failures != 0)
        std::cerr << tests.failures << " check(s) failed\n";
    return tests.failures == 0 ? 0 : 1;
}
