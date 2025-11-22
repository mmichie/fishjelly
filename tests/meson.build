# GoogleTest dependency
gtest_dep = dependency('gtest', main: true, required: false)

if not gtest_dep.found()
  # Try to find GoogleTest using pkg-config
  gtest_dep = dependency('gtest_main', required: false)
endif

if gtest_dep.found()
  # Test files
  test_files = [
    'test_token.cc',
    'test_http.cc',
    'test_mime.cc',
    'test_filter.cc',
    'test_content_negotiator.cc'
  ]

  # Create test executables
  foreach test_file : test_files
    test_name = test_file.split('.')[0].replace('test_', '')
    test_exe = executable(test_name + '_test',
      test_file,
      dependencies : gtest_dep,
      link_with : fishjelly_lib,
      cpp_args : ['-DGIT_HASH="test"', '-Wno-deprecated-declarations', '-Wno-error=#warnings']
    )
    test(test_name, test_exe)
  endforeach
else
  warning('GoogleTest not found. Tests will not be built.')
  warning('Install GoogleTest: sudo apt install libgtest-dev (Ubuntu) or brew install googletest (macOS)')
endif