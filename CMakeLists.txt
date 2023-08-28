cmake_minimum_required(VERSION 3.21) # Minimum CEF requirement, but building with the latest cmake is strongly recommended
project(Bolt VERSION 0.1 LANGUAGES C CXX)
if(NOT WIN32)
    # necessary for installing files in both /opt and /usr
    # can be overridden using --install-prefix in the initial cmake setup command
    if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
        set(CMAKE_INSTALL_PREFIX /)
    endif()

    # defines DESTINATION of shell script (relative to CMAKE_INSTALL_PREFIX, unless set to an absolute path)
    # things like flatpak need to override this
    if(NOT BOLT_BINDIR)
        set(BOLT_BINDIR usr/local/bin)
    endif()

    # Short shell script to be installed in /usr/local/bin, gets renamed to "bolt" later
    file(GENERATE OUTPUT bolt-run.sh CONTENT "#!/bin/sh -eu\ncd ${CMAKE_INSTALL_PREFIX}/opt/bolt-launcher\nexec ./bolt \"$@\"\n")

    # auto-generated helper for running from build dir; this file does not get installed
    file(GENERATE OUTPUT bolt.sh CONTENT "#!/bin/sh -eu\nexport XDG_CONFIG_HOME=${CMAKE_CURRENT_BINARY_DIR}/config\nexport XDG_DATA_HOME=${CMAKE_CURRENT_BINARY_DIR}/data\ncd ${CMAKE_CURRENT_BINARY_DIR}/opt/bolt-launcher\nexec ./bolt \"$@\"\n" FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
endif()

# Generate compile_commands.json, for use by language servers for highlighting/autocomplete/etc
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Tell git to ignore everything in the new build directory
file(GENERATE OUTPUT .gitignore CONTENT "*")

# libcef C++ wrapper
if(NOT CEF_ROOT)
    set(CEF_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/cef/dist")
endif()
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CEF_ROOT}/cmake")
find_package(CEF REQUIRED)
add_subdirectory(${CEF_LIBCEF_DLL_WRAPPER_PATH} libcef_dll_wrapper)

set(WINDOW_LAUNCHER_OS_SPECIFIC src/browser/window_launcher_linux.cxx)

# This line needs to be updated manually with any new/deleted object files; cmake discourages GLOBbing source files
add_executable(bolt
    modules/fmt/src/format.cc src/main.cxx src/browser.cxx src/browser/app.cxx src/browser/client.cxx
    src/browser/resource_handler.cxx src/browser/window_launcher.cxx ${WINDOW_LAUNCHER_OS_SPECIFIC}
)

# Various build properties
target_include_directories(bolt PUBLIC ${CEF_ROOT} modules/fmt/include)
set_target_properties(bolt PROPERTIES CXX_STANDARD 20 CXX_EXTENSIONS OFF)
target_link_libraries(bolt PUBLIC libcef_dll_wrapper)
if(UNIX AND NOT APPLE)
    set_target_properties(bolt PROPERTIES INSTALL_RPATH "$ORIGIN")
    target_link_libraries(bolt PUBLIC "${CEF_ROOT}/${CMAKE_BUILD_TYPE}/libcef.so")
    target_link_libraries(bolt PUBLIC "X11")
    target_link_libraries(bolt PUBLIC "xcb")
    target_link_libraries(bolt PUBLIC "archive")
    find_package(PkgConfig REQUIRED)
elseif(MSVC)
    target_compile_options(bolt PUBLIC $<$<CONFIG:>:/MT> $<$<CONFIG:Debug>:/MTd> $<$<CONFIG:Release>:/MT>)
    set_target_properties(bolt PROPERTIES WIN32_EXECUTABLE TRUE)
    # note parentheses instead of brackets below - this variable is for substitution by Visual Studio, not cmake
    target_link_libraries(bolt PUBLIC "${CEF_ROOT}/$(ConfigurationName)/libcef.lib")
    target_link_libraries(bolt PUBLIC "${CEF_ROOT}/$(ConfigurationName)/cef_sandbox.lib")
elseif(WIN32)
    set_target_properties(bolt PROPERTIES WIN32_EXECUTABLE TRUE)
    target_link_libraries(bolt PUBLIC "${CEF_ROOT}/${CMAKE_BUILD_TYPE}/libcef.lib")
    target_link_libraries(bolt PUBLIC "${CEF_ROOT}/${CMAKE_BUILD_TYPE}/cef_sandbox.lib")
elseif(APPLE)
    # TODO: mac support
endif()

# Bolt uses GTK on all platforms, but it must specifically use gtk3
# to avoid symbols conflicting with CEF's GTK usage on Unix platforms
pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
target_include_directories(bolt PUBLIC ${GTK3_INCLUDE_DIRS})
target_link_libraries(bolt PUBLIC ${GTK3_LIBRARIES})

# Install commands for target dir layout - as per https://bitbucket.org/chromiumembedded/cef/wiki/Tutorial.md
if(APPLE)
    # TODO: mac support
else()
    install(TARGETS bolt DESTINATION opt/bolt-launcher)
    install(DIRECTORY "${CEF_ROOT}/${CMAKE_BUILD_TYPE}/" "${CEF_ROOT}/Resources/" "${CMAKE_CURRENT_SOURCE_DIR}/html" DESTINATION opt/bolt-launcher USE_SOURCE_PERMISSIONS)
endif()

# Finally, install shell script in bin dir
if(NOT WIN32)
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/bolt-run.sh" RENAME bolt DESTINATION ${BOLT_BINDIR} PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
endif()
