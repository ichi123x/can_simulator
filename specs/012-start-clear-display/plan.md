# Implementation Plan: シミュレーション開始時に表示をクリアして先頭から表示する

**Branch**: `ver.0.3`（spec ディレクトリ: `012-start-clear-display`） | **Date**: 2026-10-01 | **Spec**: [spec.md](./spec.md)

## Summary

「開始」（`OnSimStart`）で、保存先の確定の後・計測関連の初期化（実行用ネットワークの生成、計算、ログ行の作成、再生の時計・強調の
準備）の前に、表示の初期化をまとめた 1 つの関数 `ResetDisplaysForRun()` を呼ぶ。この関数は次の順で処理する。

1. ログ表示を空にし、先頭に戻す（`ClearLog`）。
2. グレー表示を取り消す（`ClearGray`）。
3. 構成図の強調を空にする（`m_active`）。
4. 構成図とログ表示エリアを無効化し、`UpdateWindow()` でその場で描き直す。

今は `ClearLog`・`ClearGray` を呼んでいるが描き直しが計算の後になるため、計算の間は前回の表示が残っている。この描き直しの時期を
計算の前に移すのが本機能の中心である。計算・CSV・ログの行・時間の進み方は変えない。

## Technical Context

**Language/Version**: C++14相当（機能 001〜011 と同じ）

**Primary Dependencies**: MFC / GDI（`InvalidateRect` + `UpdateWindow` による同期の描き直し）。新規外部依存なし。

**Storage**: なし

**Testing**:
- 画面の初期化と順序は UI のみの処理なので、GUI の自動操作で確認する。
  - 計算に数秒かかる構成で「開始」し、計算中にログ表示エリアの画面の色を読んで空であることを確かめる。
  - 前回スクロールした状態からの開始で、最初の行が一番上に出ることを確かめる。
- コアの変更はないので、checks.exe は既存項目の PASS と CSV の一致だけを確認する。

**Target Platform**: Windows 10/11（Win32/x64、Debug/Release）

**Project Type**: desktop-app

**Performance Goals**: 表示の初期化から画面への反映まで 0.2 秒以内（SC-001）。描き直しは構成図とログ表示エリアの 2 か所だけ。

**Constraints**:
- CSV・シミュレーション結果・ログ行の内容に影響しない。
- 保存先のキャンセル・保存失敗では初期化しない（FR-006）。今どおり、初期化は保存先の確定後に置く。
- `CanSim/` の変更なし、`.rc` の変更なし。

**Scale/Scope**: 変更: `MFCApplication1Dlg.h/.cpp`（`ResetDisplaysForRun` の追加と `OnSimStart` での呼び出し位置）、CLAUDE.md。

## Constitution Check

| 原則 | 判定 | 根拠 |
|---|---|---|
| I. Spec Kit ワークフロー準拠 | PASS | spec.md（012）→ 本 plan.md。 |
| II. MFC/C++ ネイティブ規約 | PASS | 既存ファイルの変更のみで 4 構成のビルドを維持。 |
| III. テスト可能性 | PASS | ドメインロジックの変更はない（画面の初期化の順序だけ）。既存の checks.exe で結果が変わらないことを確認し、表示は GUI 自動操作で確認する。 |
| IV. YAGNI | PASS | 関数 1 つの追加と呼び出し位置の整理のみ。進捗表示などは入れない。 |
| V. ドキュメント同期 | PASS（条件付き） | CLAUDE.md の「開始」の説明に初期化の順序を追記するタスクを含める。 |
| VI. ドメイン仕様準拠 | PASS | シミュレーションの挙動は変えない。 |

違反なし。Complexity Tracking は該当なし。

## Project Structure

### Documentation (this feature)

```text
specs/012-start-clear-display/
├── plan.md / research.md / data-model.md / quickstart.md
├── contracts/ui-start-reset.md
└── tasks.md（/speckit-tasks で作成）
```

### Source Code (repository root)

```text
MFCApplication1/
└── MFCApplication1Dlg.h / .cpp   # 変更: ResetDisplaysForRun()、OnSimStart で計測関連の初期化より前に呼ぶ
CLAUDE.md                         # 変更
```

**Structure Decision**: 「表示の初期化」を 1 つの関数にまとめ、`OnSimStart` の中で計測関連の処理より前に 1 か所だけ呼ぶ。こうすると、
初期化の順序（FR-005）をコード上で 1 行の位置として確認できる。

## Complexity Tracking

該当なし。
