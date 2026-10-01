# Implementation Plan: 受信中の ECU と接続線を青で強調する

**Branch**: `ver.0.3`（spec ディレクトリ: `016-receiver-blue-highlight`） | **Date**: 2026-10-01 | **Spec**: [spec.md](./spec.md)

## Summary

`CMFCApplication1Dlg::DrawBusDiagram` の受信 ECU（`m_active.Receivers` に含まれる ECU）の描き方を、次の 2 点だけ変える。

1. 箱: 緑（`kReceiverFill` = `RGB(170,225,170)`、`kReceiverFrame` = `RGB(40,140,40)`）を、青（塗り `RGB(160,195,245)`、枠はバスと同じ
   `kActiveBusColor` = `RGB(30,90,220)`、太さ 2）に変える。
2. 接続線: 送信 ECU と同じ青の太線のペン（`activeLinePen`）で描く。

強調の対象（どの ECU が受信 ECU か）・送信 ECU（赤）・バス（青）・フレーム表示 OFF の扱い・中断中の保持は変えない。

## Technical Context

**Language/Version**: C++14相当（機能 001〜015 と同じ）

**Primary Dependencies**: MFC / GDI（既存の描画）。新規外部依存なし。

**Storage**: なし

**Testing**:
- 描画の色だけの変更なので、GUI の自動操作で画面の色を読んで確認する（quickstart.md）。
- 強調の対象を決める `ActiveFrameCursor`（CanSim）は変えないので、checks.exe は既存項目の PASS と CSV の一致を確認する。

**Target Platform**: Windows 10/11（Win32/x64、Debug/Release）

**Project Type**: desktop-app

**Performance Goals**: 変更なし（描く色とペンの選び方だけ）。

**Constraints**:
- 受信 ECU の塗りは、ECU 名の黒い文字が読める明るさにする。
- 受信 ECU の塗りは、受信していない ECU の色（上段 `RGB(225,213,231)`・下段 `RGB(255,230,204)`）・送信 ECU の赤 `RGB(240,110,110)` と区別できる色にする。
- `CanSim/`・`.rc` の変更なし。

**Scale/Scope**: 変更: `MFCApplication1Dlg.cpp`（色の定数 2 つと受信 ECU の描き方）、`CLAUDE.md`、`specs/004-menu-project-settings/contracts/ui-menu-dialogs.md`（受信 ECU の色の記述）。

## Constitution Check

| 原則 | 判定 | 根拠 |
|---|---|---|
| I. Spec Kit ワークフロー準拠 | PASS | spec.md（016）→ 本 plan.md。 |
| II. MFC/C++ ネイティブ規約 | PASS | 既存ファイルの変更のみ。4 構成のビルドを維持。 |
| III. テスト可能性 | PASS | ドメインロジック（強調の対象の決定）は CanSim の `ActiveFrameCursor` のままで変えない。描画の色だけをダイアログで変える。 |
| IV. YAGNI | PASS | 色の定数と、ペンの選択の条件を変えるだけ。凡例などは足さない。 |
| V. ドキュメント同期 | PASS（条件付き） | CLAUDE.md の「受信 ECU（緑）」の記述を「青（接続線も青）」に直すタスクを含める。 |
| VI. ドメイン仕様準拠 | PASS | シミュレーションの挙動・結果は変えない。 |

違反なし。Complexity Tracking は該当なし。

**Post-design re-check**: 設計後も判定は変わらない。

## Project Structure

### Documentation (this feature)

```text
specs/016-receiver-blue-highlight/
├── spec.md / plan.md / research.md / data-model.md / quickstart.md
├── contracts/ui-receiver-highlight.md
├── checklists/requirements.md
└── tasks.md（/speckit-tasks で作成）
```

### Source Code (repository root)

```text
MFCApplication1/
└── MFCApplication1Dlg.cpp   # 変更: kReceiverFill / kReceiverFrame の値、受信 ECU の枠の太さ、接続線のペン
CLAUDE.md                    # 変更
specs/004-menu-project-settings/contracts/ui-menu-dialogs.md  # 変更: 受信 ECU の色の記述
```

**Structure Decision**: 強調の対象（送信・受信の集合）は今までどおり CanSim が決め、ダイアログは描き方だけを持つ。

## Complexity Tracking

該当なし。
