cmake_minimum_required(VERSION 3.21) # Minimum CEF requirement, but building with the latest cmake is strongly recommended
project(Bolt VERSION 0.8 LANGUAGES C CXX)

# e.g. becomes "libbolt-plugin.so" on linux
set(BOLT_PLUGIN_LIB_NAME "bolt-plugin")

if(NOT WIN32)
    # name of application in metadata, defaulting to "BoltLauncher", but usually overridden to "com.adamcake.Bolt" by flatpak for example
    if(NOT BOLT_META_NAME)
        set(BOLT_META_NAME "BoltLauncher")
    endif()

    # how this program is described in human-readable strings in metadata
    # not in plaintext for a reason
    string(ASCII 74 97 103 101 120 32 108 97 117 110 99 104 101 114 BOLT_PROGRAM_NAME)
    string(ASCII 82 117 110 101 83 99 97 112 101 BOLT_GAME_NAME)
    string(TOLOWER "${BOLT_GAME_NAME}" BOLT_GAME_NAME_LOWER)

    # .desktop file contents
    string(JOIN "\n" BOLT_DESKTOP_INFO
        "[Desktop Entry]"
        "Type=Application"
        "Terminal=false"
        "Name=Bolt"
        "GenericName=Bolt Launcher"
        "Comment=Third-party ${BOLT_PROGRAM_NAME}"
        "Keywords=games;"
        "Exec=bolt"
        "Icon=${BOLT_META_NAME}"
        "Categories=Game;"
        "StartupWMClass=BoltLauncher"
        ""
    )
    file(GENERATE OUTPUT bolt.desktop CONTENT "${BOLT_DESKTOP_INFO}")

    # extra NoDisplay .desktop entry, just for DEs to pick up on icons correctly
    string(JOIN "\n" BOLT_DESKTOP_GAME_INFO
        "[Desktop Entry]"
        "Version=1.0"
        "Type=Application"
        "Name=${BOLT_GAME_NAME}"
        "GenericName=${BOLT_GAME_NAME}"
        "Icon=${BOLT_GAME_NAME_LOWER}"
        "Terminal=false"
        "NoDisplay=true"
        "StartupWMClass=${BOLT_GAME_NAME}"
        ""
    )
    file(GENERATE OUTPUT bolt.game.desktop CONTENT "${BOLT_DESKTOP_GAME_INFO}")

    # metainfo.xml file contents
    string(JOIN "\n" BOLT_METAINFO
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
        "<component type=\"desktop\">"
        "<id>${BOLT_META_NAME}</id>"
        "<name>Bolt Launcher</name>"
        "<developer_name>Adamcake</developer_name>"
        "<summary>Third-party ${BOLT_PROGRAM_NAME}</summary>"
        "<description><p>Bolt is a free, open-source, third-party implementation of the ${BOLT_PROGRAM_NAME}.</p></description>"
        "<screenshots>"
        "    <screenshot type=\"default\">https://raw.githubusercontent.com/flathub/net.runelite.RuneLite/master/screenshot.png</screenshot>"
        "</screenshots>"
        "<releases>"
        "    <release version=\"0.8.0\" date=\"2024-02-05\"/>"
        "    <release version=\"0.7.0\" date=\"2023-12-19\"/>"
        "    <release version=\"0.6.0\" date=\"2023-12-15\"/>"
        "    <release version=\"0.5.0\" date=\"2023-12-11\"/>"
        "    <release version=\"0.4.0\" date=\"2023-11-03\"/>"
        "    <release version=\"0.3.0\" date=\"2023-11-02\"/>"
        "    <release version=\"0.2.0\" date=\"2023-09-16\"/>"
        "    <release version=\"0.1.0\" date=\"2023-09-02\"/>"
        "</releases>"
        "<metadata_license>CC0-1.0</metadata_license>"
        "<project_license>AGPL-3.0</project_license>"
        "<url type=\"homepage\">https://bolt.adamcake.com/</url>"
        "<categories>"
        "    <category>Game</category>"
        "</categories>"
        "<launchable type=\"desktop-id\">${BOLT_META_NAME}.desktop</launchable>"
        "<content_rating type=\"oars-1.1\">"
        "    <content_attribute id=\"violence-cartoon\">moderate</content_attribute>"
        "    <content_attribute id=\"violence-fantasy\">intense</content_attribute>"
        "    <content_attribute id=\"violence-bloodshed\">mild</content_attribute>"
        "    <content_attribute id=\"drugs-alcohol\">moderate</content_attribute>"
        "    <content_attribute id=\"drugs-narcotics\">mild</content_attribute>"
        "    <content_attribute id=\"drugs-tobacco\">mild</content_attribute>"
        "    <content_attribute id=\"sex-themes\">mild</content_attribute>"
        "    <content_attribute id=\"language-profanity\">mild</content_attribute>"
        "    <content_attribute id=\"social-chat\">intense</content_attribute>"
        "    <content_attribute id=\"social-info\">moderate</content_attribute>"
        "    <content_attribute id=\"money-purchasing\">mild</content_attribute>"
        "</content_rating>"
        "</component>"
        ""
    )
    file(GENERATE OUTPUT metainfo.xml CONTENT "${BOLT_METAINFO}")

    # necessary for installing files in both /opt and /usr
    # can be overridden using --install-prefix in the initial cmake setup command
    if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
        set(CMAKE_INSTALL_PREFIX /)
    endif()

    # location to install the CEF blob, default location is /opt
    if(NOT BOLT_CEF_INSTALLDIR)
        set(BOLT_CEF_INSTALLDIR opt)
    endif()

    # defines DESTINATION of shell script (relative to CMAKE_INSTALL_PREFIX, unless set to an absolute path)
    # things like flatpak need to override this
    if(NOT BOLT_BINDIR)
        set(BOLT_BINDIR usr/local/bin)
    endif()

    # as above but lib instead of bin
    if(NOT BOLT_LIBDIR)
        set(BOLT_LIBDIR usr/local/lib)
    endif()

    # defines DESTINATION of metadata like icons (relative to CMAKE_INSTALL_PREFIX, unless set to an absolute path)
    if(NOT BOLT_SHAREDIR)
        set(BOLT_SHAREDIR usr/local/share)
    endif()

    # Short shell script to be installed in /usr/local/bin, gets renamed to "bolt" later
    file(RELATIVE_PATH BOLT_SCRIPT_CD_PATH "${CMAKE_INSTALL_PREFIX}/${BOLT_BINDIR}" "${CMAKE_INSTALL_PREFIX}/${BOLT_CEF_INSTALLDIR}/bolt-launcher")
    file(GENERATE OUTPUT bolt-run.sh CONTENT "#!/bin/sh -eu\ncd \"$(dirname \"$0\")/${BOLT_SCRIPT_CD_PATH}\"\nexec ./bolt \"$@\"\n")

    # auto-generated helper for running from build dir; this file does not get installed
    # appends to LD_LIBRARY_PATH only if the plugin library is being built, otherwise the dir won't exist
    if(NOT BOLT_SKIP_LIBRARIES)
        set(BOLT_PLUGIN_LIB_EXPORT "export LD_LIBRARY_PATH=${CMAKE_CURRENT_BINARY_DIR}/${BOLT_LIBDIR}\${LD_LIBRARY_PATH:+:\${LD_LIBRARY_PATH}}")
    endif()
    string(JOIN "\n" BOLT_STAGING_SCRIPT
        "#!/bin/sh -eu"
        "# This is a helper script for running a staging build, i.e. a build that has been installed in this directory."
        "# This script will not be installed anywhere by `cmake --install`, as it's for development/staging purposes only."
        "${BOLT_PLUGIN_LIB_EXPORT}"
        "export XDG_DATA_DIRS=${CMAKE_CURRENT_BINARY_DIR}/${BOLT_SHAREDIR}\${XDG_DATA_DIRS:+:\${XDG_DATA_DIRS}}"
        "export XDG_CONFIG_HOME=${CMAKE_CURRENT_BINARY_DIR}/config"
        "export XDG_DATA_HOME=${CMAKE_CURRENT_BINARY_DIR}/data"
        "cd ${CMAKE_CURRENT_BINARY_DIR}/${BOLT_CEF_INSTALLDIR}/bolt-launcher"
        "exec ./bolt \"$@\""
        ""
    )
    file(GENERATE OUTPUT bolt.sh CONTENT ${BOLT_STAGING_SCRIPT} FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
endif()

# Generate compile_commands.json, for use by language servers for highlighting/autocomplete/etc
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Tell git to ignore everything in the new build directory
file(GENERATE OUTPUT .gitignore CONTENT "*")

# source html directory
if(NOT BOLT_HTML_DIR)
    set(BOLT_HTML_DIR "${CMAKE_CURRENT_SOURCE_DIR}/app/dist")
endif()

# libcef C++ wrapper
if(NOT CEF_ROOT)
    set(CEF_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/cef/dist")
endif()
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CEF_ROOT}/cmake")
find_package(CEF REQUIRED)
add_subdirectory(${CEF_LIBCEF_DLL_WRAPPER_PATH} libcef_dll_wrapper)

# select an implementation for OS-specific behaviour of launcher window
if(WIN32)
    set(WINDOW_LAUNCHER_OS_SPECIFIC src/browser/window_launcher_win.cxx)
else()
    set(WINDOW_LAUNCHER_OS_SPECIFIC src/browser/window_launcher_posix.cxx)
endif()

# compile an auto-generator, then use it to auto-generate a C++ file containing icon data
set(BOLT_ICON_DIR "${CMAKE_CURRENT_SOURCE_DIR}/icon")
add_executable(icon_gen icon/generator.cxx modules/lodepng/lodepng.cpp)
set_target_properties(icon_gen PROPERTIES CXX_STANDARD 20 CXX_EXTENSIONS OFF)
add_custom_command(
    OUTPUT client_cmake_gen.cxx
    DEPENDS icon_gen ${BOLT_ICON_DIR}/16.png ${BOLT_ICON_DIR}/32.png ${BOLT_ICON_DIR}/64.png ${BOLT_ICON_DIR}/256.png
    COMMAND icon_gen "${CMAKE_CURRENT_SOURCE_DIR}" "${BOLT_ICON_DIR}" ">client_cmake_gen.cxx"
)

# compile the generator for embedded launcher file contents
if(NOT BOLT_DEV_LAUNCHER_DIRECTORY)
    add_executable(html_gen html/generator.cxx src/mime.cxx)
    set_target_properties(html_gen PROPERTIES CXX_STANDARD 20 CXX_EXTENSIONS OFF)
    file(GLOB_RECURSE BOLT_HTML_FILE_LIST FOLLOW_SYMLINKS LIST_DIRECTORIES false CONFIGURE_DEPENDS "${BOLT_HTML_DIR}/*")
    add_custom_command(
        OUTPUT file_manager_launcher_gen.cxx
        DEPENDS html_gen ${BOLT_HTML_FILE_LIST}
        COMMAND html_gen "${CMAKE_CURRENT_SOURCE_DIR}" "${BOLT_HTML_DIR}" ${BOLT_HTML_FILE_LIST} ">file_manager_launcher_gen.cxx"
    )
    set(BOLT_FILE_MANAGER_LAUNCHER_GEN file_manager_launcher_gen.cxx)
endif()

# This line needs to be updated manually with any new/deleted object files; cmake discourages GLOBbing source files
set(BOLT_SRCS
    modules/fmt/src/format.cc src/main.cxx src/browser.cxx src/browser/app.cxx src/browser/client.cxx
    src/browser/resource_handler.cxx src/browser/window_launcher.cxx ${WINDOW_LAUNCHER_OS_SPECIFIC}
    src/mime.cxx src/file_manager/directory.cxx client_cmake_gen.cxx ${BOLT_FILE_MANAGER_LAUNCHER_GEN}
)

if(APPLE)
    add_executable(bolt MACOSX_BUNDLE ${BOLT_SRCS})
    # Copy the CEF framework into the Frameworks directory.
    add_custom_command(
        TARGET bolt
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CEF_ROOT}/${CMAKE_BUILD_TYPE}/Chromium Embedded Framework.framework"
                "${CMAKE_BINARY_DIR}/bolt.app/Contents/Frameworks/Chromium Embedded Framework.framework"
        VERBATIM
    )

    set(BOLT_HELPER_SRCS
        mac/process_helper.cc
    )

    # Create the multiple Helper app bundle targets.
    foreach(_suffix_list ${CEF_HELPER_APP_SUFFIXES})
        # Convert to a list and extract the suffix values.
        string(REPLACE ":" ";" _suffix_list ${_suffix_list})
        list(GET _suffix_list 0 _name_suffix)
        list(GET _suffix_list 1 _target_suffix)
        list(GET _suffix_list 2 _plist_suffix)

        # Define Helper target and output names.
        set(_helper_target "Bolt_Helper${_target_suffix}")
        set(_helper_output_name "Bolt Helper${_name_suffix}")

        # Create Helper executable target.
        add_executable(${_helper_target} MACOSX_BUNDLE ${BOLT_HELPER_SRCS})
        add_dependencies(${_helper_target} libcef_dll_wrapper)
        target_include_directories(${_helper_target} PUBLIC "${CEF_ROOT}")
        set_target_properties(${_helper_target} PROPERTIES CXX_STANDARD 20 CXX_EXTENSIONS OFF)
        target_link_libraries(${_helper_target} libcef_dll_wrapper ${CEF_STANDARD_LIBS})
        set_target_properties(${_helper_target} PROPERTIES
            OUTPUT_NAME ${_helper_output_name}
        )

        # Add the Helper as a dependency of the main executable target.
        add_dependencies(bolt "${_helper_target}")

        # Copy the Helper app bundle into the Frameworks directory.
        add_custom_command(
            TARGET bolt
            POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                    "${CMAKE_BINARY_DIR}/${_helper_output_name}.app"
                    "${CMAKE_BINARY_DIR}/bolt.app/Contents/Frameworks/${_helper_output_name}.app"
            VERBATIM
        )
    endforeach()
