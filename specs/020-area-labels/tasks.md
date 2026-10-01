---

description: "Task list for feature 020-area-labels"
---

# Tasks: イベント表示・ログ表示のラベル

## Phase 1: Foundational

- [X] T001 `MFCApplication1/CanSim/BusDiagramLayout.h/.cpp` に、ラベルの範囲を追加する（96 dpi、コンテンツ座標）。
  - 定数:
    - `kEventLabelLeft = 10`、`kEventLabelTop = 8`、`kEventLabelRight = 120`、`kEventLabelBottom = 26`
    - `kLogLabelTop = 236`、`kLogLabelRight = 300`、`kLogLabelBottom = 254`（左端は `kLogViewLeft - kLogFrameGap`）
  - 関数: `DiagramRect EventLabelRect()`、`DiagramRect LogLabelRect()`。
- [X] T002 [P] `MFCApplication1/Resource.h` に `IDS_EVENT_VIEW_LABEL 249`・`IDS_LOG_VIEW_LABEL 250`（`_APS_NEXT_SYMED_VALUE` は 251）を追加する。`MFCApplication1.rc` の文字列テーブルに「イベント表示」「ログ表示」を追加する（UTF-16LE を保ち、読み直して確認する）。

## Phase 2: User Story 1 (P1)

- [X] T003 [US1] `MFCApplication1/MFCApplication1Dlg.cpp` で、ラベルを描く。（依存: T001, T002）
  - イベント表示: `DrawBusDiagram` の枠の後に、`ScaleRect(EventLabelRect())` へ描く。
  - ログ表示: `DrawLogFrame` の枠の後に、`LogLabelRect()` を換算し、`ContentTop()` だけ下げた範囲へ描く。
  - 書式: 12 px、Meiryo UI、`RGB(90,90,90)`、`DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX`、背景は透明。
- [X] T004 [P] [US1] `tools/cansim_runner/checks.cpp` に、`CheckFeature020()` を追加する（plan.md の検証項目）。（依存: T001）
- [X] T005 [US1] ビルドし、実機の画面写しで確かめる。対象は、起動直後（ECU 2）と、16 ECU・フレーム表示 ON の実行中の 2 つ。
  - 2 つのラベルの位置と読みやすさ。
  - ほかの描画物と重ならないこと。

## Phase 3: Polish

- [X] T006 [P] `CLAUDE.md`・`tools/cansim_runner/README.md` を更新する。
- [X] T007 4 構成のビルド（警告なし）・checks.exe 全 PASS・CSV の一致を確かめる。

## 実装結果（2026-10-01）

- 変更:
  - `CanSim/BusDiagramLayout.h/.cpp`（`EventLabelRect`、`ScaleLogLabelRect`）
  - `MFCApplication1Dlg.h/.cpp`（`DrawAreaLabel`、構成図・ログの枠での描画）
  - `Resource.h`・`MFCApplication1.rc`（`IDS_EVENT_VIEW_LABEL` / `IDS_LOG_VIEW_LABEL`。読み直して確認済み）
  - `tools/cansim_runner/checks.cpp`・`README.md`、`CLAUDE.md`
- 実装中の修正: ログ表示のラベルの左端は、当初 147 px を換算して求めていた。DPI 144 では、枠の左端（150 と 3 を別々に換算して引く）と 1 px ずれた。
  そこで、`ScaleLogLabelRect(dpi)` で、枠の換算から直接求めるようにした。
- ビルド: Debug/Release × x64/x86 の 4 構成とも、警告・エラーなしでビルドできた。
- checks.exe: 全 168 項目 PASS（020 の 2 項目を追加）。基準 CSV との比較も一致した。
- GUI（Release|x64、120 dpi）: 起動直後と、`sample_16ecu`（フレーム表示 ON・実行中）の画面写し（`t020/idle.png`・`t020/running16.png`）を撮った。
  - 「イベント表示」は、構成図の枠の内側の左上にあった。
  - 「ログ表示」は、ログの枠のすぐ上の左端にあった。
  - どちらも、ECU の箱・「CAN BUS」・ログの行と重ならず、読めた。

## 改訂 1（ユーザーの指示）

- [X] T008 `MFCApplication1/MFCApplication1.rc` の `IDS_LOG_VIEW_LABEL` を「イベント＆ログ表示」（全角の＆）に変え、CLAUDE.md の記述を直す。ビルドして画面写しで、文字がラベルの範囲（x 147〜300）に収まることを確かめる。

## 改訂 2（ユーザーの指示）

- [X] T009 構成図の「イベント表示」のラベルを外す。
  - 削除するもの:
    - `EventLabelRect` と `kEventLabel*`
    - `DrawBusDiagram` の描画
    - `IDS_EVENT_VIEW_LABEL`（Resource.h・.rc）
    - checks 020 の該当項目
  - CLAUDE.md・README の記述を直す。4 構成のビルドと checks を確かめる。
