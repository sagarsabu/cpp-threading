cmake_minimum_required(VERSION 3.21)
project(cpp-threading VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_C_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

file(GLOB_RECURSE SRCS
    src/**.cpp
)

add_executable(cpp-threading ${SRCS})

target_compile_options(cpp-threading PRIVATE
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

find_package(Threads REQUIRED)
find_library(LIB_RT NAMES rt REQUIRED)

target_include_directories(cpp-threading PRIVATE
    src/
)

target_link_libraries(cpp-threading PRIVATE
    Threads::Threads
    ${LIB_RT}
)
