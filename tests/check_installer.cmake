# Builds the Windows installer script (packaging/windows/tisma.nsi) over a
# small stand-in package, so a mistake in it shows here and not at release.
#   cmake -DMAKENSIS=<makensis> -DSOURCE_DIR=<repo> -DWORK=<folder> -P check_installer.cmake
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}/package/solidcraft")
file(WRITE "${WORK}/package/tisma.exe" "stand-in")
file(WRITE "${WORK}/package/tisma-engine.exe" "stand-in")
file(WRITE "${WORK}/package/solidcraft/LICENSE-MIT" "stand-in")
execute_process(
  COMMAND "${MAKENSIS}" -V2 -WX -DVERSION=0.0.0-check -DSOURCE=${WORK}/package -DOUTFILE=${WORK}/tisma-check-setup-windows-x64.exe
          -DICON=${SOURCE_DIR}/assets/brand/app/tisma.ico ${SOURCE_DIR}/packaging/windows/tisma.nsi
  RESULT_VARIABLE result)
if(NOT result EQUAL 0 OR NOT EXISTS "${WORK}/tisma-check-setup-windows-x64.exe")
  message(FATAL_ERROR "the installer script does not build (makensis: ${result})")
endif()
