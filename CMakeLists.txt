cmake_minimum_required(VERSION 3.21)

project(tisma_engine VERSION 0.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(TISMA_BUILD_TESTS "Build the Catch2 tests" ON)

# Warnings for our own targets only.
add_library(tisma_warnings INTERFACE)
if(MSVC)
  target_compile_options(tisma_warnings INTERFACE /W4 /permissive- /utf-8)
else()
  target_compile_options(tisma_warnings INTERFACE -Wall -Wextra -Wpedantic -Wshadow -Wconversion)
endif()

add_library(tisma_core
  src/mesh/mesh.cpp
  src/io/stl.cpp
  src/cli/cli.cpp
)
target_include_directories(tisma_core PUBLIC src)
target_link_libraries(tisma_core PRIVATE tisma_warnings)
target_compile_definitions(tisma_core PUBLIC TISMA_VERSION="${PROJECT_VERSION}")

add_executable(tisma-engine apps/main.cpp)
target_link_libraries(tisma-engine PRIVATE tisma_core tisma_warnings)

if(TISMA_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
