# Unpacks an .xz file: cmake -DXZ=<xz> -DIN=<file.xz> -DOUT=<file> -P unxz.cmake
execute_process(COMMAND ${XZ} -dc ${IN} OUTPUT_FILE ${OUT} RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "xz failed on ${IN}: ${rc}")
endif()
