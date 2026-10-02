# Implementation Plan: 解析・ログ表示メニューの追加とシミュレーションログの表示

**Branch**: `ver.0.5`（spec ディレクトリ: `025-analysis-log-menus`） | **Date**: 2026-10-02 | **Spec**: [spec.md](./spec.md)

## Summary

メニューバーに「解析(&A)」（グレーの「準備中」1 項目）と「ログ表示(&L)」（「シミュレーションログ(&L)...」1 項目）を足す。
「シミュレーションログ」は、現在のプロジェクトファイルの隣のフレームログ `<プロジェクトファイル>.log.csv` を、別ウィンドウ（モードレス）の読み取り専用のテキスト表示で開く。
ログが無ければメッセージを出して開かない（Q1 = A: フレームログだけ）。

大きなログ（数百万行・数百 MB）を全行読めるようにするため（Q2 = A）、ログ全体を読み込まない:

- **行の索引**（CanSim、新規 `LogFileIndex`）: 開くときにファイルを 1 回だけ先頭から読み、256 行ごとの行頭のバイト位置・行数・最も長い行の文字数を記録する。
  表示に要る行は、近い索引の位置から読む。メモリは行数 / 256 × 8 バイト（700 万行で約 220 KB）。
- **表示**（ダイアログ側、新規 `CLogViewDlg` と子ウィンドウ `CLogTextView`）: 見えている行だけを索引から読んで等幅フォントで描く自前のスクロール表示。
  縦・横のスクロールバー、ホイール、キー操作、文字単位の選択（マウスのドラッグ・Shift + 矢印）、Ctrl+C・Ctrl+A。編集の操作は無い（読み取り専用）。

## 動作の要点

| 場面 | 動作 |
|---|---|
| 「ログ表示 → シミュレーションログ」 | プロジェクトファイルが無い → `IDS_LOG_NONE_NOPROJECT`。`<path>.log.csv` が無い → `IDS_LOG_NONE`（プロジェクトファイル名）。開けない → `IDS_LOG_READ_ERROR`（ログのフルパス）。あれば索引を作ってウィンドウを開く |
| もう一度選んだ | 開いているウィンドウを前面に出すだけ（中身は同じファイル。下の「閉じる」で食い違いは起きない） |
| 実行中・中断中 | `UpdateTopMenus` が位置 0・1 に加えて 3（解析）・4（ログ表示）もグレーにする。開いているログ表示は「開始」で閉じているので、実行中は無い |
| 閉じる（FR-009） | 次の操作の前に `CloseLogViewer()`: 「開始」（新しい実行・保存先の確認の前）、読込み（ファイル選択の前）、セッティング、`SetProjectPath` でパスが変わるとき（別名で保存・読込み） |
| メインウィンドウを閉じる | `OnCancel` でログ表示も閉じる |

ウィンドウ: タイトル「シミュレーションログ - <ログのフルパス>」、大きさを変えられる、初期サイズは 900 × 600 px（96 dpi）でメインウィンドウの中央、オーナーはメインウィンドウ（メインの前面に出る・最小化で一緒に隠れる）。
閉じるのは ✖・Esc（`DestroyWindow`）。閉じたらオーナーに `WM_APP_LOG_VIEWER_CLOSED`（`WM_APP + 5`）を送り、`delete this`（機能 010 の編集ウィンドウと同じ作り）。

## Technical Context

