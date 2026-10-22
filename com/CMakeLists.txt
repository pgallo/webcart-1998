# COM business tier: MIDL-compiled interfaces, the local server and a C++
# client test. Configure from a Visual Studio developer prompt so that
# midl.exe and cl.exe are on PATH.

enable_language(C)

set(GEN_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${GEN_DIR})
file(TO_NATIVE_PATH ${GEN_DIR} GEN_DIR_NATIVE)
file(TO_NATIVE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/webcart.idl IDL_NATIVE)
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
  set(MIDL_ENV x64)
else()
  set(MIDL_ENV win32)
endif()

set(MIDL_OUTPUTS ${GEN_DIR}/webcart_h.h ${GEN_DIR}/webcart_i.c ${GEN_DIR}/webcart.tlb)
add_custom_command(
  OUTPUT ${MIDL_OUTPUTS}
  COMMAND midl /nologo /env ${MIDL_ENV} /out ${GEN_DIR_NATIVE}
          /h webcart_h.h /iid webcart_i.c /tlb webcart.tlb
          /proxy webcart_p.c /dlldata dlldata.c ${IDL_NATIVE}
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/webcart.idl
  COMMENT "MIDL webcart.idl"
  VERBATIM)
add_custom_target(webcart_idl DEPENDS ${MIDL_OUTPUTS})

add_executable(webcart-server
  server/main.cpp
  server/module.cpp
  server/objects.cpp
  server/registration.cpp
  ${GEN_DIR}/webcart_i.c)
add_dependencies(webcart-server webcart_idl)
target_include_directories(webcart-server PRIVATE ${GEN_DIR})
target_link_libraries(webcart-server PRIVATE webcart_core ole32 oleaut32 advapi32 uuid)
# The type library and the catalog ship next to the executable.
add_custom_command(TARGET webcart-server POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different ${GEN_DIR}/webcart.tlb $<TARGET_FILE_DIR:webcart-server>/webcart.tlb
  COMMAND ${CMAKE_COMMAND} -E copy_directory ${PROJECT_SOURCE_DIR}/data $<TARGET_FILE_DIR:webcart-server>/data)

add_executable(com_client_test ${PROJECT_SOURCE_DIR}/tests/com/com_client_test.cpp ${GEN_DIR}/webcart_i.c)
add_dependencies(com_client_test webcart_idl)
target_include_directories(com_client_test PRIVATE ${GEN_DIR})
target_link_libraries(com_client_test PRIVATE ole32 oleaut32 uuid)
