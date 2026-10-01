# Data Model: 015 スピードスライダー

プロジェクトファイルの形式・CSV は変わらない。スピードは今までどおり `ProjectSettings::Speed`（-13〜+10）。

## CanSim（`CanSim/SimControls.h` に追加、UI 非依存）

| 要素 | 内容 |
|---|---|
| `kSpeedSliderLeft = 200` / `kSpeedTickPitch = 26` | -13 の目盛りの x と、目盛りの間隔（96 dpi） |
| `kSpeedTrackY = 20` / `kSpeedTrackThickness = 4` | 溝の y 中央と太さ |
| `kSpeedThumbWidth = 10` / `kSpeedThumbTop = 7` / `kSpeedThumbBottom = 33` | つまみ |
| `kSpeedTickTop = 28` / `kSpeedTickBottom = 33` | 目盛りの線 |
| `kSpeedLabelTop = 38` / `kSpeedLabelBottom = 56` | 目盛りの数字 |
| `kSpeedHitTop = 4` / `kSpeedHitBottom = 60` | 当たり判定の上下（左右は端の目盛り ± pitch/2） |
| `int SpeedTickX(int speed, int dpi)` | 目盛りの x（DPI 換算済み） |
| `int SpeedAtX(int x, int dpi)` | x に一番近い目盛りの値（同距離は小さい方、範囲外は端） |
| `bool IsOnSpeedSlider(int x, int y, int dpi)` | スライダーの操作を始める範囲か |
| `DiagramRect SpeedTrackRect(int dpi)` / `SpeedThumbRect(int speed, int dpi)` / `SpeedLabelRect(int speed, int dpi)` | 描画用の範囲 |
| `enum class SpeedChangeAction { None, Rebase, FinishNow }` | スピードを変えたときの動作 |
| `SpeedChangeAction GetSpeedChangeAction(RunPhase phase, int newSpeed)` | 下の表 |

### スピードを変えたときの動作

| RunPhase | newSpeed < +10 | newSpeed = +10 |
|---|---|---|
| Idle | None | None |
| Running | Rebase（今の時刻を基準に新しいスピードで続ける） | FinishNow（その場で完了） |
| Paused | None（時刻は止めたまま、再開で効く） | FinishNow |

## ダイアログ（`CMFCApplication1Dlg`）

| 要素 | 内容 |
|---|---|
| `m_settings.Speed` | スピードの唯一の保持場所。スライダーはこれを描く |
| `m_draggingSpeed` | スライダーのドラッグ中か |
| `SetSpeed(int speed)` | 値の変更と、`GetSpeedChangeAction` に応じた基準の付け替え・その場での完了 |

## セッティング（`CSettingsDlg`）

- スピードの項目を削除する。ECU 数・通信速度・シミュレーション時間だけになる。`Settings.Speed` は受け取った値をそのまま返す。
