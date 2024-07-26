cmake_minimum_required(VERSION 3.21) # Minimum CEF requirement, but building with the latest cmake is strongly recommended
project(Bolt VERSION 0.9 LANGUAGES C CXX)

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
        "    <screenshot xml:lang=\"en\" type=\"default\">"
        "        <image>https://raw.githubusercontent.com/adamcake/Bolt/master/screenshot/dark.png</image>"
        "        <caption>Bolt Launcher GUI (default)</caption>"
        "    </screenshot>"
        "    <screenshot xml:lang=\"en\">"
        "        <image>https://raw.githubusercontent.com/adamcake/Bolt/master/screenshot/light.png</image>"
        "        <caption>Bolt Launcher GUI (light mode)</caption>"
        "    </screenshot>"
        "</screenshots>"
        "<releases>"
        "    <release version=\"0.9.0\" date=\"2024-05-12\">"
        "        <url type=\"details\">https://github.com/Adamcake/Bolt/releases/tag/0.9.0</url>"
        "        <description>"
        "            <p>New UI</p>"
        "            <ul>"
        "                <li>Added a fancy new UI</li>"
        "                <li>Removed tray icon - Bolt process will now stop itself when it's no longer needed</li>"
        "                <li>Added descriptive error messages for Java not being installed when it's needed</li>"
        "                <li>Improved handling of XDG directories, including avoiding a widely-experienced issue with Pulseaudio</li>"
        "                <li>Removed RuneLite's \"flatpak rich presence\" setting - to continue using rich presence, see https://wiki.archlinux.org/title/Discord#Enabling_rich_presence_on_Flatpak</li>"
        "            </ul>"
        "        </description>"
        "    </release>"
        "    <release version=\"0.8.0\" date=\"2024-02-05\">"
        "        <url type=\"details\">https://github.com/Adamcake/Bolt/releases/tag/0.8.0</url>"
        "        <description>"
        "            <p>RuneLite-related improvements</p>"
        "            <ul>"
        "                <li>Consistent capitalisation of \"RuneLite\" throughout the UI</li>"
        "                <li>Added \"Configure RuneLite\" launch option, for opening RuneLite's built-in settings UI</li>"
        "                <li>Removed RuneLite scale setting (as this can now be configured using the \"Configure RuneLite\" option)</li>"
        "                <li>Added a \"browse data\" button for browsing to Bolt's data directory</li>"
        "            </ul>"
        "        </description>"
        "    </release>"
        "    <release version=\"0.7.0\" date=\"2023-12-19\">"
        "        <url type=\"details\">https://github.com/Adamcake/Bolt/releases/tag/0.7.0</url>"
        "        <description>"
        "            <p>New Flatpak-Discord setting and bugfixes</p>"
        "            <ul>"
        "                <li>Fixed certain games failing to launch if Bolt was run via a terminal (note: this bug was introduced by an engine update to the game itself)</li>"
        "                <li>Fixed game windows not having window icons</li>"
        "                <li>Fixed RuneLite re-downloading its JAR every time it checks for an update</li>"
        "                <li>Added a setting to expose Flatpak's Discord rich presence to old-school clients</li>"
        "            </ul>"
        "        </description>"
        "    </release>"
        "    <release version=\"0.6.0\" date=\"2023-12-15\">"
        "        <url type=\"details\">https://github.com/Adamcake/Bolt/releases/tag/0.6.0</url>"
        "        <description>"
        "            <p>Bugfixes</p>"
        "            <ul>"
        "                <li>Fixed a bug relating to the HDOS fix in 0.5; HDOS is now playable when Bolt is installed via Flatpak</li>"
        "                <li>Fixed a graphical issue in the launcher UI</li>"
        "            </ul>"
        "        </description>"
        "    </release>"
        "    <release version=\"0.5.0\" date=\"2023-12-11\">"
        "        <url type=\"details\">https://github.com/Adamcake/Bolt/releases/tag/0.5.0</url>"
        "        <description>"
        "            <p>Added Java 17+ support for HDOS</p>"
        "            <ul>"
        "                <li>HDOS is now playable on any Java version</li>"
        "                <li>Fixed a bug seen in some DEs relating to window-grouping</li>"
        "            </ul>"
        "        </description>"
        "    </release>"
        "    <release version=\"0.4.0\" date=\"2023-11-03\">"
        "        <url type=\"details\">https://github.com/Adamcake/Bolt/releases/tag/0.4.0</url>"
        "        <description>"
        "            <p>Removed dependencies on local mime-type databases</p>"
        "        </description>"
        "    </release>"
        "    <release version=\"0.3.0\" date=\"2023-11-02\">"
        "        <url type=\"details\">https://github.com/Adamcake/Bolt/releases/tag/0.3.0</url>"
        "        <description>"
        "            <p>Added support for HDOS</p>"
        "            <ul>"
        "                <li>HDOS can now be launched, as long as a supported Java version is used (i.e. openjdk-11 or lower)</li>"
        "                <li>added \"scale\" setting for RuneLite</li>"
        "            </ul>"
        "        </description>"
        "    </release>"
        "    <release version=\"0.2.0\" date=\"2023-09-16\">"
        "        <url type=\"details\">https://github.com/Adamcake/Bolt/releases/tag/0.2.0</url>"
        "        <description>"
        "            <p>Fixed a bug selecting Java version when launching RuneLite</p>"
        "        </description>"
        "    </release>"
        "    <release version=\"0.1.0\" date=\"2023-09-02\">"
        "        <url type=\"details\">https://github.com/Adamcake/Bolt/releases/tag/0.1.0</url>"
        "        <description>"
        "            <p>First release of Bolt Launcher</p>"
        "            <ul>"
        "                <li>support for both legacy and new account login</li>"
        "                <li>linux-only support for RS3 and RuneLite</li>"
        "            </ul>"
        "        </description>"
        "    </release>"
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
        "<branding>"
        "    <color type=\"primary\" scheme_preference=\"light\">#e9ecf0</color>"
        "    <color type=\"primary\" scheme_preference=\"dark\">#415980</color>"
        "</branding>"
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