else()
    add_executable(bolt ${BOLT_SRCS})
endif()

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
elseif(MSVC)
    target_compile_options(bolt PUBLIC $<$<CONFIG:>:/MT> $<$<CONFIG:Debug>:/MTd> $<$<CONFIG:Release>:/MT>)
    set_target_properties(bolt PROPERTIES WIN32_EXECUTABLE TRUE)
    # note parentheses instead of brackets below - this variable is for substitution by Visual Studio, not cmake
    target_link_libraries(bolt PUBLIC "${CEF_ROOT}/$(ConfigurationName)/libcef.lib")
    target_link_libraries(bolt PUBLIC "${CEF_ROOT}/$(ConfigurationName)/cef_sandbox.lib")
elseif(WIN32)
    # note: this is not used on Windows if compiler is MSVC; tht above section is used instead
    set_target_properties(bolt PROPERTIES WIN32_EXECUTABLE TRUE)
    target_link_libraries(bolt PUBLIC "${CEF_ROOT}/${CMAKE_BUILD_TYPE}/libcef.lib")
    target_link_libraries(bolt PUBLIC "${CEF_ROOT}/${CMAKE_BUILD_TYPE}/cef_sandbox.lib")
elseif(APPLE)
    execute_process(
        COMMAND brew --prefix libarchive
        OUTPUT_VARIABLE LIBARCHIVE_PREFIX
        OUTPUT_STRIP_TRAILING_WHITESPACE
        COMMAND_ERROR_IS_FATAL ANY
    )
    set(LibArchive_INCLUDE_DIR "${LIBARCHIVE_PREFIX}/include")
    find_package(LibArchive REQUIRED)
    target_link_libraries(bolt PRIVATE LibArchive::LibArchive)
