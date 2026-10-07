# CMake build of the CanSim core and its GoogleTest suite (feature 030).
#
# The MFC application is still built with MFCApplication1.sln; this file only builds the
# UI-independent simulation core (MFCApplication1/CanSim/*.cpp, not moved) as a static
# library and the tests in tests/.
#
#   cmake -S . -B build
#   cmake --build build --config Release
#   ctest --test-dir build -C Release --output-on-failure
cmake_minimum_required(VERSION 3.22)
project(can_simulator CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# tests/ のソースは BOM なし UTF-8 の日本語コメント・メッセージを含む。日本語版 Windows の MSVC は
# 既定でコードページ 932 として読み、文字化けと警告 C4819 になるので UTF-8 として扱わせる。
add_compile_options($<$<CXX_COMPILER_ID:MSVC>:/utf-8>)

file(GLOB CANSIM_SOURCES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/MFCApplication1/CanSim/*.cpp")

add_library(cansim_core STATIC ${CANSIM_SOURCES})

# The CanSim sources include "pch.h" by convention; the empty one under tools/cansim_runner
# stands in for the MFC project's precompiled header.
target_include_directories(cansim_core
  PUBLIC  "${CMAKE_SOURCE_DIR}/MFCApplication1/CanSim"
  PRIVATE "${CMAKE_SOURCE_DIR}/tools/cansim_runner"
)

# framework.h defines NOMINMAX in the application; do the same here.
target_compile_definitions(cansim_core PUBLIC NOMINMAX)

if(MSVC)
  target_compile_options(cansim_core PRIVATE /W3 /EHsc)
endif()

enable_testing()
add_subdirectory(tests)
