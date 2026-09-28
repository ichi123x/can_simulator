# Data Model: ノードのスタートアップ制御

機能 001（`specs/001-can-simulation-core/data-model.md`）のモデルに対する**差分**を示す。既存
エンティティの意味は変更せず、フィールドと状態を追加する。すべて `CanSim/` 配下のプレーン C++ と
して表現し、MFC/GUI 型には依存しない。

## Node（既存エンティティへの追加フィールド）

| フィールド | 型のイメージ | 説明 / 検証ルール |
|---|---|---|
| PowerOnTimeUs | 整数（0以上、既定0） | 電源投入時刻。FR-001 |
| InitDurationUs | 整数（0以上、既定0） | 初期化に要する時間。FR-001 |
| StartupState | 列挙 | `PoweredOff` / `Initializing` / `BusSyncDetecting` / `Normal`。この順にのみ遷移する（FR-002） |
| InitDoneUs | 整数 or 未設定 | 初期化が完了し、バス同期検出に入った時刻（記録用）。FR-012 |
| NormalEntryUs | 整数 or 未設定 | 通常動作に入った時刻。未到達なら未設定のまま（FR-011, FR-013） |
| SyncTargetNs | 整数 or 未設定 | 現在有効な同期完了予定時刻。遅延無効化の判定に使う（research.md 決定2）。`BusSyncDetecting` のときのみ意味を持つ |

**状態遷移（StartupState）**:

```
PoweredOff       --時刻が PowerOnTimeUs に達した-------------------> Initializing
Initializing     --InitDurationUs が経過した------------------------> BusSyncDetecting
BusSyncDetecting --検出開始以降に11ビット時間の連続レセシブが成立---> Normal
BusSyncDetecting --途中でドミナント（他ノードの送信開始）----------> BusSyncDetecting（計数やり直し、FR-007）
Normal           --（本機能では他状態へ戻らない。バスオフはスコープ外）
```

**状態が振る舞いに与える制約**:

- `Normal` 以外のノードは調停の候補に含めない（FR-008）。
- `Normal` 以外のノードは受信対象に含めない＝そのノードを受信側とするログ行を生成しない（FR-009）。
- `Normal` 以外のノードでも送信要求は通常どおり発生し、送信待ちとして保持される（FR-010）。保持中の
  上書き規則（機能 001 の FR-011）はそのまま適用される。

## Event（既存エンティティへの追加）

`EventKind` に3種を追加し、同一時刻の処理順序を次のとおりとする（既存種別の相対順序は不変、
research.md 決定3）。

| 優先順位 | 種別 | 追加/既存 | 対象 |
|---|---|---|---|
| 0 | TxComplete | 既存 | 送信ノード |
| 1 | IntermissionEnd | 既存 | 送信ノード |
| 2 | RxProcess | 既存 | （値として算出、イベントは発行しない） |
| 3 | **PowerOn** | 新規 | 対象ノード |
| 4 | **InitDone** | 新規 | 対象ノード |
| 5 | **SyncComplete** | 新規 | 対象ノード |
| 6 | TxRequest | 既存 | 対象ノード |
| 7 | BusIdle | 既存 | バス全体（NodeIndex = -1） |
| 8 | SimulationEnd | 既存 | バス全体（NodeIndex = -1） |

同一時刻・同一種別の中ではノードの位置（`Network::Nodes` の添字＝設定上の NodeId 昇順）で処理
順を決める（機能 001 の FR-007 と同じ規則）。

## バスのレセシブ期間（シミュレーション状態）

`SimulationEngine` が保持する単一の値。エンティティというより実行時状態。

| フィールド | 型のイメージ | 説明 |
|---|---|---|
| RecessiveRunStartNs | 整数 | 現在（または直近）のレセシブ期間の開始時刻。送信開始時に `送信開始 + 伝送時間 − 8ビット時間` へ更新する。シミュレーション開始時は 0 |

同期完了予定時刻は `max(検出開始時刻, RecessiveRunStartNs) + 11 * ビット時間` で算出する
（research.md 決定1）。

## StartupRecord（新規エンティティ）

起動記録。ノード1台につき1件、シミュレーション終了時に確定する。

| フィールド | 型のイメージ | 説明 |
|---|---|---|
| NodeId | 整数 | 設定上のノード番号 |
| NodeName | 文字列 | ノード名称 |
| PowerOnUs | 整数 | 設定された電源投入時刻 |
| InitDoneUs | 整数 or 空 | 初期化完了時刻（未到達なら空） |
| NormalEntryUs | 整数 or 空 | 通常動作開始時刻（未到達なら空） |
| ReachedNormal | 真偽 | シミュレーション終了までに通常動作へ到達したか（FR-013） |

## エンティティ関係の概要

```
Network 1 ── * Node（起動設定と起動状態を保持）
                 │
                 └── 1 StartupRecord（シミュレーション終了時に Node から生成）

SimulationEngine
  ├── RecessiveRunStartNs（バス1本ぶん）
  └── Event（PowerOn / InitDone / SyncComplete を含む）を時刻順に処理
```
