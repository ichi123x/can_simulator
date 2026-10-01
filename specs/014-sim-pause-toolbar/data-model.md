# Data Model: 014 中断とツールバー

プロジェクトファイル・CSV の形式は変わらない。

## CanSim（`CanSim/SimControls.h`、UI 非依存）

| 要素 | 内容 |
|---|---|
| `enum class RunPhase { Idle, Running, Paused }` | 実行の状態。Idle = 待機、Running = 実行中（時計が進む）、Paused = 中断中（時計が止まる） |
| `struct SimCommandStates { bool Start, Pause, Stop; }` | 開始・中断・終了の有効／無効 |
| `SimCommandStates GetSimCommandStates(RunPhase, bool editingFrames)` | 状態 → 有効／無効（下の表） |
| `const int kToolbarHeight = 64` 他 | ツールバーの寸法（96 dpi。research.md 決定3） |
| `DiagramRect SimButtonRect(int index, int dpi)` | ボタン i（0 = 開始、1 = 中断、2 = 終了）の当たり判定の範囲（箱 + 名前）。DPI で換算済み |
| `DiagramRect SimButtonBoxRect(int index, int dpi)` / `SimButtonLabelRect(int index, int dpi)` | 箱・名前の範囲（描画用） |
| `int FindSimButtonAt(int x, int y, int dpi)` | クライアント座標の点がどのボタンか（0〜2）。どれでもなければ -1 |
| `long long ResumedSimTimeUs(long long baseUs, long long elapsedRealUs, int speed)` | `baseUs + SimTimeAtUs(elapsedRealUs, speed)`。加算は飽和する |

### 有効／無効（FR-004）

| RunPhase | editingFrames | Start | Pause | Stop |
|---|---|---|---|---|
| Idle | false | ✔ | – | – |
| Idle | true | – | – | – |
| Running | 任意 | – | ✔ | ✔ |
| Paused | 任意 | ✔（再開） | – | ✔ |

## ダイアログ（`CMFCApplication1Dlg`）

| メンバ | 内容 |
|---|---|
| `m_runPhase`（`CanSim::RunPhase`） | 旧 `m_runState`。Paused を追加 |
| `m_simBaseUs` | 再開の基準のシミュレーション時刻。開始時は 0、再開のたびに中断の時刻 |
| `m_pressedButton` | 押下中のツールバーのボタン（-1 = なし） |
| `InRun()` | `m_runPhase != Idle` |
| `ContentTop()` | 帯の高さ（DPI 換算）。構成図・ログ表示エリアのずらし |
| `UpdateSimControls()` | メニューのトップレベルの状態とツールバーの再描画 |
| `OnSimPause()` | 中断（Running のときだけ） |

## 状態遷移

```text
          開始（保存先の確定・計算・表示の初期化）
  Idle ────────────────────────────────────▶ Running
   ▲  ▲                                      │  ▲
   │  │ 自然終了（全結果の CSV）               │  │ 開始（再開。計算・初期化なし）
   │  └──────────────────────────────────────┤  │
   │        終了（その時点までの CSV）          │  │
   │  ◀──────────────────────────────────────┘  │
   │                                  中断 │     │
   │                                       ▼     │
   └──────── 終了（中断の時刻までの CSV） ── Paused
```

- スピード +10 の開始は、Running を経ずに Idle に戻る（即時完了）。
- 中断の時点で終端に達していた場合は、自然終了として Idle に戻る。
