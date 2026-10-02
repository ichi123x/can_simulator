# Implementation Plan: プロジェクトファイル名の表示と関連付けからの起動

**Branch**: `ver.0.5`（spec ディレクトリ: `024-project-path-title`） | **Date**: 2026-10-02 | **Spec**: [spec.md](./spec.md)

## Summary

ウィンドウのタイトルバーを「CAN Bus Simulator␣␣␣␣プロジェクト名：<フルパス>」にする。
現在のプロジェクトファイル `m_projectPath` を変える箇所を 1 つの関数 `SetProjectPath` にまとめ、そこでタイトルを書き直す。
起動時のコマンドライン引数に .canprj のパスがあれば、ダイアログの表示後にメニューの読込みと同じ処理（新しい共通関数 `LoadProjectFrom`）で読み込む。
関連付けの登録はしない（spec FR-008、Q1 = A）。

- **タイトルの組み立て**・**引数からのパスの取り出し**・**拡張子の判定**は、CanSim（`ProjectFile.h/.cpp`）の純粋関数にして checks で検証する。
- **絶対パスへの変換**（相対パス・8.3 形式）は Win32 の `GetFullPathNameW` / `GetLongPathNameW` を使うため、ダイアログ側で行う。

## 処理の流れ

| 場面 | 変更前 | 変更後 |
|---|---|---|
| 起動（引数なし） | タイトル「CAN Bus Simulator」 | `OnInitDialog` で `SetProjectPath(L"")` → 「CAN Bus Simulator␣␣␣␣プロジェクト名：」 |
| 起動（引数あり） | 引数は無視 | `InitInstance` が `CanSim::FindStartupProjectArg(__wargv)` の結果をダイアログに渡す → `OnInitDialog` の最後に `WM_APP_LOAD_STARTUP_PROJECT` を Post → ハンドラで絶対パスに直して `LoadProjectFrom` |
| 読込み（メニュー・ツールバー） | `OnFileLoadProject` が検査・読込み・反映 | ファイル選択の後は `LoadProjectFrom(path)` を呼ぶだけ（中身は今の処理をそのまま移す） |
| 保存（メニュー・ツールバー・「開始」） | `SaveProjectTo` が成功時に `m_projectPath = path` | 成功時に `SetProjectPath(path)` |

- `LoadProjectFrom` は今の `OnFileLoadProject` の後半（拡張子の検査 → `LoadConfig` → エラー表示 or 反映）で、成功時の `m_projectPath = path` を `SetProjectPath(path)` にする。
  編集ウィンドウを閉じる処理（`CloseFrameEditor`）は `OnFileLoadProject` に残す（起動直後は編集ウィンドウが無い）。
- 起動時の読込みを Post にするのは、エラーのメッセージボックスをメインウィンドウが表示された後に出し、オーナーをメインウィンドウにするため。

## Technical Context

- **Language/Version**: C++14 / MFC（Visual Studio 2022、v143）。新規依存なし（`__wargv` は CRT、パスの変換は kernel32）
- **Primary Dependencies**: MFC、CanSim（プレーン C++）
- **Storage**: `.canprj`（形式は変えない）
- **Testing**: `tools/cansim_runner/checks.cpp`（タイトルの組み立て・引数の取り出し・拡張子の判定）。GUI と起動は [quickstart.md](./quickstart.md)
- **Target Platform**: Windows、Win32 / x64 × Debug / Release
- **Project Type**: デスクトップアプリ（単一ダイアログ）
- **Constraints**: `CanSim/` は ASCII のみ。ラベル「プロジェクト名：」は `.rc` の文字列テーブル（UTF-16LE）に置き、CanSim には `std::wstring` で渡す
- **Scale/Scope**: タイトルの表示、起動時の引数の読込み、読込み・保存の処理の小さな整理

## Constitution Check

| 原則 | 判定 | 理由 |
|---|---|---|
| I. Spec Kit | PASS | spec（Q1 回答済み）→ plan → tasks の順 |
| II. ビルド | PASS | 新規ファイル・設定の変更なし。4 構成のビルドを tasks で確認する |
| III. テスト可能性 | PASS | タイトルの組み立て・引数の選び方・拡張子の判定を CanSim の純粋関数にして checks で検証する。ダイアログは Win32 のパス変換と表示だけ |
| IV. シンプルさ | PASS | ドキュメント／ビュー（MRU・`ProcessShellCommand`）は使わず、`SetWindowText` と引数 1 つの読込みだけ。関連付けの登録もしない |
| V. ドキュメント | PASS | CLAUDE.md（プロジェクトと実行の節、ProjectFile の説明、checks の対象）と README を更新する |
| VI. ドメイン仕様 | PASS | UI と起動だけの変更で、シミュレーションの計算・CSV に影響しない |

設計後の再確認: 変更なし（PASS）。

## Project Structure

```text
specs/024-project-path-title/
  spec.md / plan.md / research.md / data-model.md / quickstart.md
  contracts/ui-window-title-and-startup.md
  checklists/requirements.md

MFCApplication1/CanSim/ProjectFile.h/.cpp   ComposeWindowTitle、FindStartupProjectArg、HasProjectExtension（ダイアログから移す）
MFCApplication1/MFCApplication1.cpp         InitInstance: 引数をダイアログに渡す
MFCApplication1/MFCApplication1Dlg.h/.cpp   SetProjectPath、LoadProjectFrom、WM_APP_LOAD_STARTUP_PROJECT、タイトルの更新
MFCApplication1/Resource.h, MFCApplication1.rc   IDS_PROJECT_PATH_LABEL
tools/cansim_runner/checks.cpp, README.md   024 の検証項目
CLAUDE.md
```

**Structure Decision**: 既存の構成のまま。新しいソースファイルは足さず（vcxproj を変えない）、プロジェクトファイルに関わる純粋関数として `ProjectFile` に置く。

## Complexity Tracking

なし。
