project('lispy', 'c', default_options: ['c_std=c99'])

editline_dep = dependency('libedit')
m_dep = meson.get_compiler('c').find_library('m')

executable('parsing',
  sources: ['parsing.c', 'mpc.c'],
  dependencies: [editline_dep, m_dep],
)
