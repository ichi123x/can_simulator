# Data Model: 005

## ProjectSettings（`CanSim/CanTypes.h`、004 に追加）
| 項目 | 型 | 既定 | 検証 |
|---|---|---|---|
| Speed | int | 0 | -13〜+10 の整数 |

定数: `kMinSpeed = -13`、`kMaxSpeed = 10`、`kInstantSpeed = 10`（待ちなし）。

## ReplayClock（`CanSim/ReplayClock.h`、ASCII のみ）
- `long long SimTimeAtUs(long long elapsedRealUs, int speed)` — `speed` が -13〜+9 のとき
  `elapsedRealUs × 2^speed`（整数演算、オーバーフローしない範囲で飽和）。`speed == 10` は呼ばない（即時）。
- `struct ActiveFrames { std::vector<int> Senders; std::vector<int> Receivers; bool BusBusy; }`
  - `Receivers` には `Senders` に含まれる ECU を入れない（送信の強調を優先、spec US4 AS3）。重複なし、昇順。
- `class ActiveFrameCursor` — `FrameEvents`（TxStart 昇順）への参照とカーソルを持ち、
  `ActiveFrames Collect(long long fromUs, long long toUs)` で区間 `(fromUs, toUs]` と送信区間
  `[TxStartUs, TxEndUs]` が重なるフレームを集める。`toUs` は呼び出しごとに単調増加すること。
  完全に `fromUs` 以前に終わったフレームはカーソルで読み飛ばす。

## 実行の進行状況（UI、`CMFCApplication1Dlg`）
| 項目 | 内容 |
|---|---|
| m_replayStartQpc | 再生開始時の QPC 値 |
| m_prevSimUs / m_curSimUs | 直前／今回の書き換え時のシミュレーション時刻 |
| m_active | 今回の書き換えで強調する `ActiveFrames` |
| m_runState | 004 の Idle / Running をそのまま使う |

終端: `m_curSimUs >= DurationSec × 10^6` になったら、時刻を終端に丸めて最後の区間を表示し、CSV を書いて完了。
