project('Bolt', 'cpp', subproject_dir: 'cef', default_options: ['buildtype=debugoptimized', 'cpp_std=c++17', 'prefix=/opt', 'bindir=bolt'])
cmake = import('cmake')
libcef_dll_wrapper = cmake.subproject('dist').dependency('libcef_dll_wrapper')
libcef = custom_target('libcef', output: 'libcef.so', command: ['./cef/install.sh', '@SOURCE_ROOT@', '@OUTDIR@'])
meson.add_install_script('cef/install.sh')
executable(
    'bolt',
    'src/main.cxx', 'src/browser/app.cxx', 'src/browser.cxx', 'src/browser/client.cxx', 'src/browser/window_delegate.cxx',
    'src/browser/browser_view_delegate.cxx', 'src/browser/handler/life_span_handler.cxx',
    dependencies: [dependency('fmt'), dependency('tesseract'), dependency('x11'), libcef_dll_wrapper],
    install: true,
    install_rpath: '.',
    link_with: [libcef]
)
