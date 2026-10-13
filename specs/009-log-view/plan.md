# Implementation Plan: ログ表示エリア

**Branch**: `ver.0.2`（spec ディレクトリ: `009-log-view`） | **Date**: 2026-09-30 | **Spec**: [spec.md](./spec.md)

## Summary

CAN バス構成図の下に薄いグレーの「ログ表示エリア」を追加する。実行結果（`LogRow` 群と、ノードの起動状態の遷移時刻）から、時刻順の表示用の行一覧を `CanSim/LogLines`（UI 非依存）で作り、ダイアログは機能 005 の再生時計（実経過時間 × 2^スピード）に合わせて、その一覧の先頭から順に「表示済み」に加えていく。描画はメイン `OnPaint` でメモリ DC に見えている範囲（十数行）だけを文字にする自前描画とし、右側に標準の縦スクロールバー（`CScrollBar`）を置く。最新行への自動追従と、過去を見ている間の固定は、スクロールバーの位置で判定する。数万行でも軽く動く。

## Technical Context

**Language/Version**: C++14相当（機能 001〜008 と同じ）

**Primary Dependencies**: MFC（`CScrollBar`、GDI）。C++ 標準ライブラリのみ。新規外部依存なし。

**Storage**: なし（表示のみ。行は実行のたびにメモリ上に作る）

**Testing**: `CanSim/LogLines`（行の生成・並び順・時刻）は `tools/cansim_runner/checks.cpp` で検証。描画・スクロール・追従は GUI の自動操作（スクリーンショット）で確認。

**Target Platform**: Windows 10/11（Win32/x64、Debug/Release）

**Project Type**: desktop-app（`CanSim/` にコア、UI にダイアログ変更）

**Performance Goals**: 約 12 万行でもスライドバー操作 1 回 0.2 秒未満（描画は見えている行数分のみ）。行の生成は実行 1 回につき 1 秒未満。

**Constraints**: CSV 出力に影響しない（FR-011）。`CanSim/` は ASCII のみ、表示文字列は `.rc`。フレーム表示の ON/OFF とは独立。

**Scale/Scope**: 新規 `LogLines`（CanSim）。変更: `MFCApplication1Dlg`（領域、スクロールバー、供給、描画）、`.rc`（ダイアログ寸法、スクロールバー、文字列）、`checks.cpp`、CLAUDE.md。

## Constitution Check

| 原則 | 判定 | 根拠 |
|---|---|---|
| I. Spec Kit ワークフロー準拠 | PASS | spec.md（009）→ 本 plan.md。 |
| II. MFC/C++ ネイティブ規約 | PASS | 新規ソース 1 組を 4 構成に登録するのみ。 |
| III. テスト可能性 | PASS | 行の生成・並び順は `CanSim/LogLines` に分離し checks.exe で検証。ダイアログは供給・描画・スクロールのみ。 |
| IV. YAGNI | PASS | 専用コントロールクラスは作らず、既存のダイアログに描画と `CScrollBar` を足す。検索・絞り込み・コピーは入れない。 |
| V. ドキュメント同期 | PASS（条件付き） | CLAUDE.md（`LogLines`、ログ表示エリアの動作、ダイアログの寸法）を更新するタスクを含める。 |
| VI. ドメイン仕様準拠 | PASS | ログ項目は Doc FR-051 と同じ単位（1 フレーム × 受信ノード）。状態遷移は Doc FR-030 系（スタートアップ）。エラー状態は枠のみ。 |

違反なし。Complexity Tracking は該当なし。

## Project Structure

### Documentation (this feature)

```text
specs/009-log-view/
├── plan.md / research.md / data-model.md / quickstart.md
├── contracts/ui-log-view.md
└── tasks.md
```

### Source Code (repository root)

```text
MFCApplication1/
├── MFCApplication1.rc / Resource.h     # 変更: ダイアログを縦に拡大、IDC_SCROLL_LOG、状態名・ステータス名の文字列（UTF-16LE 注意）
├── MFCApplication1Dlg.h / .cpp         # 変更: ログ表示エリアの矩形、スクロールバー、行の供給（OnTimer / 完了時）、描画、ホイール、開始時のクリア
└── CanSim/
    └── LogLines.h / .cpp               # 新規: 実行結果 → 時刻順の表示用行の一覧（UI 非依存、ASCII のみ）
tools/cansim_runner/checks.cpp          # 変更: LogLines の検証
CLAUDE.md                               # 変更
```

**Structure Decision**: 表示用の行は「時刻 + 種類 + 元データへの添字」だけを持つ軽い構造体とし（文字列は描画時に見えている行だけ作る）、12 万行でもメモリと生成時間を抑える。

## Complexity Tracking

該当なし。
