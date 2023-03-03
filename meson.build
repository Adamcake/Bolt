project('Bolt', 'cpp', default_options: ['buildtype=debugoptimized', 'cpp_std=c++17', 'prefix=/opt', 'bindir=bolt'])
executable('bolt', 'src/main.cxx', install: true, dependencies: [dependency('fmt'), dependency('tesseract')])
meson.add_install_script('cef-install.sh')
