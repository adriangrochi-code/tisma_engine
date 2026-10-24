cmake_minimum_required(VERSION 3.25)

project(tisma_engine VERSION 0.0.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(TISMA_BUILD_TESTS "Build the Catch2 tests" ON)
option(TISMA_GPU "Build the Vulkan compute backend (used only when a device is present)" ON)

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

# Clipper2 1.5.4, BSL-1.0 (see THIRD_PARTY.md): polygon booleans and offsets.
set(CLIPPER2_UTILS OFF CACHE BOOL "" FORCE)
set(CLIPPER2_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CLIPPER2_TESTS OFF CACHE BOOL "" FORCE)
set(CLIPPER2_USINGZ "OFF" CACHE STRING "" FORCE)
FetchContent_Declare(
  Clipper2
  GIT_REPOSITORY https://github.com/AngusJohnson/Clipper2.git
  GIT_TAG ef88ee97c0e759792e43a2b2d8072def6c9244e8 # Clipper2_1.5.4
  SOURCE_SUBDIR CPP
  SYSTEM
)
FetchContent_MakeAvailable(Clipper2)

# nlohmann/json 3.12.0, MIT (see THIRD_PARTY.md): the settings file. Only its
# headers are used; its tests (one of them GPL) are not built.
set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
set(JSON_Install OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG 55f93686c01528224f448c19128836e7df245f72 # v3.12.0
  SYSTEM
)
FetchContent_MakeAvailable(nlohmann_json)

if(TISMA_GPU)
  # Vulkan-Headers (Apache-2.0) and volk (MIT), see THIRD_PARTY.md. volk loads
  # the Vulkan loader at run time, so the engine starts without Vulkan.
  set(VULKAN_HEADERS_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
  set(VULKAN_HEADERS_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
  set(VULKAN_HEADERS_ENABLE_MODULE OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(
    VulkanHeaders
    GIT_REPOSITORY https://github.com/KhronosGroup/Vulkan-Headers.git
    GIT_TAG 8864cdc896bbc2a9b6eb36b3218fc9ef57908d77 # vulkan-sdk-1.4.350.1
    SYSTEM
  )
  FetchContent_MakeAvailable(VulkanHeaders)
  # Point volk at these headers, not at whatever Vulkan SDK the machine has.
  set(VULKAN_HEADERS_INSTALL_DIR ${vulkanheaders_SOURCE_DIR} CACHE PATH "" FORCE)
  FetchContent_Declare(
    volk
    GIT_REPOSITORY https://github.com/zeux/volk.git
    GIT_TAG 3ca312a4f38baa63d8006b6905abbeeb89c8087d # vulkan-sdk-1.4.350.1
    SYSTEM
  )
  FetchContent_MakeAvailable(volk)
endif()
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
  src/geom/polygons.cpp
  src/config/settings.cpp
  src/slice/pipeline.cpp
  src/io/stl.cpp
  src/cli/cli.cpp
)
if(TISMA_GPU)
  target_sources(tisma_core PRIVATE src/gpu/compute_vulkan.cpp)
  target_link_libraries(tisma_core PRIVATE volk::volk)
else()
  target_sources(tisma_core PRIVATE src/gpu/compute_none.cpp)
endif()
target_include_directories(tisma_core PUBLIC src)
# Kernels must give the same bits on the CPU and the GPU: no fused multiply-add.
if(NOT MSVC)
  target_compile_options(tisma_core PRIVATE -ffp-contract=off)
endif()
target_link_libraries(tisma_core PUBLIC TBB::tbb Clipper2 PRIVATE nlohmann_json::nlohmann_json tisma_warnings)
target_compile_definitions(tisma_core PUBLIC TISMA_VERSION="${PROJECT_VERSION}")

add_executable(tisma-engine apps/main.cpp)
target_link_libraries(tisma-engine PRIVATE tisma_core tisma_warnings)

if(TISMA_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
