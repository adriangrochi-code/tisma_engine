# The interface, tisma: GLFW for the window, Dear ImGui for the panels and
# OpenGL 3.3 for the 3D view, or Vulkan for the window as it moves there
# (THIRD_PARTY.md, PLAN.md «Vista con Vulkan»). The logic it can share without
# OpenGL lives in tisma_ui (src/ui/) and is tested with the engine.

# Dear ImGui with its GLFW and OpenGL 3 backends; the OpenGL one is built on
# the program's own loader (imgui_opengl3.cpp).
add_library(tisma_imgui STATIC
  ${imgui_SOURCE_DIR}/imgui.cpp
  ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp
  ${imgui_SOURCE_DIR}/imgui_widgets.cpp
  ${imgui_SOURCE_DIR}/misc/cpp/imgui_stdlib.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
  imgui_opengl3.cpp
)
target_include_directories(tisma_imgui SYSTEM PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
target_include_directories(tisma_imgui PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(tisma_imgui PUBLIC IMGUI_DISABLE_OBSOLETE_FUNCTIONS GLFW_INCLUDE_NONE)
target_link_libraries(tisma_imgui PUBLIC glfw)
# With Vulkan (TISMA_GPU), also its Vulkan backend, on volk like the engine's
# compute (PLAN.md, «Vista con Vulkan»).
if(TISMA_GPU)
  target_sources(tisma_imgui PRIVATE ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp)
  target_compile_definitions(tisma_imgui PUBLIC IMGUI_IMPL_VULKAN_USE_VOLK TISMA_VULKAN_WINDOW)
  target_link_libraries(tisma_imgui PUBLIC volk::volk)
endif()

# The icons, the font and the credits travel inside the program.
set(TISMA_EMBEDDED
  ${PROJECT_SOURCE_DIR}/assets/brand/app/tisma.svg
  ${PROJECT_SOURCE_DIR}/docs/CONFIG.md
  ${imgui_SOURCE_DIR}/misc/fonts/Roboto-Medium.ttf
  ${PROJECT_SOURCE_DIR}/resources/credits.json
)
# The credits and the license notices the libraries ask to hand out with the
# program (docs/spec/features/credits.md, rule 4).
file(GLOB TISMA_LICENSES ${PROJECT_SOURCE_DIR}/resources/licenses/*.txt)
list(SORT TISMA_LICENSES)
list(APPEND TISMA_EMBEDDED ${TISMA_LICENSES})
file(GLOB TISMA_ICONS ${PROJECT_SOURCE_DIR}/assets/brand/icons/*.svg ${PROJECT_SOURCE_DIR}/assets/icons/*.svg)
list(SORT TISMA_ICONS)
list(APPEND TISMA_EMBEDDED ${TISMA_ICONS})
add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/embedded.cpp
  COMMAND ${CMAKE_COMMAND} "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/embedded.cpp" "-DFILES=${TISMA_EMBEDDED}"
          -P ${PROJECT_SOURCE_DIR}/cmake/embed.cmake
  DEPENDS ${TISMA_EMBEDDED} ${PROJECT_SOURCE_DIR}/cmake/embed.cmake
  COMMENT "Embedding the interface's icons and font"
  VERBATIM
)

add_executable(tisma WIN32
  main.cpp
  app.cpp
  file_browser.cpp
  illustrations.cpp
  gl.cpp
  renderer.cpp
  vk_view.cpp
  vk_window.cpp
  ${CMAKE_CURRENT_BINARY_DIR}/embedded.cpp
)
if(WIN32)
  configure_file(tisma.rc.in ${CMAKE_CURRENT_BINARY_DIR}/tisma.rc @ONLY)
  target_sources(tisma PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/tisma.rc)
endif()

target_include_directories(tisma PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(tisma PRIVATE tisma_ui tisma_net tisma_bambu tisma_imgui nlohmann_json::nlohmann_json tisma_warnings)
if(WIN32)
  target_link_libraries(tisma PRIVATE comdlg32) # the system's Open and Save As dialogs (BH40)
endif()
if(APPLE)
  # The system's Open and Save dialogs (AppKit), as on Windows; Objective-C++
  # is enabled at the top (its standard follows CXX_STANDARD).
  target_sources(tisma PRIVATE file_dialog_macos.mm)
  target_link_libraries(tisma PRIVATE "-framework AppKit" "-framework UniformTypeIdentifiers")
endif()
if(MSVC)
  # A windowed program that still starts at main().
  target_link_options(tisma PRIVATE /ENTRY:mainCRTStartup)
endif()

# The design component (cad_design.md, rule 1; THIRD_PARTY.md, SolidCraft):
# the packages carry solidcraft-cli at the pinned version; a build of one's
# own downloads it with TISMA_SOLIDCRAFT=ON, checks it and puts it next to
# the program. Off by default: the tests use their own stand-in.
option(TISMA_SOLIDCRAFT "Download the pinned solidcraft-cli next to the program for the Design tab" OFF)
if(TISMA_SOLIDCRAFT)
  set(sc_version 0.30.0)
  set(sc_dir ${CMAKE_BINARY_DIR}/_deps/solidcraft-${sc_version})
  if(WIN32)
    set(sc_asset solidcraft-${sc_version}-windows-x64-portable.zip)
    set(sc_sha ae5f502bc0062656965bff31f50739504e24c219baf520526fbd9208dfc261ba)
  elseif(UNIX AND NOT APPLE)
    set(sc_asset solidcraft-${sc_version}-linux-x86_64.tar.gz)
    set(sc_sha 5cd95727352b9a343fc8ff4bfb688671a5901a1b601a71c46e94c7b20750c9a0)
  else()
    message(WARNING "TISMA_SOLIDCRAFT: no solidcraft-cli ${sc_version} for this system; the Design tab says so")
  endif()
  if(sc_asset)
    if(NOT EXISTS ${sc_dir}/extracted)
      file(DOWNLOAD https://github.com/tangency-labs/solidcraft/releases/download/v${sc_version}/${sc_asset}
           ${sc_dir}/${sc_asset} EXPECTED_HASH SHA256=${sc_sha} STATUS sc_status)
      list(GET sc_status 0 sc_code)
      if(sc_code EQUAL 0)
        file(ARCHIVE_EXTRACT INPUT ${sc_dir}/${sc_asset} DESTINATION ${sc_dir}/extracted)
      else()
        message(WARNING "TISMA_SOLIDCRAFT: could not download ${sc_asset}: ${sc_status}")
      endif()
    endif()
    file(GLOB_RECURSE sc_files ${sc_dir}/extracted/*solidcraft-cli* ${sc_dir}/extracted/*LICENSE-MIT
         ${sc_dir}/extracted/*LICENSE-APACHE ${sc_dir}/extracted/*NOTICE ${sc_dir}/extracted/*THIRD-PARTY-LICENSES.txt)
    if(sc_files)
      add_custom_command(TARGET tisma POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:tisma>/solidcraft
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${sc_files} $<TARGET_FILE_DIR:tisma>/solidcraft/
        COMMENT "solidcraft-cli ${sc_version} next to the program")
    endif()
  endif()
endif()
