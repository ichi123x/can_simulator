# Contract: ネットワーク構成ファイル（入力）— 002 での追加分

機能 001 の [`contracts/config-input-format.md`](../../001-can-simulation-core/contracts/config-input-format.md)
に対する**差分**。既存キーの意味・検証ルールは変更しない。既存の構成ファイルは修正不要で、
追加キーを省略した場合は既定値（0）が適用される。

## 追加キー

| セクション | キー | 必須 | 型 | 検証ルール |
|---|---|---|---|---|
| `[NodeN]` | `PowerOnTimeUs` | 任意（既定 0） | 0以上の整数 | 電源投入時刻（FR-001） |
| `[NodeN]` | `InitDurationUs` | 任意（既定 0） | 0以上の整数 | 初期化に要する時間（FR-001） |

## 記述例

```ini
[Network]
BitRateBps=500000

[Node1]
NodeId=1
Name=ECU1 Engine Control
ReceiveDelayUs=0
PowerOnTimeUs=0              ; 省略時は 0
InitDurationUs=0             ; 省略時は 0

[Node1.Frame1]
CanId=0x100
Dlc=8
Data=00,00,00,00,00,00,00,00
PeriodUs=10000
InitialOffsetUs=0

[Node2]
NodeId=2
Name=ECU2 Transmission
PowerOnTimeUs=50000          ; 50ms 後に電源投入
InitDurationUs=1000          ; 初期化に 1ms
```

## エラー時の振る舞い

既存の拒否条件（ノード数17以上、CAN ID 重複、DLC とデータ長の不一致など）に加え、以下を検出した
場合もシミュレーションを開始せず、該当箇所とエラー理由を示して拒否する。

- `PowerOnTimeUs` / `InitDurationUs` が整数として解釈できない、または負の値である

## 挙動上の注意（利用者向け）

- 追加キーをすべて省略した既定構成でも、各ノードはバスが空であることを11ビット時間ぶん観測して
  から通常動作に入る。したがって、機能 001 で得られていたログと比べて**全時刻が一律11ビット時間
  （500 kbps なら 22 µs）だけ後ろへずれる**。送信順序・調停結果・送信回数は変わらない
  （`spec.md` SC-006）。
- `PowerOnTimeUs` がシミュレーション時間より後のノードは通常動作へ到達せず、そのノードのフレーム
  は1件も送信されない（`startup.csv` の `Reached_Normal` が `FALSE` になる）。

## 対応する要件

FR-001, FR-013
