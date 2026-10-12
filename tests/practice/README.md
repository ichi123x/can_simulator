# テストを手で書く練習（機能 001）

[feature_001_spec_tests.cpp](../unit/feature_001_spec_tests.cpp) と同じ流れで、仕様テストを一から書く練習です。

| ファイル | 中身 |
|---|---|
| [practice_001_tests.cpp](practice_001_tests.cpp) | 練習用。各レッスンは `GTEST_SKIP()` と `TODO` だけの空のテスト |
| [practice_001_answers.cpp](practice_001_answers.cpp) | 解答例（ビルドして全件 PASSED を確認済み）。書き終えてから見る |

## 1 本のテストの型

feature_001_spec_tests.cpp のテストは、どれも次の形をしています。

```cpp
// ---------------------------------------------------------------------------
// <ID>: <仕様の文を自分の言葉で 1〜2 行>
// 根拠: spec.md の <ID>（期待値をどこから持ってきたか。手計算ならその式）
// ---------------------------------------------------------------------------
TEST(<スイート名>, F<機能番号>_<ID>_<条件>_<結果>)
{
    RecordProperty("spec", "<機能番号>:<ID>");
    // 準備: 構成（Network / 設定ファイルの文字列）を組み立てる
    // 実行: 対象の関数を呼ぶ（ComputeFrameTiming / LoadFromText / RunSim ...）
    // 確認: ASSERT_* / EXPECT_* に日本語のメッセージを付ける
}
```

決まり（CLAUDE.md「テストの実行」）:

- テスト名は英数字と `_` だけ。日本語はコメントとメッセージに書く
- 期待値の根拠は仕様にあるものだけ。実装を読んで期待値を決めない
- 仕様と実装が食い違ったら `GTEST_SKIP() << "仕様と実装の食い違い（人の判断待ち）: ..."` で残す

## レッスン

| # | テーマ | 根拠 | 新しく覚えること |
|---|---|---|---|
| 1 | フレームのビット数・伝送時間 | SC-003, FR-010 | `TEST`、`EXPECT_EQ`、メッセージ、`RecordProperty` |
| 2 | 設定ファイルの読み込みと拒否 | FR-002 | `ASSERT_*` と `EXPECT_*` の使い分け、`LoadFromText` |
| 3 | 1 ノードの周期送信 | FR-003, 機能 006 | 準備 → 実行 → 確認、`MakeNode` / `RunSim` / `UniqueTransmissions` |
| 4 | 受信フィルタと受信処理遅延 | FR-005, FR-016 | 全行のループ + 件数の確認（空振りの防止） |
| 5 | 通信速度ごとの伝送時間 | SC-003, FR-002 | `TEST_P` / `INSTANTIATE_TEST_SUITE_P`（一から書く） |

各レッスンの進め方:

1. コメントの「根拠」を [spec.md](../../specs/001-can-simulation-core/spec.md) で読む
2. `GTEST_SKIP()` の行を消して、`TODO` を書く
3. ビルドして実行し、PASSED を確かめる
4. **期待値を 1 つわざと変えて FAILED になるのを見る**（テストが本当に確かめているかの確認）。見たら戻す

## ビルドと実行

PowerShell で、リポジトリのルートから実行します（`cmake` が PATH に無ければ、Visual Studio に付いているものを使う）。

```powershell
# cmake を PATH に入れる（VS 2022 付属のもの）
$vs = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -property installationPath
$env:Path = "$vs\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;$env:Path"

# 練習だけビルドして実行
cmake --build build --config Debug --target practice_001_tests
ctest --test-dir build -C Debug -L practice --output-on-failure

# テストの exe を直接動かすと、失敗の詳細が見やすい（--gtest_filter で 1 本だけにもできる）
build\tests\Debug\practice_001_tests.exe --gtest_filter=*L1*
```

新しいテストを足した直後に ctest で見つからないときは `cmake -S . -B build` をやり直してください。

## 注意

- 練習のテストを **FAILED のまま** にして Claude に別の作業を頼むと、Stop フック（`.claude/hooks/run-tests.ps1`）がテストの失敗を検出して、Claude が実装を直そうとします。手を止めるときは、そのテストを PASSED にするか、`GTEST_SKIP()` を戻しておいてください
- レッスン 5 で `TEST_P` だけを書いて `INSTANTIATE_TEST_SUITE_P` を書き忘れると、`GoogleTestVerification.UninstantiatedParameterizedTestSuite` という失敗が出ます
- 練習を終えて不要になったら、[tests/CMakeLists.txt](../CMakeLists.txt) の末尾の 2 行と、このフォルダーを消せば元に戻ります
