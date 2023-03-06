# Bolt
An alternative, CEF-based client for running your favourite helper applications for your favourite MMO.

# Building
If you just want to get Bolt installed then you don't need to build it from source! Check the "Releases" section on the right.

But if you do want to build from source, the first thing you should know is that Bolt is based on [Chromium Embedded Framework](https://bitbucket.org/chromiumembedded/cef) (CEF), so to build it, you'll first need to build or otherwise obtain a binary distribution of Chromium. Place the entire distribution folder into the `cef` directory and rename it to `dist`. Alternatively you can put a symbolic link with the name `dist` if you prefer.
You'll also need the following dependencies:
- [fmt](https://fmt.dev) (`fmt-devel` or `libfmt-dev` on most Linux package managers)
- [tesseract](https://tesseract-ocr.github.io) (`tesseract-devel`, `tesseract-ocr-devel` or `libtesseract-dev` on most Linux package managers)
- For Linux, X11 development libraries (`x11-devel` or `libx11-dev` on most Linux package managers)

Once that's done, simply build with meson: `meson setup build` then `meson compile -C buiid`. If it compiled successfully you'll be able to run Bolt from inside the `build` directory. On a non-Windows platform, you may also install it by running `meson install -C build`.
