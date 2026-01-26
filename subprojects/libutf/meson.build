project('libutf', 'c')
add_project_arguments(
  '-Wno-missing-braces',
  '-Wno-parentheses',
  '-Wno-sign-compare',
  language: 'c'
)

inc = include_directories('.')

libutf = static_library(
  'utf',
  [
    'rune.c',
    'runestrcat.c',
    'runestrchr.c',
    'runestrcmp.c',
    'runestrcpy.c',
    'runestrdup.c',
    'runestrecpy.c',
    'runestrlen.c',
    'runestrncat.c',
    'runestrncmp.c',
    'runestrncpy.c',
    'runestrrchr.c',
    'runestrstr.c',
    'runetype.c',
    'utfecpy.c',
    'utflen.c',
    'utfnlen.c',
    'utfrrune.c',
    'utfrune.c',
    'utfutf.c',
  ],
)

libutf_dep = declare_dependency(
  include_directories: inc,
  link_with: libutf
)

