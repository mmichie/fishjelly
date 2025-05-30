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