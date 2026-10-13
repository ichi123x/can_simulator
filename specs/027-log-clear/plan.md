# Implementation Plan: ログ表示メニューへのログクリア（ログファイルの削除）の追加

**Branch**: `ver.0.6`（spec ディレクトリ: `027-log-clear`） | **Date**: 2026-10-03 | **Spec**: [spec.md](./spec.md)

## Summary

「ログ表示」メニューの「シミュレーションログ」の下に「ログクリア」を足す。選ぶと、現在のプロジェクトファイルの隣にある実行結果の 3 つのファイル（`.log.csv`・`.summary.csv`・`.startup.csv`）のうち、あるものを確認の後に削除する。1 つでも削除できたら、メイン画面のログ表示エリアの行も消す。

- **CanSim**（`LogFileIndex.h/.cpp`）: 3 つのファイルのパスを組み立てる処理と、ある・削除するの処理を持たせる。ファイルの扱いは UI から切り離し、checks で検証する（憲章 III）。`WriteResults` も同じパスの組み立てを使い、書く名前と消す名前が食い違わないようにする。
- **ダイアログ**: メニューのハンドラ `OnLogClear` を置き、通知・確認のメッセージ、シミュレーションログのウィンドウを閉じる処理、ログ表示エリアの消去（既存の `ClearLog`）だけを担う。

## 流れ（`OnLogClear`）

```text
InRun() → 何もしない（メニューはグレー。ほかの経路にも備える）
m_projectPath が空 → IDS_LOG_NONE_NOPROJECT（025 と同じ）→ 終わり
existing = ExistingResultFiles(m_projectPath)
existing が空 → IDS_LOG_NONE（025 と同じ。プロジェクトのファイル名）→ 終わり
確認 IDS_LOG_CLEAR_CONFIRM（existing のファイル名を 1 行ずつ、元に戻せない旨）MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2
  「いいえ」→ 何も変えない → 終わり
CloseLogViewer()                      // FR-007
result = DeleteResultFiles(existing)
result.Deleted が 1 つ以上 → ClearLog()   // FR-008（Q3）
result.Failed が空 → IDS_LOG_CLEAR_DONE（削除したファイル名）MB_ICONINFORMATION
それ以外        → IDS_LOG_CLEAR_FAILED（削除できなかったファイル名）MB_ICONWARNING
```

- 確認の既定のボタンは「いいえ」にする（Enter で誤って消さない）。
- データフレーム編集ウィンドウは閉じない（spec Edge Cases）。プロジェクトの内容・`m_projectPath`・構成図の強調・シミュレーション時間の表示は変えない。

## Technical Context

- **Language/Version**: C++14 / MFC（Visual Studio 2022、v143）。新規依存なし
- **Primary Dependencies**: CRT のファイル関数（`_wstat64`・`_wremove`）。CanSim は MFC に依存しないまま
- **Storage**: プロジェクトファイルの隣の CSV 3 つ（形式は変えない）
- **Testing**: `tools/cansim_runner/checks.cpp` に 027 の項目（一時フォルダーに作ったファイルで、パス・あるものの列挙・削除・読み取り専用の失敗・`.canprj` が残ること）。GUI は [quickstart.md](./quickstart.md)
- **Target Platform**: Windows、Win32 / x64 × Debug / Release
- **Project Type**: デスクトップアプリ（単一ダイアログ）
- **Constraints**: `CanSim/` は ASCII のみ。日本語のメニュー名・メッセージは `.rc`（UTF-16LE）
- **Scale/Scope**: メニュー項目 1 つ、メッセージ 3 つ、CanSim の関数 3 つ

## Constitution Check

| 原則 | 判定 | 理由 |
|---|---|---|
| I. Spec Kit | PASS | spec（確認 3 件回答済み）→ plan → tasks の順 |
| II. ビルド | PASS | 4 構成のビルドを tasks で確認する |
| III. テスト可能性 | PASS | 削除の対象の決め方と削除の処理は CanSim の関数にし、checks で実ファイルを使って検証する。ダイアログは確認・通知だけ |
| IV. シンプルさ | PASS | 既存の `LogPathFor`・`ClearLog`・`CloseLogViewer`・025 の通知を使い回す。ごみ箱への移動などは足さない |
| V. ドキュメント | PASS | CLAUDE.md の 025 の節・`LogFileIndex` の説明、checks の README を更新する |
| VI. ドメイン仕様 | PASS | 出力済みのファイルを消すだけで、シミュレーションの計算・CSV の形式に影響しない |

設計後の再確認: 変更なし（PASS）。

## Project Structure

```text
specs/027-log-clear/
  spec.md / plan.md / research.md / data-model.md / quickstart.md
  contracts/ui-log-clear.md
  checklists/requirements.md

MFCApplication1/CanSim/LogFileIndex.h/.cpp   ResultFilePaths / ExistingResultFiles / DeleteResultFiles
MFCApplication1/MFCApplication1Dlg.h/.cpp    OnLogClear、メニューの有効／無効、WriteResults のパス
MFCApplication1/Resource.h, MFCApplication1.rc   ID_LOG_CLEAR、IDS_LOG_CLEAR_CONFIRM / DONE / FAILED、メニュー項目
tools/cansim_runner/checks.cpp, README.md   027 の項目
CLAUDE.md
```

**Structure Decision**: 既存の構成のまま。ログのパスを扱う `LogFileIndex` に結果ファイルの関数を足す。

## Complexity Tracking

なし。
