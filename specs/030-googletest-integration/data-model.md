# Data Model: GoogleTest/CTest による自動テストの導入

本機能にドメインデータは無い。ビルド・テストの構成要素と、移行の記録の形を定める。

## ビルドターゲット

| ターゲット | 種類 | 入力 | リンク | 備考 |
|---|---|---|---|---|
| `cansim_core` | STATIC ライブラリ | `MFCApplication1/CanSim/*.cpp`（移動しない） | なし | インクルード: `CanSim/`（PUBLIC）、`tools/cansim_runner/`（PRIVATE、空の `pch.h`）。`NOMINMAX` |
| `runTests` | 実行ファイル | `tests/feature_NNN_tests.cpp`、`tests/support/*.cpp` | `cansim_core`、`gtest_main` | `CANSIM_SAMPLE_DIR`・`CANSIM_TEST_TMP_DIR` を定義 |

## テストの名前

- スイート: `Feature<NNN>`（NNN = 3 桁の機能番号。時間のかかるものは `Feature<NNN>Slow`）
- ケース（実装）: その機能のラベルに現れる ids を `_` でつないだもの（例: `SC001to005_FR004`）。当初案: `<SC|FR><id>_<内容>`（例: `SC003_ArbitrationLowestIdWins`）。ID を持たない検証は `Rev<n>_<内容>`
- 規則の詳細は [contracts/test-naming.md](contracts/test-naming.md)

## テストの入力

| 名前 | 取得元 | 用途 |
|---|---|---|
| サンプルディレクトリ | コンパイル定義 `CANSIM_SAMPLE_DIR` | `.canprj` の入力（複製しない） |
| 一時出力ディレクトリ | コンパイル定義 `CANSIM_TEST_TMP_DIR`（ビルドツリー内） | CSV など生成物 |
| 基準 CSV ディレクトリ | 環境変数 `CANSIM_BASELINE_DIR`（任意） | 機能 004 SC-009 などの比較。無ければ該当テストは skip |

## 移行台帳（`tests/migration-ledger.md`）

機能ごとに 1 行の表。

| 列 | 内容 |
|---|---|
| 機能 | 3 桁の番号 |
| checks.cpp の項目数 | `Check(` の数（移行前に数えて固定） |
| 移した項目数 | 移行後のテストの `CHECK(` の数 |
| テストファイル | `tests/feature_NNN_tests.cpp` |
| 状態 | 未着手 / 移行済み / 一部対象外 |
| 備考 | 対象外の項目とその理由（GUI 固有など）、skip 条件 |

**検証規則**: 「状態 = 移行済み」の機能は、項目数 = 移した項目数。差があるときは備考に項目と理由を書く（SC-002）。

## 状態の遷移

`未着手` → `移行済み`（テストが緑 かつ `checks.exe` も緑） / `一部対象外`（差分の理由を記録）。
