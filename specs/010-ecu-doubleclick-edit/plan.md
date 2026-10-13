# Implementation Plan: ECU ブロックのダブルクリックでデータフレーム編集を開く

**Branch**: `ver.0.2`（spec ディレクトリ: `010-ecu-doubleclick-edit`） | **Date**: 2026-09-30（US3 追加により改訂） | **Spec**: [spec.md](./spec.md)

## Summary

メインダイアログの CAN バス構成図で ECU のブロックがダブルクリックされたら、機能 006 のデータフレーム編集ウィンドウ（`CFrameEditDlg`）をその ECU を選んだ状態で開く。当たり判定は構成図の配置計算（`CanSim/BusDiagramLayout`）に UI 非依存の `FindEcuAt` として追加し、描画と同じDPI 換算関数 `ScaleToDpi` を使って「見えている箱」と「効く範囲」を一致させる。

US3（開いたまま別の ECU をダブルクリックして切り替える）のため、編集ウィンドウを **モードレス**（開いたままメインウィンドウを操作できる）に変える。メインダイアログは編集ウィンドウを高々 1 つ持ち、違う ECU のダブルクリックでは「閉じる → 500 ms のワンショットタイマー → 最後にダブルクリックされた ECU で開く」を行う。編集ウィンドウが開いている間（待ちを含む）は「開始」を無効にし、「読込み」「セッティング」は先に編集ウィンドウを閉じてから行う。編集ウィンドウが入力画面・削除確認を出している間は、メインウィンドウを無効化してダブルクリックもメニューも受け付けない（FR-015、入れ子のモーダル中に編集ウィンドウを壊さないため）。

## Technical Context

**Language/Version**: C++14相当（機能 001〜009 と同じ）

**Primary Dependencies**: MFC（`CDialogEx` のモードレス生成 `Create` / `DestroyWindow` / `PostNcDestroy`、`ON_WM_LBUTTONDBLCLK`、`SetTimer`）。新規外部依存なし。

**Storage**: なし（編集結果は機能 006 と同じくその場で `m_definitions` に反映、保存はプロジェクト保存）

**Testing**: 当たり判定（`FindEcuAt`・`ScaleToDpi`）は `tools/cansim_runner/checks.cpp` で ECU 数 0〜16 × DPI 96/120/144 を検証。ダブルクリック・切り替え・待ち時間・メニューの有効／無効は GUI の自動操作（実マウス入力によるダブルクリック、ウィンドウの列挙、時刻の計測、スクリーンショット）で確認。

**Target Platform**: Windows 10/11（Win32/x64、Debug/Release）

**Project Type**: desktop-app（`CanSim/` にコア、UI にダイアログ変更）

**Performance Goals**: 判定は ECU 最大 16 個の矩形比較。切り替えの待ちは 500 ms（SC-006: 0.4〜0.7 秒）で、待ちの間も UI は固まらない（タイマーで待つ）。

**Constraints**: CSV・シミュレーション結果に影響しない。`CanSim/` は ASCII のみ。構成図の見た目（機能 003）・ログ表示エリア（機能 009）は変えない。編集ウィンドウが開いていないときのメニュー「データフレーム編集」の動作（最初は ECU 01）は変えない（FR-008）。`.rc` の変更なし（既存の`IDD_FRAME_EDIT` をモードレスでも使う）。

**Scale/Scope**: 変更: `CanSim/BusDiagramLayout.h/.cpp`、`MFCApplication1Dlg.h/.cpp`（ダブルクリック、編集ウィンドウの保持・切り替え・待ちタイマー、メニュー制御、終了時の後始末、`ScalePx` の統一）、`FrameEditDlg.h/.cpp`（モードレス化、最初の ECU、表示中の ECU の取得、入れ子のモーダル中のメインウィンドウ無効化、閉じたことの通知）、`checks.cpp`、CLAUDE.md。

## Constitution Check

| 原則 | 判定 | 根拠 |
|---|---|---|
| I. Spec Kit ワークフロー準拠 | PASS | spec.md（010、US3 追加）→ 本 plan.md（改訂）。 |
| II. MFC/C++ ネイティブ規約 | PASS | 新規ファイルなし。既存ファイルの変更のみで、4 構成のビルドを維持。 |
| III. テスト可能性 | PASS | 当たり判定は `CanSim/BusDiagramLayout` の純粋関数とし checks.exe で検証。切り替えの状態管理はウィンドウの生成・破棄とタイマーそのものなのでダイアログに置き、GUI 自動操作で確認する。 |
| IV. YAGNI | PASS | 新しい画面は作らない。モードレス化は US3 の要件（開いたまま構成図を操作）に必要な最小の変更。カーソル変更・ホバー強調・右クリックメニューは入れない。 |
| V. ドキュメント同期 | PASS（条件付き） | CLAUDE.md（構成図のダブルクリック、`FindEcuAt`、編集ウィンドウのモードレス化と切り替え、開始の無効化）を更新するタスクを含める。 |
| VI. ドメイン仕様準拠 | PASS | ドメイン仕様書に記述のない UI 入口の追加で、シミュレーションの挙動は変えない。実行中の編集禁止（Doc FR-080 / 機能 007）を、編集中の開始禁止（FR-013）で逆方向からも守る。 |

違反なし。Complexity Tracking は該当なし。

## Project Structure

### Documentation (this feature)

```text
specs/010-ecu-doubleclick-edit/
├── plan.md / research.md / data-model.md / quickstart.md
├── contracts/ui-ecu-doubleclick.md
└── tasks.md（/speckit-tasks で再作成）
```

### Source Code (repository root)

```text
MFCApplication1/
├── MFCApplication1Dlg.h / .cpp   # 変更: OnLButtonDblClk、m_frameEditor / m_pendingEcu / 切替タイマー、OpenFrameEditor / CloseFrameEditor、
│                                 #       開始の無効化、読込み・セッティング前に閉じる、OnDestroy の後始末、ScalePx → CanSim::ScaleToDpi
├── FrameEditDlg.h / .cpp         # 変更: モードレス化（OnOK/OnCancel → DestroyWindow、PostNcDestroy で通知して delete this）、
│                                 #       initialEcu、CurrentEcu()、入力画面・削除確認の間はメインウィンドウを無効化
└── CanSim/
    └── BusDiagramLayout.h / .cpp # 変更: ScaleToDpi、FindEcuAt
tools/cansim_runner/checks.cpp    # 変更: CheckFeature010
CLAUDE.md                         # 変更
```

**Structure Decision**: 当たり判定を描画と同じ配置計算・同じ換算から求めて、描画と判定のずれを構造的に防ぐ（FR-004、SC-005）。編集ウィンドウの生存管理（開いている／待ち／閉じている）はメインダイアログの 2 つのメンバ（編集ウィンドウへのポインタ、開く予定の ECU）だけで表し、状態を増やさない。

## Complexity Tracking

該当なし。