endif()

# compilation setting for enabling chromium dev tools
if(BOLT_DEV_SHOW_DEVTOOLS)
    target_compile_definitions(bolt PUBLIC BOLT_DEV_SHOW_DEVTOOLS)
endif()

# flag indicating flathub is creating this build, used only for some minor UI changes
if(BOLT_FLATHUB_BUILD)
    target_compile_definitions(bolt PUBLIC BOLT_FLATHUB_BUILD=1)
else()
    target_compile_definitions(bolt PUBLIC BOLT_FLATHUB_BUILD=0)
endif()

# compilation setting for directory-watching for launcher UI
if(BOLT_DEV_LAUNCHER_DIRECTORY)
    target_compile_definitions(bolt PUBLIC BOLT_DEV_LAUNCHER_DIRECTORY="${BOLT_HTML_DIR}")
endif()

# fake java wrapper for running hdos
if(NOT WIN32)
    add_executable(java-proxy src/fake_java.c)
    install(TARGETS java-proxy DESTINATION ${BOLT_CEF_INSTALLDIR}/bolt-launcher)
endif()

# Bolt uses GTK on all platforms, but it must specifically use gtk3
# to avoid symbols conflicting with CEF's GTK usage on Unix platforms
find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
target_include_directories(bolt PUBLIC ${GTK3_INCLUDE_DIRS})
target_link_libraries(bolt PUBLIC ${GTK3_LIBRARIES})
if(MSVC OR APPLE)
    target_link_directories(bolt PUBLIC ${GTK3_LIBRARY_DIRS})
