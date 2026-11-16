# The release gate's G14 (docs/spec/RELEASE_GATE.md): the published profile
# pack, downloaded and installed the way the interface does it, only for the
# vendors the gate picks its printers from. Nothing of it enters the
# repository; it lands in the build folder.
#   cmake -DENGINE=<tisma-engine> -DDATA=<folder> -DVENDORS=<id,id,...> -P gate_pack.cmake
foreach(v ENGINE DATA VENDORS)
  if(NOT ${v})
    message(FATAL_ERROR "gate_pack.cmake needs -D${v}=...")
  endif()
endforeach()
set(DL ${DATA}/download)
file(REMOVE_RECURSE ${DL})
file(MAKE_DIRECTORY ${DL})

function(fetch name url)
  file(DOWNLOAD ${url} ${DL}/${name} STATUS st TLS_VERIFY ON)
  list(GET st 0 code)
  if(NOT code EQUAL 0)
    message(FATAL_ERROR "error: downloading ${url}: ${st}")
  endif()
endfunction()

# The index first, then what the engine plans from it.
execute_process(COMMAND ${ENGINE} pack plan --data ${DATA} OUTPUT_VARIABLE plan RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "error: pack plan failed")
endif()
string(JSON url GET "${plan}" downloads 0 url)
fetch(index.json ${url})
execute_process(COMMAND ${ENGINE} pack plan --data ${DATA} --index ${DL}/index.json --vendors ${VENDORS}
                OUTPUT_VARIABLE plan RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "error: pack plan with the index failed")
endif()
string(JSON n LENGTH "${plan}" downloads)
math(EXPR last "${n} - 1")
foreach(i RANGE ${last})
  string(JSON name GET "${plan}" downloads ${i} name)
  string(JSON url GET "${plan}" downloads ${i} url)
  fetch(${name} ${url})
endforeach()
execute_process(COMMAND ${ENGINE} pack install --data ${DATA} --from ${DL} RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "error: pack install failed")
endif()
