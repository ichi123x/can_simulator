# Quickstart: 008 の検証

## 1. コアの検証（自動、checks.exe）
- `sample_buffer_full.canprj`（10 秒）:
  - 要求がそろう時刻（100 ms ごと、100 回）のたびに、ECU1 の ID の大きい 4 フレーム（0x110〜0x113）の要求が捨てられる → 送信側
    OVERWRITE 行 = 400、ECU1 の TxDiscard_Count = 400。捨てられた行の Tx_Start 等は空欄。
  - 各時点の ECU1 の送信待ち ≤ 16、各ノードの受信バッファ ≤ 8。
  - ECU2（受信処理遅延 2 ms）の受信側 OVERWRITE 行の数 = 集計の RxDiscard_Count > 0。Rx_Process は空欄。
  - 2 回実行してログが一致。
- 受信処理遅延 0 の構成（既存サンプル）で受信側 OVERWRITE = 0。
- 既存サンプル 5 つ: 基準 CSV（006 以降で作成）と比べて、ログは見出しの列名以外、集計は追加の 2 列以外、起動記録はすべて一致。
- 001〜007 の既存項目が PASS（`OverwriteCount` → `UpdateCount` の改名を反映）。

## 2. GUI の確認
- `sample_buffer_full.canprj` をフレーム表示 ON・スピード 0 で実行し、例外なく再生・完了すること（捨てた送信要求は表示されない）。

## 3. ビルド
Debug/Release × x64/x86 の 4 構成で警告なし。
