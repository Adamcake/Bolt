project('Bolt', 'cpp', default_options: ['buildtype=debugoptimized', 'cpp_std=c++17', 'prefix=/opt', 'bindir=bolt'])
libcef = custom_target('libcef', output: 'libcef.so', command: ['./cef/install.sh', '@SOURCE_ROOT@', '@OUTDIR@'])
meson.add_install_script('cef/install.sh')
executable(
    'bolt',
    'src/main.cxx', 'src/browser/app.cxx', 'src/browser/client.cxx', 'src/browser/window_delegate.cxx',
    'src/browser/browser_view_delegate.cxx', 'src/browser/handler/life_span_handler.cxx',
    dependencies: [dependency('fmt'), dependency('tesseract'), dependency('x11')],
    include_directories: 'cef/dist',
    install: true,
    install_rpath: '.',
    link_with: [libcef]
)
