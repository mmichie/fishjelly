# Library sources (without main)
lib_sources = files(
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
  'asio_server.cc',
  'asio_server.h',
  'asio_http_connection.cc',
  'asio_http_connection.h',
  'http_output_interface.h',
  'asio_socket_adapter.cc',
  'asio_socket_adapter.h',
  'middleware.h',
  'footer_middleware.h',
  'footer_middleware.cc',
  'logging_middleware.h',
  'logging_middleware.cc',
  'security_middleware.h',
  'security_middleware.cc',
  'compression_middleware.h',
  'compression_middleware.cc',
  'content_negotiator.h',
  'content_negotiator.cc',
  'auth.h',
  'auth.cc',
  'ssl_context.h',
  'ssl_context.cc',
  'asio_ssl_server.h',
  'asio_ssl_server.cc',
  'websocket_handler.h',
  'websocket_handler.cc',
  'http2_server.h',
  'http2_server.cc'
)

# Build dependency list
deps = [boost_dep, openssl_dep]
if nghttp2_dep.found()
  deps += [nghttp2_dep]
endif

# Create static library for testing
fishjelly_lib = static_library('fishjelly',
  lib_sources,
  dependencies : deps,
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
  dependencies : deps,
  include_directories : inc,
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

# Build middleware demo
executable('middleware_demo',
  'middleware_demo.cc',
  link_with : fishjelly_lib,
  dependencies : deps,
  include_directories : inc,
  cpp_args : [
    '-DGIT_HASH="' + git_hash + '"',
    '-fstack-protector-strong',
    '-march=native',
    '-g',
    '-Wno-error=#warnings',
    '-Wno-deprecated-declarations'
  ],
  install : false
)

