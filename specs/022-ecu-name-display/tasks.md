---

description: "Task list for feature 022-ecu-name-display"
---

# Tasks: ECU 名の入力と構成図への表示（ECUラベル名）

**Input**: Design documents from `/specs/022-ecu-name-display/`

**Prerequisites**: spec.md（計画での見直しを含む）、plan.md、research.md、data-model.md、contracts/ui-ecu-label.md、quickstart.md。

**Tests**: 新しい純粋関数・保存の往復は checks.exe に加える（憲章 III）。画面は GUI の自動操作・画面写しで確かめる。

**Organization**: ユーザーストーリーは 1 つ（US1 ECU 名を入力して構成図で見る P1）。

## Format: `[ID] [P?] [Story] Description`

## Path Conventions

- コア: `MFCApplication1/CanSim/`（ASCII のみ）。
- UI: `MFCApplication1/FrameEditDlg.*`、`MFCApplication1/MFCApplication1Dlg.*`。
- リソース: `MFCApplication1/Resource.h`・`MFCApplication1.rc`（UTF-16LE を保つ）。
- GUI 検証スクリプト: scratchpad の `t022/`。自分で起動したアプリだけを閉じる。

---

## Phase 1: Foundational

- [X] T001 `MFCApplication1/CanSim/CanTypes.h` の `Node` に `std::string Label;` を追加する。
  - コメント: 「022: ECU label shown in the diagram, UTF-8, 0..32 characters, empty by default」。
  - 定数: `kMaxEcuLabelChars = 32`、`kEcuLabelShownChars = 10`。
- [X] T002 `MFCApplication1/CanSim/FrameEditor.h/.cpp` に、関数を追加する（research.md 決定2）。（依存: T001）
  - `int CountUtf8Chars(const std::string&)`: UTF-8 の先頭バイト（`(b & 0xC0) != 0x80`）を数える。
  - `std::string LimitEcuLabel(const std::string&)`: 先頭 `kMaxEcuLabelChars` 文字で切る。
  - `std::string TrimEcuLabel(const std::string&)`: 前後の ` `・`\t`・全角空白（UTF-8 で `E3 80 80`）を除く。
  - `std::string EcuLabelForDiagram(const std::string&)`: `kEcuLabelShownChars` 文字以下はそのまま。超えれば、先頭 10 文字 + `"..."`。
- [X] T003 `MFCApplication1/CanSim/ConfigLoader.cpp` で、`[NodeN]` の `Label` を読む（無ければ空、`LimitEcuLabel(TrimEcuLabel(value))`）。
  - `MFCApplication1/CanSim/ProjectFile.cpp` の `SaveProject` では、`Label` が空でなければ `Name=` の行の次に `Label=` を書く。（依存: T002）
- [X] T004 `MFCApplication1/CanSim/BusDiagramLayout.h` の `kEcuBoxHeight` を 34 から 48 にする（022、コメントを更新）。
- [X] T005 [P] リソースを変更する。
  - `MFCApplication1/Resource.h`:
    - `#define IDC_EDIT_ECU_LABEL 1026` を追加し、`_APS_NEXT_CONTROL_VALUE` を 1027 にする。
  - `MFCApplication1.rc` の `IDD_FRAME_EDIT`:
    - `LTEXT "ECUラベル名:",IDC_STATIC,130,12,50,8` と `EDITTEXT IDC_EDIT_ECU_LABEL,182,10,120,12,ES_AUTOHSCROLL` を、コンボの行の後に追加する。
    - 読み直して、日本語が化けていないことを確かめる。

## Phase 2: User Story 1 - ECU 名を入力して構成図で見る (P1) 🎯

- [X] T006 [US1] `MFCApplication1/FrameEditDlg.h/.cpp` に、ECUラベル名の入力を追加する（research.md 決定4）。（依存: T002, T005）
  - `const UINT WM_APP_ECU_LABEL_CHANGED = WM_APP + 3;` を `FrameEditDlg.h` に追加する（オーナーへの通知）。
  - `OnInitDialog`:
    - `SetLimitText(kMaxEcuLabelChars)` を呼ぶ。
    - `ShowEcuLabel()`（選んだ ECU の `Label` を UTF-8 → CString で入れる。ECU が無ければ空で無効）を呼ぶ。
  - `OnNodeChanged`: `RefreshList` の後に `ShowEcuLabel()` を呼ぶ。
  - `ShowEcuLabel` では、`m_settingLabel = true` の間に `SetDlgItemText` する。
  - `ON_EN_CHANGE(IDC_EDIT_ECU_LABEL, OnEcuLabelChanged)` を追加する。`OnEcuLabelChanged`:
    1. `m_settingLabel` なら戻る。
    2. 入力欄の文字を UTF-8 にする。
    3. `CanSim::EnsureNode(m_definitions, nodeId).Label = LimitEcuLabel(TrimEcuLabel(text))` にする。
    4. オーナーに `WM_APP_ECU_LABEL_CHANGED` を送る。
- [X] T007 [US1] `MFCApplication1/MFCApplication1Dlg.h/.cpp` を変える。（依存: T004, T006）
  - `ON_MESSAGE(WM_APP_ECU_LABEL_CHANGED, OnEcuLabelChanged)` を追加する。中身は `InvalidateRect(GetDiagramRect(), FALSE)`。
  - `DrawBusDiagram` の ECU の文字:
    - `m_definitions` からその ECU の `Label` を探し（無ければ空）、`EcuLabelForDiagram` を通す。
    - 空でなければ、「ECU nn」を箱の上から `ScalePx(6)`〜`ScalePx(26)` に描く（今の太字）。
    - 続けてラベル名を `ScalePx(26)`〜`ScalePx(44)` に、10 px・通常・同じ文字色で中央寄せに描く。UTF-8 → CString は `CA2W(..., CP_UTF8)`。
    - 空なら、今までどおり「ECU nn」を箱の中央に描く。
