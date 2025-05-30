# Source files
sources = files(
  'webserver.cc',
  'webserver.h',
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

# Build executable with additional flags
executable('shelob',
  sources,
  cpp_args : [
    '-DGIT_HASH="' + git_hash + '"',
    '-fstack-protector-strong',
    '-march=native',
    '-g',
    '-Wno-error=#warnings'
  ],
  c_args : [
    '-Wno-shift-negative-value',
    '-Wno-deprecated-non-prototype'
  ],
  install : true
)

# Install man page
install_man('fishjelly.1')