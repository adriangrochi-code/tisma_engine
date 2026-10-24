# Catch2 v3.7.1, BSL-1.0 (see THIRD_PARTY.md). Set FETCHCONTENT_SOURCE_DIR_CATCH2
# to a local checkout of the same tag to build offline.
FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG fa43b77429ba76c462b1898d6cd2f2d7a9416b14 # v3.7.1
  GIT_SHALLOW FALSE
  SYSTEM
)
FetchContent_MakeAvailable(Catch2)

add_executable(tisma_tests
  test_stl.cpp
  test_mesh.cpp
  test_cli.cpp
  test_indexed_mesh.cpp
  test_level_set.cpp
  test_contours.cpp
  test_layers.cpp
  test_polygons.cpp
  test_gpu.cpp
  test_settings.cpp
  test_medial_axis.cpp
  test_beading.cpp
  test_transitions.cpp
  test_sites.cpp
  test_coverage.cpp
  test_surfaces.cpp
  test_infill.cpp
  test_gcode.cpp
  test_order.cpp
)
target_link_libraries(tisma_tests PRIVATE tisma_core tisma_warnings Catch2::Catch2WithMain)
target_compile_definitions(tisma_tests PRIVATE TISMA_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data")

list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(Catch)
# Exit code 4 is Catch2's "all tests skipped" (for example GPU tests without a device).
catch_discover_tests(tisma_tests PROPERTIES SKIP_RETURN_CODE 4)

# End-to-end checks of the installed program.
add_test(NAME cli_version COMMAND tisma-engine --version)
add_test(NAME cli_info_ascii_cube COMMAND tisma-engine info ${CMAKE_CURRENT_SOURCE_DIR}/data/cube_ascii.stl)
set_tests_properties(cli_info_ascii_cube PROPERTIES PASS_REGULAR_EXPRESSION "triangles: 12")
add_test(NAME cli_slice_writes_gcode
  COMMAND tisma-engine slice ${CMAKE_CURRENT_SOURCE_DIR}/data/cube_ascii.stl
          --config ${CMAKE_CURRENT_SOURCE_DIR}/data/settings.json -o ${CMAKE_CURRENT_BINARY_DIR}/cube.gcode)
set_tests_properties(cli_slice_writes_gcode PROPERTIES PASS_REGULAR_EXPRESSION "wrote ")

# The engine's license must stay closable: every dependency listed in
# THIRD_PARTY.md with a permissive license, no copyleft text in our files.
find_package(Python3 COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND)
  add_test(NAME license_check COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/check_licenses.py ${PROJECT_SOURCE_DIR})
  # Specs from the team that knows Tisma Slicer carry no code and no internal
  # names (see CLEANROOM.md).
  if(EXISTS ${PROJECT_SOURCE_DIR}/docs/spec)
    add_test(NAME spec_check COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/check_spec.py ${PROJECT_SOURCE_DIR}/docs/spec)
  endif()
endif()
