cmake_minimum_required(VERSION 3.25)

# The version lives in the VERSION file (docs/VERSIONING.md): MAJOR.MINOR.PATCH
# with an optional pre-release label, such as 0.6.0-alpha.1. CMake takes the
# numbers; the whole text goes into the program (--version, the G-code header).
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" TISMA_VERSION_TEXT LIMIT_COUNT 1)
if(NOT TISMA_VERSION_TEXT MATCHES "^([0-9]+\\.[0-9]+\\.[0-9]+)(-[0-9A-Za-z.-]+)?$")
  message(FATAL_ERROR "VERSION must read MAJOR.MINOR.PATCH[-label], not '${TISMA_VERSION_TEXT}'")
endif()
project(tisma_engine VERSION ${CMAKE_MATCH_1} LANGUAGES C CXX)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(TISMA_BUILD_TESTS "Build the Catch2 tests" ON)
option(TISMA_GPU "Build the Vulkan compute backend (used only when a device is present)" ON)
option(TISMA_BUILD_UI "Build the interface, tisma (OpenGL 3.3; on Linux it needs the X11 development packages)" ON)

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

# Boost.Polygon (segment Voronoi) and the only Boost library it needs,
# Boost.Config; both BSL-1.0 and header-only (see THIRD_PARTY.md). Their own
# CMake files expect the whole Boost tree, so only the headers are used.
FetchContent_Declare(
  BoostConfig
  GIT_REPOSITORY https://github.com/boostorg/config.git
  GIT_TAG a7d5a9b05d70c9cfea980dc3539ca3d3461411b3 # boost-1.90.0
  SOURCE_SUBDIR no-cmake
)
FetchContent_Declare(
  BoostPolygon
  GIT_REPOSITORY https://github.com/boostorg/polygon.git
  GIT_TAG 8ba35b57c1436c4b36f7544aadd78c2b24acc7db # boost-1.90.0
  SOURCE_SUBDIR no-cmake
)
FetchContent_MakeAvailable(BoostConfig BoostPolygon)
add_library(tisma_boost_polygon INTERFACE)
target_include_directories(tisma_boost_polygon SYSTEM INTERFACE ${boostconfig_SOURCE_DIR}/include ${boostpolygon_SOURCE_DIR}/include)

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
if(TISMA_BUILD_UI)
  # GLFW 3.4 (Zlib), Dear ImGui 1.92.9b (MIT; its Roboto Medium font is
  # Apache-2.0) and nanosvg (Zlib), see THIRD_PARTY.md: the interface only.
  # GLFW is built static, with X11 on Linux and without Wayland, examples,
  # tests or docs. ImGui and nanosvg are used as sources, without their builds.
  set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
  set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
  set(GLFW_BUILD_WAYLAND OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 7b6aead9fb88b3623e3b3725ebb42670cbe4c579 # 3.4
    SYSTEM
  )
  FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG f1cc2ae15e53a861a874c3034aae6798fde194ab # v1.92.9b
    SOURCE_SUBDIR no-cmake
  )
  FetchContent_Declare(
    nanosvg
    GIT_REPOSITORY https://github.com/memononen/nanosvg.git
    GIT_TAG 239e102ec2c691f2902e20ace2ed36ee4a35cfe6 # no tags; 2026-07-10
    SOURCE_SUBDIR no-cmake
  )
  FetchContent_MakeAvailable(glfw imgui nanosvg)
endif()
set(BUILD_SHARED_LIBS ${_tisma_shared})

# Warnings for our own targets only.
add_library(tisma_warnings INTERFACE)
if(MSVC)
  target_compile_options(tisma_warnings INTERFACE /W4 /permissive- /utf-8)
  # std::getenv is only read here, never kept across a change of the environment.
  target_compile_definitions(tisma_warnings INTERFACE _CRT_SECURE_NO_WARNINGS)
else()
  target_compile_options(tisma_warnings INTERFACE -Wall -Wextra -Wpedantic -Wshadow -Wconversion)
endif()

# The printer and filament catalog travels inside the engine (docs/CATALOG.md).
set(TISMA_CATALOG ${PROJECT_SOURCE_DIR}/resources/catalog/printers.json ${PROJECT_SOURCE_DIR}/resources/catalog/filaments.json)
add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/catalog_files.cpp
  COMMAND ${CMAKE_COMMAND} "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/catalog_files.cpp" "-DFILES=${TISMA_CATALOG}"
          "-DNAMESPACE=tisma::catalog_data" "-DHEADER=catalog/catalog_files.hpp"
          -P ${PROJECT_SOURCE_DIR}/cmake/embed.cmake
  DEPENDS ${TISMA_CATALOG} ${PROJECT_SOURCE_DIR}/cmake/embed.cmake
  COMMENT "Embedding the printer and filament catalog"
  VERBATIM
)

