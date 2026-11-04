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
  test_node.cpp
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
  test_corners.cpp
  test_gcode_arcs.cpp
  test_firmware.cpp
  test_template.cpp
  test_binary.cpp
  test_retraction.cpp
  test_walls_extra.cpp
  test_sequential.cpp
  test_labels.cpp
  test_multi_extruder.cpp
  test_color_mix.cpp
  test_curved.cpp
  test_resin.cpp
  test_belt.cpp
  test_order.cpp
  test_scarf.cpp
  test_settings_layout.cpp
  test_settings_view.cpp
  test_printer_card.cpp
  test_transform.cpp
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
  test_lattice.cpp
  test_adhesion.cpp
  test_gcode_check.cpp
  test_gcode_preview.cpp
  test_calibration.cpp
  test_determinism.cpp
  test_profiles.cpp
  test_project.cpp
  test_orient.cpp
  test_catalog.cpp
  test_device.cpp
  test_cad.cpp
  test_sketch_io.cpp
  test_sketch_edit.cpp
  test_aero.cpp
  test_fasteners.cpp
  test_gears.cpp
  test_cloud.cpp
  test_pack.cpp
  test_xz.cpp
  test_pack_fetch.cpp
  test_print_host.cpp
)
# The stand-in for the bundled solidcraft-cli (cad_design.md, rule 1): the
# tests drive it instead of the real tool.
add_executable(tisma_fake_solidcraft fake_solidcraft_cli.cpp)
target_link_libraries(tisma_fake_solidcraft PRIVATE tisma_core tisma_warnings nlohmann_json::nlohmann_json)
add_dependencies(tisma_tests tisma_fake_solidcraft)
target_compile_definitions(tisma_tests PRIVATE TISMA_FAKE_SOLIDCRAFT="$<TARGET_FILE:tisma_fake_solidcraft>")
# Boost.Polygon is the oracle the own Voronoi diagram is tested against.
# nlohmann/json reads the analysis report back.
target_link_libraries(tisma_tests PRIVATE tisma_core tisma_warnings tisma_boost_polygon nlohmann_json::nlohmann_json Catch2::Catch2WithMain)
# The cloud client, and cpp-httplib's server for its mock server (test_cloud.cpp).
target_link_libraries(tisma_tests PRIVATE tisma_net tisma_httplib)
# A stand-in for libsecret's secret-tool (the token's store on Linux).
add_executable(tisma_fake_secret_tool fake_secret_tool.cpp)
add_dependencies(tisma_tests tisma_fake_secret_tool)
target_compile_definitions(tisma_tests PRIVATE TISMA_FAKE_SECRET_TOOL="$<TARGET_FILE:tisma_fake_secret_tool>")
# The mock Tisma Cloud server around a program (the interface's cloud tests).
add_executable(tisma_mock_cloud mock_cloud_main.cpp)
target_link_libraries(tisma_mock_cloud PRIVATE tisma_core tisma_httplib nlohmann_json::nlohmann_json tisma_warnings)
target_compile_definitions(tisma_tests PRIVATE TISMA_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data")
# The interface's logic without OpenGL (src/ui/), when the interface is built.
if(TISMA_BUILD_UI)
  target_sources(tisma_tests PRIVATE test_ui.cpp test_raytrace.cpp test_plate.cpp test_help.cpp test_credits.cpp test_assistant.cpp
    test_system_info.cpp)
  target_link_libraries(tisma_tests PRIVATE tisma_ui)
  target_compile_definitions(tisma_tests PRIVATE TISMA_ASSETS_DIR="${PROJECT_SOURCE_DIR}/assets"
                                                 TISMA_DOCS_DIR="${PROJECT_SOURCE_DIR}/docs"
                                                 TISMA_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
endif()

list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(Catch)
# Exit code 4 is Catch2's "all tests skipped" (for example GPU tests without a device).
catch_discover_tests(tisma_tests PROPERTIES SKIP_RETURN_CODE 4)
# The aerodynamic drag coefficients (aerodynamics.md, AE1 to AE4) run the
# lattice at a useful size: too slow for the debug build with the sanitizers.
if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
  add_test(NAME aero_drag_coefficients COMMAND tisma_tests "[aero-full]")
  set_tests_properties(aero_drag_coefficients PROPERTIES TIMEOUT 1800)
endif()

# The random search for G-code the checker rejects (tests/fuzz_gcode.cpp); a
# short run here, long runs with new seeds in the maintenance blocks.
add_executable(tisma_fuzz_gcode fuzz_gcode.cpp)
target_link_libraries(tisma_fuzz_gcode PRIVATE tisma_core tisma_warnings)
add_test(NAME fuzz_gcode_smoke COMMAND tisma_fuzz_gcode 12 7)

# The settings panel as data for the phone (mobile_app.md, rule 7): no window,
# so it runs everywhere the interface builds.
if(TISMA_BUILD_UI)
  add_test(NAME ui_settings_layout COMMAND $<TARGET_FILE:tisma> --settings-layout - --lang en)
  set_tests_properties(ui_settings_layout PROPERTIES
    PASS_REGULAR_EXPRESSION "\"id\": \"material\".*\"key\": \"layers.height\"|\"key\": \"layers.height\".*\"id\": \"material\"")
endif()

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
    # The strict view check of every screenshot (no black pixel, few
    # sparkles) with the occlusion at its strongest: a strength past 1 made the
    # ambient light negative, drawn black (2026-10-28).
    # The rays find the crease under the cube closed (no ambient light at all).
    foreach(render ao rt)
      foreach(view prepare preview)
        add_test(NAME ui_view_${render}_strongest_${view}
          COMMAND ${TISMA_XVFB_RUN} -a -s "-screen 0 1280x800x24" $<TARGET_FILE:tisma>
                  ${CMAKE_CURRENT_SOURCE_DIR}/data/cube_ascii.stl --lang en --size 900x600 --view ${view}
                  --render ${render} --ray-samples 2 --ao-strength 1.5 --graphics opengl
                  --screenshot ${CMAKE_CURRENT_BINARY_DIR}/ui_view_${render}_strongest_${view}.png)
        set_tests_properties(ui_view_${render}_strongest_${view}
                             PROPERTIES ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0" TIMEOUT 300)
      endforeach()
    endforeach()
    # A printer picked from the catalog in the printer card's list, on an empty
    # settings folder (a crash, 2026-10-28: the card kept a pointer into the
    # profile lists that adding the printer reads again).
    add_test(NAME ui_catalog_printer_home_clean
      COMMAND ${CMAKE_COMMAND} -E rm -rf ${CMAKE_CURRENT_BINARY_DIR}/ui_catalog_home)
    add_test(NAME ui_catalog_printer_picked
      COMMAND ${TISMA_XVFB_RUN} -a -s "-screen 0 1280x800x24" $<TARGET_FILE:tisma>
              ${CMAKE_CURRENT_SOURCE_DIR}/data/cube_ascii.stl --lang es --light --size 1200x760
              --pick-catalog-printer "Creality CR-5 Pro H"
              --screenshot ${CMAKE_CURRENT_BINARY_DIR}/ui_catalog_printer.png)
    # The free profile pack (profile_pack.md): the invented test pack installed
    # by the engine, then a printer of it added and chosen with its filament
    # and print profile, as factory profiles (rules 9 and 13, PK6).
    add_test(NAME ui_pack_install
      COMMAND $<TARGET_FILE:tisma-engine> pack install --data ${CMAKE_CURRENT_BINARY_DIR}/ui_pack
              --from ${CMAKE_CURRENT_SOURCE_DIR}/data/pack/release1)
    set_tests_properties(ui_pack_install PROPERTIES FIXTURES_SETUP ui_pack)
    add_test(NAME ui_pack_printer_picked
      COMMAND ${TISMA_XVFB_RUN} -a -s "-screen 0 1280x800x24" $<TARGET_FILE:tisma>
              ${CMAKE_CURRENT_SOURCE_DIR}/data/cube_ascii.stl --lang es --light --size 1200x760
              --pack ${CMAKE_CURRENT_BINARY_DIR}/ui_pack/pack --pick-pack-printer "vendors/acme/models/A1.json#0.4"
              --screenshot ${CMAKE_CURRENT_BINARY_DIR}/ui_pack_printer.png)
    set_tests_properties(ui_pack_printer_picked PROPERTIES FIXTURES_REQUIRED "ui_pack;ui_catalog_home" RESOURCE_LOCK ui_catalog_home
                         ENVIRONMENT "HOME=${CMAKE_CURRENT_BINARY_DIR}/ui_catalog_home;XDG_CONFIG_HOME=;ASAN_OPTIONS=detect_leaks=0"
                         PASS_REGULAR_EXPRESSION "pack printer: Acme One 0.4 · Acme PLA · Acme Standard" TIMEOUT 300)
    # The filament catalog offers the pack's filaments for that printer (rule 12).
    add_test(NAME ui_pack_filaments
      COMMAND ${TISMA_XVFB_RUN} -a -s "-screen 0 1280x800x24" $<TARGET_FILE:tisma>
              ${CMAKE_CURRENT_SOURCE_DIR}/data/cube_ascii.stl --lang es --light --size 1200x760
              --pack ${CMAKE_CURRENT_BINARY_DIR}/ui_pack/pack --pick-pack-printer "vendors/acme/models/A1.json#0.4"
              --catalog filament --catalog-search acme
              --screenshot ${CMAKE_CURRENT_BINARY_DIR}/ui_pack_filaments.png)
    set_tests_properties(ui_pack_filaments PROPERTIES FIXTURES_REQUIRED "ui_pack;ui_catalog_home" RESOURCE_LOCK ui_catalog_home
                         ENVIRONMENT "HOME=${CMAKE_CURRENT_BINARY_DIR}/ui_catalog_home;XDG_CONFIG_HOME=;ASAN_OPTIONS=detect_leaks=0"
                         TIMEOUT 300)
    # The pack printer's «Datos publicados», and a maker's value used (rules 23 and 24).
    add_test(NAME ui_pack_published
      COMMAND ${TISMA_XVFB_RUN} -a -s "-screen 0 1280x800x24" $<TARGET_FILE:tisma>
              ${CMAKE_CURRENT_SOURCE_DIR}/data/cube_ascii.stl --lang es --light --size 1200x760
              --pack ${CMAKE_CURRENT_BINARY_DIR}/ui_pack/pack --pick-pack-printer "vendors/acme/models/A1.json#0.4"
              --ui-script ${CMAKE_CURRENT_SOURCE_DIR}/data/ui/pack_published.txt
              --screenshot ${CMAKE_CURRENT_BINARY_DIR}/ui_pack_published.png)
    set_tests_properties(ui_pack_published PROPERTIES FIXTURES_REQUIRED "ui_pack;ui_catalog_home" RESOURCE_LOCK ui_catalog_home
                         ENVIRONMENT "HOME=${CMAKE_CURRENT_BINARY_DIR}/ui_catalog_home;XDG_CONFIG_HOME=;ASAN_OPTIONS=detect_leaks=0"
                         TIMEOUT 300)
    # A pack printer the pack no longer has stays chosen, with a notice (rule 20);
    # on a pack of its own, since the script takes a file out of it.
    add_test(NAME ui_pack_gone_install
      COMMAND $<TARGET_FILE:tisma-engine> pack install --data ${CMAKE_CURRENT_BINARY_DIR}/ui_pack_gone
              --from ${CMAKE_CURRENT_SOURCE_DIR}/data/pack/release1)
    set_tests_properties(ui_pack_gone_install PROPERTIES FIXTURES_SETUP ui_pack_gone)
    add_test(NAME ui_pack_gone
      COMMAND ${TISMA_XVFB_RUN} -a -s "-screen 0 1280x800x24" $<TARGET_FILE:tisma>
              ${CMAKE_CURRENT_SOURCE_DIR}/data/cube_ascii.stl --lang es --light --size 1200x760
              --pack ${CMAKE_CURRENT_BINARY_DIR}/ui_pack_gone/pack --pick-pack-printer "vendors/acme/models/A1.json#0.4"
              --ui-script ${CMAKE_CURRENT_SOURCE_DIR}/data/ui/pack_gone.txt
              --screenshot ${CMAKE_CURRENT_BINARY_DIR}/ui_pack_gone.png)
    set_tests_properties(ui_pack_gone PROPERTIES FIXTURES_REQUIRED "ui_pack_gone;ui_catalog_home" RESOURCE_LOCK ui_catalog_home
                         ENVIRONMENT "HOME=${CMAKE_CURRENT_BINARY_DIR}/ui_catalog_home;XDG_CONFIG_HOME=;ASAN_OPTIONS=detect_leaks=0"
                         TIMEOUT 300)
    add_test(NAME ui_pack_catalog
      COMMAND ${TISMA_XVFB_RUN} -a -s "-screen 0 1280x800x24" $<TARGET_FILE:tisma>
              --lang es --light --size 1200x760 --catalog printer --catalog-search acme
              --pack ${CMAKE_CURRENT_BINARY_DIR}/ui_pack/pack
              --screenshot ${CMAKE_CURRENT_BINARY_DIR}/ui_pack_catalog.png)
    set_tests_properties(ui_pack_catalog PROPERTIES FIXTURES_REQUIRED ui_pack ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0" TIMEOUT 300)
    # A 2.85 mm printer: the filament catalog offers only that diameter, and
    # says why it is empty (pellets.md, rule 1; profiles.md, rule 5).
    add_test(NAME ui_catalog_filament_by_diameter
      COMMAND ${TISMA_XVFB_RUN} -a -s "-screen 0 1280x800x24" $<TARGET_FILE:tisma>
              ${CMAKE_CURRENT_SOURCE_DIR}/data/cube_ascii.stl --lang es --light --size 1200x760
              --pick-catalog-printer "LulzBot TAZ 6" --catalog filament
              --screenshot ${CMAKE_CURRENT_BINARY_DIR}/ui_catalog_filament_285.png)
    set_tests_properties(ui_catalog_printer_home_clean PROPERTIES FIXTURES_SETUP ui_catalog_home)
    set_tests_properties(ui_catalog_filament_by_diameter PROPERTIES FIXTURES_REQUIRED ui_catalog_home
                         RESOURCE_LOCK ui_catalog_home ENVIRONMENT "HOME=${CMAKE_CURRENT_BINARY_DIR}/ui_catalog_home;XDG_CONFIG_HOME=;ASAN_OPTIONS=detect_leaks=0"
                         TIMEOUT 300)
    set_tests_properties(ui_catalog_printer_picked PROPERTIES FIXTURES_REQUIRED ui_catalog_home RESOURCE_LOCK ui_catalog_home
                         ENVIRONMENT "HOME=${CMAKE_CURRENT_BINARY_DIR}/ui_catalog_home;XDG_CONFIG_HOME=;ASAN_OPTIONS=detect_leaks=0"
                         TIMEOUT 300)
    # Fields edited with the mouse and the keyboard, as a person does
    # (--ui-script, tests/data/ui): the printer's page, and a number of the
    # print settings typed with a decimal comma; units, limits, a checkbox and
    # the purge volume (unit_fields); values near the top of their range (wide_limits).
    foreach(script printer_fields print_fields unit_fields wide_limits)
      add_test(NAME ui_script_${script}
        COMMAND ${TISMA_XVFB_RUN} -a -s "-screen 0 1280x800x24" $<TARGET_FILE:tisma>
                ${CMAKE_CURRENT_SOURCE_DIR}/data/cube_ascii.stl --lang es --light --size 1200x760
                --ui-script ${CMAKE_CURRENT_SOURCE_DIR}/data/ui/${script}.txt
                --screenshot ${CMAKE_CURRENT_BINARY_DIR}/ui_script_${script}.png)
      set_tests_properties(ui_script_${script} PROPERTIES ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0" TIMEOUT 300)
    endforeach()
    # The setting rows as JSON (--dump-ui), for the spec team's text checks.
    add_test(NAME ui_dump_rows
      COMMAND ${TISMA_XVFB_RUN} -a -s "-screen 0 1280x800x24" $<TARGET_FILE:tisma>
              ${CMAKE_CURRENT_SOURCE_DIR}/data/cube_ascii.stl --lang es --size 1200x760 --level advanced
              --dump-ui ${CMAKE_CURRENT_BINARY_DIR}/ui_dump_rows.json
              --screenshot ${CMAKE_CURRENT_BINARY_DIR}/ui_dump_rows.png)
    set_tests_properties(ui_dump_rows PROPERTIES ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0" TIMEOUT 300)
    # TR25: at 1440 x 900, every section, both languages: no label is cut
    # without its «…» (the dump counts the rows whose label the column hides).
    foreach(lang es en)
      foreach(section quality strength speed support adhesion other)
        add_test(NAME ui_labels_fit_${lang}_${section}
          COMMAND ${TISMA_XVFB_RUN} -a -s "-screen 0 1500x950x24" $<TARGET_FILE:tisma>
                  ${CMAKE_CURRENT_SOURCE_DIR}/data/cube_ascii.stl --lang ${lang} --size 1440x900 --level advanced
                  --section print/${section}
                  --dump-ui ${CMAKE_CURRENT_BINARY_DIR}/ui_labels_${lang}_${section}.json
                  --screenshot ${CMAKE_CURRENT_BINARY_DIR}/ui_labels_${lang}_${section}.png)
        set_tests_properties(ui_labels_fit_${lang}_${section} PROPERTIES ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0"
          TIMEOUT 300 PASS_REGULAR_EXPRESSION "dump-ui: [0-9]+ rows, 0 with the label cut")
      endforeach()
    endforeach()
    # A still view draws nothing new (idle_view): its draw calls counted.
    add_test(NAME ui_script_idle_view
      COMMAND ${TISMA_XVFB_RUN} -a -s "-screen 0 1280x800x24" $<TARGET_FILE:tisma>
              ${CMAKE_CURRENT_SOURCE_DIR}/data/sphere.stl --render ao --graphics opengl --draw-stats
              --ui-script ${CMAKE_CURRENT_SOURCE_DIR}/data/ui/idle_view.txt
              --screenshot ${CMAKE_CURRENT_BINARY_DIR}/ui_script_idle_view.png)
    set_tests_properties(ui_script_idle_view PROPERTIES ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0" TIMEOUT 300)
    # A dialog takes every key: Delete behind the catalog leaves the model (UI25).
    add_test(NAME ui_script_modal_keys
      COMMAND ${TISMA_XVFB_RUN} -a -s "-screen 0 1280x800x24" $<TARGET_FILE:tisma>
              ${CMAKE_CURRENT_SOURCE_DIR}/data/cube_ascii.stl --lang en --size 1200x760 --catalog printer
              --ui-script ${CMAKE_CURRENT_SOURCE_DIR}/data/ui/modal_keys.txt
              --screenshot ${CMAKE_CURRENT_BINARY_DIR}/ui_script_modal_keys.png)
    set_tests_properties(ui_script_modal_keys PROPERTIES ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0" TIMEOUT 300)
    # A tool's tip closes with the tool (UI38).
    add_test(NAME ui_script_tip_closes
      COMMAND ${TISMA_XVFB_RUN} -a -s "-screen 0 1280x800x24" $<TARGET_FILE:tisma>
              ${CMAKE_CURRENT_SOURCE_DIR}/data/cube_ascii.stl --lang es --size 1200x760 --tips
              --ui-script ${CMAKE_CURRENT_SOURCE_DIR}/data/ui/tip_closes.txt
              --screenshot ${CMAKE_CURRENT_BINARY_DIR}/ui_script_tip_closes.png)
    set_tests_properties(ui_script_tip_closes PROPERTIES ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0" TIMEOUT 300)
    add_test(NAME ui_script_tutorial_again
      COMMAND ${TISMA_XVFB_RUN} -a -s "-screen 0 1280x800x24" $<TARGET_FILE:tisma>
              ${CMAKE_CURRENT_SOURCE_DIR}/data/cube_ascii.stl --lang es --size 1200x760
              --ui-script ${CMAKE_CURRENT_SOURCE_DIR}/data/ui/tutorial_again.txt
              --screenshot ${CMAKE_CURRENT_BINARY_DIR}/ui_script_tutorial_again.png)
    set_tests_properties(ui_script_tutorial_again PROPERTIES ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0" TIMEOUT 300)
    add_test(NAME ui_script_paint_drag
      COMMAND ${TISMA_XVFB_RUN} -a -s "-screen 0 1280x800x24" $<TARGET_FILE:tisma>
              ${CMAKE_CURRENT_SOURCE_DIR}/data/cube_ascii.stl --lang es --size 1200x760
              --ui-script ${CMAKE_CURRENT_SOURCE_DIR}/data/ui/paint_drag.txt
              --screenshot ${CMAKE_CURRENT_BINARY_DIR}/ui_script_paint_drag.png)
    set_tests_properties(ui_script_paint_drag PROPERTIES ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0" TIMEOUT 300)
    foreach(lang es en)
      add_test(NAME ui_script_view_bar_narrow_${lang}
        COMMAND ${TISMA_XVFB_RUN} -a -s "-screen 0 1280x800x24" $<TARGET_FILE:tisma>
                ${CMAKE_CURRENT_SOURCE_DIR}/data/cube_ascii.stl --lang ${lang} --size 900x700
                --ui-script ${CMAKE_CURRENT_SOURCE_DIR}/data/ui/view_bar_narrow.txt
                --screenshot ${CMAKE_CURRENT_BINARY_DIR}/ui_script_view_bar_narrow_${lang}.png)
      set_tests_properties(ui_script_view_bar_narrow_${lang} PROPERTIES ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0" TIMEOUT 300)
    endforeach()
    add_test(NAME ui_script_paint_holds_occlusion
      COMMAND ${TISMA_XVFB_RUN} -a -s "-screen 0 1280x800x24" $<TARGET_FILE:tisma>
              ${CMAKE_CURRENT_SOURCE_DIR}/data/cube_ascii.stl --lang es --size 1200x760 --graphics opengl --render ao
              --ui-script ${CMAKE_CURRENT_SOURCE_DIR}/data/ui/paint_holds_occlusion.txt
              --screenshot ${CMAKE_CURRENT_BINARY_DIR}/ui_script_paint_holds_occlusion.png)
    set_tests_properties(ui_script_paint_holds_occlusion PROPERTIES ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0" TIMEOUT 300)
    add_test(NAME ui_script_top_bar_narrow
      COMMAND ${TISMA_XVFB_RUN} -a -s "-screen 0 1280x800x24" $<TARGET_FILE:tisma>
              ${CMAKE_CURRENT_SOURCE_DIR}/data/cube_ascii.stl --lang en --size 900x600
              --ui-script ${CMAKE_CURRENT_SOURCE_DIR}/data/ui/top_bar_narrow.txt
              --screenshot ${CMAKE_CURRENT_BINARY_DIR}/ui_script_top_bar_narrow.png)
    set_tests_properties(ui_script_top_bar_narrow PROPERTIES ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0" TIMEOUT 300)
    add_test(NAME ui_script_config
      COMMAND ${TISMA_XVFB_RUN} -a -s "-screen 0 1280x800x24" $<TARGET_FILE:tisma>
              ${CMAKE_CURRENT_SOURCE_DIR}/data/cube_ascii.stl --lang es --size 1200x760
              --ui-script ${CMAKE_CURRENT_SOURCE_DIR}/data/ui/config.txt
              --screenshot ${CMAKE_CURRENT_BINARY_DIR}/ui_script_config.png)
    set_tests_properties(ui_script_config PROPERTIES ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0" TIMEOUT 300)
    add_test(NAME ui_script_panel_fold
      COMMAND ${TISMA_XVFB_RUN} -a -s "-screen 0 1280x800x24" $<TARGET_FILE:tisma>
              ${CMAKE_CURRENT_SOURCE_DIR}/data/cube_ascii.stl --lang es --size 1200x760 --tool move
              --ui-script ${CMAKE_CURRENT_SOURCE_DIR}/data/ui/panel_fold.txt
              --screenshot ${CMAKE_CURRENT_BINARY_DIR}/ui_script_panel_fold.png)
    set_tests_properties(ui_script_panel_fold PROPERTIES ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0" TIMEOUT 300)
    # The Design tab (cad_design.md, stage D1) driven through the stand-in
    # solidcraft-cli: the tab, the primitives, the command line, sending to the
    # plate and editing again; without the component (CD21); hidden (CD1).
    foreach(script aerodynamics design_tab design_parameters design_constraints design_operations design_sketch_edit design_sketch_fillet design_command design_hidden design_sketch fasteners gears structural_tab structural_analyze)
      add_test(NAME ui_script_${script}
        COMMAND ${TISMA_XVFB_RUN} -a -s "-screen 0 1280x800x24" $<TARGET_FILE:tisma>
                ${CMAKE_CURRENT_SOURCE_DIR}/data/cube_ascii.stl --lang es --size 1200x760
                --solidcraft $<TARGET_FILE:tisma_fake_solidcraft>
                --ui-script ${CMAKE_CURRENT_SOURCE_DIR}/data/ui/${script}.txt
                --screenshot ${CMAKE_CURRENT_BINARY_DIR}/ui_script_${script}.png)
      set_tests_properties(ui_script_${script} PROPERTIES ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0" TIMEOUT 300)
    endforeach()
    # The timeline (rule 8): the stand-in refuses the body «Caja 2».
    add_test(NAME ui_script_design_timeline
      COMMAND ${TISMA_XVFB_RUN} -a -s "-screen 0 1280x800x24" $<TARGET_FILE:tisma>
              ${CMAKE_CURRENT_SOURCE_DIR}/data/cube_ascii.stl --lang es --size 1200x760
              --solidcraft $<TARGET_FILE:tisma_fake_solidcraft>
              --ui-script ${CMAKE_CURRENT_SOURCE_DIR}/data/ui/design_timeline.txt
              --screenshot ${CMAKE_CURRENT_BINARY_DIR}/ui_script_design_timeline.png)
    set_tests_properties(ui_script_design_timeline PROPERTIES
      ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0;TISMA_FAKE_SOLIDCRAFT_REFUSE=Caja 2" TIMEOUT 300)
    add_test(NAME ui_script_design_missing
      COMMAND ${TISMA_XVFB_RUN} -a -s "-screen 0 1280x800x24" $<TARGET_FILE:tisma>
              ${CMAKE_CURRENT_SOURCE_DIR}/data/cube_ascii.stl --lang es --size 1200x760
              --solidcraft ${CMAKE_CURRENT_BINARY_DIR}/no-such-solidcraft-cli
              --ui-script ${CMAKE_CURRENT_SOURCE_DIR}/data/ui/design_missing.txt
              --screenshot ${CMAKE_CURRENT_BINARY_DIR}/ui_script_design_missing.png)
    set_tests_properties(ui_script_design_missing PROPERTIES ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0" TIMEOUT 300)
    # The plate's right-click menu adds a cube and a sphere as models.
    add_test(NAME ui_script_plate_menu
      COMMAND ${TISMA_XVFB_RUN} -a -s "-screen 0 1280x800x24" $<TARGET_FILE:tisma>
              ${CMAKE_CURRENT_SOURCE_DIR}/data/cube_ascii.stl --lang en --size 1200x760
              --ui-script ${CMAKE_CURRENT_SOURCE_DIR}/data/ui/plate_menu.txt
              --screenshot ${CMAKE_CURRENT_BINARY_DIR}/ui_script_plate_menu.png)
    set_tests_properties(ui_script_plate_menu PROPERTIES ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0" TIMEOUT 300)
    # A SIGTERM (a logout, a shutdown) ends the program as its window's close
    # button does: it exits with 0 and keeps the preferences (UI21).
    add_test(NAME ui_sigterm_keeps_preferences
      COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/ui_sigterm.sh ${CMAKE_CURRENT_BINARY_DIR}/ui_sigterm_home ${TISMA_XVFB_RUN}
              $<TARGET_FILE:tisma>)
    # And while it starts, before its window is up.
    add_test(NAME ui_sigterm_while_starting
      COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/ui_sigterm.sh ${CMAKE_CURRENT_BINARY_DIR}/ui_sigterm_start_home
              ${TISMA_XVFB_RUN} $<TARGET_FILE:tisma> start)
    set_tests_properties(ui_sigterm_keeps_preferences ui_sigterm_while_starting PROPERTIES
                         ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0" TIMEOUT 120)
    # The Slice button at the bottom right, then its save menu.
    # At 1440 x 900, where the move slider is not in the legend (UI23).
    add_test(NAME ui_script_slice_panel
      COMMAND ${TISMA_XVFB_RUN} -a -s "-screen 0 1600x1000x24" $<TARGET_FILE:tisma>
              ${CMAKE_CURRENT_SOURCE_DIR}/data/cube_ascii.stl --lang es --size 1440x900
              --ui-script ${CMAKE_CURRENT_SOURCE_DIR}/data/ui/slice_panel.txt
              --screenshot ${CMAKE_CURRENT_BINARY_DIR}/ui_script_slice_panel.png)
    set_tests_properties(ui_script_slice_panel PROPERTIES ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0" TIMEOUT 300)
    # The Move and Scale cards' sliders, clicked.
    foreach(tool move rotate scale)
      add_test(NAME ui_script_${tool}_sliders
        COMMAND ${TISMA_XVFB_RUN} -a -s "-screen 0 1280x800x24" $<TARGET_FILE:tisma>
                ${CMAKE_CURRENT_SOURCE_DIR}/data/cube_ascii.stl --lang es --size 1200x760 --tool ${tool}
                --ui-script ${CMAKE_CURRENT_SOURCE_DIR}/data/ui/${tool}_sliders.txt
                --screenshot ${CMAKE_CURRENT_BINARY_DIR}/ui_script_${tool}_sliders.png)
      set_tests_properties(ui_script_${tool}_sliders PROPERTIES ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0" TIMEOUT 300)
    endforeach()
    # The handles drawn on the model (move arrows, rotation rings, scale
    # boxes), dragged in the view.
    # The Tisma Cloud dialog against the mock server; the token goes to the
    # stand-in secret-tool, never to this machine's store.
    add_test(NAME ui_script_cloud
      COMMAND $<TARGET_FILE:tisma_mock_cloud> ${TISMA_XVFB_RUN} -a -s "-screen 0 1280x800x24" $<TARGET_FILE:tisma>
              ${CMAKE_CURRENT_SOURCE_DIR}/data/cube_ascii.stl --lang en --size 1200x760 --cloud-server {url}
              --ui-script ${CMAKE_CURRENT_SOURCE_DIR}/data/ui/cloud.txt
              --screenshot ${CMAKE_CURRENT_BINARY_DIR}/ui_script_cloud.png)
    set_tests_properties(ui_script_cloud PROPERTIES TIMEOUT 300 ENVIRONMENT
      "ASAN_OPTIONS=detect_leaks=0;TISMA_SECRET_TOOL=$<TARGET_FILE:tisma_fake_secret_tool>;TISMA_FAKE_SECRETS=${CMAKE_CURRENT_BINARY_DIR}/ui_cloud_secrets.txt")
    add_test(NAME ui_script_cloud_default
      COMMAND ${TISMA_XVFB_RUN} -a -s "-screen 0 1280x800x24" $<TARGET_FILE:tisma>
              ${CMAKE_CURRENT_SOURCE_DIR}/data/cube_ascii.stl --lang en --size 1200x760
              --ui-script ${CMAKE_CURRENT_SOURCE_DIR}/data/ui/cloud_default.txt
              --screenshot ${CMAKE_CURRENT_BINARY_DIR}/ui_script_cloud_default.png)
    set_tests_properties(ui_script_cloud_default PROPERTIES TIMEOUT 300 ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0")
    # A server that does not answer (port 9 of this machine, nothing listens there).
    add_test(NAME ui_script_cloud_unreachable
      COMMAND ${TISMA_XVFB_RUN} -a -s "-screen 0 1280x800x24" $<TARGET_FILE:tisma>
              ${CMAKE_CURRENT_SOURCE_DIR}/data/cube_ascii.stl --lang en --size 1200x760 --cloud-server http://127.0.0.1:9
              --ui-script ${CMAKE_CURRENT_SOURCE_DIR}/data/ui/cloud_unreachable.txt
              --screenshot ${CMAKE_CURRENT_BINARY_DIR}/ui_script_cloud_unreachable.png)
    set_tests_properties(ui_script_cloud_unreachable PROPERTIES TIMEOUT 300 ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0")
    add_test(NAME ui_script_cloud_slice
      COMMAND $<TARGET_FILE:tisma_mock_cloud> ${TISMA_XVFB_RUN} -a -s "-screen 0 1280x800x24" $<TARGET_FILE:tisma>
              ${CMAKE_CURRENT_SOURCE_DIR}/data/cube_ascii.stl --lang en --size 1200x760 --cloud-server {url}
              --ui-script ${CMAKE_CURRENT_SOURCE_DIR}/data/ui/cloud_slice.txt
              --screenshot ${CMAKE_CURRENT_BINARY_DIR}/ui_script_cloud_slice.png)
    set_tests_properties(ui_script_cloud_slice PROPERTIES TIMEOUT 300 ENVIRONMENT
      "ASAN_OPTIONS=detect_leaks=0;TISMA_SECRET_TOOL=$<TARGET_FILE:tisma_fake_secret_tool>;TISMA_FAKE_SECRETS=${CMAKE_CURRENT_BINARY_DIR}/ui_cloud_slice_secrets.txt")
    # Analysing in the cloud from Structural (engineering_panel.md, rule 10;
    # cloud.md, rules 6 to 10b): a fea job, its .tea kept with the part.
    add_test(NAME ui_script_cloud_structural
      COMMAND $<TARGET_FILE:tisma_mock_cloud> ${TISMA_XVFB_RUN} -a -s "-screen 0 1280x800x24" $<TARGET_FILE:tisma>
              ${CMAKE_CURRENT_SOURCE_DIR}/data/cube_ascii.stl --lang en --size 1200x760 --cloud-server {url}
              --ui-script ${CMAKE_CURRENT_SOURCE_DIR}/data/ui/cloud_structural.txt
              --screenshot ${CMAKE_CURRENT_BINARY_DIR}/ui_script_cloud_structural.png)
    set_tests_properties(ui_script_cloud_structural PROPERTIES TIMEOUT 300 ENVIRONMENT
      "ASAN_OPTIONS=detect_leaks=0;TISMA_SECRET_TOOL=$<TARGET_FILE:tisma_fake_secret_tool>;TISMA_FAKE_SECRETS=${CMAKE_CURRENT_BINARY_DIR}/ui_cloud_structural_secrets.txt")
    # After an unexpected closing (problem_reports.md, rule 10; IP9): a mark
    # of a process that is gone, planted in a configuration folder of its own.
    add_test(NAME ui_crash_report_setup
      COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/data/ui/crash_config
              ${CMAKE_CURRENT_BINARY_DIR}/ui_crash_config)
    set_tests_properties(ui_crash_report_setup PROPERTIES FIXTURES_SETUP ui_crash_config)
    add_test(NAME ui_script_crash_report
      COMMAND ${TISMA_XVFB_RUN} -a -s "-screen 0 1280x800x24" $<TARGET_FILE:tisma>
              ${CMAKE_CURRENT_SOURCE_DIR}/data/cube_ascii.stl --lang en --size 1200x760 --crash-check
              --ui-script ${CMAKE_CURRENT_SOURCE_DIR}/data/ui/crash_report.txt
              --screenshot ${CMAKE_CURRENT_BINARY_DIR}/ui_script_crash_report.png)
    set_tests_properties(ui_script_crash_report PROPERTIES TIMEOUT 300 FIXTURES_REQUIRED ui_crash_config
      RESOURCE_LOCK ui_crash_config
      ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0;XDG_CONFIG_HOME=${CMAKE_CURRENT_BINARY_DIR}/ui_crash_config")
    add_test(NAME ui_script_report
      COMMAND $<TARGET_FILE:tisma_mock_cloud> ${TISMA_XVFB_RUN} -a -s "-screen 0 1280x800x24" $<TARGET_FILE:tisma>
              ${CMAKE_CURRENT_SOURCE_DIR}/data/cube_ascii.stl --lang en --size 1200x760 --cloud-server {url}
              --ui-script ${CMAKE_CURRENT_SOURCE_DIR}/data/ui/report.txt
              --screenshot ${CMAKE_CURRENT_BINARY_DIR}/ui_script_report.png)
    set_tests_properties(ui_script_report PROPERTIES TIMEOUT 300 ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0")
    add_test(NAME ui_script_place_undo
      COMMAND ${TISMA_XVFB_RUN} -a -s "-screen 0 1280x800x24" $<TARGET_FILE:tisma>
              ${CMAKE_CURRENT_SOURCE_DIR}/data/cube_ascii.stl --lang es --size 1200x760 --tool move
              --ui-script ${CMAKE_CURRENT_SOURCE_DIR}/data/ui/place_undo.txt
              --screenshot ${CMAKE_CURRENT_BINARY_DIR}/ui_script_place_undo.png)
    set_tests_properties(ui_script_place_undo PROPERTIES ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0" TIMEOUT 300)
    foreach(tool move rotate scale)
      add_test(NAME ui_script_${tool}_gizmo
        COMMAND ${TISMA_XVFB_RUN} -a -s "-screen 0 1280x800x24" $<TARGET_FILE:tisma>
                ${CMAKE_CURRENT_SOURCE_DIR}/data/cube_ascii.stl --lang es --size 1200x760 --tool ${tool}
                --ui-script ${CMAKE_CURRENT_SOURCE_DIR}/data/ui/${tool}_gizmo.txt
                --screenshot ${CMAKE_CURRENT_BINARY_DIR}/ui_script_${tool}_gizmo.png)
      set_tests_properties(ui_script_${tool}_gizmo PROPERTIES ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0" TIMEOUT 300)
    endforeach()
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
    # The same project as a 3MF package (formats.md, rule 11): opened as a
    # project, not as a model, and sliced to the same G-code.
    add_test(NAME ui_project_3mf_save
      COMMAND ${TISMA_XVFB_RUN} -a -s "-screen 0 1280x800x24" $<TARGET_FILE:tisma>
              ${CMAKE_CURRENT_BINARY_DIR}/ui_project.tisma --lang en --size 900x600
              --save-project ${CMAKE_CURRENT_BINARY_DIR}/ui_project.3mf
              --screenshot ${CMAKE_CURRENT_BINARY_DIR}/ui_project_3mf_a.png)
    add_test(NAME ui_project_3mf_reopen
      COMMAND ${TISMA_XVFB_RUN} -a -s "-screen 0 1280x800x24" $<TARGET_FILE:tisma>
              ${CMAKE_CURRENT_BINARY_DIR}/ui_project.3mf --lang en --size 900x600
              --export ${CMAKE_CURRENT_BINARY_DIR}/ui_project_c.gcode
              --screenshot ${CMAKE_CURRENT_BINARY_DIR}/ui_project_3mf_b.png)
    add_test(NAME ui_project_3mf_same_gcode
      COMMAND ${CMAKE_COMMAND} -E compare_files ${CMAKE_CURRENT_BINARY_DIR}/ui_project_a.gcode
              ${CMAKE_CURRENT_BINARY_DIR}/ui_project_c.gcode)
    set_tests_properties(ui_project_3mf_save PROPERTIES FIXTURES_REQUIRED ui_project FIXTURES_SETUP ui_project_3mf
                         ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0" TIMEOUT 300)
    set_tests_properties(ui_project_3mf_reopen PROPERTIES FIXTURES_REQUIRED ui_project_3mf FIXTURES_SETUP ui_project_c
                         ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0" TIMEOUT 300)
    set_tests_properties(ui_project_3mf_same_gcode PROPERTIES FIXTURES_REQUIRED "ui_project;ui_project_c")
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
      # The moiré filter's check with Vulkan drawing the view.
      add_test(NAME ui_moire_filter_smooths_fine_lines_vulkan
        COMMAND ${TISMA_XVFB_RUN} -a -s "-screen 0 1280x800x24" $<TARGET_FILE:tisma>
                ${CMAKE_CURRENT_SOURCE_DIR}/data/cube_ascii.stl --lang en --size 1200x760 --view preview --moire-check
                --graphics vulkan --screenshot ${CMAKE_CURRENT_BINARY_DIR}/ui_moire_vulkan.png)
      set_tests_properties(ui_moire_filter_smooths_fine_lines_vulkan
                           PROPERTIES ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0" TIMEOUT 300)
      # The 3D view with Vulkan (V2) against OpenGL's, picture against picture:
      # the model with a painted seam, close; then the sliced cube's preview.
      # Then the preview from above, where the layers lie over each other
      # (drawn nearest first, 2026-10-28), and from far away, its lines a
      # pixel or less (the moiré filter).
      set(TISMA_VIEW_prepare --view prepare --tool paint --paint seam,enforce,133,131,5,2 --look 124,122,5,60,45,25)
      set(TISMA_VIEW_preview --view preview)
      set(TISMA_VIEW_preview_top --view preview --look 128,128,5,70,30,85)
      set(TISMA_VIEW_preview_far --view preview --look 128,128,5,600,30,35)
      # And each of the preview's colour views (preview.md, rule 1).
      set(TISMA_VIEWS prepare preview preview_top preview_far)
      foreach(colour height width speed actual_speed fan temperature flow actual_flow layer_time layer_time_log tool
                     color_print)
        set(TISMA_VIEW_preview_${colour} --view preview --color ${colour})
        list(APPEND TISMA_VIEWS preview_${colour})
      endforeach()
      # A curved part too (tools/make_test_sphere.py), close and from above.
      foreach(view prepare preview preview_top)
        set(TISMA_VIEW_sphere_${view} ${TISMA_VIEW_${view}})
        set(TISMA_MODEL_sphere_${view} sphere.stl)
        list(APPEND TISMA_VIEWS sphere_${view})
      endforeach()
      set(TISMA_VIEW_sphere_prepare --view prepare --look 128,128,10,60,30,35)
      foreach(view ${TISMA_VIEWS})
        if(NOT TISMA_MODEL_${view})
          set(TISMA_MODEL_${view} cube_ascii.stl)
        endif()
        add_test(NAME ui_view_${view}_opengl
          COMMAND ${TISMA_XVFB_RUN} -a -s "-screen 0 1280x800x24" $<TARGET_FILE:tisma>
                  ${CMAKE_CURRENT_SOURCE_DIR}/data/${TISMA_MODEL_${view}} --lang en --size 900x600
                  ${TISMA_VIEW_${view}} --render fast --graphics opengl
                  --screenshot ${CMAKE_CURRENT_BINARY_DIR}/ui_view_${view}_opengl.png)
        add_test(NAME ui_view_${view}_vulkan_same_as_opengl
          COMMAND ${TISMA_XVFB_RUN} -a -s "-screen 0 1280x800x24" $<TARGET_FILE:tisma>
                  ${CMAKE_CURRENT_SOURCE_DIR}/data/${TISMA_MODEL_${view}} --lang en --size 900x600
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
    # The settings panel (settings_panel.md): the same G-code in Basic and in
    # Custom (SP9), the search going to a setting (SP6) with all the sections
    # together (SP6b), and the dialog that chooses what Custom shows.
    add_test(NAME ui_settings_basic
      COMMAND ${TISMA_XVFB_RUN} -a -s "-screen 0 1280x800x24" $<TARGET_FILE:tisma>
              ${CMAKE_CURRENT_SOURCE_DIR}/data/cube_ascii.stl --lang en --size 900x600 --level basic
              --export ${CMAKE_CURRENT_BINARY_DIR}/ui_level_basic.gcode
              --screenshot ${CMAKE_CURRENT_BINARY_DIR}/ui_level_basic.png)
    add_test(NAME ui_settings_custom_search
      COMMAND ${TISMA_XVFB_RUN} -a -s "-screen 0 1280x800x24" $<TARGET_FILE:tisma>
              ${CMAKE_CURRENT_SOURCE_DIR}/data/cube_ascii.stl --lang en --size 900x600 --level custom
              --search seam --single-column --choose-settings
              --export ${CMAKE_CURRENT_BINARY_DIR}/ui_level_custom.gcode
              --screenshot ${CMAKE_CURRENT_BINARY_DIR}/ui_level_custom.png)
    add_test(NAME ui_settings_same_gcode
      COMMAND ${CMAKE_COMMAND} -E compare_files ${CMAKE_CURRENT_BINARY_DIR}/ui_level_basic.gcode
              ${CMAKE_CURRENT_BINARY_DIR}/ui_level_custom.gcode)
    # The printer's card with a catalog printer of several nozzles and beds
    # (SP22), and the printer's page on its calibrations (SP28, SP29).
    add_test(NAME ui_printer_card
      COMMAND ${TISMA_XVFB_RUN} -a -s "-screen 0 1280x800x24" $<TARGET_FILE:tisma>
              ${CMAKE_CURRENT_SOURCE_DIR}/data/cube_ascii.stl --lang en --size 900x600
              "--profile" "printer=Raise3D E3 0.4 mm" --screenshot ${CMAKE_CURRENT_BINARY_DIR}/ui_printer_card.png)
    add_test(NAME ui_printer_page
      COMMAND ${TISMA_XVFB_RUN} -a -s "-screen 0 1280x800x24" $<TARGET_FILE:tisma>
              ${CMAKE_CURRENT_SOURCE_DIR}/data/cube_ascii.stl --lang en --size 900x600 --section printer/calibration
              --screenshot ${CMAKE_CURRENT_BINARY_DIR}/ui_printer_page.png)
    # A cube cut at 4 mm in the interface: two pieces, sliced and exported
    # with their G-code checked (model_tools.md, rule 13).
    add_test(NAME ui_cut
      COMMAND ${TISMA_XVFB_RUN} -a -s "-screen 0 1280x800x24" $<TARGET_FILE:tisma>
              ${CMAKE_CURRENT_SOURCE_DIR}/data/cube_ascii.stl --lang en --size 900x600 --cut 4
              --export ${CMAKE_CURRENT_BINARY_DIR}/ui_cut.gcode --screenshot ${CMAKE_CURRENT_BINARY_DIR}/ui_cut.png)
    # The help with the engine's own test data: a problem with its actions
    # (assistant.md, rules 5 to 8), and the ABS card with its warnings (AS9).
    add_test(NAME ui_help_problem
      COMMAND ${TISMA_XVFB_RUN} -a -s "-screen 0 1280x800x24" $<TARGET_FILE:tisma>
              ${CMAKE_CURRENT_SOURCE_DIR}/data/cube_ascii.stl --lang en --size 1280x720
              --knowledge ${CMAKE_CURRENT_SOURCE_DIR}/data/knowledge --help-panel problems --help-item stringing
              --screenshot ${CMAKE_CURRENT_BINARY_DIR}/ui_help_problem.png)
    add_test(NAME ui_material_card
      COMMAND ${TISMA_XVFB_RUN} -a -s "-screen 0 1280x800x24" $<TARGET_FILE:tisma>
              ${CMAKE_CURRENT_SOURCE_DIR}/data/cube_ascii.stl --lang en --size 1280x720
              --knowledge ${CMAKE_CURRENT_SOURCE_DIR}/data/knowledge --profile filament=ABS --section material/temperature
              --screenshot ${CMAKE_CURRENT_BINARY_DIR}/ui_material_card.png)
    # A guided tutorial of the test data on its first step (assistant.md, AS12).
    add_test(NAME ui_tutorial
      COMMAND ${TISMA_XVFB_RUN} -a -s "-screen 0 1280x800x24" $<TARGET_FILE:tisma>
              ${CMAKE_CURRENT_SOURCE_DIR}/data/cube_ascii.stl --lang en --size 1280x720
              --knowledge ${CMAKE_CURRENT_SOURCE_DIR}/data/knowledge --tutorial first_print,1
              --screenshot ${CMAKE_CURRENT_BINARY_DIR}/ui_tutorial.png)
    # A calibration guide with a guide before it not done (assistant.md, AS16, AS17).
    add_test(NAME ui_calibration_guide
      COMMAND ${TISMA_XVFB_RUN} -a -s "-screen 0 1280x800x24" $<TARGET_FILE:tisma>
              ${CMAKE_CURRENT_SOURCE_DIR}/data/cube_ascii.stl --lang en --size 1280x720
              --knowledge ${CMAKE_CURRENT_SOURCE_DIR}/data/knowledge --help-panel calibrations --help-item max_flow
              --screenshot ${CMAKE_CURRENT_BINARY_DIR}/ui_calibration_guide.png)
    set_tests_properties(ui_help_problem ui_material_card ui_tutorial ui_calibration_guide PROPERTIES ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0" TIMEOUT 300)
    # Measure between the cube's bottom and top (model_tools.md, rule 20).
    add_test(NAME ui_measure
      COMMAND ${TISMA_XVFB_RUN} -a -s "-screen 0 1280x800x24" $<TARGET_FILE:tisma>
              ${CMAKE_CURRENT_SOURCE_DIR}/data/cube_ascii.stl --lang en --size 900x600 --tool measure --measure 0,2
              --screenshot ${CMAKE_CURRENT_BINARY_DIR}/ui_measure.png)
    # An object's own settings in their window (settings_levels.md, rule 11).
    add_test(NAME ui_object_settings
      COMMAND ${TISMA_XVFB_RUN} -a -s "-screen 0 1280x800x24" $<TARGET_FILE:tisma>
              ${CMAKE_CURRENT_SOURCE_DIR}/data/cube_ascii.stl --lang en --size 1280x720
              --object-settings "{\"infill\": {\"density\": 40, \"pattern\": \"grid\"}, \"walls\": {\"count\": 3}}"
              --screenshot ${CMAKE_CURRENT_BINARY_DIR}/ui_object_settings.png)
    # A modifier, a negative volume and a height range of the cube (model_tools.md, rules 2, 3 and 6).
    add_test(NAME ui_object_volumes
      COMMAND ${TISMA_XVFB_RUN} -a -s "-screen 0 1280x800x24" $<TARGET_FILE:tisma>
              ${CMAKE_CURRENT_SOURCE_DIR}/data/cube_ascii.stl --lang en --size 1280x720 --object-add modifier,negative,range
              --screenshot ${CMAKE_CURRENT_BINARY_DIR}/ui_object_volumes.png)
    # The same turned 45° about Z: the modifier and the negative turn with it.
    add_test(NAME ui_object_volumes_turned
      COMMAND ${TISMA_XVFB_RUN} -a -s "-screen 0 1280x800x24" $<TARGET_FILE:tisma>
              ${CMAKE_CURRENT_SOURCE_DIR}/data/cube_ascii.stl --lang en --size 1280x720 --object-add modifier,negative
              --object-turn 0,0,45 --screenshot ${CMAKE_CURRENT_BINARY_DIR}/ui_object_volumes_turned.png)
    # The Objects tab (settings_levels.md, rules 11 and 13): the cube with its
    # own infill, a modifier, a negative volume and a range, its Infill row chosen.
    add_test(NAME ui_objects_list
      COMMAND ${TISMA_XVFB_RUN} -a -s "-screen 0 1280x800x24" $<TARGET_FILE:tisma>
              ${CMAKE_CURRENT_SOURCE_DIR}/data/cube_ascii.stl --lang en --size 1280x720
              --object-settings "{\"infill\": {\"density\": 15, \"pattern\": \"grid\"}}" --object-add modifier,negative,range
              --section objects/infill --screenshot ${CMAKE_CURRENT_BINARY_DIR}/ui_objects_list.png)
    set_tests_properties(ui_cut ui_measure ui_object_settings ui_object_volumes ui_object_volumes_turned ui_objects_list PROPERTIES ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0" TIMEOUT 300)
    # More › Credits and sources, in English (credits.md, CR2 and CR4).
    add_test(NAME ui_credits
      COMMAND ${TISMA_XVFB_RUN} -a -s "-screen 0 1280x800x24" $<TARGET_FILE:tisma>
              ${CMAKE_CURRENT_SOURCE_DIR}/data/cube_ascii.stl --lang en --size 900x600 --credits
              --screenshot ${CMAKE_CURRENT_BINARY_DIR}/ui_credits.png)
    # The help panel without the pack's data says it comes with the profiles
    # (assistant.md, AS1), with an empty data folder of its own.
    add_test(NAME ui_help_panel
      COMMAND ${TISMA_XVFB_RUN} -a -s "-screen 0 1280x800x24" $<TARGET_FILE:tisma>
              ${CMAKE_CURRENT_SOURCE_DIR}/data/cube_ascii.stl --lang en --size 1280x720 --help-panel problems
              --screenshot ${CMAKE_CURRENT_BINARY_DIR}/ui_help_panel.png)
    set_tests_properties(ui_help_panel PROPERTIES ENVIRONMENT
                         "ASAN_OPTIONS=detect_leaks=0;XDG_CONFIG_HOME=${CMAKE_CURRENT_BINARY_DIR}/ui_help_config" TIMEOUT 300)
    set_tests_properties(ui_printer_card ui_printer_page ui_credits PROPERTIES ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0"
                         TIMEOUT 300)
    set_tests_properties(ui_settings_basic PROPERTIES FIXTURES_SETUP ui_level_basic)
    set_tests_properties(ui_settings_custom_search PROPERTIES FIXTURES_SETUP ui_level_custom)
    set_tests_properties(ui_settings_same_gcode PROPERTIES FIXTURES_REQUIRED "ui_level_basic;ui_level_custom")
    set_tests_properties(ui_settings_basic ui_settings_custom_search PROPERTIES ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0"
                         TIMEOUT 300)
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
  # Every number of the settings panel reaches its whole range in docs/CONFIG.md.
  add_test(NAME ui_limits_check COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/check_ui_limits.py ${PROJECT_SOURCE_DIR})
  # A large model (239 200 triangles): its box tree is made on another
  # thread right after the plate, and the first click on it finds it made
  # (docs/LARGE_MODELS.md).
  if(TARGET tisma AND TISMA_XVFB_RUN)
    add_test(NAME ui_big_sphere_make
      COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/make_test_sphere.py
              ${CMAKE_CURRENT_BINARY_DIR}/big_sphere.stl 400 300)
    set_tests_properties(ui_big_sphere_make PROPERTIES FIXTURES_SETUP big_sphere)
    add_test(NAME ui_script_click_large_model
      COMMAND ${TISMA_XVFB_RUN} -a -s "-screen 0 1280x800x24" $<TARGET_FILE:tisma>
              ${CMAKE_CURRENT_BINARY_DIR}/big_sphere.stl --lang en --size 1200x760
              --ui-script ${CMAKE_CURRENT_SOURCE_DIR}/data/ui/click_model.txt
              --screenshot ${CMAKE_CURRENT_BINARY_DIR}/ui_script_click_large_model.png)
    set_tests_properties(ui_script_click_large_model PROPERTIES FIXTURES_REQUIRED big_sphere
      ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0" TIMEOUT 300 PASS_REGULAR_EXPRESSION "the box tree was made ahead"
      FAIL_REGULAR_EXPRESSION "error: ")
  endif()
endif()
