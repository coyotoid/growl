project(
    'growl',
    'c',
    meson_version: '>= 1.3.0',
    version: '0.1',
    default_options: [
        'buildtype=debugoptimized',
        'c_std=gnu11',
        'warning_level=2',
    ],
)

growl_sources = [
    'src/core/alien.c',
    'src/core/arena.c',
    'src/core/callable.c',
    'src/core/compiler.c',
    'src/core/dictionary.c',
    'src/core/disasm.c',
    'src/core/dynarray.c',
    'src/core/file.c',
    'src/core/gc.c',
    'src/core/hash.c',
    'src/core/lexer.c',
    'src/core/list.c',
    'src/core/native.c',
    'src/core/path.c',
    'src/core/print.c',
    'src/core/sleb128.c',
    'src/core/string.c',
    'src/core/table.c',
    'src/core/tuple.c',
    'src/core/value.c',
    'src/core/vm.c',
]

cc = meson.get_compiler('c')
m_dep = cc.find_library('m', required: false)

growl = executable(
    'growl',
    'src/main.c',
    growl_sources,
    dependencies: [m_dep],
    include_directories: ['src/include'],
    install: true,
)
