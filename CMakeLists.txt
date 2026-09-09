cmake_minimum_required(VERSION 3.25)
project(reg LANGUAGES CXX)

set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_compile_options(
    "$<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall;-Wextra;-Wpedantic>"
)

add_library(reg src/ast.h src/parser.cpp src/parser.h)
target_compile_features(reg PUBLIC cxx_std_23)
target_include_directories(reg PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")

add_subdirectory(examples/testParse)

include(CTest)
if(BUILD_TESTING)
    add_executable(reg_tests tests/main.cpp tests/parser_test.cpp)
    target_link_libraries(reg_tests PRIVATE reg)
    add_test(NAME reg_tests COMMAND reg_tests)
    set_tests_properties(reg_tests PROPERTIES TIMEOUT 10)
endif()
