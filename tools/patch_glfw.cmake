# Patches GLFW 3.4's sources after FetchContent fetches them (Zlib license:
# altered source, marked as such in the file). Idempotent.
#
# macOS: with the environment variable TISMA_SOFTWARE_GL set, the OpenGL
# context asks for Apple's software renderer instead of an accelerated one,
# so the interface's tests open their window on CI runners without a GPU
# (QA, 2026-11-16). Without the variable nothing changes.
set(file "${SOURCE_DIR}/src/nsgl_context.m")
file(READ "${file}" text)
if(text MATCHES "TISMA_SOFTWARE_GL")
  return()
endif()
set(old "    ADD_ATTRIB(NSOpenGLPFAAccelerated);\n")
string(FIND "${text}" "${old}" at)
if(at EQUAL -1)
  message(FATAL_ERROR "patch_glfw: the line to change is not in ${file}")
endif()
set(new "    // Altered for Tisma: with TISMA_SOFTWARE_GL set, Apple's software\n    // renderer instead of an accelerated one (test runners without a GPU).\n    if (getenv(\"TISMA_SOFTWARE_GL\"))\n        SET_ATTRIB(NSOpenGLPFARendererID, kCGLRendererGenericFloatID)\n    else\n        ADD_ATTRIB(NSOpenGLPFAAccelerated);\n")
string(REPLACE "${old}" "${new}" text "${text}")
string(REPLACE "#include <unistd.h>\n" "#include <unistd.h>\n#include <stdlib.h>\n#include <OpenGL/CGLRenderers.h>\n" text "${text}")
file(WRITE "${file}" "${text}")
