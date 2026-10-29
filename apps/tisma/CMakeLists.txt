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
target_link_libraries(tisma PRIVATE tisma_ui tisma_imgui nlohmann_json::nlohmann_json tisma_warnings)
if(MSVC)
  # A windowed program that still starts at main().
  target_link_options(tisma PRIVATE /ENTRY:mainCRTStartup)
endif()
