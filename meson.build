project(
  'growl',
  'c',
  meson_version : '>= 1.3.0',
  version : '0.1',
  default_options : ['buildtype=debugoptimized', 'c_std=c11', 'warning_level=3'],
)

sources = [
  'src/arena.c',
  'src/chunk.c',
  'src/compile.c',
  'src/debug.c',
  'src/dictionary.c',
  'src/object.c',
  'src/gc.c',
  'src/parser.c',
  'src/print.c',
  'src/vm.c',
  'src/vendor/mpc.c',
  'src/vendor/yar.c',
]

exe = executable(
  'growl',
  'src/main.c', sources,
  install : true,
)
