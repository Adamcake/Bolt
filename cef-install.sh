#!/bin/sh -eu

mkdir -p "${DESTDIR:-}/${MESON_INSTALL_PREFIX}/bolt"

cp -r "${CEF_BINARY_DISTRIB}"/Release/chrome-sandbox \
      "${CEF_BINARY_DISTRIB}"/Release/libcef.so \
      "${CEF_BINARY_DISTRIB}"/Release/snapshot_blob.bin \
      "${CEF_BINARY_DISTRIB}"/Release/v8_context_snapshot.bin \
      "${CEF_BINARY_DISTRIB}"/Resources/* \
      "${DESTDIR:-}/${MESON_INSTALL_PREFIX}/bolt"
