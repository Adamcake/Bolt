project('Bolt', 'cpp', subproject_dir: 'cef', default_options: ['buildtype=debugoptimized', 'cpp_std=c++17', 'cpp_eh=none', 'prefix=/opt', 'bindir=bolt'])
cmake = import('cmake')
libcef_dll_wrapper = cmake.subproject('dist').dependency('libcef_dll_wrapper')
libcef = custom_target('libcef', output: 'libcef.so', command: ['./cef/install.sh', '@SOURCE_ROOT@', '@OUTDIR@'])
meson.add_install_script('cef/install.sh')

deps = [libcef_dll_wrapper, dependency('fmt'), dependency('tesseract')]
if build_machine.system() == 'linux'
    deps += [dependency('x11')]
endif
executable(
    'bolt',
    'src/main.cxx', 'src/browser/app.cxx', 'src/browser.cxx', 'src/browser/client.cxx', 'src/browser/dom_visitor.cxx',
    'src/browser/app_frame_data.cxx',
    cpp_args: ['-Wall'],
    dependencies: deps,
    install: true,
    install_rpath: '.',
    link_with: [libcef]
)
