cmake_minimum_required(VERSION 3.14)
# C も有効にする（GoogleTest 1.15.2 の project() が C を使う。CXX だけだと、Visual Studio の
# ジェネレーターで C コンパイラーを判定できずに構成が止まる）
project(can_simulator C CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ソースを UTF-8 として扱う（日本語のコメントやメッセージを文字化けさせない）
add_compile_options($<$<CXX_COMPILER_ID:MSVC>:/utf-8>)

# テスト対象のロジックを静的ライブラリにまとめる（ソースは移動しない）
file(GLOB_RECURSE APP_CORE_SOURCES CONFIGURE_DEPENDS MFCApplication1/CanSim/*.cpp)
add_library(app_core STATIC ${APP_CORE_SOURCES})
target_include_directories(app_core PUBLIC MFCApplication1/CanSim)
# CanSim/*.cpp がインクルードする "pch.h"（MFC アプリのプリコンパイル済みヘッダー）を、
# tests/stubs の空のヘッダーで満たす。MFC には依存させない
target_include_directories(app_core BEFORE PRIVATE tests/stubs)
# アプリでは framework.h が NOMINMAX を定義している（std::min / std::max を使うため）。ここでも同じにする
target_compile_definitions(app_core PUBLIC NOMINMAX)

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