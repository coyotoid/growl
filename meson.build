project(
  'growl',
  'c',
  meson_version : '>= 1.3.0',
  version : '0.1',
  default_options : ['buildtype=debugoptimized', 'c_std=c99', 'warning_level=3'],
)

sources = [
  'src/chunk.c',
  'src/compile.c',
  'src/gc.c',
  'src/object.c',
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
