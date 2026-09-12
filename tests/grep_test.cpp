#include "test.h"
#include "pipeline.h"
#include "batch_reader.h"
#include "compiler.h"
#include "parser.h"

#include <fstream>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <streambuf>
#include <string>

namespace {
auto compile(std::string_view pattern) {
    auto parsed = reg::parse(pattern);
    if (!parsed)
        throw std::runtime_error("invalid test pattern");
    return reg::compile(**parsed);
}

std::string reference(const std::string &text, const reg::bytecode::Program &program) {
    std::istringstream input(text);
    std::ostringstream output;
    std::string line;
    std::size_t number = 0;
    while (std::getline(input, line)) {
        ++number;
        if (!input.eof() && !line.empty() && line.back() == '\r')
            line.pop_back();
        const auto result = reg::execute(program, line);
        if (!result)
            throw std::runtime_error("serial reference exceeded execution limits");
        if (*result)
            output << number << ':' << line << '\n';
    }
    return output.str();
}

void compare(Tests &tests, const std::string &text, std::string_view pattern, std::size_t workers,
             std::size_t capacity, std::size_t read_bytes = 4096) {
    const auto program = compile(pattern);
    const auto expected = reference(text, program);
    std::istringstream input(text);
    std::ostringstream output, errors;
    grep::Options options{.worker_count = workers,
                          .batch_bytes = 1024,
                          .read_bytes = read_bytes,
                          .max_outstanding_batches = capacity};
    CHECK(tests, grep::run(input, output, errors, program, options) == 0);
    CHECK(tests, errors.str().empty());
    CHECK(tests, output.str() == expected);
}

void batch_tests(Tests &tests, const std::string &text, std::size_t read_bytes) {
    std::istringstream input(text);
    grep::detail::BatchReader reader(input, 1024, read_bytes);
    std::string bytes, reconstructed;
    std::size_t count = 0;
    bool oversized = false;
    while (reader.next(bytes)) {
        ++count;
        CHECK(tests, !bytes.empty());
        if (reconstructed.size() + bytes.size() < text.size()) {
            CHECK(tests, bytes.back() == '\n');
            CHECK(tests, bytes.size() >= 1024);
        }
        // No earlier newline at or after the target may have been skipped.
        if (bytes.size() >= 1024) {
            const auto first = bytes.find('\n', 1023);
            CHECK(tests, first == std::string::npos || first == bytes.size() - 1);
        }
        oversized |= bytes.size() > 4096;
        reconstructed += bytes;
    }
    CHECK(tests, count > 5);
    CHECK(tests, oversized);
    CHECK(tests, reconstructed == text);
}

class FailingOutput : public std::streambuf {
    std::streamsize xsputn(const char *, std::streamsize) override { return 0; }
    int_type overflow(int_type) override { return traits_type::eof(); }
};

void error_tests(Tests &tests, const std::string &text) {
    const auto program = compile("needle");
    grep::Options options{
        .worker_count = 4, .batch_bytes = 1024, .read_bytes = 4096, .max_outstanding_batches = 2};
    {
        std::istringstream input(text);
        FailingOutput buffer;
        std::ostream output(&buffer);
        std::ostringstream errors;
        CHECK(tests, grep::run(input, output, errors, program, options) == 1);
        CHECK(tests, errors.str().find("failed to write") != std::string::npos);
    }
    {
        std::istringstream input(text);
        input.setstate(std::ios::badbit);
        std::ostringstream output, errors;
        CHECK(tests, grep::run(input, output, errors, program, options) == 1);
        CHECK(tests, errors.str().find("failed to read") != std::string::npos);
    }
    {
        std::istringstream input("needle\nother\n");
        std::ostringstream output, errors;
        options.execution_limits.max_visited_states = 0;
        CHECK(tests, grep::run(input, output, errors, program, options) == 1);
        CHECK(tests, output.str().empty());
        CHECK(tests, errors.str() == "line 1: visited-state limit exceeded\n"
                                     "line 2: visited-state limit exceeded\n");
    }
}
} // namespace

int main() {
    Tests tests;
    try {
        std::ifstream fixture(GREP_FIXTURE, std::ios::binary);
        if (!fixture)
            throw std::runtime_error("cannot open grep fixture");
        const std::string text{std::istreambuf_iterator<char>(fixture), {}};
        CHECK(tests, text.size() >= 18000 && text.size() <= 24000);
        batch_tests(tests, text, 4096);
        batch_tests(tests, text, 127);
        for (auto workers : {1u, 4u}) {
            for (auto capacity : {1u, 3u}) {
                for (auto pattern : {"needle", "^apple", "violet$", "^", "^$", "NOTPRESENT"})
                    compare(tests, text, pattern, workers, capacity);
            }
        }
        for (const std::string text : {"", "\n", "\n\n", "last", "a\n", "a\r\n\r\nb\r"})
            compare(tests, text, "^", 4, 2, 1);
        error_tests(tests, text);
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return tests.failures ? 1 : 0;
}
