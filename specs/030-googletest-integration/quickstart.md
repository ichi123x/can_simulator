# Quickstart: GoogleTest/CTest による自動テストの導入

前提: Windows + Visual Studio 2022（MSVC v143）+ CMake 3.14 以上、ネットワーク接続（初回の GoogleTest 取得）。呼び出しの約束は [contracts/build-and-test-interface.md](contracts/build-and-test-interface.md)。

## 1. 基本（SC-001）

```powershell
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

期待: 全件成功、終了コード 0。`ctest -N --test-dir build -C Release` の一覧が `Feature<NNN>.` で始まる。

## 2. テストが壊れたコードを検出する（SC-004）

`CanSim/` のロジックを 1 か所だけ一時的に壊し（例: `FrameTiming.cpp` のビット数、`Network.cpp` の優先度比較）、再ビルドして `ctest` を実行する。

期待: 対応する機能のテスト（001・006・008・028・029 の代表）が失敗し、失敗した項目のラベルが出る。戻すと全件成功に戻る。

## 3. 移行の突き合わせ（SC-002・SC-003）

- `tests/migration-ledger.md` の「checks.cpp の項目数」と「移した項目数」が、全機能で一致するか、差に理由がある。
- `ctest -N` の全テスト名が `Feature\d{3}(Slow)?\.` で始まる。

## 4. checks.exe の並存（SC-006）

[CLAUDE.md](../../CLAUDE.md) の「ビルド・実行」の手順で `checks.exe` をビルド・実行し、終了コード 0。

## 5. 既存ビルドに影響が無い（SC-005）

```powershell
git diff --stat -- MFCApplication1/MFCApplication1.sln MFCApplication1/MFCApplication1.vcxproj MFCApplication1/MFCApplication1.vcxproj.filters
foreach ($c in 'Debug','Release') { foreach ($p in 'x86','x64') {
  msbuild MFCApplication1\MFCApplication1.sln /p:Configuration=$c /p:Platform=$p /v:minimal } }
```

期待: 差分なし、4 構成すべて成功。

## 6. 実行時間（SC-007）

`Measure-Command { ctest --test-dir build -C Release }` が 60 秒以内。超える場合は research R-04 に従い遅いテストを `slow` に分ける。

## 7. 基準 CSV の比較（任意）

`$env:CANSIM_BASELINE_DIR` を設定して `ctest` を実行すると、機能 004 SC-009 などの比較も走る。未設定のときは該当テストが skip と表示される。
