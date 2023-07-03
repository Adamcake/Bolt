# Bolt
A third-party launcher and helper for your favourite MMO

## Building
If you just want to get Bolt installed then you don't need to build it from source! Check the "Releases" section on the right.

But if you do want to build from source, the first thing you should know is that Bolt is based on [Chromium Embedded Framework](https://bitbucket.org/chromiumembedded/cef) (CEF), so to build it, you'll first need to build or otherwise obtain a binary distribution of CEF. Official builds of Bolt are currently based on Chromium **114.0.5735.134**. Place the entire binary distribution folder inside the `cef` directory with the name "dist", or create a symbolic link with the same effect.

You'll also need the following dependencies:
- [fmt](https://fmt.dev) (`fmt-devel` or `libfmt-dev` on most Linux package managers)
- Linux:
  - X11 development libraries (`libX11-devel` or `libx11-dev` on most package managers)

Once that's done, simply build with cmake as you usually would. It's recommended to build with the same compiler you compiled CEF with; specify CC and CXX env vars to point to your C and C++ compiler respectively, and optionally supply `-G Ninja` to the first arg if you have Ninja installed and would prefer to use it over Make.
- `cmake -S . -B build -D CMAKE_BUILD_TYPE=Release`
- `cmake --build build`
- `cmake --install build --prefix build`

Then you can run Bolt by running `build/bolt.sh`.

If the first listed command doesn't work, it's advised to delete the `build/` directory before trying again. If you make changes and want to rebuild, rerun only the second and third commands. To install permanently (in Program Files on Windows or /usr/local anywhere else), run the third command without the --prefix arg.

## Disclaimer
Bolt is an unofficial third-party project and is not in any way affiliated with any of the games or companies it interacts with. Said games and companies are not responsible for any problems with Bolt nor any damage caused by using Bolt.

Bolt is NOT a game client. It simply downloads and runs unmodified game clients. Bolt has absolutely no ability to modify or automate gameplay.
