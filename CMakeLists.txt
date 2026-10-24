cmake_minimum_required(VERSION 3.25)

project(tisma_engine VERSION 0.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(TISMA_BUILD_TESTS "Build the Catch2 tests" ON)

include(FetchContent)

# oneTBB v2022.3.0, Apache-2.0 (see THIRD_PARTY.md), built as a static library
# so that tisma-engine is a single program without DLLs next to it. Set
# FETCHCONTENT_SOURCE_DIR_TBB to a local checkout of the same tag to build offline.
set(TBB_TEST OFF CACHE BOOL "" FORCE)
set(TBB_EXAMPLES OFF CACHE BOOL "" FORCE)
set(TBB_STRICT OFF CACHE BOOL "" FORCE)
set(TBBMALLOC_BUILD OFF CACHE BOOL "" FORCE)
set(TBB_ENABLE_IPO OFF CACHE BOOL "" FORCE)
set(_tisma_shared ${BUILD_SHARED_LIBS})
set(BUILD_SHARED_LIBS OFF)
FetchContent_Declare(
  TBB
  GIT_REPOSITORY https://github.com/uxlfoundation/oneTBB.git
  GIT_TAG f1862f38f83568d96e814e469ab61f88336cc595 # v2022.3.0
  SYSTEM # its headers do not get our warning flags
)
FetchContent_MakeAvailable(TBB)
set(BUILD_SHARED_LIBS ${_tisma_shared})

# Warnings for our own targets only.
add_library(tisma_warnings INTERFACE)
if(MSVC)
  target_compile_options(tisma_warnings INTERFACE /W4 /permissive- /utf-8)
else()
  target_compile_options(tisma_warnings INTERFACE -Wall -Wextra -Wpedantic -Wshadow -Wconversion)
endif()

add_library(tisma_core
  src/mesh/mesh.cpp
  src/mesh/indexed_mesh.cpp
  src/slice/level_set.cpp
  src/slice/contours.cpp
  src/slice/layers.cpp
  src/io/stl.cpp
  src/cli/cli.cpp
)
target_include_directories(tisma_core PUBLIC src)
target_link_libraries(tisma_core PUBLIC TBB::tbb PRIVATE tisma_warnings)
target_compile_definitions(tisma_core PUBLIC TISMA_VERSION="${PROJECT_VERSION}")

add_executable(tisma-engine apps/main.cpp)
target_link_libraries(tisma-engine PRIVATE tisma_core tisma_warnings)

if(TISMA_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
