#include "compiler.h"
#include "parser.h"
#include "vm.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

void consume(const void* value);

namespace {
constexpr std::size_t kIterations = 10000;
using Clock = std::chrono::steady_clock;

std::string format_character(char value) {
    switch (value) {
    case '\\': return "\\\\";
    case '\'': return "\\'";
    case '\n': return "\\n";
    case '\r': return "\\r";
    case '\t': return "\\t";
    default:
        const auto byte = static_cast<unsigned char>(value);
        if (byte >= 0x20 && byte <= 0x7e) return std::string(1, value);
        constexpr char hex[] = "0123456789ABCDEF";
        std::string escaped = "\\x";
        escaped += hex[byte >> 4];
        escaped += hex[byte & 0x0f];
        return escaped;
    }
}

std::string format_bytecode(const reg::bytecode::Program& program) {
    namespace bc = reg::bytecode;
    std::string result;
    for (std::size_t pc = 0; pc < program.instructions.size(); ++pc) {
        result += std::to_string(pc) + ": ";
        result += std::visit([](const auto& instruction) -> std::string {
            using T = std::decay_t<decltype(instruction)>;
            if constexpr (std::is_same_v<T, bc::Char>)
                return "CHAR '" + format_character(instruction.value) + "'";
            else if constexpr (std::is_same_v<T, bc::Any>)
                return "ANY";
            else if constexpr (std::is_same_v<T, bc::Split>)
                return "SPLIT " + std::to_string(instruction.preferred) + ", " +
                       std::to_string(instruction.fallback);
            else if constexpr (std::is_same_v<T, bc::Jump>)
                return "JUMP " + std::to_string(instruction.target);
            else if constexpr (std::is_same_v<T, bc::AssertStart>)
                return "ASSERT_START";
            else if constexpr (std::is_same_v<T, bc::AssertEnd>)
                return "ASSERT_END";
            else if constexpr (std::is_same_v<T, bc::Match>)
                return "MATCH";
        }, program.instructions[pc]);
        result += '\n';
    }
    return result;
}

struct Sample {
    std::string name;
    std::string pattern;
    std::vector<std::string> strings;
};

Sample load(const std::filesystem::path& path) {
    std::ifstream file(path);
    Sample sample{path.filename().string(), {}, {}};
    std::string separator;
    if (!std::getline(file, sample.pattern) || !std::getline(file, separator) || !separator.empty())
        throw std::runtime_error(sample.name + ": expected regex followed by an empty separator line");
    std::string input;
    while (std::getline(file, input)) sample.strings.push_back(input);
    if (file.bad()) throw std::runtime_error(sample.name + ": failed to read file");
    if (sample.strings.empty())
        throw std::runtime_error(sample.name + ": expected at least one input line");
    return sample;
}

struct Counts {
    std::size_t matches = 0;
    std::size_t nonmatches = 0;
    std::size_t errors = 0;
};

void count(const reg::ExecutionResult& result, Counts& counts) {
    if (!result) ++counts.errors;
    else if (*result) ++counts.matches;
    else ++counts.nonmatches;
}

void print_time(std::string_view phase, Clock::duration elapsed, std::size_t operations) {
    const double total_ms = std::chrono::duration<double, std::milli>(elapsed).count();
    std::cout << "  " << phase << ": " << total_ms << " ms total, "
              << total_ms * 1000.0 / static_cast<double>(operations) << " us/op\n";
}

bool benchmark(const Sample& sample) {
    using namespace reg;
    auto ast = parse(sample.pattern);
    if (!ast)
        throw std::runtime_error(sample.name + ": parse error at byte " +
                                 std::to_string(ast.error().offset) + ": " +
                                 std::string(ast.error().message));
    const auto program = compile(**ast);

    Counts warmup;
    for (const auto& input : sample.strings) count(execute(program, input), warmup);

    auto start = Clock::now();
    for (std::size_t i = 0; i < kIterations; ++i) {
        auto parsed = parse(sample.pattern);
        consume(&parsed);
    }
    const auto parsing_time = Clock::now() - start;

    start = Clock::now();
    for (std::size_t i = 0; i < kIterations; ++i) {
        auto compiled = compile(**ast);
        consume(&compiled);
    }
    const auto compilation_time = Clock::now() - start;

    Counts counts;
    start = Clock::now();
    for (std::size_t i = 0; i < kIterations; ++i)
        for (const auto& input : sample.strings)
            count(execute(program, input), counts);
    const auto matching_time = Clock::now() - start;

    std::cout << sample.name << "  regex: " << sample.pattern << '\n';
    std::cout << format_bytecode(program);
    print_time("parse", parsing_time, kIterations);
    print_time("compile", compilation_time, kIterations);
    print_time("match", matching_time, kIterations * sample.strings.size());
    std::cout << "  strings: " << sample.strings.size()
              << ", executions: " << kIterations * sample.strings.size()
              << ", matches: " << counts.matches << ", nonmatches: " << counts.nonmatches
              << ", errors: " << counts.errors << "\n\n";
    return counts.errors == 0 && warmup.errors == 0;
}
} // namespace

int main(int argc, char** argv) {
    if (argc > 2) {
        std::cerr << "usage: benchmark [samples-directory]\n";
        return 1;
    }
    try {
        const std::filesystem::path directory = argc == 2 ? argv[1] : REG_BENCHMARK_SAMPLES;
        std::vector<std::filesystem::path> paths;
        for (const auto& entry : std::filesystem::directory_iterator(directory))
            if (entry.is_regular_file() && entry.path().extension() == ".txt")
                paths.push_back(entry.path());
        std::sort(paths.begin(), paths.end());
        if (paths.empty()) throw std::runtime_error("no .txt sample files found");
        std::vector<Sample> samples;
        for (const auto& path : paths) samples.push_back(load(path));

        std::cout << std::fixed << std::setprecision(3)
                  << "Iterations: " << kIterations
                  << "\nParse/compile timings include temporary result destruction.\n\n";
        bool success = true;
        for (const auto& sample : samples)
            if (!benchmark(sample)) success = false;
        return success ? 0 : 1;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
