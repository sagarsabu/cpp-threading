cmake_minimum_required(VERSION 3.21)
project(cpp-threading VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_C_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

add_compile_options(
    -Wall
    -Wextra
    -Werror
    -Wattributes
    -Wconversion
    -Wduplicated-cond
    -Wduplicated-branches
    -Wformat
    -Wimplicit-fallthrough
    -Wpedantic
)

file(GLOB_RECURSE SRCS
    src/**.cpp
)

add_executable(cpp-threading ${SRCS})

target_include_directories(cpp-threading PUBLIC
    src/
)

target_link_libraries(
    cpp-threading PUBLIC pthread rt
)