endif()

install(TARGETS bolt DESTINATION ${BOLT_CEF_INSTALLDIR}/bolt-launcher)
# Install commands for target dir layout - as per https://bitbucket.org/chromiumembedded/cef/wiki/Tutorial.md
if(APPLE)
else()
    install(DIRECTORY "${CEF_ROOT}/${CMAKE_BUILD_TYPE}/" "${CEF_ROOT}/Resources/" DESTINATION ${BOLT_CEF_INSTALLDIR}/bolt-launcher USE_SOURCE_PERMISSIONS)
endif()

# Build helper libraries
if(NOT BOLT_SKIP_LIBRARIES)
    # todo: anything other than linux
    if(UNIX AND NOT APPLE)
        add_library(${BOLT_PLUGIN_LIB_NAME} SHARED src/library/so/main.c src/library/plugin.c src/library/gl.c src/library/rwlock_posix.c modules/hashmap/hashmap.c)
        install(TARGETS ${BOLT_PLUGIN_LIB_NAME} DESTINATION "${BOLT_LIBDIR}")
    endif()
    target_include_directories(${BOLT_PLUGIN_LIB_NAME} PUBLIC "${CEF_ROOT}" "${BOLT_LUAJIT_INCLUDE_DIR}" modules/fmt/include)
    target_link_libraries(${BOLT_PLUGIN_LIB_NAME} luajit-5.1)
    target_compile_definitions(bolt PUBLIC BOLT_LIB_NAME="${BOLT_PLUGIN_LIB_NAME}")
