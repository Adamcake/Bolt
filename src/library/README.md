# Plugin Library
This directory contains the source for the plugin library for RS3 (e.g. `libbolt-plugin.so`). If BOLT_SKIP_LIBRARIES is specified at build time, those libraries will not be built, and so the contents of this directory will be unused.

All API documentation is in the form of a Texinfo document in doc.texi. The docs aren't touched by the build system, you need to generate them yourself using `texi2any` if you want to get them. Here's the command I use:

```texi2any --html --no-split --no-number-sections --set-customization-variable='HIGHLIGHT_SYNTAX highlight' src/library/doc.texi```

Alternatively the docs for the latest stable version can be found at https://bolt.adamcake.com/doc.
