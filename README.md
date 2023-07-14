# Bolt
A third-party launcher and helper for your favourite MMO

## Building
If you just want to get Bolt installed then you don't need to build it from source! Check the "Releases" section on the right.

But if you do want to build from source, the first thing you should know is that Bolt is based on [Chromium Embedded Framework](https://bitbucket.org/chromiumembedded/cef) (CEF), so to build it, you'll first need either to [build](https://bitbucket.org/chromiumembedded/cef/wiki/MasterBuildQuickStart.md) or [download](https://adamcake.com/cef) a binary distribution of CEF.

Clone this repository with submodules:
- `git clone --recurse-submodules https://github.com/Adamcake/Bolt.git`

If you accidentally cloned without submodules (no `modules` directory), you can checkout submodules like so:
- `git submodule update --init --recursive`

Place your entire CEF binary distribution folder inside the `cef` directory with the name "dist", or create a symbolic link with the same effect.

If building on Linux, install the following dependencies:
- X11 development libraries (`libX11-devel` or `libx11-dev` on most package managers)
- libarchive development libraries (`libarchive-devel` or `libarchive-dev` on most package managers)

Once that's done, you can start building. Open a command window or terminal in the root directory of this repository, then follow the build instructions for your platform.

### Linux
- `cmake -S . -B build -D CMAKE_BUILD_TYPE=Release`
  - note: build types "Debug" and "Release" are supported
  - note: if you have Ninja installed, specify `-G Ninja` for much faster builds
  - note: specify CC and CXX env variables at this stage to direct cmake to the C and C++ compilers you want it to use
- `cmake --build build`
- `cmake --install build --prefix build`
  - note: if a `--prefix` is not specified, Bolt will be installed to /usr/local, requiring root privileges

After that, the helper script `./build/bolt.sh` can be used to launch Bolt from its staging location.

### Windows
Windows builds have only been tested using Visual Studio 2022 IDE as Release builds. Other configurations may work but have not been tested.
- `cmake -S . -B build -G "Visual Studio 17" -A x64`
  - note: use `-A Win32` instead for 32-bit targets
- Open the .sln file in the `build` directory with Visual Studio and press F5 to build
- Create a new directory and copy all of the following into it: bolt.exe, the entire contents of the "Release" and "Resources" directories from your CEF distribution, and the "html" folder from this repository. Then you can run bolt.exe from that directory.

### Mac
Not yet supported

## Disclaimer
Bolt is an unofficial third-party project and is not in any way affiliated with any of the games or companies it interacts with. Said games and companies are not responsible for any problems with Bolt nor any damage caused by using Bolt.

Bolt is NOT a game client. It simply downloads and runs unmodified game clients. Bolt has absolutely no ability to modify or automate gameplay.
