cmake_minimum_required(VERSION 3.21) # Minimum CEF requirement, but building with the latest cmake is strongly recommended
project(Bolt VERSION 0.1 LANGUAGES C CXX)
if(NOT WIN32)
    # necessary for installing files in both /opt and /usr
    set(CMAKE_INSTALL_PREFIX /)

    # Short shell script to be installed in /usr/local/bin, gets renamed to "bolt" later
    file(GENERATE OUTPUT bolt-run.sh CONTENT "#!/bin/sh\ncd /opt/Bolt\n./bolt \"$@\"\n")

    # auto-generated helper for running from build dir; this file does not get installed
    file(GENERATE OUTPUT bolt.sh CONTENT "#!/bin/sh\ncd ${CMAKE_CURRENT_BINARY_DIR}/opt/Bolt\n./bolt \"$@\"\n" FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
endif()

# Generate compile_commands.json, for use by language servers for highlighting/autocomplete/etc
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Tell git to ignore everything in the new build directory
file(GENERATE OUTPUT .gitignore CONTENT "*")

# libcef C++ wrapper
set(CEF_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/cef/dist")
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CEF_ROOT}/cmake")
find_package(CEF REQUIRED)
add_subdirectory(${CEF_LIBCEF_DLL_WRAPPER_PATH} libcef_dll_wrapper)

# Copy and link libcef
if(UNIX AND NOT APPLE)
    set(CEF_LIB_NAME "libcef.so")
elseif(WIN32)
    set(CEF_LIB_NAME "libcef.dll")
elseif(APPLE)
    # TODO: mac support
endif()

# This line needs to be updated manually with any new/deleted object files; cmake discourages GLOBbing source files
add_executable(bolt src/main.cxx src/browser.cxx src/browser/app.cxx src/browser/client.cxx)
target_include_directories(bolt PUBLIC ${CEF_ROOT})
target_link_directories(bolt PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
set_target_properties(bolt PROPERTIES CXX_STANDARD 20 CXX_EXTENSIONS OFF)
if(UNIX AND NOT APPLE)
    set_target_properties(bolt PROPERTIES INSTALL_RPATH "$ORIGIN")
endif()
target_link_libraries(bolt PUBLIC libcef_dll_wrapper)
target_link_libraries(bolt PUBLIC "${CEF_ROOT}/${CMAKE_BUILD_TYPE}/${CEF_LIB_NAME}")
target_link_libraries(bolt PUBLIC "X11")
target_link_libraries(bolt PUBLIC "fmt")

# Install commands for target dir layout - as per https://bitbucket.org/chromiumembedded/cef/wiki/Tutorial.md
if(APPLE)
    # TODO: mac support
else()
    install(TARGETS bolt DESTINATION opt/Bolt)
    install(DIRECTORY "${CEF_ROOT}/${CMAKE_BUILD_TYPE}/" "${CEF_ROOT}/Resources/" "${CMAKE_CURRENT_SOURCE_DIR}/html" DESTINATION opt/Bolt)
endif()

# Finally, install shell script in bin dir
if(NOT WIN32)
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/bolt-run.sh" RENAME bolt DESTINATION usr/local/bin PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
endif()
