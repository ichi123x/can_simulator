---

description: "Task list for feature 019-toolbar-compact-layout"
---

# Tasks: ツールバーの並びの入れ替えとボタン・スライダーの縮小

**Input**: `/specs/019-toolbar-compact-layout/` の spec.md・plan.md（座標の表）

## Phase 1: Foundational

- [X] T001 `MFCApplication1/CanSim/SimControls.h/.cpp` の定数を、plan.md の「新しい座標」の表のとおりに変える。
  - 追加する定数:
    - `kSpeedCaptionLeft = 430`、`kSpeedCaptionRight = 500`、`kSpeedCaptionTop = 10`、`kSpeedCaptionBottom = 30`
    - `kSpeedLabelWidth = 26`（数字の範囲の幅。これまでの `kSpeedTickPitch` の代わり）
  - 追加する関数:
    - `DiagramRect SpeedCaptionRect(int dpi)`
    - `bool IsSpeedLabelShown(int speed, int currentSpeed)`
      - 真になるのは、`speed == currentSpeed` のとき。
      - または、speed が {-13, -10, -5, 0, 5, 10} のどれかで、`|speed - currentSpeed| > 2` のとき。
  - 変える関数: `SpeedLabelRect` の幅は `kSpeedLabelWidth`、`IsSpeedLabelShown` の当たり判定は目盛り ± `kSpeedTickPitch / 2`（= 6）。
- [X] T002 [P] `MFCApplication1/Resource.h` に `IDS_SPEED_CAPTION 248`（`_APS_NEXT_SYMED_VALUE` は 249）を追加する。`MFCApplication1.rc` の文字列テーブルに `IDS_SPEED_CAPTION "スピード："` を追加する（UTF-16LE・BOM・CRLF を保ち、読み直して確認する）。

## Phase 2: User Story 1 - 小さくまとまったツールバーを使う (P1)

- [X] T003 [US1] `MFCApplication1/MFCApplication1Dlg.cpp` の `DrawSimToolbar` を直す。（依存: T001, T002）
  - ボタンの記号の直書きの値を半分にする（棒 `ScalePx(3)`・間 `ScalePx(3)`、四角 `ScalePx(8)`）。
  - ボタンの名前は、10 px の別のフォントで描く。チェックボックス・時刻の欄の文字は、今の 12 px のまま。
  - スライダーの数字は、`IsSpeedLabelShown(s, m_settings.Speed)` のものだけを描く。
  - スライダーの左に、`IDS_SPEED_CAPTION` を `SpeedCaptionRect` に描く（12 px、白、`DT_RIGHT | DT_VCENTER | DT_SINGLELINE`）。
- [X] T004 [P] [US1] `tools/cansim_runner/checks.cpp` の 015・017 の項目を、新しい決まりに合わせる。新しい `CheckFeature019()` を追加する。（依存: T001）
  - 015 の「数字が重ならない」: すべての今の値（-13〜+10）について、`IsSpeedLabelShown` が真の数字どうしが重ならないことにする。
  - 015・017 の「並び」: ボタン → チェックボックス → 時刻の欄 → 「スピード：」→ スライダーの順にする。
  - 019:
    - 並びと重なりの無さ（DPI 96/120/144）。
    - ボタンの箱が 24 × 18（96 dpi）。
    - 目盛りの間隔が 13。
    - `IsSpeedLabelShown` の表（例: 今の値が 0 なら -13・-10・-5・0・+5・+10。今の値が -12 なら -12・-5・0・+5・+10。今の値が +9 なら -13・-10・-5・0・+5・+9）。
- [X] T005 [US1] Debug|x64 をビルドし、実機で確かめる。（依存: T003）
  - 起動直後の画面写しで、並び・大きさ・「スピード：」・数字の間引きを目視で確認する。
  - 新しい位置で、次を確かめる。
    - 開始ボタンで実行が始まる。
    - チェックボックスで切り替わる。
    - スライダーの +4 の目盛りのクリックで、+4 になる（数字の表示が変わる）。
    - 中断・終了ボタンが効く。

## Phase 3: Polish

- [X] T006 [P] `CLAUDE.md`（ツールバーの並び・大きさ・スライダーの数字）と `tools/cansim_runner/README.md`（019 の項目）を更新する。
- [X] T007 Debug/Release × x64/x86 の全 4 構成で、次を確かめる。（依存: T005）
  - 警告なしでビルドできる。
  - checks.exe の全項目が PASS する。
  - サンプルの CSV が基準と一致する。

## 実装結果（2026-10-01）

- 変更:
  - `CanSim/SimControls.h/.cpp`（座標の定数、`kSpeedLabelWidth`、`SpeedCaptionRect`、`IsSpeedLabelShown`）
  - `MFCApplication1Dlg.cpp`（記号の大きさ、名前の 10 px のフォント、数字の間引き、「スピード：」）
  - `Resource.h`・`MFCApplication1.rc`（`IDS_SPEED_CAPTION`。読み直して確認済み）
  - `tools/cansim_runner/checks.cpp`・`README.md`、`CLAUDE.md`
- ビルド: Debug/Release × x64/x86 の 4 構成とも、警告・エラーなしでビルドできた。
- checks.exe: 全 166 項目 PASS（019 の 3 項目を追加し、015・017 の並びと数字の重なりの項目を 019 の並びで検証）。基準 CSV との比較も一致した。
- GUI（Release|x64、120 dpi。Debug 版はユーザーが起動中で書き込めなかったため）: scratchpad の `t019/gui.ps1` で、全項目が合格した。
  - 並び・大きさ・「スピード：」・数字の間引きは、`t019/toolbar.png` で目視で確認した。
  - 起動直後のボタンの状態は正しかった。
  - スライダーの +4 の目盛りのクリックで +4 になった。
  - チェックボックスは、欄と文字のどちらでも切り替わった。
  - 開始・中断・再開・終了のボタンは、新しい位置で効いた。
- 作業中の不手際: 確認スクリプトの冒頭に、以前の確認から引き継いだ「起動中の MFCApplication1 をすべて終了する」処理が残っていた。そのため、ユーザーが起動していた Debug 版を終了させてしまった。scratchpad のすべてのスクリプトから、この処理を外した。
