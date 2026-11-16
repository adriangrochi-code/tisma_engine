# The release gate (docs/spec/RELEASE_GATE.md, the owner's decision of
# 2026-11-16): each critical function from end to end through the interface,
# checked by its result. They carry the label "gate"; the CI job «Puerta de
# lanzamiento» runs them on Linux (under Xvfb) and Windows, and nothing is
# published unless they all pass. None is skipped or relaxed: a function
# that changes on purpose changes the spec first.

# The unpacker the Benchy needs everywhere (no xz program on Windows).
add_executable(tisma_unxz unxz_main.cpp)
target_link_libraries(tisma_unxz PRIVATE tisma_core tisma_warnings)

if(NOT TISMA_BUILD_UI OR NOT TARGET tisma)
  return()
endif()
if(WIN32 OR APPLE)
  # Windows and macOS draw on the runner's own display (gate.yml, gate_macos.yml).
  set(TISMA_GATE_RUN "")
elseif(TISMA_XVFB_RUN)
  set(TISMA_GATE_RUN ${TISMA_XVFB_RUN} -a -s "-screen 0 1280x800x24")
else()
  return()
endif()

set(TISMA_GATE_DATA ${CMAKE_CURRENT_SOURCE_DIR}/data)
set(TISMA_GATE_OUT ${CMAKE_CURRENT_BINARY_DIR}/gate)
file(MAKE_DIRECTORY ${TISMA_GATE_OUT})

