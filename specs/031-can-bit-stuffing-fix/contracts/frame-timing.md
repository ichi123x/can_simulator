# Contract: フレームの伝送時間（`CanSim::ComputeFrameTiming`）

`FrameTiming.h` の宣言は変えない。本機能で変わるのは、結果の `TotalBits`・`StuffBitCount`・`TxDurationNs` の値だけ（スタッフビットを規格どおりに数えるため）。

```cpp
FrameTimingResult ComputeFrameTiming(unsigned canId, unsigned dlc,
    const std::vector<std::uint8_t>& data, long long bitRateBps);
```

## 入力

| 引数 | 範囲 | 備考 |
|---|---|---|
| `canId` | 0x000〜0x7FF | 11 ビットの標準 ID |
| `dlc` | 0〜8 | |
| `data` | `dlc` バイト | アプリでは常に `MakeFrameData(dlc)`（DLC 分の 0xFF） |
| `bitRateBps` | 125000 / 250000 / 500000 / 1000000 | |

## 出力の約束

- `TotalBits` = 34 + 8 × `dlc` + `StuffBitCount` + 10。
- `StuffBitCount` は、SOF〜CRC シーケンスに規格どおりのビットスタッフィング（[data-model.md](../data-model.md) の「差し込みの決まり」）を適用したときに差し込まれるビットの数。0 以上、⌊(34 + 8 × `dlc` − 1) / 4⌋ 以下。
- `TxDurationNs` = `TotalBits` × `BitTimeNs`、`IntermissionNs` = 3 × `BitTimeNs`、`BitTimeNs` = 1e9 / `bitRateBps`。
- 同じ入力には常に同じ結果を返す。

## 規格どおりの値の例（SC-001、500 kbps）

| canId | dlc | data | TotalBits | StuffBitCount | TxDurationNs |
|---|---|---|---|---|---|
| 0x000 | 0 | — | 50 | 6 | 100000 |
| 0x7FF | 0 | — | 47 | 3 | 94000 |
| 0x555 | 1 | 0xAA | 54 | 2 | 108000 |
| 0x123 | 8 | 01 02 03 04 05 06 07 08 | 116 | 8 | 232000 |
| 0x080 | 8 | FF × 8 | 123 | 15 | 246000 |

本機能の前の値（参考）: 0x000・DLC 0 は 52 / 8、0x555 は 55 / 3、0x080・DLC 8・FF は 127 / 19。0x7FF・DLC 0 と 0x123・DLC 8 は同じ。

## 使う所

| 使う所 | 本機能の後 |
|---|---|
| `SimulationEngine` | 実行の開始時に、フレームの定義ごとに 1 回呼んで表に持ち、送信開始で表を引く（[data-model.md](../data-model.md)「伝送時間の表」） |
| `FrameEntryDlg`（入力画面） | 今までどおり入力のたびに呼び、「ビット長」「伝送時間」に表示する |
| `tools/cansim_runner/checks.cpp`、`tests/` | 今までどおり呼んで確かめる |