- **Language/Version**: C++14 / MFC（Visual Studio 2022、v143）。新規の外部依存なし
- **Primary Dependencies**: MFC（モードレスダイアログ、自前描画の子ウィンドウ、クリップボード）、CanSim（`std::ifstream` による索引と行の読み出し）
- **Storage**: 読むだけ（`<プロジェクトファイル>.log.csv`、機能 001 の形式。ASCII、改行は CRLF／LF のどちらでも）
- **Testing**: `tools/cansim_runner/checks.cpp`（索引の行数・任意の行の読み出し・最終行の改行の有無・CRLF/LF・空ファイル・最長行・大きなファイルの所要時間）。表示は [quickstart.md](./quickstart.md)
- **Target Platform**: Windows、Win32 / x64 × Debug / Release
- **Project Type**: デスクトップアプリ（単一ダイアログ + モードレスの子ウィンドウ）
- **Performance Goals**: sample_16ecu を 600 秒実行したログ（約 700 万行・約 700 MB）を 2 秒以内に開く（索引の作成はディスク読みの速さで決まる。OS のキャッシュにある実行直後を想定）。スクロールの 1 回の描き直しは見えている行（数十行）の読み出しだけ
- **Constraints**: `CanSim/` は ASCII のみ。メニュー・メッセージ・ウィンドウのタイトルの日本語は `.rc`（UTF-16LE）。32bit 版でもファイル位置は 64bit（`std::streamoff` / `long long`）で扱う。スクロール位置は `SCROLLINFO` の 32bit で足りる（行数は 2^31 未満）が、ドラッグ中は `SIF_TRACKPOS` を使う（16bit の `nPos` は使わない）
- **Scale/Scope**: メニュー 2 つ、表示のウィンドウ 1 つ、CanSim のクラス 1 つ

## Constitution Check

| 原則 | 判定 | 理由 |
|---|---|---|
| I. Spec Kit | PASS | spec（Q1・Q2 回答済み）→ plan → tasks の順 |
| II. ビルド | PASS | 新規ソース 4 つを vcxproj・filters に足す。4 構成のビルドを tasks で確認する。ファイル位置は 64bit で扱い、x86 でも大きなログを読める |
| III. テスト可能性 | PASS | 行の索引と読み出し（ファイル処理の中心）は CanSim の UI 非依存のクラスにして checks で検証する。ダイアログは描画・スクロール・選択・クリップボードだけ |
| IV. シンプルさ | PASS | ドキュメント／ビューは使わない。CSV を表の形に整える・検索などは足さない。リッチエディットや仮想リストではなく、行の索引 + 自前描画の 1 クラスで全行の表示と文字単位の選択を満たす（research 決定2） |
| V. ドキュメント | PASS | CLAUDE.md（アーキテクチャ・メニュー・新しいクラス）と checks の README を更新する |
| VI. ドメイン仕様 | PASS | ログを読むだけで、シミュレーションの計算・CSV の内容に影響しない |

設計後の再確認: 変更なし（PASS）。

## Project Structure

```text
specs/025-analysis-log-menus/
  spec.md / plan.md / research.md / data-model.md / quickstart.md
  contracts/ui-log-viewer.md
  checklists/requirements.md

MFCApplication1/CanSim/LogFileIndex.h/.cpp     新規: 行の索引（Build / LineCount / MaxLineLength / ReadLines）
MFCApplication1/LogViewDlg.h/.cpp              新規: CLogViewDlg（モードレス）と CLogTextView（自前描画のテキスト表示）
MFCApplication1/MFCApplication1Dlg.h/.cpp      メニューのハンドラ、CloseLogViewer、UpdateTopMenus（位置 3・4）、閉じる箇所
MFCApplication1/FrameEditDlg.h                 WM_APP_LOG_VIEWER_CLOSED（WM_APP + 5。WM_APP の番号はここで管理）
MFCApplication1/Resource.h, MFCApplication1.rc メニュー、IDD_LOG_VIEW、文字列（メッセージ・タイトル・準備中）
MFCApplication1/MFCApplication1.vcxproj(.filters)  新規ソース 4 つ
tools/cansim_runner/checks.cpp, README.md      025 の検証項目
CLAUDE.md
```

**Structure Decision**: 既存の構成のまま。ファイルの読み出しは CanSim に置き（checks の `cl` の一括ビルド `CanSim\*.cpp` にそのまま入る）、表示はダイアログ側の新しいファイルに分ける（`MFCApplication1Dlg.cpp` をこれ以上大きくしない）。

## Complexity Tracking

| 追加したもの | 必要な理由 | より簡単な案を採らない理由 |
|---|---|---|
| 自前描画のテキスト表示（`CLogTextView`） | Q2 = A（数百万行を全行読める）と spec の「文字を選んでコピー」を両立する | 読み取り専用のエディット／リッチエディットは全行を読み込む必要があり、数百 MB では開くのに時間とメモリがかかる。仮想リスト（`LVS_OWNERDATA`）は行単位の選択しかできない（research 決定2） |
