# Data Model: 004

## ProjectSettings（`CanSim/CanTypes.h`）
| 項目 | 型 | 既定 | 検証 |
|---|---|---|---|
| EcuCount | int | 2 | 0〜16 |
| DurationSec | int | 10 | 1〜600 |
| FrameDisplay | bool | false | — |

通信速度は `Network.BitRateBps`（125000 / 250000 / 500000 / 1000000）を使う。

## 読込み結果
`ConfigLoadResult` に `ProjectSettings Project` を追加する。`Config`（`Network`）はファイル内の全ノード定義を保持する（EcuCount より多い番号の定義も、保存で失わないよう保持する）。

ECU 番号 n は `Node::NodeId` = n に対応する（ECU 番号 1〜EcuCount）。

## 関数（ASCII のみ）
- `LoadConfig(path)` → 既存。`[Project]` セクションを読み、ノード 0 個は拒否する（機能 001 FR-001）。エラー時は`Success=false`＋メッセージ。
- `SaveProject(path, settings, network)` → bool。同形式のテキストを UTF-8 で書く。
- `BuildRunNetwork(settings, definitions)` → `Network`。research.md 決定2のとおり ECU 1〜EcuCount のノード。
- `BuildFrameEvents(rows)` → `vector<FrameEvent>`。

## FrameEvent（`CanSim/FrameEvents.h`）
| 項目 | 型 | 内容 |
|---|---|---|
| TxStartUs, TxEndUs | long long | 送信開始／終了時刻 |
| TxNodeId | int | 送信元 ECU 番号 |
| RxNodeIds | vector<int> | 受信した ECU 番号（LogRow の RxNodeId != -1 を集約） |
| CanId | unsigned | 表示用 |

`LogRow` は「1 フレーム × 受信ノード」で 1 行のため、`(TxNodeId, TxStartUs)` が同じ行を 1 つの`FrameEvent` にまとめ、`TxStartUs` 昇順に並べる。

## 実行状態（UI）
`Idle` / `Running`（フレーム再生中。フレーム表示 OFF では実行が同期で完結するため長く続かない）。メニューの有効/無効を決める（FR-019）。再生位置 `m_playIndex`（現在表示中の `FrameEvent`）。
