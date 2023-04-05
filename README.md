# Bolt
An alternative, CEF-based client for running your favourite helper applications for your favourite MMO.

## Building
If you just want to get Bolt installed then you don't need to build it from source! Check the "Releases" section on the right.

But if you do want to build from source, the first thing you should know is that Bolt is based on [Chromium Embedded Framework](https://bitbucket.org/chromiumembedded/cef) (CEF), so to build it, you'll first need to build or otherwise obtain a binary distribution of Chromium. Bolt was developed based on Chromium **111.0.5563.65**; there will be issues with newer or older versions. When building, all of the patches in the `cef/patch` directory should be applied to the chromium/src directory before running the ninja build command; four of them are fixes required to compile with llvm15 and the final one adds a transparency feature which is used by Bolt.

Place the entire binary distribution folder inside the `cef` directory with the name "dist", or create a symbolic link with the same effect.

You'll also need the following dependencies:
- [fmt](https://fmt.dev) (`fmt-devel` or `libfmt-dev` on most Linux package managers)
- [tesseract](https://tesseract-ocr.github.io) (`tesseract-devel`, `tesseract-ocr-devel` or `libtesseract-dev` on most Linux package managers)
- Linux:
  - X11 development libraries (`libX11-devel` or `libx11-dev` on most package managers)
  - xcb development libraries (`libxcb-devel` or `libxcb-dev` on most package managers)
  - development libraries for libxcb-composite and libxcb-record if your package manager has them separate from the base xcb package

Once that's done, simply build with meson: `meson setup build` then `meson compile -C buiid`. If it compiled successfully you'll be able to run Bolt from inside the `build` directory. On a non-Windows platform, you may also install it by running `meson install -C build`.
