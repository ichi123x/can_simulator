include(GoogleTest)

# add_gtest(<名前> <unit|integration|acceptance|slow ...> SOURCES <ファイル...>)
function(add_gtest name)
  cmake_parse_arguments(ARG "" "" "SOURCES" ${ARGN})
  # support/console_utf8.cpp は、日本語の出力を Windows のコンソールで文字化けさせないため、すべてのテストに入れる
  add_executable(${name} ${ARG_SOURCES} ${CMAKE_CURRENT_SOURCE_DIR}/support/console_utf8.cpp)
  target_link_libraries(${name} PRIVATE app_core GTest::gtest_main GTest::gmock)
  target_include_directories(${name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
  gtest_discover_tests(${name} PROPERTIES LABELS "${ARG_UNPARSED_ARGUMENTS}")
endfunction()

add_gtest(smoke_test unit SOURCES unit/smoke_test.cpp)
add_gtest(feature_001_spec_tests unit SOURCES unit/feature_001_spec_tests.cpp)
add_gtest(feature_001_edge_tests unit SOURCES unit/feature_001_edge_tests.cpp)
