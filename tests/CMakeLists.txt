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
  test_local_layers.cpp
  test_variable_layers.cpp
  test_zaa.cpp
  test_combine.cpp
  test_adaptive.cpp
  test_lightning.cpp
  test_nonplanar.cpp
  test_hostile.cpp
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
  test_resolution.cpp
  test_gcode_arcs.cpp
  test_firmware.cpp
  test_template.cpp
  test_binary.cpp
  test_retraction.cpp
  test_order.cpp
  test_scarf.cpp
  test_settings_layout.cpp
  test_vase.cpp
  test_travel.cpp
  test_cooling.cpp
  test_print_time.cpp
  test_adhesion.cpp
  test_fuzzy.cpp
  test_paint.cpp
  test_support.cpp
  test_tree_support.cpp
  test_arcs.cpp
  test_stability.cpp
  test_speeds.cpp
  test_formats.cpp
  test_bitgrid.cpp
  test_voronoi.cpp
  test_fea.cpp
  test_analysis.cpp
  test_gcode_check.cpp
  test_gcode_preview.cpp
  test_calibration.cpp
  test_profiles.cpp
  test_project.cpp
  test_catalog.cpp
  test_device.cpp
)
# Boost.Polygon is the oracle the own Voronoi diagram is tested against.
# nlohmann/json reads the analysis report back.
target_link_libraries(tisma_tests PRIVATE tisma_core tisma_warnings tisma_boost_polygon nlohmann_json::nlohmann_json Catch2::Catch2WithMain)
target_compile_definitions(tisma_tests PRIVATE TISMA_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data")
# The interface's logic without OpenGL (src/ui/), when the interface is built.
if(TISMA_BUILD_UI)
  target_sources(tisma_tests PRIVATE test_ui.cpp test_raytrace.cpp test_plate.cpp test_help.cpp)
  target_link_libraries(tisma_tests PRIVATE tisma_ui)
  target_compile_definitions(tisma_tests PRIVATE TISMA_ASSETS_DIR="${PROJECT_SOURCE_DIR}/assets"
                                                 TISMA_DOCS_DIR="${PROJECT_SOURCE_DIR}/docs")
endif()

list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(Catch)
# Exit code 4 is Catch2's "all tests skipped" (for example GPU tests without a device).
catch_discover_tests(tisma_tests PROPERTIES SKIP_RETURN_CODE 4)

# The random search for G-code the checker rejects (tests/fuzz_gcode.cpp); a
# short run here, long runs with new seeds in the maintenance blocks.
add_executable(tisma_fuzz_gcode fuzz_gcode.cpp)
target_link_libraries(tisma_fuzz_gcode PRIVATE tisma_core tisma_warnings)
add_test(NAME fuzz_gcode_smoke COMMAND tisma_fuzz_gcode 12 7)

# The whole interface, end to end, where a display can be faked: open a
# model, slice it, export the checked G-code and draw the preview.
if(TISMA_BUILD_UI AND NOT WIN32)
  find_program(TISMA_XVFB_RUN xvfb-run)
  if(TISMA_XVFB_RUN)
    add_test(NAME ui_slice_export_screenshot
      COMMAND ${TISMA_XVFB_RUN} -a -s "-screen 0 1280x800x24" $<TARGET_FILE:tisma>
              ${CMAKE_CURRENT_SOURCE_DIR}/data/cube_ascii.stl --lang en --size 1200x760 --view preview
              --export ${CMAKE_CURRENT_BINARY_DIR}/ui_test.gcode --screenshot ${CMAKE_CURRENT_BINARY_DIR}/ui_test.png)
    # Mesa's OpenGL keeps memory until exit; the leak checker would count it.
    # The ray tracing on the GPU against RayScene, the CPU reference: the same
    # shadow rays from the same surfaces must give the same answers.
    add_test(NAME ui_raytrace_matches_cpu
      COMMAND ${TISMA_XVFB_RUN} -a -s "-screen 0 1280x800x24" $<TARGET_FILE:tisma>
              ${CMAKE_CURRENT_SOURCE_DIR}/data/cube_ascii.stl --lang en --size 800x520 --ray-check --ray-samples 2
              --screenshot ${CMAKE_CURRENT_BINARY_DIR}/ui_raytrace.png)
    # The moiré filter: the preview from far away, its layers finer than a
    # pixel, gets smoother with the filter where the filter acts.
    add_test(NAME ui_moire_filter_smooths_fine_lines
      COMMAND ${TISMA_XVFB_RUN} -a -s "-screen 0 1280x800x24" $<TARGET_FILE:tisma>
              ${CMAKE_CURRENT_SOURCE_DIR}/data/cube_ascii.stl --lang en --size 1200x760 --view preview --moire-check
              --screenshot ${CMAKE_CURRENT_BINARY_DIR}/ui_moire.png)
    # The Device page: the cube, sliced, printed to the simulated printer
    # until it finishes.
    add_test(NAME ui_device_prints_to_simulator
      COMMAND ${TISMA_XVFB_RUN} -a -s "-screen 0 1280x800x24" $<TARGET_FILE:tisma>
              ${CMAKE_CURRENT_SOURCE_DIR}/data/cube_ascii.stl --lang en --size 1200x760 --view device --device sim
              --device-print --screenshot ${CMAKE_CURRENT_BINARY_DIR}/ui_device.png)
    # A project saved with chosen profiles opens again to the same G-code.
    add_test(NAME ui_project_save
      COMMAND ${TISMA_XVFB_RUN} -a -s "-screen 0 1280x800x24" $<TARGET_FILE:tisma>
              ${CMAKE_CURRENT_SOURCE_DIR}/data/cube_ascii.stl --lang en --size 900x600
              --profile filament=PETG "--profile" "print=0.28 mm"
              --export ${CMAKE_CURRENT_BINARY_DIR}/ui_project_a.gcode
              --save-project ${CMAKE_CURRENT_BINARY_DIR}/ui_project.tisma
              --screenshot ${CMAKE_CURRENT_BINARY_DIR}/ui_project_a.png)
    add_test(NAME ui_project_reopen
      COMMAND ${TISMA_XVFB_RUN} -a -s "-screen 0 1280x800x24" $<TARGET_FILE:tisma>
              ${CMAKE_CURRENT_BINARY_DIR}/ui_project.tisma --lang en --size 900x600
              --export ${CMAKE_CURRENT_BINARY_DIR}/ui_project_b.gcode
              --screenshot ${CMAKE_CURRENT_BINARY_DIR}/ui_project_b.png)
    add_test(NAME ui_project_same_gcode
      COMMAND ${CMAKE_COMMAND} -E compare_files ${CMAKE_CURRENT_BINARY_DIR}/ui_project_a.gcode
              ${CMAKE_CURRENT_BINARY_DIR}/ui_project_b.gcode)
    # A plate of two models (the second one added and the plate arranged),
    # with the Scale tool open, sliced; its project opens to the same G-code.
    add_test(NAME ui_plate_save
      COMMAND ${TISMA_XVFB_RUN} -a -s "-screen 0 1280x800x24" $<TARGET_FILE:tisma>
              ${CMAKE_CURRENT_SOURCE_DIR}/data/cube_ascii.stl ${CMAKE_CURRENT_SOURCE_DIR}/data/cube_ascii.stl
              --lang en --size 900x600 --tool scale
              --export ${CMAKE_CURRENT_BINARY_DIR}/ui_plate_a.gcode
              --save-project ${CMAKE_CURRENT_BINARY_DIR}/ui_plate.tisma
              --screenshot ${CMAKE_CURRENT_BINARY_DIR}/ui_plate_a.png)
    add_test(NAME ui_plate_reopen
      COMMAND ${TISMA_XVFB_RUN} -a -s "-screen 0 1280x800x24" $<TARGET_FILE:tisma>
              ${CMAKE_CURRENT_BINARY_DIR}/ui_plate.tisma --lang en --size 900x600
              --export ${CMAKE_CURRENT_BINARY_DIR}/ui_plate_b.gcode
              --screenshot ${CMAKE_CURRENT_BINARY_DIR}/ui_plate_b.png)
    add_test(NAME ui_plate_same_gcode
      COMMAND ${CMAKE_COMMAND} -E compare_files ${CMAKE_CURRENT_BINARY_DIR}/ui_plate_a.gcode
              ${CMAKE_CURRENT_BINARY_DIR}/ui_plate_b.gcode)
    # Help: the assistant's fourth step; the Paint tool's first-use tip with
    # the seam setting's info card open.
    add_test(NAME ui_help_tour
      COMMAND ${TISMA_XVFB_RUN} -a -s "-screen 0 1280x800x24" $<TARGET_FILE:tisma>
              ${CMAKE_CURRENT_SOURCE_DIR}/data/cube_ascii.stl --lang es --size 1100x700 --tour 3
              --screenshot ${CMAKE_CURRENT_BINARY_DIR}/ui_help_tour.png)
    add_test(NAME ui_help_tip_and_info
      COMMAND ${TISMA_XVFB_RUN} -a -s "-screen 0 1280x800x24" $<TARGET_FILE:tisma>
              ${CMAKE_CURRENT_SOURCE_DIR}/data/cube_ascii.stl --lang en --size 1100x700 --tips --tool paint
              --info walls.seam --screenshot ${CMAKE_CURRENT_BINARY_DIR}/ui_help_tip.png)
    set_tests_properties(ui_help_tour ui_help_tip_and_info PROPERTIES ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0" TIMEOUT 300)
    # The window with Vulkan (PLAN.md «Vista con Vulkan», V1): the panels
    # drawn by Vulkan and read back from its swapchain for the picture.
    if(TISMA_GPU)
      add_test(NAME ui_vulkan_window
        COMMAND ${TISMA_XVFB_RUN} -a -s "-screen 0 1280x800x24" $<TARGET_FILE:tisma>
                ${CMAKE_CURRENT_SOURCE_DIR}/data/cube_ascii.stl --lang en --size 900x600 --graphics vulkan
                --screenshot ${CMAKE_CURRENT_BINARY_DIR}/ui_vulkan.png)
      set_tests_properties(ui_vulkan_window PROPERTIES ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0" TIMEOUT 300)
      # The 3D view with Vulkan (V2) against OpenGL's, picture against picture:
      # the model with a painted seam, close; then the sliced cube's preview.
      set(TISMA_VIEW_prepare --tool paint --paint seam,enforce,133,131,5,2 --look 124,122,5,60,45,25)
      set(TISMA_VIEW_preview)
      foreach(view prepare preview)
        add_test(NAME ui_view_${view}_opengl
          COMMAND ${TISMA_XVFB_RUN} -a -s "-screen 0 1280x800x24" $<TARGET_FILE:tisma>
                  ${CMAKE_CURRENT_SOURCE_DIR}/data/cube_ascii.stl --lang en --size 900x600 --view ${view}
                  ${TISMA_VIEW_${view}} --render fast --graphics opengl
                  --screenshot ${CMAKE_CURRENT_BINARY_DIR}/ui_view_${view}_opengl.png)
        add_test(NAME ui_view_${view}_vulkan_same_as_opengl
          COMMAND ${TISMA_XVFB_RUN} -a -s "-screen 0 1280x800x24" $<TARGET_FILE:tisma>
                  ${CMAKE_CURRENT_SOURCE_DIR}/data/cube_ascii.stl --lang en --size 900x600 --view ${view}
                  ${TISMA_VIEW_${view}} --render fast --graphics vulkan
                  --compare-view ${CMAKE_CURRENT_BINARY_DIR}/ui_view_${view}_opengl.png
                  --screenshot ${CMAKE_CURRENT_BINARY_DIR}/ui_view_${view}_vulkan.png)
        set_tests_properties(ui_view_${view}_opengl PROPERTIES FIXTURES_SETUP ui_view_${view})
        set_tests_properties(ui_view_${view}_vulkan_same_as_opengl PROPERTIES FIXTURES_REQUIRED ui_view_${view})
        set_tests_properties(ui_view_${view}_opengl ui_view_${view}_vulkan_same_as_opengl
                             PROPERTIES ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0" TIMEOUT 300)
      endforeach()
    endif()
    # A seam painted down one side of a cube, sliced; its project keeps the
    # paint and opens to the same G-code.
    set(TISMA_PAINT_STROKES)
    foreach(z 0.5 1.5 2.5 3.5 4.5 5.5 6.5 7.5 8.5 9.5)
      list(APPEND TISMA_PAINT_STROKES --paint seam,enforce,133,131,${z},1)
    endforeach()
    add_test(NAME ui_paint_save
      COMMAND ${TISMA_XVFB_RUN} -a -s "-screen 0 1280x800x24" $<TARGET_FILE:tisma>
              ${CMAKE_CURRENT_SOURCE_DIR}/data/cube_ascii.stl --lang en --size 900x600 --tool paint
              ${TISMA_PAINT_STROKES}
              --export ${CMAKE_CURRENT_BINARY_DIR}/ui_paint_a.gcode
              --save-project ${CMAKE_CURRENT_BINARY_DIR}/ui_paint.tisma
              --screenshot ${CMAKE_CURRENT_BINARY_DIR}/ui_paint_a.png)
    add_test(NAME ui_paint_reopen
      COMMAND ${TISMA_XVFB_RUN} -a -s "-screen 0 1280x800x24" $<TARGET_FILE:tisma>
              ${CMAKE_CURRENT_BINARY_DIR}/ui_paint.tisma --lang en --size 900x600
              --export ${CMAKE_CURRENT_BINARY_DIR}/ui_paint_b.gcode
              --screenshot ${CMAKE_CURRENT_BINARY_DIR}/ui_paint_b.png)
    add_test(NAME ui_paint_same_gcode
      COMMAND ${CMAKE_COMMAND} -E compare_files ${CMAKE_CURRENT_BINARY_DIR}/ui_paint_a.gcode
              ${CMAKE_CURRENT_BINARY_DIR}/ui_paint_b.gcode)
    set_tests_properties(ui_paint_save PROPERTIES FIXTURES_SETUP ui_paint)
    set_tests_properties(ui_paint_reopen PROPERTIES FIXTURES_REQUIRED ui_paint FIXTURES_SETUP ui_paint_b)
    set_tests_properties(ui_paint_same_gcode PROPERTIES FIXTURES_REQUIRED "ui_paint;ui_paint_b")
    set_tests_properties(ui_paint_save ui_paint_reopen PROPERTIES ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0" TIMEOUT 300)
    set_tests_properties(ui_plate_save PROPERTIES FIXTURES_SETUP ui_plate)
    set_tests_properties(ui_plate_reopen PROPERTIES FIXTURES_REQUIRED ui_plate FIXTURES_SETUP ui_plate_b)
    set_tests_properties(ui_plate_same_gcode PROPERTIES FIXTURES_REQUIRED "ui_plate;ui_plate_b")
    set_tests_properties(ui_plate_save ui_plate_reopen PROPERTIES ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0" TIMEOUT 300)
    set_tests_properties(ui_project_save PROPERTIES FIXTURES_SETUP ui_project)
    set_tests_properties(ui_project_reopen PROPERTIES FIXTURES_REQUIRED ui_project FIXTURES_SETUP ui_project_b)
    set_tests_properties(ui_project_same_gcode PROPERTIES FIXTURES_REQUIRED "ui_project;ui_project_b")
    set_tests_properties(ui_slice_export_screenshot ui_raytrace_matches_cpu ui_moire_filter_smooths_fine_lines
                         ui_device_prints_to_simulator ui_project_save ui_project_reopen PROPERTIES
                         ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0" TIMEOUT 300)
  endif()
endif()

# End-to-end checks of the installed program.
add_test(NAME cli_version COMMAND tisma-engine --version)
add_test(NAME cli_info_ascii_cube COMMAND tisma-engine info ${CMAKE_CURRENT_SOURCE_DIR}/data/cube_ascii.stl)
set_tests_properties(cli_info_ascii_cube PROPERTIES PASS_REGULAR_EXPRESSION "triangles: 12")
add_test(NAME cli_slice_writes_gcode
  COMMAND tisma-engine slice ${CMAKE_CURRENT_SOURCE_DIR}/data/cube_ascii.stl
          --config ${CMAKE_CURRENT_SOURCE_DIR}/data/settings.json -o ${CMAKE_CURRENT_BINARY_DIR}/cube.gcode)
set_tests_properties(cli_slice_writes_gcode PROPERTIES PASS_REGULAR_EXPRESSION "wrote ")
add_test(NAME cli_analyze_writes_report
  COMMAND tisma-engine analyze ${CMAKE_CURRENT_SOURCE_DIR}/data/cube_ascii.stl
          --config ${CMAKE_CURRENT_SOURCE_DIR}/data/settings.json
          --case ${CMAKE_CURRENT_SOURCE_DIR}/data/analysis_case.json -o ${CMAKE_CURRENT_BINARY_DIR}/cube_analysis.json)
set_tests_properties(cli_analyze_writes_report PROPERTIES PASS_REGULAR_EXPRESSION "min_safety: ")

# The engine's license must stay closable: every dependency listed in
# THIRD_PARTY.md with a permissive license, no copyleft text in our files.
find_package(Python3 COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND)
  add_test(NAME license_check COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/check_licenses.py ${PROJECT_SOURCE_DIR})
  # VERSION has its section in CHANGELOG.md (docs/VERSIONING.md).
  add_test(NAME release_notes_check COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/release_notes.py --check)
  # Specs from the team that knows Tisma Slicer carry no code and no internal
  # names (see CLEANROOM.md).
  if(EXISTS ${PROJECT_SOURCE_DIR}/docs/spec)
    add_test(NAME spec_check COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/check_spec.py ${PROJECT_SOURCE_DIR}/docs/spec)
  endif()
endif()
