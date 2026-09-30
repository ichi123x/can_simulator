# Contract: ネットワーク構成ファイル（入力）

> **後継**: 機能 004 で、この形式に `[Project]` セクション（`EcuCount` / `DurationSec` / `FrameDisplay`）を
> 追加したプロジェクトファイル（拡張子 `.canprj`）が後継となった。アプリの読込みは `.canprj` のみ。
> 契約は `specs/004-menu-project-settings/contracts/project-file-format.md` を参照。以下は機能 001 時点の原文。

利用者がシミュレータに与える設定ファイルの契約。`research.md` の決定に基づき、セクション区切り
＋ `key=value` の独自テキスト形式（UTF-8）とする。

## 構文

```ini
[Network]
BitRateBps=500000

[Node1]
NodeId=1
Name=ECU1 エンジン制御
RxFilter=0x080,0x081        ; 省略時は全ID受信
ReceiveDelayUs=0             ; 省略時は既定値0（FR-016）

[Node1.Frame1]
CanId=0x100
Dlc=8
Data=00,00,00,00,00,00,00,00
PeriodUs=10000
InitialOffsetUs=0

[Node1.Frame2]
CanId=0x101
Dlc=8
Data=00,00,00,00,00,00,00,00
PeriodUs=100000
InitialOffsetUs=0

[Node2]
NodeId=2
Name=ECU2 トランスミッション
...
```

## フィールド定義

| セクション | キー | 必須 | 型 | 検証ルール |
|---|---|---|---|---|
| `[Network]` | `BitRateBps` | 任意（既定 500000） | 整数 | 125000 / 250000 / 500000 / 1000000 のいずれか |
| `[NodeN]` | `NodeId` | 必須 | 整数 | 1〜16、ファイル内で一意 |
| `[NodeN]` | `Name` | 任意 | 文字列 | — |
| `[NodeN]` | `RxFilter` | 任意 | 16進IDのカンマ区切り | 省略時は全ID受信 |
| `[NodeN]` | `ReceiveDelayUs` | 任意（既定 0） | 0以上の整数 | 受信完了時刻に加算する受信処理遅延（FR-016） |
| `[NodeN.FrameM]` | `CanId` | 必須 | 16進（`0x000`〜`0x7FF`） | 全ノードを通して重複不可 |
| `[NodeN.FrameM]` | `Dlc` | 必須 | 整数 (0〜8) | `Data` のバイト数と一致必須 |
| `[NodeN.FrameM]` | `Data` | 必須（`Dlc=0` なら空） | 16進バイトのカンマ区切り | バイト数 = `Dlc` |
| `[NodeN.FrameM]` | `PeriodUs` | 必須 | 正の整数 | — |
| `[NodeN.FrameM]` | `InitialOffsetUs` | 任意（既定 0） | 0以上の整数 | — |

## エラー時の振る舞い

読み込み時に以下を検出した場合、シミュレーションを開始せず、該当箇所とエラー理由を示して
拒否する（`spec.md` Acceptance Scenario US-1 #2）:

- ノード数が17以上
- 同一 `CanId` を複数ノードが送信
- `Data` のバイト数が `Dlc` と不一致
- `NodeId` の重複、または範囲外（1〜16 以外）

## 対応する要件

FR-001, FR-002, FR-003, FR-004, FR-005
