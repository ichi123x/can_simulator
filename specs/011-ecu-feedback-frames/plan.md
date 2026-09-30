# Implementation Plan: ECU ダブルクリック時のグレー表示と、表示エリアの枠

**Branch**: `ver.0.2`（spec ディレクトリ: `011-ecu-feedback-frames`） | **Date**: 2026-09-30 | **Spec**: [spec.md](./spec.md)

## Summary

機能 010 で受け付けた ECU のダブルクリック（開く・前面に出す・切り替え・待ちの間の置き換え）のたびに、その ECU のブロックを 500 ms の
ワンショットタイマーの間だけグレーの配色で描く。状態はメインダイアログのメンバ 1 つ（グレー表示中の ECU 番号）で表し、機能 010 の動作
（すぐ開く・500 ms 後に開く・前面に出す）には手を入れない。

構成図の領域の内側（96 dpi 基準で 4 px 内側）と、ログ表示エリア（スクロールバー込み）の外側（3 px 外側）に、1 px のグレーの枠を描く。
構成図の枠の位置は `CanSim/BusDiagramLayout` の配置計算に加え、中身（バス線・「CAN BUS」の文字・ECU の箱）と重ならないことを checks.exe で
検証する。メインウィンドウの大きさは変えない（両方の枠が今のクライアント領域に収まる）。

## Technical Context

**Language/Version**: C++14相当（機能 001〜010 と同じ）

**Primary Dependencies**: MFC / GDI（既存の `DrawBusDiagram`・`OnPaint`・`SetTimer`）。新規外部依存なし。

**Storage**: なし

**Testing**: 構成図の枠の位置は `tools/cansim_runner/checks.cpp`（ECU 数 0〜16 × DPI 96/120/144 で中身と重ならない）。グレー表示の色・時間、
受け付けないダブルクリックでグレーにならないこと、ログの枠は GUI の自動操作（実マウス入力、画面の画素の色の読み取り、時刻の計測、
スクリーンショット）で確認。

**Target Platform**: Windows 10/11（Win32/x64、Debug/Release）

**Project Type**: desktop-app

**Performance Goals**: グレー表示はダブルクリックの処理の中で描き直す（SC-001: 0.1 秒以内）。500 ms のタイマーで戻す（0.4〜0.7 秒）。

**Constraints**: CSV・シミュレーション結果に影響しない。`CanSim/` は ASCII のみ。機能 010 のタイマー（切替 ID 2）・再生タイマー（ID 1）の
動作を変えない。`.rc` の変更なし（ウィンドウの大きさは変えない）。当たり判定（`FindEcuAt`）は変えない。

**Scale/Scope**: 変更: `CanSim/BusDiagramLayout.h/.cpp`（構成図の枠の矩形）、`MFCApplication1Dlg.h/.cpp`（グレー表示の状態とタイマー、ECU の
配色、2 つの枠の描画）、`checks.cpp`、CLAUDE.md。

## Constitution Check

| 原則 | 判定 | 根拠 |
|---|---|---|
| I. Spec Kit ワークフロー準拠 | PASS | spec.md（011）→ 本 plan.md。 |
| II. MFC/C++ ネイティブ規約 | PASS | 新規ファイルなし。既存ファイルの変更のみで 4 構成のビルドを維持。 |
| III. テスト可能性 | PASS | 構成図の枠の位置を配置計算（純粋関数）に置き、checks.exe で中身と重ならないことを検証。タイマーと描画の色はダイアログ固有なので GUI 自動操作で確認。 |
| IV. YAGNI | PASS | グレー表示の時間・枠の色は設定にしない（spec Assumptions）。アニメーション（フェード）は入れない。 |
| V. ドキュメント同期 | PASS（条件付き） | CLAUDE.md（グレー表示、枠）を更新するタスクを含める。 |
| VI. ドメイン仕様準拠 | PASS | 表示だけの変更で、シミュレーションの挙動・ログは変えない。 |

違反なし。Complexity Tracking は該当なし。

## Project Structure

### Documentation (this feature)

```text
specs/011-ecu-feedback-frames/
├── plan.md / research.md / data-model.md / quickstart.md
├── contracts/ui-ecu-feedback-frames.md
└── tasks.md（/speckit-tasks で作成）
```

### Source Code (repository root)

```text
MFCApplication1/
├── MFCApplication1Dlg.h / .cpp   # 変更: m_grayEcu、グレー表示タイマー（ID 3）、OnLButtonDblClk で開始、OnTimer で終了、
│                                 #       SetEcuCount・OnSimStart・OnDestroy で取り消し、DrawBusDiagram でグレーの配色と構成図の枠、
│                                 #       OnPaint でログ表示エリアの枠
└── CanSim/
    └── BusDiagramLayout.h / .cpp # 変更: kDiagramFrameInset、BusDiagramLayout::Frame
tools/cansim_runner/checks.cpp    # 変更: CheckFeature011
CLAUDE.md                         # 変更
```

**Structure Decision**: グレー表示は「今グレーにしている ECU 番号（0 = なし）」と 1 本のタイマーだけで表す。受け付けたダブルクリックのたびに
番号を置き換えてタイマーを張り直すので、FR-004（常に 1 つ・最後のもの）と「同じ ECU の連続は最後から 0.5 秒」が同じ処理で満たされる。

## Complexity Tracking

該当なし。
