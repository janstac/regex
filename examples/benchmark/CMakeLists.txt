add_executable(benchmark main.cpp consume.cpp)
target_link_libraries(benchmark PRIVATE reg)
target_compile_definitions(benchmark PRIVATE
    REG_BENCHMARK_SAMPLES="${CMAKE_CURRENT_SOURCE_DIR}/samples"
)
