project(
  'growl',
  'c',
  meson_version : '>= 1.3.0',
  version : '0.1',
  default_options : ['buildtype=debugoptimized', 'c_std=gnu11', 'warning_level=3'],
)

libutf = subproject('libutf')
libutf_dep = libutf.get_variable('libutf_dep')

sources = [
  'src/arena.c',
  'src/chunk.c',
  'src/compile.c',
  'src/debug.c',
  'src/dictionary.c',
  'src/file.c',
  'src/lexer.c',
  'src/object.c',
  'src/gc.c',
  'src/parser.c',
  'src/primitive.c',
  'src/print.c',
  'src/stream.c',
  'src/string.c',
  'src/userdata.c',
  'src/vm.c',
  'src/vendor/linenoise.c',
  'src/vendor/mpc.c',
  'src/vendor/yar.c',
]

exe = executable(
  'growl',
  'src/main.c', sources,
  dependencies : [libutf_dep],
  install : true,
)
