# Implementation Plan: ツールバーの並びの入れ替えとボタン・スライダーの縮小

**Branch**: `ver.0.3`（spec ディレクトリ: `019-toolbar-compact-layout`） | **Date**: 2026-10-01 | **Spec**: [spec.md](./spec.md)

## Summary

ツールバーの座標は `CanSim/SimControls.h` の定数に集まっているので、定数を変えて並びと大きさを変え、表示する数字の判定
`IsSpeedLabelShown` と「スピード：」の範囲 `SpeedCaptionRect` を加える。ダイアログはボタンの記号の大きさ（直書きの値）と名前の文字の大きさ、
数字の間引き、「スピード：」の描画を直す。checks の 015・017 の並び・数字の重なりの項目を新しい決まりに合わせる。

## 新しい座標（96 dpi、帯の中）

| 部品 | 変更前 | 変更後 |
|---|---|---|
| ボタン i の箱 | 左 `8+56i`、上 6、48 × 36、角丸 8 | 左 `8+28i`、上 14、24 × 18、角丸 4 |
| ボタンの名前 | y 42〜60、文字 12 px | y 33〜49、文字 10 px |
| ボタンの記号 | 20 px（棒 6・間 6、四角 16） | 10 px（棒 3・間 3、四角 8） |
| チェックボックス | 欄 x 186・文字 206〜290・当たり 182〜294 | 欄 x 104・文字 124〜208・当たり 100〜212 |
| シミュレーション時間 | x 950〜1130（数値〜1100、μs 1104〜） | x 230〜410（数値〜380、μs 384〜） |
| 「スピード：」 | なし | x 430〜500、y 10〜30 |
| スライダー | -13 の目盛り x 320、間隔 26（+10 は 918） | -13 の目盛り x 520、間隔 13（+10 は 819） |
| つまみ | 幅 10、y 7〜33 | 幅 5、y 13〜27 |
| 溝・目盛りの線 | 太さ 4、線 y 28〜33 | 太さ 2、線 y 24〜27 |
| 数字 | 24 個、y 38〜56、幅 26 | 5 刻み＋今の値、y 32〜48、幅 26 |
| スライダーの当たり判定 | 目盛り ± 13、y 4〜60 | 目盛り ± 6（間隔の半分）、y 4〜60 |

- 数字の表示: `IsSpeedLabelShown(s, current)` = `s == current` か、（s が 5 刻みの値で `|s − current| > 2`）。
  - 5 刻みの値は -13・-10・-5・0・+5・+10 とする。
  - 表示する数字どうしは 3 目盛り（39 px）以上離れるので、幅 26 の範囲は重ならない。

## Technical Context / Constitution Check

- C++14・MFC/GDI、新規依存なし。変更するのは `SimControls.h/.cpp`、`MFCApplication1Dlg.cpp`、`Resource.h`・`.rc`（`IDS_SPEED_CAPTION` = 248）、`checks.cpp`・README、`CLAUDE.md`。
- 憲章 I〜VI: PASS。座標と表示の判定は CanSim の純粋関数に置き、checks で検証する（III）。並びと大きさだけの変更で、新しい仕組みは足さない（IV）。CLAUDE.md を更新する（V）。

## Project Structure

```text
specs/019-toolbar-compact-layout/ spec.md / plan.md / tasks.md / checklists/requirements.md
MFCApplication1/CanSim/SimControls.h/.cpp, MFCApplication1/MFCApplication1Dlg.cpp, Resource.h, MFCApplication1.rc
tools/cansim_runner/checks.cpp, README.md, CLAUDE.md
```
