# Library sources (without main)
lib_sources = files(
  'socket.cc',
  'socket.h',
  'http.cc',
  'http.h',
  'filter.cc',
  'filter.h',
  'log.cc',
  'log.h',
  'mime.cc',
  'mime.h',
  'token.cc',
  'token.h',
  'cgi.cc',
  'cgi.h',
  'global.h',
  'mktime.c'
)

# Create static library for testing
fishjelly_lib = static_library('fishjelly',
  lib_sources,
  cpp_args : [
    '-DGIT_HASH="' + git_hash + '"',
    '-fstack-protector-strong',
    '-march=native',
    '-g',
    '-Wno-error=#warnings',
    '-Wno-deprecated-declarations'
  ],
  c_args : [
    '-Wno-shift-negative-value',
    '-Wno-deprecated-non-prototype'
  ]
)

# Build executable with main
executable('shelob',
  'webserver.cc',
  'webserver.h',
  link_with : fishjelly_lib,
  cpp_args : [
    '-DGIT_HASH="' + git_hash + '"',
    '-fstack-protector-strong',
    '-march=native',
    '-g',
    '-Wno-error=#warnings',
    '-Wno-deprecated-declarations'
  ],
  install : true
)

# Install man page
install_man('fishjelly.1')