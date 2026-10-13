# Implementation Plan: ツールバーのフレーム表示チェックボックスとシミュレーション時間の表示

**Branch**: `ver.0.3`（spec ディレクトリ: `017-toolbar-display-time`） | **Date**: 2026-10-01 | **Spec**: [spec.md](./spec.md)

## Summary

1. **メニュー**: 「編集 → フレーム表示」（`ID_EDIT_FRAME_DISPLAY`）を削除する。
2. **チェックボックス**: ツールバーの終了ボタンとスピードスライダーの間に、自前で描く「フレーム表示」のチェックボックスを置く。
   - 押すたびに `m_settings.FrameDisplay` を反転し、構成図を描き直す。
   - 実行中・中断中もその場で反映する（Q1: A。`DrawBusDiagram` は今も `InRun() && FrameDisplay` で強調を描くので、値を変えて描き直すだけでよい）。
   - スピードスライダーは 120 px（96 dpi）右へずらす。
3. **時刻の表示**: スライダーの右（帯の右端）に、上段「シミュレーション時間」、下段に今のシミュレーション時刻の数値（3 けた区切り）と「μs」を描く（Q2: A）。
   - 表示する値は `m_displaySimUs` に持つ。
   - 開始で 0、再生の書き換えで `m_curSimUs`、自然終了・即時完了で終端、「終了」で停止時刻にする。

チェックボックス・時刻の表示欄の座標と、数値の書式（3 けた区切り）は `CanSim/SimControls` の純粋関数にして checks.exe で検証する。計算・CSV・プロジェクトファイルの形式は変えない。

## Technical Context

**Language/Version**: C++14相当（機能 001〜016 と同じ）

**Primary Dependencies**: MFC / GDI（014・015 のツールバーの描画に追加）。新規外部依存なし。

**Storage**: プロジェクトファイルの `[Project] FrameDisplay=` を今までどおり使う（形式は変えない）。

**Testing**:
- checks.exe に 017 の項目を加える。
  - 座標（帯の中・重ならない・左から ボタン → チェックボックス → スライダー → 時刻の順。DPI 96/120/144）
  - `IsOnFrameDisplayToggle` の当たり判定
  - `FormatSimTimeUs`（0、999、1000、10,000,000、600,000,000）
- 既存の 001〜016 の PASS と CSV の一致を確認する（015 のスライダーの項目は、ずらした後の座標で通ること）。
- GUI は自動操作で確認する（quickstart.md）。

**Target Platform**: Windows 10/11（Win32/x64、Debug/Release）

**Project Type**: desktop-app

**Performance Goals**: 時刻の表示は画面の書き換え（約 40 回/秒）ごとに更新する。描き直すのは時刻の欄だけ。

**Constraints**:
- `CanSim/` は ASCII のみ。日本語の文字列（「フレーム表示」「シミュレーション時間」「μs」）は `.rc` の文字列テーブルに置く。
- `.rc` は UTF-16LE で、エンコーディングを明示して編集する。
- 構成図・ログ表示エリアの位置は変えない。帯の高さ（64）とウィンドウの大きさも変えない。

**Scale/Scope**:
- 変更:
  - `CanSim/SimControls.h/.cpp`（チェックボックス・時刻の欄の座標、`FormatSimTimeUs`、スライダーの左端の値）
  - `MFCApplication1Dlg.h/.cpp`（メニューの処理の削除、チェックボックス・時刻の描画と操作、`m_displaySimUs`）
  - `Resource.h`・`MFCApplication1.rc`（メニュー項目の削除、文字列 3 つの追加）
  - `tools/cansim_runner/checks.cpp`・`README.md`
  - `CLAUDE.md`

## Constitution Check

| 原則 | 判定 | 根拠 |
|---|---|---|
| I. Spec Kit ワークフロー準拠 | PASS | spec.md（017、Q1・Q2 回答済み）→ 本 plan.md。 |
| II. MFC/C++ ネイティブ規約 | PASS | 既存ファイルの変更のみ。4 構成のビルドを維持する。 |
| III. テスト可能性 | PASS | 座標・当たり判定・数値の書式を CanSim の純粋関数にし、checks で検証する。ダイアログは描画と状態の更新だけ。 |
| IV. YAGNI | PASS | 014・015 の自前描画のツールバーに描き足す。標準のチェックボックス（`CButton`）は使わない（暗い帯の上の見た目と、フォーカスを持たない方針。research.md 決定1）。 |
| V. ドキュメント同期 | PASS（条件付き） | CLAUDE.md（メニュー・ツールバー・フレーム表示の切り替え）と checks の README を更新するタスクを含める。 |
| VI. ドメイン仕様準拠 | PASS | 表示だけの変更。シミュレーションの挙動・結果は変えない。 |

違反なし。Complexity Tracking は該当なし。

**Post-design re-check**: 設計後も判定は変わらない。

## Project Structure

### Documentation (this feature)

```text
specs/017-toolbar-display-time/
├── spec.md / plan.md / research.md / data-model.md / quickstart.md
├── contracts/ui-toolbar-display-time.md
├── checklists/requirements.md
└── tasks.md（/speckit-tasks で作成）
```

### Source Code (repository root)

```text
MFCApplication1/
├── CanSim/SimControls.h / .cpp      # 追加: チェックボックス・時刻の欄の座標、FormatSimTimeUs。変更: kSpeedSliderLeft 200 → 320
├── MFCApplication1Dlg.h / .cpp      # 変更: OnEditFrameDisplay を削除し ToggleFrameDisplay、チェックボックス・時刻の描画と押下、m_displaySimUs
├── Resource.h / MFCApplication1.rc  # 変更: ID_EDIT_FRAME_DISPLAY の削除、IDS_FRAME_DISPLAY_LABEL / IDS_SIM_TIME_CAPTION / IDS_SIM_TIME_UNIT の追加
tools/cansim_runner/checks.cpp, README.md
CLAUDE.md
```

**Structure Decision**: 014・015 と同じく、座標と書式は `SimControls`、描画は `DrawSimToolbar`、押下は `OnLButtonDown` に足す。フレーム表示の値は `m_settings.FrameDisplay` の 1 か所に持ち、チェックボックスはそれを描くだけにする。

## Complexity Tracking

該当なし。
