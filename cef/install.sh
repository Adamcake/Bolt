#!/bin/sh -eu

# This is a Meson custom target and install script. You probably don't want to run this directly.

if [ $# -eq 2 ]; then
    src="$1"
    out="$2"
else
    src="${MESON_SOURCE_ROOT}"
    out="${DESTDIR:-}/${MESON_INSTALL_PREFIX}/bolt"
fi
mkdir -p "${out}"
cp -r "${src}"/cef/dist/Release/*\
      "${src}"/cef/dist/Resources/*\
      "${out}"