# Build plugin library
if(NOT BOLT_SKIP_LIBRARIES)
    add_subdirectory(src/library)
endif()

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

# select implementations for OS-specific behaviour
if(WIN32)
    set(WINDOW_LAUNCHER_OS_SPECIFIC src/browser/window_launcher_win.cxx)
    set(CLIENT_OS_SPECIFIC src/browser/client_win32.cxx)
    if(NOT BOLT_SKIP_LIBRARIES)
        set(LIB_IPC_OS_SPECIFIC src/library/ipc_win32.c)
    endif()
else()
    set(WINDOW_LAUNCHER_OS_SPECIFIC src/browser/window_launcher_posix.cxx)
    set(CLIENT_OS_SPECIFIC src/browser/client_posix.cxx)
    if(NOT BOLT_SKIP_LIBRARIES)
        set(LIB_IPC_OS_SPECIFIC src/library/ipc_posix.c)
    endif()
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
    add_executable(html_gen app/generator.cxx src/mime.cxx)
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
add_executable(bolt
    modules/fmt/src/format.cc src/main.cxx src/browser.cxx src/browser/app.cxx src/browser/client.cxx
    ${CLIENT_OS_SPECIFIC} src/browser/resource_handler.cxx src/browser/window_launcher.cxx
    ${WINDOW_LAUNCHER_OS_SPECIFIC} src/mime.cxx src/file_manager/directory.cxx client_cmake_gen.cxx
    ${LIB_IPC_OS_SPECIFIC} ${BOLT_FILE_MANAGER_LAUNCHER_GEN} ${BOLT_STUB_INJECT_CXX}
)
if(BOLT_STUB_INJECT_CXX)
    add_dependencies(bolt BOLT_STUB_INJECT_DEPENDENCY)
endif()

# Various build properties
if(BOLT_PLUGIN_LIB_NAME)
    target_compile_definitions(bolt PUBLIC BOLT_LIB_NAME="${BOLT_PLUGIN_LIB_NAME}" BOLT_PLUGINS)
endif()
target_include_directories(bolt PUBLIC ${CEF_ROOT} modules/fmt/include)
set_target_properties(bolt PROPERTIES CXX_STANDARD 20 CXX_EXTENSIONS OFF)
target_link_libraries(bolt PUBLIC libcef_dll_wrapper)
if(UNIX AND NOT APPLE)
    set_target_properties(bolt PROPERTIES INSTALL_RPATH "$ORIGIN")
    target_link_libraries(bolt PUBLIC "${CEF_ROOT}/${CMAKE_BUILD_TYPE}/libcef.so")
    target_link_libraries(bolt PUBLIC "X11")
    target_link_libraries(bolt PUBLIC "xcb")
    target_link_libraries(bolt PUBLIC "archive")
elseif(WIN32)
    set_target_properties(bolt PROPERTIES WIN32_EXECUTABLE TRUE)
    target_link_libraries(bolt PUBLIC "${CEF_ROOT}/${CMAKE_BUILD_TYPE}/libcef.lib")
    target_link_libraries(bolt PUBLIC "${CEF_ROOT}/${CMAKE_BUILD_TYPE}/cef_sandbox.lib")
    if(MSVC)
        set_property(TARGET bolt PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    endif()
elseif(APPLE)
    # TODO: mac support
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
    install(TARGETS java-proxy DESTINATION "${BOLT_CEF_INSTALLDIR}/bolt-launcher")
endif()

# Install commands for target dir layout - as per https://bitbucket.org/chromiumembedded/cef/wiki/Tutorial.md
if(APPLE)
    # TODO: mac support
else()
    install(TARGETS bolt DESTINATION "${BOLT_CEF_INSTALLDIR}/bolt-launcher")
    install(DIRECTORY "${CEF_ROOT}/${CMAKE_BUILD_TYPE}/" "${CEF_ROOT}/Resources/" DESTINATION "${BOLT_CEF_INSTALLDIR}/bolt-launcher" USE_SOURCE_PERMISSIONS)
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
