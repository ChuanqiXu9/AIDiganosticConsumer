project(AI-diagnostic-consumer)
cmake_minimum_required(VERSION 3.12)

list(PREPEND CMAKE_PREFIX_PATH ${LLVM_INSTALLED_PATH})

find_package(Clang REQUIRED)
find_package(LLVM REQUIRED)

find_package(CURL REQUIRED)

include_directories(SYSTEM "${LLVM_INCLUDE_DIRS};${CLANG_INCLUDE_DIRS}")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 -fno-rtti")

add_library(AIDiagnosticConsumer SHARED AIDiagnosticConsumer.cpp Plugin.cpp)
target_link_libraries(AIDiagnosticConsumer PUBLIC CURL::libcurl)
