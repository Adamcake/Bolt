cmake_minimum_required(VERSION 3.21) # Minimum CEF requirement, but building with the latest cmake is strongly recommended
project(Bolt VERSION 0.1 LANGUAGES C CXX)

# Generate compile_commands.json, for use by language servers for highlighting/autocomplete/etc
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Tell git to ignore everything in the new build directory
file(GENERATE OUTPUT .gitignore CONTENT "*")

# libcef C++ wrapper
set(CEF_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/cef/dist")
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CEF_ROOT}/cmake")
find_package(CEF REQUIRED)
add_subdirectory(${CEF_LIBCEF_DLL_WRAPPER_PATH} libcef_dll_wrapper)

# Files to copy from cef to build
configure_file("${CEF_ROOT}/Release/libcef.so" "libcef.so" COPYONLY)

# This line needs to be updated manually with any new/deleted object files; cmake discourages GLOBbing source files
add_executable(bolt src/main.cxx src/browser.cxx src/browser/app_frame_data.cxx src/browser/app.cxx src/browser/client.cxx src/browser/dom_visitor.cxx)
target_include_directories(bolt PUBLIC ${CEF_ROOT})
target_link_directories(bolt PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
set_target_properties(bolt PROPERTIES CXX_STANDARD 20 CXX_EXTENSIONS OFF BUILD_RPATH_USE_ORIGIN TRUE)
target_link_libraries(bolt PUBLIC libcef_dll_wrapper)
target_link_libraries(bolt PUBLIC "cef")
target_link_libraries(bolt PUBLIC "X11")
target_link_libraries(bolt PUBLIC "fmt")
