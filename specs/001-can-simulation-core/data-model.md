# Data Model: CAN バスシミュレーション コア機能

`spec.md` の Key Entities を、実装単位のエンティティとして詳細化する。すべて `CanSim/`配下のプレーン C++ 型として表現し、MFC/GUI 型には依存しない。

## Network（ネットワーク）

| フィールド | 型のイメージ | 説明 / 検証ルール |
|---|---|---|
| BitRateBps | 整数 | 125,000 / 250,000 / 500,000 / 1,000,000 のいずれか（既定 500,000）。FR-002 |
| Nodes | Node の一覧 | 1〜16件。17件以上は読み込み時に拒否（FR-001, SC-005） |

**検証ルール**:
- `Nodes` 内で同一 `NodeId` の重複は不可。
- 全 `Node.Frames` を通して同一 `CanId` を送信するノードは1つに限る（FR-004）。

## Node（ノード）

| フィールド | 型のイメージ | 説明 |
|---|---|---|
| NodeId | 整数 | 1〜16の一意な番号。同一時刻イベントの処理順（FR-007）にも使用 |
| Name | 文字列 | 表示用ラベル（例: "ECU1 エンジン制御"） |
| Frames | FrameDefinition の一覧 | このノードが送信するフレーム定義 |
| RxFilter | CAN ID の集合（省略可） | 省略時は全 ID を受信（FR-005） |
| ReceiveDelayUs | 整数（0以上、既定0） | 受信完了時刻に加算する受信処理遅延（FR-016） |
| TxState | 列挙 | `Idle` / `ArbitratingTransmitting` / `Won_Complete` / `Lost_Waiting` / `RetryPending`（FR-012, FR-013） |
| PendingFrames | PendingFrame の一覧 | 送信待ち行列。ID の小さい順に調停へ提出（FR-014） |

**状態遷移（TxState）**:

```
Idle --送信要求 & バスアイドル--> ArbitratingTransmitting
Idle --送信要求 & バス送信中----> RetryPending（自動再送信準備で待機）
ArbitratingTransmitting --調停に勝った--> Won_Complete --EOF送信完了--> Idle
ArbitratingTransmitting --調停に負けた--> Lost_Waiting --勝者のEOF検出--> RetryPending
RetryPending --インターミッション後バスアイドル--> ArbitratingTransmitting
```

## FrameDefinition（送信フレーム定義）

| フィールド | 型のイメージ | 説明 / 検証ルール |
|---|---|---|
| CanId | 整数 (0x000〜0x7FF) | 11ビット標準ID |
| Dlc | 整数 (0〜8) | データ長 |
| Data | バイト列 | 長さは `Dlc` と一致しなければならない（不一致は読み込み時に拒否、FR-003） |
| PeriodUs | 整数（正の値） | 送信周期（µs） |
| InitialOffsetUs | 整数（0以上） | 初回送信要求までのオフセット |

## PendingFrame（送信待ちフレーム）

| フィールド | 型のイメージ | 説明 |
|---|---|---|
| Definition | FrameDefinition への参照 | 元のフレーム定義 |
| TxRequestUs | 整数 | 最初の送信要求時刻（上書きされても変化しない、FR-011） |
| CurrentData | バイト列 | 上書きされた場合は最新値（FR-011） |
| RetryCount | 整数（0以上） | 調停負けによる再送信回数（FR-013） |
| OverwriteCount | 整数（0以上） | 次周期到来による上書き回数（FR-011） |

## Event（イベント）

| フィールド | 型のイメージ | 説明 |
|---|---|---|
| TimeNs | 整数（ns） | イベント発生時刻。ログ出力時は µs に変換 |
| Kind | 列挙 | `TxRequest` / `BusIdle` / `TxComplete` / `IntermissionEnd` / `RxProcess` / `SimulationEnd` 等（FR-006） |
| NodeId | 整数（該当する場合） | 対象ノード。同一時刻内の処理順の副キーとして使用（FR-007） |

**順序付け**: `EventQueue` は `(TimeNs, Kind の優先順位, NodeId)` の順で決定的にソートする（種類ごとの優先順位は `spec.md` Edge Cases および憲章 P③ の「同一構成から常に同一結果」を満たすよう `SimulationEngine` 内で固定順とする）。

## LogRow（ログ行）

| フィールド | 型のイメージ | 説明 |
|---|---|---|
| TimestampUs | 整数 | 行の代表時刻（受信処理時刻。受信ノードがない場合は送信完了時刻） |
| CanId | 整数 | 16進表記で出力（例: `0x080`） |
| Dlc | 整数 (0〜8) | |
| TxNodeId / RxNodeId | 整数（RxNodeId は受信ノードがない場合は空） | |
| TxRequestUs / TxStartUs / TxEndUs | 整数 | 送信要求／開始／完了時刻 |
| RxStartUs / RxEndUs / RxProcessUs | 整数（受信ノードがない場合は空） | 受信開始／完了／処理時刻 |
| BusLoadPct | 実数 | 直近の集計ウィンドウでのバス負荷率（FR-018, 既定ウィンドウ100ms） |
| Status | 列挙 | `SUCCESS` / `ARBITRATION_LOST_RETRY`（FR-017） |
| RetryCount | 整数 | 調停負けによる再送信回数 |
| OverwriteCount | 整数 | 上書き回数 |

**生成規則**: 1フレーム×受信ノードごとに1行。受信ノードがないフレームは送信側の情報のみで1行出力する（FR-017）。

## MetricsSummary（性能指標集計、FR-018〜020）

| フィールド | 型のイメージ | 説明 |
|---|---|---|
| GroupKey | 文字列 or 整数 | 集計単位（CAN ID／ノード番号／全体のいずれか） |
| TotalLatencyStats | 最小・最大・平均 | `RxProcessUs - TxRequestUs` の分布 |
| QueueingDelayStats | 最小・最大・平均 | `TxStartUs - TxRequestUs` の分布 |
| TxDurationStats | 最小・最大・平均 | `TxEndUs - TxStartUs` の分布 |
| BusLoadPct | 実数 | 集計ウィンドウ内の送信時間合計 ÷ ウィンドウ長 |
| JitterStats | 最大・最小・標準偏差 | ID ごとの「連続する TxStartUs の間隔 − PeriodUs」（FR-019） |

## エンティティ関係の概要

```
Network 1 ── * Node 1 ── * FrameDefinition
                 │
                 └── * PendingFrame ── 1 FrameDefinition (参照)

SimulationEngine が Event を時刻順に処理し、
  TxComplete イベントの都度 LogRow を1件以上生成する
LogRow の集合から MetricsSummary を算出する
```
