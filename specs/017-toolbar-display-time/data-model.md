# Data Model: 017 フレーム表示チェックボックスとシミュレーション時間の表示

プロジェクトファイル・CSV の形式は変わらない。フレーム表示は今までどおり `ProjectSettings::FrameDisplay`。

## CanSim（`CanSim/SimControls.h` に追加・変更、UI 非依存）

| 要素 | 内容 |
|---|---|
| `kSpeedSliderLeft` | 200 → **320**（015 の定数。チェックボックスの分だけ右へ） |
| `kFrameDisplayBoxLeft = 186` / `kFrameDisplayBoxTop = 25` / `kFrameDisplayBoxSize = 14` | チェック欄 |
| `kFrameDisplayLabelLeft = 206` / `kFrameDisplayLabelRight = 290` / `kFrameDisplayLabelTop = 22` / `kFrameDisplayLabelBottom = 42` | 文字 |
| `kFrameDisplayHitLeft = 182` / `kFrameDisplayHitRight = 294` / `kFrameDisplayHitTop = 4` / `kFrameDisplayHitBottom = 60` | 当たり判定 |
| `kSimTimeLeft = 950` / `kSimTimeRight = 1130` / `kSimTimeCaptionTop = 8` / `kSimTimeCaptionBottom = 28` / `kSimTimeValueTop = 30` / `kSimTimeValueBottom = 54` / `kSimTimeUnitLeft = 1104` / `kSimTimeValueRight = 1100` | 時刻の欄 |
| `DiagramRect FrameDisplayBoxRect(int dpi)` / `FrameDisplayLabelRect(int dpi)` | 描画用 |
| `bool IsOnFrameDisplayToggle(int x, int y, int dpi)` | 当たり判定（換算後の `[Left, Right) × [Top, Bottom)`） |
| `DiagramRect SimTimeCaptionRect(int dpi)` / `SimTimeValueRect(int dpi)` / `SimTimeUnitRect(int dpi)` / `SimTimeAreaRect(int dpi)` | 描画用・無効化用 |
| `std::string FormatSimTimeUs(long long us)` | 3 けた区切り（負は 0） |

## ダイアログ（`CMFCApplication1Dlg`）

| 要素 | 内容 |
|---|---|
| `m_settings.FrameDisplay` | フレーム表示の唯一の保持場所。チェックボックスはこれを描く |
| `m_displaySimUs` | 表示するシミュレーション時刻（research.md 決定3 の表） |
| `ToggleFrameDisplay()` | 反転と描き直し（いつでも） |
| `SetDisplaySimUs(long long us)` | 値が変わったら時刻の欄を無効化 |
| 削除 | `OnEditFrameDisplay`、`ON_COMMAND(ID_EDIT_FRAME_DISPLAY)`、メニューのチェック |

## ツールバーの並び（左から、96 dpi）

| 要素 | x |
|---|---|
| 開始・中断・終了のボタン | 8〜168 |
| フレーム表示のチェックボックス | 182〜294 |
| スピードスライダー | 307〜931（目盛り 320〜918） |
| シミュレーション時間 | 950〜1130 |
