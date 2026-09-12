#include "pipeline.h"
#include "compiler.h"
#include "parser.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <thread>

int main(int argc, char **argv) {
    if (argc != 3) {
        std::cerr << "usage: grep <filename> <pattern>\n";
        return 1;
    }
    try {
        grep::Options options;
        options.worker_count = std::max(1u, std::thread::hardware_concurrency());
        options.max_outstanding_batches = options.worker_count;
        if (options.worker_count <= static_cast<std::size_t>(-1) / 3) {
            options.max_outstanding_batches *= 3;
        }

        auto ast = reg::parse(argv[2]);
        if (!ast) {
            std::cerr << "byte " << ast.error().offset << ": " << ast.error().message << '\n';
            return 1;
        }

        const auto program = reg::compile(**ast);

        std::ifstream input(argv[1], std::ios::binary);
        if (!input) {
            std::cerr << "failed to open file: " << argv[1] << '\n';
            return 1;
        }

        return grep::run(input, std::cout, std::cerr, program, options);
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
