cmake_minimum_required(VERSION 3.25)
project(reg LANGUAGES CXX)

set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_compile_options(
    "$<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall;-Wextra;-Wpedantic>"
)

add_library(reg src/reg.cpp src/reg.h)
target_compile_features(reg PUBLIC cxx_std_23)
target_include_directories(reg PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")

add_subdirectory(examples/hello)
