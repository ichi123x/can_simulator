cmake_minimum_required(VERSION 3.14)
project(can_simulator CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ソースを UTF-8 として扱う（日本語のコメントやメッセージを文字化けさせない）
add_compile_options($<$<CXX_COMPILER_ID:MSVC>:/utf-8>)

# テスト対象のロジックを静的ライブラリにまとめる（ソースは移動しない）
file(GLOB_RECURSE APP_CORE_SOURCES CONFIGURE_DEPENDS MFCApplication1/CanSim/*.cpp)
add_library(app_core STATIC ${APP_CORE_SOURCES})
target_include_directories(app_core PUBLIC MFCApplication1/CanSim)
# CanSim/ のソースが取り込む pch.h（MFC 依存）の代わりに、空のヘッダーを使う
target_include_directories(app_core PRIVATE tests/stubs)

# GoogleTest の取得（バージョンを固定する）
include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.zip
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
# MSVC でランタイム（/MD・/MDd）を製品側と揃える
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()
add_subdirectory(tests)