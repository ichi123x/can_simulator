# Contract: プロジェクトファイル（.canprj）

> **機能 005 による変更**: `[Project]` に `Speed` キー（-13〜10、省略時 0）が追加された。
> 契約は `specs/005-simulation-speed/contracts/project-file-speed.md` を参照。以下は機能 004 時点の原文。
>
> **機能 006 による変更**: `[NodeN.FrameM]` に `Name=`（0〜32 文字）を追加。`Data=` は読まない・書かない（データは
> 常に `Dlc` 個の 0xFF）。1 ノード最大 100 フレーム。`InitialOffsetUs` は `PeriodUs` 未満で、起点は送信ノードの
> 通常動作開始時刻。契約は `specs/006-frame-editor/contracts/project-file-frames.md` を参照。

機能 001 の `contracts/config-input-format.md` の形式に `[Project]` セクションを追加した後継。UTF-8、`key=value`、`;` 以降はコメント。拡張子は `.canprj`。

```ini
[Project]
EcuCount=6            ; 0..16、省略時 2
DurationSec=10        ; 1..600、省略時 10
FrameDisplay=0        ; 0 / 1、省略時 0

[Network]
BitRateBps=500000     ; 125000 / 250000 / 500000 / 1000000、省略時 500000

[Node1]               ; 以降は 001 と同じ（NodeId, Name, RxFilter, ReceiveDelayUs, PowerOnTimeUs, InitDurationUs）
NodeId=1

[Node1.Frame1]
CanId=0x100
Dlc=8
Data=00,00,00,00,00,00,00,00
PeriodUs=10000
InitialOffsetUs=0
```

## 検証
機能 001 の検証規則（CAN ID 重複、`Data` バイト数 = `Dlc`、`NodeId` 1〜16 かつ一意、ノード数 ≤ 16、通信速度の 4 種）に加え:
- `EcuCount` は 0〜16 の整数、`DurationSec` は 1〜600 の整数、`FrameDisplay` は 0 または 1。
- `[NodeN]` が 1 つ以上必要（機能 001 FR-001。0 個は読込みエラー）。ECU 数 0 のプロジェクトは、定義済みのノードを持ったまま `EcuCount=0` にして表す。
- 違反したファイルは読み込まず、ファイル名・キー・理由を含むメッセージを返す（FR-010）。

## 実行時の解釈
- 実行に使うノードは `EcuCount` までの番号（research.md 決定2）。それを超える番号の定義は読込みと保存では保持され、実行には使われない。
- 保存はこの形式を書き出す（コメントは保存されない）。

## 対応する要件
FR-008, FR-009, FR-010, FR-011。
