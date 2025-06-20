cmake_minimum_required(VERSION 3.21)
project(cpp-threading VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_C_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(ExternalProject)

set(LIBURING_PREFIX ${CMAKE_BINARY_DIR}/liburing)
set(LIBURING_TAR ${CMAKE_SOURCE_DIR}/archives/liburing-2.9.tar.gz)

ExternalProject_Add(
  liburing
  PREFIX ${LIBURING_PREFIX}
  URL ${LIBURING_TAR}
  URL_HASH "MD5=50cbca5567878abaeb6824909b647422"
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  CONFIGURE_COMMAND cd ${LIBURING_PREFIX}/src/liburing && ./configure
                    --prefix=${LIBURING_PREFIX}
  BUILD_COMMAND make -C ${LIBURING_PREFIX}/src/liburing -j$(nproc)
  INSTALL_COMMAND make -C ${LIBURING_PREFIX}/src/liburing install
  UPDATE_DISCONNECTED 1
)

find_package(Threads REQUIRED)
find_library(LIB_RT NAMES rt REQUIRED)

file(GLOB_RECURSE SRCS
    src/**.cpp
)

add_executable(cpp-threading ${SRCS})
add_dependencies(cpp-threading liburing)

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

target_compile_definitions(cpp-threading PRIVATE
    # force use of posix semaphores
    # _GLIBCXX_USE_POSIX_SEMAPHORE=1
)

target_include_directories(cpp-threading PRIVATE
    src/
)

target_include_directories(cpp-threading SYSTEM PRIVATE
    ${LIBURING_PREFIX}/include
)

target_link_libraries(cpp-threading PRIVATE
    Threads::Threads
    ${LIB_RT}
    ${LIBURING_PREFIX}/lib/liburing.a
)

if(DEFINED ENV{ASAN})
    message(STATUS "==== ASAN enabled")
    target_compile_options(cpp-threading PRIVATE
        -fsanitize=address
        -fsanitize-recover=address,undefined
        -fno-omit-frame-pointer
    )
    target_link_options(cpp-threading PRIVATE
        -fsanitize=address
    )
endif()

if(DEFINED ENV{TSAN})
    message(STATUS "==== TSAN enabled")
    target_compile_options(cpp-threading PRIVATE
        -fsanitize=thread
        -fno-omit-frame-pointer
    )
    target_link_options(cpp-threading PRIVATE
        -fsanitize=thread
    )
endif()
