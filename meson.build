project('GsGame', 'c', license: ['MIT'], default_options: ['c_std=c18'])

opengl_dep = dependency('opengl')

c_compiler = meson.get_compiler('c')
winmm_lib = c_compiler.find_library('winmm', required: true)

executable('Game', sources: ['main.c'], include_directories: ['./gunslinger'], dependencies: [opengl_dep, winmm_lib], c_args: ['-Wno-incompatible-pointer-types', '-D_CRT_SECURE_NO_WARNINGS'])