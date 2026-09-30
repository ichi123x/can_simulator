# Implementation Plan: データフレーム編集ウィンドウ

**Branch**: `ver.0.2`（spec ディレクトリ: `006-frame-editor`） | **Date**: 2026-09-30 | **Spec**: [spec.md](./spec.md)

## Summary

メニュー「編集 → データフレーム編集」で開く編集ウィンドウ（ノード選択・送信フレーム一覧・追加／編集／削除）と、
追加・編集用の入力画面（フレーム名・ID・DLC・送信周期・初回オフセット、ビット長と伝送時間の表示）を追加する。
入力検査・一覧の操作・データ（DLC 分の 0xFF）の生成は `CanSim/FrameEditor` の UI 非依存 C++ に置く（憲章 III）。
あわせて、Doc v1.11 に合わせて (1) フレームのデータを常に DLC 分の 0xFF にする（ファイルの `Data` は読まない・
書かない）、(2) フレーム名を `.canprj` に保存する、(3) 1 ノード 100 フレームの上限を読込みでも検査する、
(4) 最初の送信要求の起点を「通常動作開始時刻」に変える（`SimulationEngine`）。(1)(4) は既存サンプルの結果を
変えるため、`checks.exe` の期待値を更新する。

## Technical Context

**Language/Version**: C++14相当（機能 001〜005 と同じ）

**Primary Dependencies**: MFC（動的リンク）、Win32 コモンコントロール（`CListCtrl` レポート表示、`CComboBox`）、
C++ 標準ライブラリのみ。新規外部ライブラリなし。

**Storage**: `.canprj` の `[NodeN.FrameM]` に `Name=` を追加。`Data=` は読み飛ばし、保存しない。

**Testing**: 既存方針どおり。`FrameEditor`（検査・追加・編集・削除・データ生成）、読込み時の 0xFF 化と 100 上限、
フレーム名の保存・再読込み、送信要求の起点変更を `tools/cansim_runner/checks.cpp` で検証。GUI は quickstart の手順
（画面操作の自動化）で確認。

**Target Platform**: Windows 10/11（Win32/x64、Debug/Release）

**Project Type**: desktop-app（既存 `MFCApplication1` を拡張）

**Performance Goals**: 16 ECU × 100 フレームの一覧表示・切替が体感で即時（0.2 秒未満）。

**Constraints**: `CanSim/` は ASCII のみ。UI の日本語は `.rc` のみ。フレーム名（日本語可）はファイル上 UTF-8、UI 上
UTF-16 で扱い、変換は UI 側で行う。実行中は編集ウィンドウを開けない（005 FR-011 と同じ扱い）。

**Scale/Scope**: 新規 `FrameEditor`（CanSim）、`FrameEditDlg`・`FrameEntryDlg`（UI）、`.rc` のダイアログ 2 つとメニュー
1 項目と文字列。変更: `CanTypes.h`（`FrameDefinition::Name`）、`ConfigLoader`、`ProjectFile`、`SimulationEngine`、
`MFCApplication1Dlg`、サンプル、`checks.cpp`、CLAUDE.md。

## Constitution Check

| 原則 | 判定 | 根拠 |
|---|---|---|
| I. Spec Kit ワークフロー準拠 | PASS | spec.md（006）→ 本 plan.md。 |
| II. MFC/C++ ネイティブ規約 | PASS | 新規ソースを 4 構成に登録するのみ。 |
| III. テスト可能性 | PASS | 入力検査・一覧操作・データ生成は `CanSim/FrameEditor` に分離し checks.exe で検証。ダイアログは表示と入出力の受け渡しのみ。 |
| IV. YAGNI | PASS | データ値の編集、受信フィルタ等の編集、元に戻す、並べ替え、コピーは入れない。 |
| V. ドキュメント同期 | PASS（条件付き） | CLAUDE.md（新規モジュール、編集ウィンドウ、データ 0xFF、起点の変更、サンプルの `Data` の扱い）と 004 の契約（`.canprj`）への後継参照の更新をタスクに含める。 |
| VI. ドメイン仕様準拠 | PASS | Doc v1.11 の 3.1a・FR-004・FR-020a・FR-091 に合わせる変更。CAN 2.0A（ID 11 ビット、DLC 0〜8）を維持。3.7a（バッファ）は範囲外と明記。 |

違反なし。Complexity Tracking は該当なし。

**Post-Design Re-check**: 新規依存・新規 VS プロジェクトなし。全項目 PASS。

## Project Structure

### Documentation (this feature)

```text
specs/006-frame-editor/
├── plan.md / research.md / data-model.md / quickstart.md
├── contracts/
│   ├── project-file-frames.md   # [NodeN.FrameM] の Name 追加、Data の扱い、100 上限
│   └── ui-frame-editor.md       # メニュー、編集ウィンドウ、入力画面、メッセージ
└── tasks.md
```

### Source Code (repository root)

```text
MFCApplication1/
├── MFCApplication1.rc / Resource.h     # 変更: IDD_FRAME_EDIT・IDD_FRAME_ENTRY、メニュー項目、文字列（UTF-16LE 注意）
├── FrameEditDlg.h / .cpp               # 新規: 編集ウィンドウ（ノード選択、一覧、追加/編集/削除）
├── FrameEntryDlg.h / .cpp              # 新規: 追加・編集の入力画面（検査結果の表示、ビット長・伝送時間）
├── MFCApplication1Dlg.h / .cpp         # 変更: ID_EDIT_FRAMES の配線、実行中は無効
└── CanSim/
    ├── CanTypes.h                      # 変更: FrameDefinition::Name、定数 kMaxFramesPerNode = 100
    ├── FrameEditor.h / .cpp            # 新規: 入力検査、追加・置換・削除、0xFF データ生成、ノード補完
    ├── ConfigLoader.cpp                # 変更: Data を無視し 0xFF 生成、Name 読込み、100 上限
    ├── ProjectFile.cpp                 # 変更: Name を保存、Data を保存しない
    ├── SimulationEngine.cpp            # 変更: 最初の TxRequest を通常動作開始時刻 + オフセットに
    └── SampleConfigs/*.canprj          # 変更: Data 行を削除（0xFF 前提）
tools/cansim_runner/checks.cpp          # 変更: 006 の検証、001/002/004 の期待値更新
CLAUDE.md                               # 変更
```

**Structure Decision**: 編集ウィンドウはモーダルで開き、現在のプロジェクトの `Network`（全ノード定義）を直接編集する。
変更は即時にプロジェクトへ反映（spec Assumptions）。入力画面は検査を `FrameEditor::ValidateFrame` に委ね、結果の
コードを `.rc` の日本語メッセージに対応付けて表示する。

## Complexity Tracking

該当なし。
