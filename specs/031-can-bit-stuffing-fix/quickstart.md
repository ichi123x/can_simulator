# Quickstart: ビットスタッフィングを CAN 2.0A の規格どおりに数える

## 前提

- ブランチ `ver.0.9`。Visual Studio 2022（C++・MFC）、CMake、Git。
- テストは機能 030 の GoogleTest（CLAUDE.md の手順）。

## 1. 自動テスト

```powershell
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
ctest --test-dir build -C Release -R Feature031     # 本機能のテストだけ
```

期待: 全件成功。`Feature031` は SC-001〜SC-006 を確かめる（[contracts/frame-timing.md](contracts/frame-timing.md) の値の例、全 ID × DLC 0〜8 の照合、最悪値、通信速度ごとの伝送時間、2 回の実行の一致）。

テストファーストの確認（実装の前）: `Feature031` を先に足して実行し、0x000・DLC 0、0x555・DLC 1、0x080・DLC 8 と全 ID の照合が失敗すること、0x7FF・DLC 0 と 0x123・DLC 8 が通ることを確かめる。

## 2. ドメイン仕様書のログの例の再現（SC-002）

`sample_collision`（ECU 1 が 0x100、ECU 3 が 0x080、DLC 8、500 kbps、同時に送信要求）を実行する（アプリの「開始」、または `tools/cansim_runner` の `runner.exe`）。

期待（フレームログ `.log.csv`）:

- 0x080 の行: `Tx_End − Tx_Start` = 246 µs。
- 0x100 の行: `Tx_Start` = 0x080 の `Tx_End` + 6 µs、`Tx_End − Tx_Start` = 246 µs、`Status` は `ARBITRATION_LOST_RETRY`、`Retry_Count` は 1。

## 3. 入力画面の表示（User Story 2、手動）

Release x64 のアプリで、通信速度 500 kbps のプロジェクトを開き、「編集 → データフレーム編集」の「追加」を押す。

| 入力 | 期待する表示 |
|---|---|
| ID 0x080、DLC 8 | ビット長 123 ビット、伝送時間 246 µs |
| ID 0x7FF、DLC 0 | ビット長 47 ビット、伝送時間 94 µs |
| ID 0x000、DLC 0 | ビット長 50 ビット、伝送時間 100 µs |

## 4. 本機能の前との比較（User Story 3、SC-007）

1. 本機能の前のコミットで、調停の起きない構成（例: `sample_vehicle_2ecu`、再送 0）を実行してフレームログを残す。
2. 本機能の後で同じ構成を実行する。
3. 2 つのログを比べる。期待: 行数・行の順序・CAN ID・送信 ECU・受信 ECU・状態・再送回数・更新回数・送信要求・送信開始は同じ。送信完了・受信の時刻は、そのフレームのスタッフビットの差 × 2 µs だけ早い（差が 0 のフレームは同じ）。`Bus_Load_Pct` は前の値以下。

## 5. サンプルの数値の更新

`tools/vehicle_samples/verify_log.ps1` で 4 つの車両サンプルを確かめ、`tools/vehicle_samples/README.md` の結果の表、`README.md` の数値、CLAUDE.md の SampleConfigs の説明を新しい値に直す（research R5）。基準 CSV（`CANSIM_BASELINE_DIR`）を使う場合は、本機能の後のビルドで作り直す。