- [X] T008 [P] [US1] `tools/cansim_runner/checks.cpp` に `CheckFeature022()` を追加する（quickstart.md 1）。（依存: T002, T003）
  - 文字数と切り詰め:
    - `CountUtf8Chars`
    - `EcuLabelForDiagram`（10・11・14 文字、全角）
    - `TrimEcuLabel`（全角空白を含む）
    - `LimitEcuLabel`（33 → 32）
  - `Label=` の往復: 保存 → 読込みで同じ。空なら行が無い。`Label=` の無いファイルは空。
  - 箱の高さ: 箱の高さが 48。上段 20〜68、下段 152〜200 で、枠（4〜212）の内側にある。
- [X] T009 [US1] ビルドし、実機で quickstart.md 手順 2-1〜2-5 を確かめる（画面写し）。（依存: T007）

## Phase 3: Polish

- [X] T010 [P] `CLAUDE.md` を更新する。
  - `Node::Label` / `Label=`。
  - データフレーム編集ウィンドウの「ECUラベル名」。
  - 構成図の箱（48 px・2 段）と、`EcuLabelForDiagram`。
  - `tools/cansim_runner/README.md` に 022 の項目を加える。
- [X] T011 4 構成のビルド（警告なし）・checks.exe 全 PASS・CSV の一致を確かめる（`Label` は CSV に出ない）。

---

## Dependencies & Execution Order

- T001 → T002 → T003。T004・T005 は並行可。
- T006（T002・T005 の後）→ T007（T004 の後）→ T009。T008 は T003 の後、並行可。
- T010 は並行可、T011 は最後。

## Implementation Strategy

1. Foundational（T001〜T005）: データ・保存・箱の大きさ・リソース。
2. US1（T006〜T009）: 入力と表示（これで要望はすべて）。
3. Polish（T010〜T011）。

## 実装結果（2026-10-01）

- 変更:
  - `CanSim/CanTypes.h`（`Node::Label`、定数）
  - `CanSim/FrameEditor.h/.cpp`（`CountUtf8Chars` / `LimitEcuLabel` / `TrimEcuLabel` / `EcuLabelForDiagram`）
  - `CanSim/ConfigLoader.cpp`（`Label=` の読込み）
    - 同名の内部関数 `CountUtf8Chars`（フレーム名の文字数の検査用、同じ処理）を共通の関数に一本化した。
  - `CanSim/ProjectFile.cpp`（空でなければ `Label=`）、`CanSim/BusDiagramLayout.h`（`kEcuBoxHeight` 48）
  - `FrameEditDlg.h/.cpp`（入力欄・`ShowEcuLabel`・`OnEcuLabelChanged`・`WM_APP_ECU_LABEL_CHANGED`）
  - `MFCApplication1Dlg.h/.cpp`（2 段の描画・描き直し）
  - `Resource.h`・`.rc`（`IDC_EDIT_ECU_LABEL`、「ECUラベル名:」）
  - `tools/cansim_runner/checks.cpp`・`README.md`、`CLAUDE.md`
- 実装中の修正: 全角 10 文字 + 「...」は、10 px の文字では 96 dpi で約 110 px になり、箱の内側（約 90 px）に入らなかった。
  - 120 dpi の画面写しでも、箱の縁に触れていた。
  - そこで、箱の幅に収まるまで、文字を 10 → 9 → 8 → 7 px と小さくして描くようにした。
- 作業中の不手際: スクリプトの書き込みの失敗で、`CanSim/FrameEditor.cpp` が一時的に空になった。
  未コミットの変更の無いファイルだったので、`git restore` で戻してから変更を当て直した。
- ビルド:
  - Release × x64/x86・Debug|x86 は、警告・エラーなしでビルドできた。
  - Debug|x64 は、ユーザーが起動中で exe が書き込めなかった。そのため、出力先を作業用フォルダにしてビルドし、コードの警告・エラーが無いことを確かめた（出たのは出力先についての MSB8029 だけ）。
- checks.exe: 全 174 項目 PASS（022 の 7 項目を追加。箱の大きさの変更後も、構成図・当たり判定・流れのエフェクトの既存項目は PASS）。基準 CSV との比較も一致した。
- GUI（Release|x64、120 dpi）: scratchpad の `t022/gui.ps1` で、全項目が合格した。
  - 入力欄は空で始まった。
  - ECU 01 に「エンジン」、ECU 02 に 14 文字の名前（前後に空白）を入力した。ノードを切り替えると、入力欄がそれぞれの名前に入れ替わった。
  - 構成図（`t022/labels_diag.png`）の表示:
    - ECU 01 は、上の段に「ECU 01」、下の段に「エンジン」。
    - ECU 02 は、下の段に「エンジンコントロール...」。小さな文字で、箱の内側に収まった。
  - 保存したファイルに、`Label=エンジン` と、前後の空白を除いた ECU 02 の名前があった。読み込み直しても表示された。
  - 16 ECU・フレーム表示 ON の実行中（`t022/running16_diag.png`）、強調と流れのエフェクトは、縦に大きくなった箱に合わせて描かれた。
  - 箱の上端近く（96 dpi で y 24）のダブルクリックで、編集ウィンドウが開いた。
