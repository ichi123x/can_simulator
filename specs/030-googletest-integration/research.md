# Research: GoogleTest/CTest による自動テストの導入

## R-01: C++ 規格

- **Decision**: C++14（`CMAKE_CXX_STANDARD 14`、拡張なし）。
- **Rationale**: `checks.exe` は `/std:c++14`、`.vcxproj` の既定も同等。GoogleTest 1.14 は C++14 以上で足りる。コアを実アプリと同じ規格でビルドすることで「テストでだけ通る」を避ける。
- **Alternatives**: 既存の雛形の C++17 → アプリ側と食い違うため不採用。

## R-02: GoogleTest の入手

- **Decision**: `FetchContent`（v1.14.0 の URL 固定 + `URL_HASH SHA256=` を実装時に記録）。`gtest_force_shared_crt ON`（MFC の動的 CRT と /MD 系で揃える）。`INSTALL_GTEST OFF`。
- **Rationale**: 開発者が事前準備せずに構成できる（FR-005）。バージョン・ハッシュ固定で再現できる。既存雛形と同方式。
- **Alternatives**: `tests/` にソースを同梱（リポジトリが肥大・更新が手作業）、vcpkg（追加の前提ツール）、git submodule（clone 手順が増える）。
- **オフライン時**: 取得に失敗すると CMake の構成が失敗し、取得元 URL がエラーに出る（0 件で成功しない。Edge Case を満たす）。`FETCHCONTENT_SOURCE_DIR_GOOGLETEST` での手元ソース指定を手順に書く。

## R-03: `cansim_core` の作り

- **Decision**: ルートの `CMakeLists.txt` で `file(GLOB CANSIM_SOURCES CONFIGURE_DEPENDS MFCApplication1/CanSim/*.cpp)` から `add_library(cansim_core STATIC ...)`。インクルードは `MFCApplication1/CanSim`（PUBLIC）と `tools/cansim_runner`（PRIVATE。空の `pch.h`）。`NOMINMAX` を PUBLIC 定義（アプリは `framework.h` で定義しており、同じ前提にする）。`/W3`、`/EHsc`。
- **Rationale**: ファイルを動かさず（FR-001）、MFC に触れず（FR-002）、`CanSim/` に追加したファイルが自動で取り込まれる。`checks.exe` が `MFCApplication1\CanSim\*.cpp` を全部コンパイルする現行方式と同じ。
- **Alternatives**: 明示列挙（追加時の更新漏れが出る）。

## R-04: 時間のかかる検証の扱い

- **Decision**: 移行時に各テストの実測時間を取り、1 件 10 秒を超えるもの（現時点の候補: 機能 029 改訂 1 の大きなログ、機能 028 の大規模）は `Feature<NNN>Slow` スイートに分けて CTest のラベル `slow` を付ける。既定の `ctest` は、全体が Release で 1 分以内（SC-007）に収まる限りすべて実行する。収まらないときだけ、`ctest -LE slow` を既定の手順とし、`ctest -L slow` を別枠にする（CLAUDE.md に両方記載）。
- **Rationale**: SC-001（全件実行）と SC-007（1 分）を両立する判断を、推測でなく実測で決める。
- **Alternatives**: 最初から別枠にする → 「ctest で全件」を損なう。

## R-05: テストデータ・一時出力・基準 CSV

- **Decision**: `tests/CMakeLists.txt` が `target_compile_definitions(runTests PRIVATE CANSIM_SAMPLE_DIR="<repo>/MFCApplication1/CanSim/SampleConfigs" CANSIM_TEST_TMP_DIR="<build>/test_tmp")` を与える。補助 `TestPaths()` が `checks.cpp` の `g_sampleDir` / `g_tempDir` の代わりになる。基準 CSV（`g_baselineDir`、機能 004 SC-009 / 006）は、環境変数 `CANSIM_BASELINE_DIR` があればその場所を使い、無ければそのテストを `GTEST_SKIP()`（理由付き）にする。
- **Rationale**: 呼び出し時の作業ディレクトリに依存しない（FR-010）、サンプルを複製しない、一時出力はビルドツリーに閉じる（FR-013）。基準 CSV は「006 以降のビルドで作ったもの」というユーザー固有の成果物なので、既存の `checks.exe` の第 3 引数と同じく任意入力にする。
- **補足**: skip は ctest 上で `Skipped` と表示され、黙って成功にはならない。基準の作り方は `tools/cansim_runner/README.md` を参照する旨を skip メッセージに入れる。
- **Alternatives**: 基準 CSV をリポジトリに追加（サイズ・複製、FR-010 に反する）。

## R-06: 移行の方法（機械的な写し）

> **実装での変更**: 1 つの機能の検証は局所変数を共有しているため、SC/FR ごとの `TEST` には分けず、機能ごとに 1 つの `TEST(FeatureNNN, <ids>)` とした。`Check()` は関数のまま残して呼び出しの形を変えていない。ids はラベルから機械的に集め、`SC001to005` のように連番をまとめて名前に入れる。

- **Decision**: `CheckFeatureNNN()` の本体をそのまま `TEST(Feature<NNN>, <群の名前>)` に移し、`Check(ok, label)` は `CHECK(ok, label)`（= `EXPECT_TRUE(ok) << label`）に置換する。`label` は `"006 SC-003: ..."` の形で既に機能番号・SC/FR を持つので、同じ接頭辞でまとめて 1 つの `TEST` にする（例: `TEST(Feature006, SC003_...)`）。ID を持たないもの（改訂の検証）は `Rev1_<内容>`。ヘルパは `tests/support/test_helpers.*` に切り出す（`checks.cpp` と二重管理になるが、`checks.exe` を残す間だけ）。
- **Rationale**: 期待値・入力を書き換えず検証を弱めない（FR-008）。EXPECT 系なので 1 項目の失敗で同じ `TEST` の残りも走る（`checks.exe` の「全項目を出す」挙動と同じ）。失敗時はラベルが出るので項目を特定できる（SC の ID 入り）。
- **Alternatives**: 項目ごとに 1 `TEST`（約 209 本に細分化。ID でまとまっていないものも多く、状態の共有が壊れやすい）、`ASSERT`（先頭の失敗で止まり情報が減る）。

## R-07: GUI 固有の項目

- **Decision**: `checks.cpp` が `CanSim/` の純粋関数（座標・当たり判定・ログ行など）を検証している項目は移す。ウィンドウ・描画・目視でしか確認できない項目は元から `checks.cpp` に無いので、台帳に「GUI 固有（対象外）」として機能ごとに 1 行で記録する。
- **Rationale**: `checks.cpp` 自体が「GUI 固有を除く」検証ツールであり、移行の対象は `checks.cpp` の中身（FR-006）。

## R-08: ルート直下の雛形ファイル

- **Decision**: `main.cpp` と `tests.cpp` は削除。ルートの `CMakeLists.txt` は置き換える（GoogleTest 取得の記述は `tests/CMakeLists.txt` へ）。いずれも未追跡で履歴が無い。
- **Rationale**: `tests.cpp` の中身は 2+2 の確認のみで検証価値が無い。`main.cpp` はどのターゲットにも使われない。

## R-09: Debug/Release と CRT

- **Decision**: CMake は Visual Studio ジェネレータ（多構成）を既定の手順とし、`cmake --build build --config Release` / `ctest --test-dir build -C Release`。CRT は動的（/MD, /MDd）で `gtest_force_shared_crt ON` に合わせる。
- **Rationale**: アプリ（`UseOfMfc=Dynamic`）と同じ CRT 方針。構成名を指定しないと ctest が見つけられない問題（多構成ジェネレータ）を手順に明記する。
