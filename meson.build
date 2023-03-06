project('Bolt', 'cpp', default_options: ['buildtype=debugoptimized', 'cpp_std=c++17', 'prefix=/opt', 'bindir=bolt'])
libcef = custom_target('libcef', output: 'libcef.so', command: ['./cef/install.sh', '@SOURCE_ROOT@', '@OUTDIR@'])
meson.add_install_script('cef/install.sh')
executable(
    'bolt',
    'src/main.cxx', 'src/cef/app.cxx',
    dependencies: [dependency('fmt'), dependency('tesseract'), dependency('x11')],
    include_directories: 'cef/dist',
    install: true,
    install_rpath: '.',
    link_with: [libcef]
)