endif()

# Finally, install shell script and metadata
if(NOT WIN32)
    install(PROGRAMS "${CMAKE_CURRENT_BINARY_DIR}/bolt-run.sh" RENAME bolt DESTINATION ${BOLT_BINDIR})
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/bolt.desktop" DESTINATION "${BOLT_SHAREDIR}/applications/" RENAME "${BOLT_META_NAME}.desktop")
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/bolt.game.desktop" DESTINATION "${BOLT_SHAREDIR}/applications/" RENAME "${BOLT_META_NAME}.${BOLT_GAME_NAME}.desktop")
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/metainfo.xml" DESTINATION "${BOLT_SHAREDIR}/metainfo/" RENAME "${BOLT_META_NAME}.metainfo.xml")
    install(FILES "${BOLT_ICON_DIR}/16.png" DESTINATION "${BOLT_SHAREDIR}/icons/hicolor/16x16/apps/" RENAME "${BOLT_META_NAME}.png")
    install(FILES "${BOLT_ICON_DIR}/32.png" DESTINATION "${BOLT_SHAREDIR}/icons/hicolor/32x32/apps/" RENAME "${BOLT_META_NAME}.png")
    install(FILES "${BOLT_ICON_DIR}/64.png" DESTINATION "${BOLT_SHAREDIR}/icons/hicolor/64x64/apps/" RENAME "${BOLT_META_NAME}.png")
    install(FILES "${BOLT_ICON_DIR}/256.png" DESTINATION "${BOLT_SHAREDIR}/icons/hicolor/256x256/apps/" RENAME "${BOLT_META_NAME}.png")
endif()
