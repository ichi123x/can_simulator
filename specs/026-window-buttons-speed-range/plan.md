# Implementation Plan: ウィンドウの最小化・最大化ボタンとスピードの範囲の変更

**Branch**: `ver.0.5`（spec ディレクトリ: `026-window-buttons-speed-range`） | **Date**: 2026-10-02 | **Spec**: [spec.md](./spec.md)

## Summary

1. **最小化・最大化**（Q1 = A、Q2 = A）: メインダイアログ `IDD_MFCAPPLICATION1_DIALOG` の `.rc` の STYLE に `WS_MINIMIZEBOX | WS_MAXIMIZEBOX` を足すだけで、Windows 標準のタイトルバーのボタン（_ □ ✖）・システムメニューの「最小化」「最大化」・タイトルバーのダブルクリック・スナップが使えるようになる。
   中身の配置は今の「端を引っ張って大きさを変えた」ときと同じ（構成図・ログ表示エリアは左上の固定位置、余りは白、ツールバーの帯は幅いっぱい）なので、描画・当たり判定のコードは変えない。
   実行中の時計は `QueryPerformanceCounter` の経過時間で決まり、`WM_TIMER` は最小化中も届くので、最小化しても実行は進む（コードの変更は要らない。確かめるだけ）。
2. **スピードの範囲 -10〜+10**: `CanSim::kMinSpeed` を -13 → -10 にする。スライダーの位置は **-10 の目盛りを今の -13 の位置（96 dpi で x 604）に置いて右へ詰める**（左端を揃え、+10 は x 864）。
   読込みは -13〜-11 を -10 として受け入れ（新しい定数 `kLegacyMinSpeed = -13`）、-14 以下・+11 以上はエラー（文言は -10〜+10）。
   checks の 005・015・019 のスピードの範囲に関わる項目を新しい範囲に直す。

## 変わる座標（96 dpi、帯の中）

| 部品 | 変更前 | 変更後 |
|---|---|---|
| 目盛り | -13〜+10 の 24 個、x 604〜903 | -10〜+10 の 21 個、x 604〜864（間隔 13 のまま） |
| 当たり判定（横） | 598〜909 | 598〜870 |
| 数字（5 刻み） | -13・-10・-5・0・+5・+10 | -10・-5・0・+5・+10（`IsSpeedLabelShown` の「`speed == kMinSpeed` は常に表示」は -10 が 5 刻みなので結果は同じ。式はそのまま） |
| +10 の数字の右端 | 916 | 877 |

縦方向、「スピード：」（514〜584）、チェックボックス・シミュレーション時間・ボタンの位置は変えない。

## Technical Context

- **Language/Version**: C++14 / MFC（Visual Studio 2022、v143）。新規依存なし
- **Primary Dependencies**: MFC（ダイアログのスタイル）、CanSim（`CanTypes.h` の定数、`ConfigLoader`、`SimControls`）
- **Storage**: `.canprj` の `Speed=`（形式は変えない。読込みの範囲だけ変わる）
- **Testing**: `tools/cansim_runner/checks.cpp`（スピードの範囲・目盛り・数字・読込みの互換）。ウィンドウの操作は [quickstart.md](./quickstart.md)
- **Target Platform**: Windows、Win32 / x64 × Debug / Release
- **Project Type**: デスクトップアプリ（単一ダイアログ）
- **Constraints**: `CanSim/` は ASCII のみ。`.rc` は UTF-16LE
- **Scale/Scope**: `.rc` のスタイル 1 行、定数 1 つ + 読込みの互換、checks の修正

## Constitution Check

| 原則 | 判定 | 理由 |
|---|---|---|
| I. Spec Kit | PASS | spec（Q1・Q2 回答済み）→ plan → tasks |
| II. ビルド | PASS | 4 構成のビルドを tasks で確認する |
| III. テスト可能性 | PASS | スピードの範囲・座標・読込みの互換は CanSim にあり、checks で検証する |
| IV. シンプルさ | PASS | 標準のボタン（スタイルの追加だけ）。独自のタイトルバーやレイアウトの伸縮は作らない |
| V. ドキュメント | PASS | CLAUDE.md（スピードの範囲 -13 の記述、スライダーの座標、メインのスタイル）と README を更新する |
| VI. ドメイン仕様 | PASS | 再生の速さの範囲と UI だけで、シミュレーションの計算・CSV に影響しない |

設計後の再確認: 変更なし（PASS）。

## Project Structure

```text
specs/026-window-buttons-speed-range/
  spec.md / plan.md / research.md / data-model.md / quickstart.md
  contracts/ui-window-and-speed.md
  checklists/requirements.md

MFCApplication1/MFCApplication1.rc            IDD_MFCAPPLICATION1_DIALOG の STYLE に WS_MINIMIZEBOX | WS_MAXIMIZEBOX
MFCApplication1/CanSim/CanTypes.h             kMinSpeed = -10、kLegacyMinSpeed = -13、Speed のコメント
MFCApplication1/CanSim/ConfigLoader.cpp       -13〜-11 → -10、範囲外のエラー
MFCApplication1/CanSim/SimControls.h/.cpp     コメントの「-13」を直す（式は kMinSpeed を使っているので変えない）
MFCApplication1/MFCApplication1Dlg.cpp        変更なしの見込み（kMinSpeed を使う箇所はそのまま。最小化中の再生・OnPaint の IsIconic を確かめる）
tools/cansim_runner/checks.cpp, README.md     005・015・019 の項目を新しい範囲に、026 の項目を追加
CLAUDE.md
```

**Structure Decision**: 既存の構成のまま。

## Complexity Tracking

なし。