add_library(tisma_core
  src/mesh/mesh.cpp
  src/mesh/indexed_mesh.cpp
  src/mesh/paint.cpp
  src/slice/level_set.cpp
  src/slice/contours.cpp
  src/slice/layers.cpp
  src/slice/local_layers.cpp
  src/slice/variable_layers.cpp
  src/slice/zaa.cpp
  src/geom/bitgrid.cpp
  src/geom/polygons.cpp
  src/config/settings.cpp
  src/config/profiles.cpp
  src/slice/painted.cpp
  src/slice/pipeline.cpp
  src/geom/medial_axis.cpp
  src/geom/voronoi.cpp
  src/perimeters/beading.cpp
  src/perimeters/fuzzy.cpp
  src/perimeters/transitions.cpp
  src/perimeters/sites.cpp
  src/perimeters/walls.cpp
  src/perimeters/coverage.cpp
  src/surfaces/surfaces.cpp
  src/surfaces/bridges.cpp
  src/infill/adaptive.cpp
  src/infill/rectilinear.cpp
  src/infill/gyroid.cpp
  src/infill/lightning.cpp
  src/infill/nonplanar.cpp
  src/infill/patterns.cpp
  src/infill/combine.cpp
  src/infill/connect.cpp
  src/infill/stress_density.cpp
  src/gcode/writer.cpp
  src/gcode/resolution.cpp
  src/gcode/scarf.cpp
  src/gcode/check.cpp
  src/gcode/export.cpp
  src/gcode/preview.cpp
  src/gcode/order.cpp
  src/gcode/travel.cpp
  src/gcode/cooling.cpp
  src/gcode/print_time.cpp
  src/report/warnings.cpp
  src/slice/printable.cpp
  src/adhesion/adhesion.cpp
  src/support/support.cpp
  src/support/tree.cpp
  src/fea/voxels.cpp
  src/fea/hex8.cpp
  src/fea/elasticity.cpp
  src/fea/operator.cpp
  src/fea/multigrid.cpp
  src/fea/material.cpp
  src/fea/as_printed.cpp
  src/fea/materials.cpp
  src/fea/analysis.cpp
  src/config/analysis_case.cpp
  src/report/analysis_json.cpp
  src/io/vtk.cpp
  src/io/ply.cpp
  src/io/stl.cpp
  src/calibration/calibration.cpp
  src/project/project.cpp
  src/catalog/catalog.cpp
  src/device/gcode_host.cpp
  src/device/printer.cpp
  src/device/serial_port.cpp
  src/device/simulator.cpp
  src/device/stream.cpp
  ${CMAKE_CURRENT_BINARY_DIR}/catalog_files.cpp
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
target_link_libraries(tisma_core PUBLIC TBB::tbb Clipper2 PRIVATE nlohmann_json::nlohmann_json tisma_boost_polygon tisma_warnings)
target_compile_definitions(tisma_core PUBLIC TISMA_VERSION="${TISMA_VERSION_TEXT}")

add_executable(tisma-engine apps/main.cpp)
target_link_libraries(tisma-engine PRIVATE tisma_core tisma_warnings)

if(TISMA_BUILD_UI)
  # What the interface does without OpenGL (camera, draw data, icons, texts,
  # files, the slicing job): tested with the engine.
  add_library(tisma_ui STATIC
    src/ui/camera.cpp
    src/ui/scene.cpp
    src/ui/icons.cpp
    src/ui/png.cpp
    src/ui/text.cpp
    src/ui/files.cpp
    src/ui/slicing.cpp
    src/ui/presets.cpp
    src/ui/settings_layout.cpp
    src/ui/raytrace.cpp
    src/ui/plate.cpp
    src/ui/help.cpp
  )
  target_include_directories(tisma_ui SYSTEM PRIVATE ${nanosvg_SOURCE_DIR}/src)
  target_link_libraries(tisma_ui PUBLIC tisma_core PRIVATE tisma_warnings)
  add_subdirectory(apps/tisma)
endif()

if(TISMA_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
