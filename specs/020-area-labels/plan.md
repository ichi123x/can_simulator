# Implementation Plan: イベント表示・ログ表示のラベル

**Branch**: `ver.0.3`（spec ディレクトリ: `020-area-labels`） | **Date**: 2026-10-01 | **Spec**: [spec.md](./spec.md)

## Summary

`CanSim/BusDiagramLayout.h` にラベルの範囲（96 dpi、コンテンツ座標）を置き、ダイアログが描く。

| ラベル | 範囲（96 dpi） | 描く所 |
|---|---|---|
| イベント表示 | x 10〜120、y 8〜26（枠 4〜 の内側、ECU の箱の上端 34・「CAN BUS」の上端 約 80 より上） | `DrawBusDiagram`（メモリ DC、枠の後・内容の前） |
| ログ表示 | x 147〜300、y 236〜254（構成図の下端 216 とログの枠の上端 257 の間、枠の左端 147 にそろえる） | `DrawLogFrame`（枠と同じ所） |

- 文字: 12 px、Meiryo UI、`RGB(90,90,90)`、`DT_LEFT | DT_VCENTER | DT_SINGLELINE`。
- 文字列: `IDS_EVENT_VIEW_LABEL` = 249「イベント表示」、`IDS_LOG_VIEW_LABEL` = 250「ログ表示」（`_APS_NEXT_SYMED_VALUE` は 251）。
- checks: 次の 2 点を確かめる（DPI 96/120/144）。
  - イベント表示のラベルが、枠の内側にあり、ECU 数 0〜16 のどの ECU の箱・「CAN BUS」（`kCaptionLeft`〜`kCaptionRight`、バスの上 26 px）・バスとも重ならない。
  - ログ表示のラベルが、構成図の下端より下・ログの枠の上端より上にある。

## Constitution Check

I〜VI PASS。座標を CanSim に置いて checks で検証する（III）。ラベル 2 つを描くだけ（IV）。CLAUDE.md を更新する（V）。

## Project Structure

```text
specs/020-area-labels/ spec.md / plan.md / tasks.md / checklists/requirements.md
MFCApplication1/CanSim/BusDiagramLayout.h/.cpp, MFCApplication1Dlg.cpp, Resource.h, MFCApplication1.rc
tools/cansim_runner/checks.cpp, README.md, CLAUDE.md
```
