project('fishjelly', ['cpp', 'c'],
  version : '0.6',
  license : 'GPL-3.0',
  default_options : [
    'cpp_std=c++23',
    'warning_level=3',
    'werror=true',
    'optimization=2',
    'b_ndebug=if-release',
    'c_std=c99'
  ]
)

# Add third-party includes
inc = include_directories('third_party')

# Find Boost dependencies
boost_dep = dependency('boost', 
  modules : ['system', 'coroutine'],
  required : true
)

# Get git hash for embedding
git = find_program('git', required : false)
if git.found()
  git_hash = run_command(git, 'rev-parse', 'HEAD', check: false).stdout().strip()
  if git_hash == ''
    git_hash = 'unknown'
  endif
else
  git_hash = 'unknown'
endif

# Add subdirectory for source files
subdir('src')

# Add tests subdirectory if testing is enabled
if get_option('enable-tests')
  subdir('tests')
endif

# Find clang-tidy
clang_tidy = find_program('clang-tidy', required : false)

if clang_tidy.found()
  # Get all source files
  source_files = files(
    'src/socket.cc',
    'src/http.cc',
    'src/filter.cc',
    'src/log.cc',
    'src/mime.cc',
    'src/token.cc',
    'src/cgi.cc',
    'src/webserver.cc',
    'src/asio_server.cc',
    'src/asio_http_connection.cc',
    'src/asio_socket_adapter.cc',
    'src/footer_middleware.cc',
    'src/logging_middleware.cc',
    'src/security_middleware.cc',
    'src/compression_middleware.cc',
    'src/content_negotiator.cc'
  )
  
  # Add clang-tidy target
  run_target('clang-tidy',
    command : [clang_tidy, '-p', meson.current_build_dir(), source_files]
  )
  
  # Add clang-tidy with fixes
  run_target('clang-tidy-fix',
    command : [clang_tidy, '-p', meson.current_build_dir(), '--fix', source_files]
  )
else
  warning('clang-tidy not found, linting targets will not be available')
endif