project(
    'growl',
    'c',
    'cpp',
    meson_version: '>= 1.3.0',
    version: '0.1',
    default_options: [
        'buildtype=debugoptimized',
        'c_std=gnu11',
        'cpp_std=c++20',
        'warning_level=2',
    ],
)

libutf = subproject('libutf')
libutf_dep = libutf.get_variable('libutf_dep')

growl_sources = [
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
    'src/vendor/yar.c',
]

growl = executable(
    'growl',
    'src/main.c',
    growl_sources,
    dependencies: [libutf_dep],
    install: true,
)

growlnext_sources = [
    'next/core/alien.c',
    'next/core/arena.c',
    'next/core/callable.c',
    'next/core/compiler.c',
    'next/core/dictionary.c',
    'next/core/disasm.c',
    'next/core/file.c',
    'next/core/gc.c',
    'next/core/hash.c',
    'next/core/lexer.c',
    'next/core/list.c',
    'next/core/native.c',
    'next/core/print.c',
    'next/core/sleb128.c',
    'next/core/string.c',
    'next/core/table.c',
    'next/core/tuple.c',
    'next/core/value.c',
    'next/core/vm.c',
    'next/main.c',
]

cc = meson.get_compiler('c')
m_dep = cc.find_library('m', required: false)

growlnext = executable(
    'growlnext',
    growlnext_sources,
    dependencies: [m_dep],
    include_directories: ['next/include'],
    install: true,
)