# tisma_gate_test(NAME <name> MODEL <file> SCRIPT <script> [LANG <lang>] [ARGS ...] [PROPERTIES ...])
function(tisma_gate_test)
  cmake_parse_arguments(G "" "NAME;MODEL;SCRIPT;LANG" "ARGS;PROPERTIES" ${ARGN})
  if(NOT G_LANG)
    set(G_LANG en)
  endif()
  add_test(NAME ${G_NAME}
    COMMAND ${TISMA_GATE_RUN} $<TARGET_FILE:tisma> ${G_MODEL} --lang ${G_LANG} --size 1200x760 ${G_ARGS}
            --ui-script ${TISMA_GATE_DATA}/ui/${G_SCRIPT}.txt --screenshot ${TISMA_GATE_OUT}/${G_NAME}.png)
  set_tests_properties(${G_NAME} PROPERTIES LABELS gate FAIL_REGULAR_EXPRESSION "error: "
                       ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0" TIMEOUT 900 ${G_PROPERTIES})
endfunction()

# The Benchy (bench/3DBenchy.stl.xz, CC0), unpacked once.
set(TISMA_GATE_BENCHY ${TISMA_GATE_OUT}/3DBenchy.stl)
add_test(NAME gate_benchy_unpack COMMAND tisma_unxz ${PROJECT_SOURCE_DIR}/bench/3DBenchy.stl.xz ${TISMA_GATE_BENCHY})
set_tests_properties(gate_benchy_unpack PROPERTIES LABELS gate FIXTURES_SETUP gate_benchy)

# G2, slice: the 20 mm cube and the Benchy, checked G-code of their size.
tisma_gate_test(NAME gate_g2_cube MODEL ${TISMA_GATE_DATA}/cube20.stl SCRIPT gate_g2_cube)
tisma_gate_test(NAME gate_g2_benchy MODEL ${TISMA_GATE_BENCHY} SCRIPT gate_g2_benchy
                PROPERTIES FIXTURES_REQUIRED gate_benchy)

# G3, automatic supports: each style on the T, a ramp leaning 60 degrees
# from the vertical and the Benchy; support under every overhang, less than
# 1 % of it floating.
foreach(style grid organic snug)
  tisma_gate_test(NAME gate_g3_tee_${style} MODEL ${TISMA_GATE_DATA}/tee.stl SCRIPT gate_g3_${style})
  tisma_gate_test(NAME gate_g3_ramp_${style} MODEL ${TISMA_GATE_DATA}/ramp60.stl SCRIPT gate_g3_${style})
  tisma_gate_test(NAME gate_g3_benchy_${style} MODEL ${TISMA_GATE_BENCHY} SCRIPT gate_g3_${style}
                  PROPERTIES FIXTURES_REQUIRED gate_benchy)
endforeach()
# Supports reach the engine from the panel, in each style (the tester's
# report, 2026-11-16): none with them off, then ;TYPE:Support in each style.
tisma_gate_test(NAME ui_supports_slice MODEL ${TISMA_GATE_DATA}/tee.stl SCRIPT supports_slice)

# G4, painted supports (QA, 2026-11-16): forced with the supports off turns
# them on «only painted»; erased, none; blocked under every overhang with
# the automatic ones on, none. The T's overhangs are at z = 20, from x =
# 108 to 123 and 133 to 148 on the bed.
set(TISMA_PAINT_on --paint support,enforce,115,128,20,4)
set(TISMA_PAINT_SCRIPT_on paint_support_on)
set(TISMA_PAINT_none_erased --paint support,enforce,115,128,20,4 --paint support,erase,115,128,20,6)
set(TISMA_PAINT_SCRIPT_none_erased paint_support_none)
set(TISMA_PAINT_none_blocked --paint support,block,115,128,20,14 --paint support,block,141,128,20,14)
set(TISMA_PAINT_SCRIPT_none_blocked paint_support_blocked)
foreach(case on none_erased none_blocked)
  tisma_gate_test(NAME ui_paint_support_${case} MODEL ${TISMA_GATE_DATA}/tee.stl SCRIPT ${TISMA_PAINT_SCRIPT_${case}}
                  ARGS ${TISMA_PAINT_${case}})
endforeach()

# G5 and G6, and the question before slicing overhangs with the supports off
# (the owner, 2026-11-16): support in the preview and its legend; the
# supports never go off without a notice.
foreach(case prompt:tee without:tee cube:cube_ascii support_preview:tee supports_off_notice:tee)
  string(REPLACE ":" ";" parts ${case})
  list(GET parts 0 name)
  list(GET parts 1 model)
  set(script overhang_${name})
  if(name STREQUAL "support_preview" OR name STREQUAL "supports_off_notice")
    set(script ${name})
  endif()
  tisma_gate_test(NAME ui_overhang_${name} MODEL ${TISMA_GATE_DATA}/${model}.stl SCRIPT ${script} LANG es)
endforeach()

# G6: a PrusaSlicer project with support_material = 1 opens with the supports
# on (the pack's translation table: support_material → support.enabled), and
# they stay on with our printer instead of the project's.
add_test(NAME gate_pack_install
  COMMAND $<TARGET_FILE:tisma-engine> pack install --data ${TISMA_GATE_OUT}/pack --from ${TISMA_GATE_DATA}/pack/release1)
set_tests_properties(gate_pack_install PROPERTIES LABELS gate FIXTURES_SETUP gate_pack)
tisma_gate_test(NAME gate_g6_project_support MODEL ${TISMA_GATE_DATA}/formats/pi4_prusaslicer_support.3mf
                SCRIPT gate_g6_project_support LANG es ARGS --light --pack ${TISMA_GATE_OUT}/pack/pack
                PROPERTIES FIXTURES_REQUIRED gate_pack)
# G6: the supports through a change of printer, filament and print profile,
# and undo: they never go off without the notice.
tisma_gate_test(NAME gate_g6_changes MODEL ${TISMA_GATE_DATA}/tee.stl SCRIPT gate_g6_changes LANG es)

# G1, open: an STL, an OBJ, our own 3MF and another program's 3MF are on the
# plate at their size.
tisma_gate_test(NAME gate_g1_stl MODEL ${TISMA_GATE_DATA}/cube20.stl SCRIPT gate_g1_stl)
tisma_gate_test(NAME gate_g1_obj MODEL ${TISMA_GATE_DATA}/formats/box.obj SCRIPT gate_g1_obj)
tisma_gate_test(NAME gate_g1_own_3mf MODEL ${TISMA_GATE_DATA}/formats/box.obj SCRIPT gate_g1_own_3mf)
set_tests_properties(gate_g1_own_3mf PROPERTIES WORKING_DIRECTORY ${TISMA_GATE_OUT})
tisma_gate_test(NAME gate_g1_other_3mf MODEL ${TISMA_GATE_DATA}/formats/pi2_prusaslicer.3mf SCRIPT gate_g1_other_3mf)

# G7, adhesion: skirt, brim and raft in the first layer at their size.
foreach(kind skirt brim raft)
  tisma_gate_test(NAME gate_g7_${kind} MODEL ${TISMA_GATE_DATA}/cube20.stl SCRIPT gate_g7_${kind})
endforeach()

# G8, infill and walls: density 0, 15 and 100 %, and 1 to 4 walls, in the
# G-code.
foreach(case density_0 density_15 density_100 walls_1 walls_2 walls_3 walls_4)
  tisma_gate_test(NAME gate_g8_${case} MODEL ${TISMA_GATE_DATA}/cube20.stl SCRIPT gate_g8_${case})
endforeach()

# G9, save and open a project: painted support, a modifier and the object's
# own settings come back, and the G-code is the same.
tisma_gate_test(NAME gate_g9_project MODEL ${TISMA_GATE_DATA}/tee.stl SCRIPT gate_g9_project
                ARGS --paint support,enforce,115,128,20,4 --object-add modifier
                     --object-settings "{\"infill\": {\"density\": 40, \"pattern\": \"grid\"}, \"walls\": {\"count\": 2}}"
                PROPERTIES WORKING_DIRECTORY ${TISMA_GATE_OUT})

# G10, export: a partial settings file opened with the model keeps the
# printer's start, heating and bed (QA, 2026-11-16).
tisma_gate_test(NAME gate_g10_partial_settings MODEL ${TISMA_GATE_DATA}/tee.stl SCRIPT gate_g10_partial_settings
                ARGS ${TISMA_GATE_DATA}/support_only.json)

# G4, part of an overhang blocked (QA, 2026-11-16): nothing under the
# blocked circle, and less support around it than without the paint.
tisma_gate_test(NAME gate_g4_auto MODEL ${TISMA_GATE_DATA}/tee.stl SCRIPT gate_g4_auto
                PROPERTIES WORKING_DIRECTORY ${TISMA_GATE_OUT} FIXTURES_SETUP gate_g4_auto)
tisma_gate_test(NAME gate_g4_block_part MODEL ${TISMA_GATE_DATA}/tee.stl SCRIPT gate_g4_block_part
                ARGS --paint support,block,115,128,20,4
                PROPERTIES WORKING_DIRECTORY ${TISMA_GATE_OUT} FIXTURES_REQUIRED gate_g4_auto)

# G13, engineering: Structural and Aerodynamics on the cube and the Benchy;
# each gives a result with a value on every face.
foreach(kind structural aero)
  tisma_gate_test(NAME gate_g13_${kind}_cube MODEL ${TISMA_GATE_DATA}/cube20.stl SCRIPT gate_g13_${kind} LANG es
                  PROPERTIES RESOURCE_LOCK all_cores TIMEOUT 1800)
  tisma_gate_test(NAME gate_g13_${kind}_benchy MODEL ${TISMA_GATE_BENCHY} SCRIPT gate_g13_${kind} LANG es
                  PROPERTIES FIXTURES_REQUIRED gate_benchy RESOURCE_LOCK all_cores TIMEOUT 1800)
endforeach()

# G14, profiles: five printers of the published pack (Prusa, Bambu,
# Creality, a Klipper Voron and a Marlin Anycubic), each picked as the
# interface does it, slice to checked G-code headed by its start G-code. The
# pack is downloaded into the build folder, only these vendors; each test has
# a home of its own for the profiles it adds.
add_test(NAME gate_g14_pack_download
  COMMAND ${CMAKE_COMMAND} -DENGINE=$<TARGET_FILE:tisma-engine> -DDATA=${TISMA_GATE_OUT}/real_pack
          -DVENDORS=PrusaResearch,Orca_BBL,Creality,Voron,Anycubic -P ${CMAKE_CURRENT_SOURCE_DIR}/gate_pack.cmake)
set_tests_properties(gate_g14_pack_download PROPERTIES LABELS gate FIXTURES_SETUP gate_real_pack TIMEOUT 600
                     FAIL_REGULAR_EXPRESSION "error: ")
foreach(case prusa:PrusaResearch/models/Original_Prusa_MK4.json
             bambu:Orca_BBL/models/Bambu_Lab_X1.json
             creality:Creality/models/Creality_Ender-3_V2.json
             klipper:Voron/models/VORON2_250mm.json
             marlin:Anycubic/models/Anycubic_i3_Mega.json)
  string(REPLACE ":" ";" parts ${case})
  list(GET parts 0 name)
  list(GET parts 1 model)
  set(home ${TISMA_GATE_OUT}/home_g14_${name})
  file(REMOVE_RECURSE ${home})
  file(MAKE_DIRECTORY ${home})
  tisma_gate_test(NAME gate_g14_${name} MODEL ${TISMA_GATE_DATA}/cube20.stl SCRIPT gate_g14_printer
                  ARGS --pack ${TISMA_GATE_OUT}/real_pack/pack --pick-pack-printer "vendors/${model}#0.4"
                  PROPERTIES FIXTURES_REQUIRED gate_real_pack)
  set_tests_properties(gate_g14_${name} PROPERTIES
                       ENVIRONMENT "HOME=${home};XDG_CONFIG_HOME=;APPDATA=${home};ASAN_OPTIONS=detect_leaks=0")
endforeach()
