# Table of Contents
- [Table of Contents](#table-of-contents)
- [Bolt](#bolt)
  - [Contact](#contact)
  - [Installing](#installing)
  - [Building](#building)
    - [Linux](#linux)
    - [Windows](#windows)
    - [Mac](#mac)
  - [Maintenance](#maintenance)
  - [Troubleshooting](#troubleshooting)
  - [Credit](#credit)
  - [Disclaimer](#disclaimer)

# Bolt

A third-party launcher for your favourite MMO

## Contact

Bolt, as well as the other Linux community projects, is being discussed at [7orm's Discord server](https://discord.gg/aX7GT2Mkdu). If you want to talk about development or need help getting set up, that's the place to go.

## Installing

For Linux/Steamdeck users, Bolt is available on the following package managers:
- flatpak: `com.adamcake.Bolt`
- AUR: `bolt-launcher`

Instructions on installing flatpak can be found [here](https://flatpak.org/setup/)
Others should see the "releases" section on the right.

## Building

If you just want to get Bolt installed then you don't need to build it from source! See the "Installing" section.

But if you do want to build from source, the first thing you should know is that Bolt is based on [Chromium Embedded Framework](https://bitbucket.org/chromiumembedded/cef) (CEF), so to build it, you'll first need either to [build](https://bitbucket.org/chromiumembedded/cef/wiki/MasterBuildQuickStart.md) or [download](https://adamcake.com/cef) a binary distribution of CEF.

Clone this repository with submodules:
- `git clone --recurse-submodules https://github.com/Adamcake/Bolt.git`

If you accidentally cloned without submodules (no `modules` directory), you can checkout submodules like so:
- `git submodule update --init --recursive`

Place your entire CEF binary distribution folder inside the `cef` directory with the name "dist", or create a symbolic link with the same effect.

If building on Linux, the following are required:
- X11 development libraries (`libX11-devel` or `libx11-dev` on most package managers)
- xcb development libraries (`libxcb-devel` or `libxcb1-dev` on most package managers)
- libarchive development libraries (`libarchive-devel` or `libarchive-dev` on most package managers)
- LuaJIT development libraries (`luajit-devel` or `luajit-dev` on most package managers) (OPTIONAL - only needed if you want to build the plugin library)

OPTIONAL: build the frontend. Bolt's html frontend is already committed to this repo in `app/dist`, so building it yourself isn't necessary. If you want to build it from source anyway then see [app/README.md](https://github.com/Adamcake/Bolt/tree/master/app#app) for full details on how this build system works, but here's the short version:
- `cd app/`
- `npm install -g bun` (needs root access on Linux)
- `bun install`
- `bun run build`
- `cd ..`

Now you can start building. Open a command window or terminal in the root directory of this repository, then follow the build instructions for your platform.

### Linux

If building with GCC, you need version 13.1 or later. If building with Clang, you need version 14 or later.
- `cmake -S . -B build -D CMAKE_BUILD_TYPE=Release`
  - note: you'll need to specify either `-D BOLT_LUAJIT_INCLUDE_DIR=/usr/include/luajit-2.1` OR `-D BOLT_SKIP_LIBRARIES=1` depending on whether you want to build the plugin library
  - note: build types "Debug" and "Release" are supported
  - note: if you have Ninja installed, specify `-G Ninja` for much faster builds
  - note: specify CC and CXX env variables at this stage to direct cmake to the C and C++ compilers you want it to use
- `cmake --build build`
- `cmake --install build --prefix build`
  - note: the last line creates a staging build in the `build` directory, this needs to be done between changing and running the program every time
  - note: if a `--prefix` is not specified, Bolt will be installed to /usr/local, requiring root privileges

After that, the helper script `./build/bolt.sh` can be used to launch Bolt from its staging location.

Lastly, here are some optional settings which may be useful if you're integrating with another build system:
- `-D BOLT_META_NAME=`: the application's metadata name; default is "BoltLauncher"
- `-D BOLT_CEF_INSTALLDIR=`: the directory (after the install prefix) where the binaries will be installed; default is "opt"
- `-D BOLT_BINDIR=`: the directory (after the install prefix) where the `bolt` binary will be installed; default is "usr/local/bin"
- `-D BOLT_LIBDIR=`: the directory (after the install prefix) where bolt's libraries, if any, will be installed; default is "usr/local/lib"
- `-D BOLT_SHAREDIR=`: the directory (after the install prefix) where metadata will be installed; default is "usr/local/share"
- `-D BOLT_CEF_INCLUDEPATH=`: the absolute path to the location **CONTAINING** CEF's "include" directory (but not to the include directory itself); default is the value of CEF_ROOT
- `-D BOLT_LIBCEF_DIRECTORY=`: the absolute path to a directory containing a pre-installed copy of libcef.so for linking against. if set, the directory containing libcef.so in CEF_ROOT will not be installed, and will therefore be unused. if unset, libcef.so will be found in the Debug or Release directory of CEF_ROOT.
- `-D BOLT_CEF_RESOURCEDIR_OVERRIDE=`: the absolute path to a pre-installed CEF resource directory. if set, the path will be hardcoded into the application as a string, and the "Resources" directory from CEF_ROOT will not be installed, and will therefore be unused.
- `-D BOLT_CEF_DLLWRAPPER=`: the absolute path to a `.a` file which will be used instead of compiling libcef_dll_wrapper out of CEF_ROOT
- `-D BOLT_SKIP_RPATH=1`: instead of setting bolt's runpath to $ORIGIN, it won't be set at all - useful only if you're intending to set it yourself

### Windows

Windows builds have only been tested using Visual Studio 2022 (a.k.a. Visual Studio 17) in Release mode, per recommendations by chromium/cef. Other configurations may work but have not been tested.
- optional: if you want to build the plugin library, start by cloning [LuaJIT](https://github.com/LuaJIT/LuaJIT) and build it by running msvcbuild.bat.
- `cmake -S . -B build -G "Visual Studio 17" -D CMAKE_BUILD_TYPE=Release`
  - note: specify `-D BOLT_CEF_INSTALLDIR=[your filepath]` to specify the install location when running the final command. Must be an absolute path. The default is `C:\bolt-launcher\`.
  - note: depending on whether you want the plugin library, you need to specify either `-D BOLT_LUAJIT_DIR=path/to/luajit/src/`, where the path is the location of lua51.dll and the various lua headers, or `-D BOLT_SKIP_LIBRARIES=1`
  - note: use `-A Win32` for 32-bit targets
- `cmake --build build --config Release`
- `cmake --install build/`

Run bolt.exe from the install location.

### Mac

Not yet supported

## Maintenance
When doing the initial cmake setup step, the following options exist which you may find useful. These are to be used for local development only.
- `-D BOLT_HTML_DIR=/some/directory`: the location of the launcher's internal webpage content, `$PWD/app/dist` by default (note: must be an ABSOLUTE path)
- `-D BOLT_DEV_SHOW_DEVTOOLS=1`: enables chromium developer tools for the launcher
- `-D BOLT_DEV_LAUNCHER_DIRECTORY=1`: instead of embedding the contents of BOLT_HTML_DIR into the output executable, the files will be served from disk at runtime; on supported platforms the launcher will automatically reload the page when those files are changed

## Troubleshooting

- LuaJIT
  - When building, you may run into an error with 'luajit'. [website](https://luajit.org/index.html)
  - You can either install it by following the instructions on their website (or just via your package manager), or specify `-D BOLT_SKIP_LIBRARIES=1` when building.
  - Keep in mind LuaJIT is based on Lua 5.1. If you have a Lua version later than that installed and Bolt is picking it up, the build will fail.
- libcrypto.so.1.1
  - This comes from openssl1.1, which is reaching deprecation but is still widely used.
  - Install it with your package manager; it is usually called `openssl1.1-devel` or something similar.
- JDK17
  - When attempting to launch, you may see an error about 'jdk17' in the console.
  - This can be solved by installing a couple packages with your package manager.  
    Something similar to `java-17-openjdk` & `java-17-openjdk-devel`.
- JAVA_HOME
  - Another launch issue you may see in the console.
  - This is solved by setting the JAVA_HOME environment variable.
  - This is usually located in /usr/lib/jvm, so, it might look like this:  
    `export JAVA_HOME="/usr/lib/jvm/java-17-openjdk-17.0.9.0.9-3.fc39.x86_64"`

## Contributing

There's not much to say here except that I have fairly high standards, so don't give me code that's messy or unfinished. Use a development environment that supports [editorconfig](https://editorconfig.org/) before starting work. If committing to the frontend UI then see [app/README.md](https://github.com/Adamcake/Bolt/tree/master/app#app), especially the Linting & Formatting section, for some UI-specific guidelines. Follow the general etiquette of git, i.e. commit messages 50 characters maximum, all changes in a commit must be relevant to the commit, and all commits in a PR must be relevant to the PR.

## Credit

Icons - [Kia](https://twitter.com/KiaWildin)  
Flatpak integration - [@nmlynch94](https://github.com/nmlynch94)  
Most of the UI - [@smithcol11](https://github.com/smithcol11)

## Disclaimer

Bolt is an unofficial third-party project and is not in any way affiliated with any of the games or companies it interacts with. Said games and companies are not responsible for any problems with Bolt nor any damage caused by using Bolt.

Bolt is NOT a game client. It simply downloads and runs unmodified game clients. Bolt has absolutely no ability to modify or automate gameplay.
