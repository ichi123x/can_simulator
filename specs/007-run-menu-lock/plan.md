# Implementation Plan: 実行中のメニュー制限と、インターミッション後のバスアイドル

**Branch**: `ver.0.2`（spec ディレクトリ: `007-run-menu-lock`） | **Date**: 2026-09-30 | **Spec**: [spec.md](./spec.md)

## Summary

(1) 実行状態が変わるたびに、メニューバーのトップレベル項目「ファイル」「編集」を位置指定で有効／無効（グレー）にし、メニューバーを再描画する。実行中は両メニューが開かず、フレーム表示の切替・保存もできなくなる。(2) インターミッション（EOF 後 3 ビット）を経てからバスアイドルになる動作は `SimulationEngine` に実装済みのため、コードは変えず、全通信速度で「次の送信開始 − 前の送信終了 = 3 ビット時間」と「伝送時間にインターミッションを含まない」を checks.exe で検証する。

## Technical Context

**Language/Version**: C++14相当（機能 001〜006 と同じ）

**Primary Dependencies**: MFC（`CMenu::EnableMenuItem` の `MF_BYPOSITION`、`CWnd::DrawMenuBar`）。新規依存なし。

**Storage**: なし

**Testing**: (2) は `tools/cansim_runner/checks.cpp` に検証を追加。(1) は GUI の自動操作（実行中にトップレベルメニューを開こうとして開かないこと、停止後に開けること）で確認。

**Target Platform**: Windows 10/11（Win32/x64、Debug/Release）

**Project Type**: desktop-app（既存 `MFCApplication1` を変更）

**Performance Goals**: 該当なし（メニュー状態の切替のみ）

**Constraints**: 「シミュレーション」メニューは常に有効。✖ は常に有効。インターミッションは画面に表示しない（spec FR-007）。

**Scale/Scope**: 変更は `MFCApplication1Dlg.h/.cpp`（メニュー状態の更新関数と、実行開始・終了時の呼び出し、`OnEditFrameDisplay`のガード）と `checks.cpp`、文書のみ。

## Constitution Check

| 原則 | 判定 | 根拠 |
|---|---|---|
| I. Spec Kit ワークフロー準拠 | PASS | spec.md（007）→ 本 plan.md。 |
| II. MFC/C++ ネイティブ規約 | PASS | 新規ファイルなし。4 構成のビルドを確認する。 |
| III. テスト可能性 | PASS | メニュー制御は UI 固有の処理でダイアログに置く。バスの時間配分の検証は CanSim を checks.exe で行う。 |
| IV. YAGNI | PASS | エンジンは変更しない（既に仕様どおり）。メニュー制御は既存の実行状態を使うだけ。 |
| V. ドキュメント同期 | PASS（条件付き） | CLAUDE.md の「実行中はセッティング・読込み・開始を受け付けない」と「フレーム表示は実行中も切替可」の記述を更新するタスクを含める。005 の契約（`ui-speed-replay.md` の「実行中のフレーム表示切替: 可」）に後継の注記を入れる。 |
| VI. ドメイン仕様準拠 | PASS | Doc FR-022（インターミッション 3 ビット）・FR-021（伝送時間）どおりであることを検証する。 |

違反なし。Complexity Tracking は該当なし。

## Project Structure

### Documentation (this feature)

```text
specs/007-run-menu-lock/
├── plan.md / research.md / data-model.md / quickstart.md
├── contracts/ui-menu-state.md
└── tasks.md
```

### Source Code (repository root)

```text
MFCApplication1/
└── MFCApplication1Dlg.h / .cpp   # 変更: UpdateTopMenus()（ファイル・編集の有効/無効 + DrawMenuBar）を、実行開始と EndRun で呼ぶ。
                                  #       OnEditFrameDisplay・OnFileSaveProject に実行中ガードを追加
tools/cansim_runner/checks.cpp    # 変更: 007 の検証（全通信速度でインターミッション 3 ビット、伝送時間 = ビット数 × ビット時間）
CLAUDE.md                         # 変更
specs/005-simulation-speed/contracts/ui-speed-replay.md  # 変更: 後継の注記
```

**Structure Decision**: トップレベルのメニューは位置（0 = ファイル、1 = 編集）で有効／無効にする。項目単位の有効／無効（`OnInitMenuPopup`）は残し、二重に守る。

## Complexity Tracking

該当なし。
