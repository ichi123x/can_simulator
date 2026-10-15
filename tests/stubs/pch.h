// pch.h（tests/stubs）
// MFC アプリのプリコンパイル済みヘッダーの代わりの、空のヘッダー。
// CanSim/*.cpp は決まりとして "pch.h" をインクルードする。CMake でテスト用にビルドするときは
// これで満たす（CanSim が MFC に依存していないことの確かめにもなる）。
#pragma once
