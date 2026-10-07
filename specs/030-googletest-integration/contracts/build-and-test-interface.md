# Contract: ビルドとテストの呼び出し

開発者・CI が使う入口の約束。実装はこの形を満たす。

## 手順（リポジトリのルートで）

```powershell
cmake -S . -B build                                  # 構成（GoogleTest を取得）
cmake --build build --config Release                 # cansim_core と runTests をビルド
ctest --test-dir build -C Release --output-on-failure   # 全テスト実行
```

- CMake 3.22 以上が必要。
- Debug でも同じ（`--config Debug` / `-C Debug`）。
- 機能を絞る: `ctest --test-dir build -C Release -R Feature006`
- 実行時間で判定するテスト（`Feature018Slow.SC005_EffectSpeed`）にはラベル `slow` が付く。Debug では遅くて閾値（1 秒）に近いので `ctest -C Debug -LE slow` で外す。Release は全件実行。`-L slow` でそれだけを実行できる
- 基準 CSV を使う検証: `$env:CANSIM_BASELINE_DIR = "<ディレクトリ>"` を設定して実行（無ければ比較は行わず、テストの最後に NOTE を表示する）
- オフライン: `cmake -S . -B build -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST=<手元の googletest>`

## 終了コード・出力

- すべて成功: `ctest` が 0。基準 CSV の比較は `CANSIM_BASELINE_DIR` が無ければ行わず NOTE を出す（checks.exe と同じ挙動）。
- 1 件でも失敗: 0 以外。失敗したテスト名（`Feature<NNN>.<SC|FR><id>_...`）と、失敗した項目のラベル（`CHECK` の第 2 引数）が出る。
- GoogleTest を取得できない: 構成が失敗し、取得元 URL がエラーに出る。

## 不変条件

- `MFCApplication1/MFCApplication1.sln`、`MFCApplication1.vcxproj`（および `.filters`）は変更しない。
- `MFCApplication1/CanSim/` のソースは移動・複製・変更しない。
- 生成物（`build/`）は `.gitignore` 済み。テストの一時出力は `build/` の下に閉じる。
- 既存の `checks.exe` の手順（`cl ...` と実行コマンド）は、移行完了まで変わらず動く。
