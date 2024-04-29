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
- `bunx tailwindcss -o src/assets/output.css --minify && bun run minify`
- `cd ..`

Now you can start building. Open a command window or terminal in the root directory of this repository, then follow the build instructions for your platform.

### Linux

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

### Windows

Windows builds have only been tested using Visual Studio 2022 (a.k.a. Visual Studio 17) in Release mode, per recommendations by chromium/cef. Other configurations may work but have not been tested.
- `cmake -S . -B build -G "Visual Studio 17"`
  - note: use `-A Win32` instead for 32-bit targets
- Open the .sln file created in the `build` directory and go to "Build" > "Build Solution"
- Create a new directory and copy all of the following into it: bolt.exe, and the entire contents of the "Release" and "Resources" directories from your CEF distribution. Then you can run bolt.exe from that directory.

### Mac

Not yet supported

<https://cef-builds.spotifycdn.com/index.html#macosarm64>

```bash
brew install cmake pkg-config gtk+3 libarchive
cmake -S . -B build -D BOLT_SKIP_LIBRARIES=True -D CMAKE_BUILD_TYPE=Release -D CEF_ROOT=$HOME/Downloads/cef_binary_122.1.13+gde5b724+chromium-122.0.6261.130_macosarm64_minimal
cmake --build build
```

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
