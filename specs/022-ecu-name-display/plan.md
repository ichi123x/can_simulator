# Implementation Plan: ECU 名の入力と構成図への表示

**Branch**: `ver.0.3`（spec ディレクトリ: `022-ecu-name-display`） | **Date**: 2026-10-01 | **Spec**: [spec.md](./spec.md)

## Summary

- ECU（ノード）に新しい項目「ECUラベル名」（`Node::Label`、UTF-8、0〜32 文字、既定は空白）を足す。
  - プロジェクトファイルには `[NodeN]` の `Label=` として、空でないときだけ保存する。
- データフレーム編集ウィンドウの「ノード:」の右に、「ECUラベル名:」のラベルと入力欄を置く。
  - 入力のたびに、選んだ ECU の `Label` へ反映する（前後の空白は除く）。
  - 反映したら、メインダイアログに構成図の描き直しを依頼する。
- 構成図の ECU の箱を、縦 34 → 48 px（96 dpi）に広げる。
  - ラベル名があれば、「ECU nn」を上の段に、ラベル名を下の段に 10 px の文字で描く。11 文字以上は先頭 10 文字と「...」にする。
  - ラベル名が無ければ、「ECU nn」を箱の中央に描く。
- 文字数の数え方・切り詰め・前後の空白の除去は、CanSim の純粋関数にして checks で検証する。

**計画中の見直し（spec の Assumptions を置き換え）**: spec では、既存のノードの名前（`Name=`）を ECU 名として使う想定だった。しかし `Name` は、起動記録の CSV（`.startup.csv`）の列に出力されている。また、名前の無い ECU には自動で「ECU1」「Node1」のような名前が付く（`EnsureNode`・読込みの既定値・`BuildRunNetwork` の補い）。そのため、そのまま使うと次の 2 つが起きる。

- 既定が空白にならない（サンプル・自動の名前が表示される）。
- 空白にすると CSV が変わる（FR-007 に反する）。

そこで、表示用の別の項目 `Label` を足す。`Name` と CSV は変えない。

## Technical Context

**Language/Version**: C++14相当

**Primary Dependencies**: MFC（`CEdit`、`EN_CHANGE`）/ GDI。新規外部依存なし。

**Storage**: `.canprj` の `[NodeN]` に省略可能な `Label=` を追加する。古い版のアプリは知らない項目を無視するので、互換性は保たれる。空のときは書かないので、ラベル名を使わないプロジェクトの保存内容は今と同じ。

**Testing**:
- checks.exe に次を加える。
  - `CountUtf8Chars` / `EcuLabelForDiagram`（全角・半角、10 / 11 文字、空）
  - `TrimEcuLabel`
  - `Label=` の保存・読込みの往復と、空なら書かないこと
  - 箱の高さ 48 で、ECU 数 0〜16 の箱がバス・枠と重ならないこと（既存の 003・011 の項目でも確かめる）
  - `Label` が CSV に影響しないこと（既存の CSV の一致）
- GUI: 入力 → 構成図の表示（画面写し）、保存・読込みの往復を確かめる。

**Target Platform / Project Type**: Windows desktop-app（Win32/x64、Debug/Release）

**Constraints**:
- `CanSim/` は ASCII のみ（ラベル名の値は UTF-8 のデータとして扱う）。
- 「ECUラベル名:」は `.rc` のダイアログに置く。
- 編集ウィンドウは実行中には開けない（変更なし）。

**Scale/Scope**:
- 変更:
  - `CanSim/CanTypes.h`（`Node::Label`、定数）
  - `CanSim/FrameEditor.h/.cpp`（文字数・切り詰め・前後の空白の除去）
  - `CanSim/ConfigLoader.cpp`・`ProjectFile.cpp`（`Label=`）
  - `CanSim/BusDiagramLayout.h`（`kEcuBoxHeight` 48）
  - `FrameEditDlg.h/.cpp`、`MFCApplication1Dlg.h/.cpp`
  - `Resource.h`・`.rc`
  - `tools/cansim_runner/checks.cpp`・README、`CLAUDE.md`
  - spec の FR-006・Assumptions（`Label=` に読み替え）

## Constitution Check

| 原則 | 判定 | 根拠 |
|---|---|---|
| I | PASS | spec（022、Q1 回答済み）→ 本 plan。 |
| II | PASS | 既存ファイルの変更のみ。4 構成のビルドを維持する。 |
| III | PASS | 文字数・切り詰め・保存の往復を CanSim の純粋関数・既存の保存／読込みで実現し、checks で検証する。ダイアログは入力と描画だけ。 |
| IV | PASS | 項目 1 つと入力欄 1 つ。凡例・色分けなどは足さない。 |
| V | PASS（条件付き） | CLAUDE.md（Node::Label、`Label=`、編集ウィンドウ、構成図の箱）を更新する。 |
| VI | PASS | 表示の名前だけで、シミュレーションの挙動・結果は変えない。 |

違反なし。

## Project Structure

```text
specs/022-ecu-name-display/ spec.md / plan.md / research.md / data-model.md / contracts/ui-ecu-label.md / quickstart.md / tasks.md
```
