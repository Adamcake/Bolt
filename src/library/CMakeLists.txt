# e.g. becomes "libbolt-plugin.so" on linux
set(BOLT_PLUGIN_LIB_NAME "bolt-plugin")
set(BOLT_PLUGIN_LIB_NAME ${BOLT_PLUGIN_LIB_NAME} PARENT_SCOPE)

# OS-specific IPC stuff is not actually needed, as windows supports AF_UNIX sockets since windows 10, build 17093.
set(LIBRARY_IPC_OS_SPECIFIC "${CMAKE_CURRENT_SOURCE_DIR}/ipc_posix.c" PARENT_SCOPE)

if(UNIX AND NOT APPLE)
    add_library(${BOLT_PLUGIN_LIB_NAME} SHARED so/main.c plugin/plugin.c plugin/plugin_api.c gl.c
    rwlock/rwlock_posix.c ipc_posix.c plugin/plugin_posix.c ../../modules/hashmap/hashmap.c
    ../miniz/miniz.c ../sha256/sha256.c ../../modules/spng/spng/spng.c)
    target_link_libraries(${BOLT_PLUGIN_LIB_NAME} luajit-5.1)
    target_include_directories(${BOLT_PLUGIN_LIB_NAME} PUBLIC "${BOLT_LUAJIT_INCLUDE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/../miniz")
    install(TARGETS ${BOLT_PLUGIN_LIB_NAME} DESTINATION "${BOLT_LIBDIR}")
endif()
if (WIN32)
    set(BOLT_STUB_ENTRYNAME entry)
    set(BOLT_STUB_ENTRYORDINAL 1)
    file(GENERATE OUTPUT stub.def CONTENT "LIBRARY STUB\nEXPORTS\n${BOLT_STUB_ENTRYNAME} @${BOLT_STUB_ENTRYORDINAL}\n")
    file(GENERATE OUTPUT plugin.def CONTENT "LIBRARY BOLT-PLUGIN\nEXPORTS\n${BOLT_STUB_ENTRYNAME} @${BOLT_STUB_ENTRYORDINAL}\n")

    add_library(${BOLT_PLUGIN_LIB_NAME} SHARED dll/main.c dll/common.c plugin/plugin.c plugin/plugin_api.c gl.c
    rwlock/rwlock_win32.c ipc_posix.c plugin/plugin_win32.c ../../modules/hashmap/hashmap.c
    ../miniz/miniz.c ../sha256/sha256.c ../../modules/spng/spng/spng.c "${CMAKE_CURRENT_BINARY_DIR}/plugin.def")
    target_compile_definitions(${BOLT_PLUGIN_LIB_NAME} PUBLIC BOLT_STUB_ENTRYNAME=${BOLT_STUB_ENTRYNAME})
    target_link_libraries(${BOLT_PLUGIN_LIB_NAME} PUBLIC "${BOLT_LUAJIT_DIR}/lua51.lib" "Ws2_32.lib")
    target_include_directories(${BOLT_PLUGIN_LIB_NAME} PUBLIC "${BOLT_LUAJIT_DIR}" "${BOLT_ZLIB_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/../miniz")
    
    add_executable(dll_inject_generator dll/dll_inject_generator.cxx dll/common.c)
    target_compile_definitions(dll_inject_generator PUBLIC BOLT_STUB_ENTRYORDINAL=${BOLT_STUB_ENTRYORDINAL})
    set_target_properties(dll_inject_generator PROPERTIES CXX_STANDARD 20 CXX_EXTENSIONS OFF)
    add_custom_command(
        OUTPUT dll_inject_cmake_gen.c
        DEPENDS ${BOLT_PLUGIN_LIB_NAME} dll_inject_generator
        COMMAND dll_inject_generator $<TARGET_FILE:${BOLT_PLUGIN_LIB_NAME}> "${BOLT_LUAJIT_DIR}/lua51.dll" "${CMAKE_CURRENT_SOURCE_DIR}/dll/dll_inject.h" ">dll_inject_cmake_gen.c"
    )

    add_library(stub SHARED dll/stub.c dll/common.c dll_inject_cmake_gen.c "${CMAKE_CURRENT_BINARY_DIR}/stub.def")
    target_compile_definitions(stub PUBLIC BOLT_STUB_ENTRYNAME=${BOLT_STUB_ENTRYNAME})
    add_executable(stub_inject_generator dll/stub_inject_generator.cxx dll/common.c)
    target_compile_definitions(stub_inject_generator PUBLIC BOLT_STUB_ENTRYORDINAL=${BOLT_STUB_ENTRYORDINAL})
    add_custom_command(
        OUTPUT stub_inject_cmake_gen.cxx
        DEPENDS stub stub_inject_generator
        COMMAND stub_inject_generator $<TARGET_FILE:stub> "${CMAKE_CURRENT_SOURCE_DIR}/dll/stub_inject.h" ">stub_inject_cmake_gen.cxx"
    )
    add_custom_target(BOLT_STUB_INJECT_DEPENDENCY DEPENDS stub_inject_cmake_gen.cxx)
    set(BOLT_STUB_INJECT_CXX "${CMAKE_CURRENT_BINARY_DIR}/stub_inject_cmake_gen.cxx" PARENT_SCOPE)

    if(MSVC)
        set_property(TARGET ${BOLT_PLUGIN_LIB_NAME} PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
        set_property(TARGET stub PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
        set_property(TARGET dll_inject_generator PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
        set_property(TARGET stub_inject_generator PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
    else()
        target_compile_options(${BOLT_PLUGIN_LIB_NAME} PUBLIC ${BOLT_WINDOWS_NONMSVC_OPTIONS})
        target_link_options(${BOLT_PLUGIN_LIB_NAME} PUBLIC ${BOLT_WINDOWS_NONMSVC_LINKOPTIONS})
        target_compile_options(stub PUBLIC ${BOLT_WINDOWS_NONMSVC_OPTIONS})
        target_link_options(stub PUBLIC ${BOLT_WINDOWS_NONMSVC_LINKOPTIONS})
        target_compile_options(dll_inject_generator PUBLIC ${BOLT_WINDOWS_NONMSVC_OPTIONS})
        target_link_options(dll_inject_generator PUBLIC ${BOLT_WINDOWS_NONMSVC_LINKOPTIONS})
        target_compile_options(stub_inject_generator PUBLIC ${BOLT_WINDOWS_NONMSVC_OPTIONS})
        target_link_options(stub_inject_generator PUBLIC ${BOLT_WINDOWS_NONMSVC_LINKOPTIONS})
    endif()
endif()
target_include_directories(${BOLT_PLUGIN_LIB_NAME} PUBLIC "${CEF_ROOT}" "${CMAKE_CURRENT_SOURCE_DIR}/src/miniz")
target_compile_definitions(${BOLT_PLUGIN_LIB_NAME} PUBLIC SPNG_STATIC=1 SPNG_USE_MINIZ=1)
if(MSVC)
    target_compile_definitions(${BOLT_PLUGIN_LIB_NAME} PUBLIC _USE_MATH_DEFINES=1)
endif()
if(BOLT_LIBRARY_VERBOSE)
    target_compile_definitions(${BOLT_PLUGIN_LIB_NAME} PUBLIC VERBOSE)
endif()
