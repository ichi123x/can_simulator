# GoogleTest suite for cansim_core (feature 030).

include(FetchContent)
FetchContent_Declare(
  googletest
  URL      https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
  URL_HASH SHA256=1F357C27CA988C3F7C6B4BF68A9395005AC6761F034046E9DDE0896E3ABA00E4
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
# Use the dynamic CRT, like the MFC application.
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

file(GLOB TEST_SOURCES CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/feature_*_tests.cpp")

add_executable(runTests ${TEST_SOURCES})

target_include_directories(runTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/support")

target_link_libraries(runTests PRIVATE cansim_core gtest_main)

# Inputs are located at configure time so the tests do not depend on the working directory.
target_compile_definitions(runTests PRIVATE
  CANSIM_SAMPLE_DIR="${CMAKE_SOURCE_DIR}/MFCApplication1/CanSim/SampleConfigs"
  CANSIM_TEST_TMP_DIR="${CMAKE_BINARY_DIR}/test_tmp"
  _CRT_SECURE_NO_WARNINGS
)

if(MSVC)
  target_compile_options(runTests PRIVATE /W3 /EHsc)
endif()

# 実行時間で判定するテスト（スイート名が Slow で終わるもの）にはラベル slow を付ける。
# Debug では遅いので、ctest -C Debug -LE slow で外す。Release では全件実行する。
include(GoogleTest)
gtest_discover_tests(runTests DISCOVERY_MODE PRE_TEST TEST_FILTER "-*Slow.*")
gtest_discover_tests(runTests DISCOVERY_MODE PRE_TEST TEST_FILTER "*Slow.*"
  PROPERTIES LABELS slow)
