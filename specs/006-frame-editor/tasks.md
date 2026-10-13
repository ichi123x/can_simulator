---

description: "Task list for feature 006-frame-editor"
---

# Tasks: データフレーム編集ウィンドウ

**Input**: Design documents from `/specs/006-frame-editor/`

**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/, quickstart.md（すべて存在）。機能 004・005 の実装が完了していること。

**Tests**: spec.md は自動テストを要求していない。plan.md の方針に従い、`CanSim/` のコア（`FrameEditor`、読込み・保存の変更、送信要求の起点）は `tools/cansim_runner/checks.cpp` に検証項目を追加し、既存項目の期待値を更新する。GUI は quickstart の手順で確認する。

**Organization**: spec.md のユーザーストーリー（US1〜US4）ごと。データ 0xFF 化・フレーム名・100 上限・起点の変更と`FrameEditor` のコアは全ストーリーの前提なので Foundational に置く。

## Format: `[ID] [P?] [Story] Description`

## Path Conventions

コアは `MFCApplication1/CanSim/`（ASCII のみ）、UI は `MFCApplication1/*.h/.cpp` と `MFCApplication1.rc`。`.rc` は UTF-16LE（BOM 付き、CRLF）のため Read/Edit ツールを使わず、エンコーディングを明示して編集する（CLAUDE.md）。UI の日本語文字列は `.rc` のみ。フレーム名はファイル上 UTF-8、UI 上 UTF-16（`CA2W`/`CW2A` の `CP_UTF8` で変換）。

---

## Phase 1: Setup

- [X] T001 `MFCApplication1/CanSim/FrameEditor.h/.cpp`（ASCII のみ）、`MFCApplication1/FrameEditDlg.h/.cpp`、`MFCApplication1/FrameEntryDlg.h/.cpp`を空の雛形で作成し、`MFCApplication1/MFCApplication1.vcxproj` と `.vcxproj.filters` に登録する（全 4 構成。CanSim はフィルタ `CanSim`、UI は `MFCApplication1Dlg.*` と同じフィルタ）

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Doc v1.11 に合わせたデータ・ファイル形式・送信要求の起点の変更と、編集のコア

- [X] T002 `MFCApplication1/CanSim/CanTypes.h` の `FrameDefinition` に `std::string Name;`（「0〜32 文字」、UTF-8）を追加し、定数 `kMaxFramesPerNode = 100`、`kMaxFrameNameChars = 32`、`kMaxPeriodMs = 3600000` を定義する（data-model.md）
- [X] T003 `MFCApplication1/CanSim/ConfigLoader.cpp` のフレーム読込みを変更する: `Data` は読み飛ばし（書かれていても誤りにしない）、`fd.Data` を「`Dlc` 個の 0xFF」で作る（spec FR-014）。`Name` を読む（省略時は空。UTF-8 のバイト列で 32 文字を超える判定はUTF-8 の文字数で行い、超えたら拒否）。`InitialOffsetUs` が `PeriodUs` 以上なら拒否。1 ノードのフレーム数が 100 を超えたら`[NodeN] has more than 100 frames.` で拒否（spec FR-013）（依存: T002）
- [X] T004 [P] `MFCApplication1/CanSim/ProjectFile.cpp` の `SaveProject` で、各フレームに `Name=` を書き、`Data=` は書かないようにする（contracts/project-file-frames.md）（依存: T002）
- [X] T005 `MFCApplication1/CanSim/SimulationEngine.cpp` の送信要求の起点を変更する（research.md 決定4、spec FR-015）: `Run` の初期化で全フレームの最初の `TxRequest` を積む処理を削除し、`HandleSyncComplete` でノードが通常動作に入ったとき、そのノードの全フレームの最初の `TxRequest` を `NormalEntry + InitialOffsetUs` に積む（シミュレーション時間を超えるものは積まない）。2 回目以降は現在どおり直前の要求 + 周期。通常動作に到達しないノードは送信要求を出さない。コメントの 002 FR-010（起動前の要求の保留）の記述を、起点の変更により起動前の要求が発生しなくなったことに合わせて更新する
- [X] T006 `MFCApplication1/CanSim/FrameEditor.h/.cpp` を実装する（data-model.md）: `FrameInput`、`FrameField`、`FrameProblem`、`FrameIssue`、`ValidateFrame`（ID は 16 進・`0x` 省略可・0x000〜0x7FF、DLC は 0〜8、ID はプロジェクト内の全ノード（ECU 数の範囲外を含む）で一意で編集時は自身を除く、送信周期は 1〜3,600,000 ms の整数、初回オフセットは空欄なら 0・0 以上かつ送信周期未満の整数、フレーム名は「0〜32 文字」、追加時にノードが 100 件なら `ListFull`。問題がなければ `out` に確定値（µs に換算、Data は 0xFF × Dlc）を入れる）、`MakeFrameData`、`EnsureNode`（NodeId 昇順の位置に `ECU<n>` の既定ノードを作る）、`AddFrame`・`ReplaceFrame`・`DeleteFrame`（依存: T002）
- [X] T007 サンプルを更新する: `MFCApplication1/CanSim/SampleConfigs/*.canprj` の `Data=` 行をすべて削除する（データは 0xFF 固定。内容のほかの値は変えない）。`sample_appendix_a.canprj` のフレームには、Doc 付録A の「内容（例）」を `Name=` として付ける（依存: T003）
- [X] T008 `tools/cansim_runner/checks.cpp` を更新する: (a) 006 の検証項目を追加（quickstart 手順1: `ValidateFrame` の各 NG と正常、編集時の自身除外、範囲外 ECU の ID 重複、101 件目、`AddFrame`/`ReplaceFrame`/`DeleteFrame` と並び、`EnsureNode`、読込みで`Data=00` が 0xFF になる、`Name` 読込み、101 フレームと「オフセット ≥ 周期」の拒否、保存→再読込みの一致と `Data=` 非出力、電源投入 50 ms のノードでオフセット 0 / 5 ms のフレームの最初の Tx_Request = 通常動作開始 / + 5 ms）。(b) データ 0xFF と起点の変更で値が変わる 001・002 の既存項目（例: 0x080 の最初の Tx_Start 162 µs、SC-003 のスタッフビット、起動待ち中の上書き 5 回、FR-010 の保留）の期待値を、新しい動作での値に更新し、変更理由をコメントに残す。(c) 004 SC-009（基準 CSV との比較）は、基準が 006 以前のものである旨の表示に変える（比較処理は残す）（依存: T003〜T007）【実施メモ】変化した既存項目: 002 FR-010（起動待ちノードの最初の Tx_Request が 0 → 通常動作開始時刻）、002 FR-011（起動待ち中の上書き 5 回 → 0 回。起動前に要求が出ないため）、002 FR-007（後発参加の同期やり直し）。FR-007 は、ECU1 の2 回目の要求が 300 → 322 µs にずれてノード3（電源投入 290 µs）が遮られずに 312 µs で同期するようになったため、`sample_startup_latejoin.canprj` のノード3 の電源投入を 305 µs に変えて「遮られてやり直す」場面を保ち、期待値を 580 µs（574 − 16 + 22）にした。0x080 の最初の Tx_Start 162 µs は変わらず。データ 0xFF で伝送時間はスタッフビット 1〜2 個分（2〜4 µs）変わり、行数は不変。004 SC-009 の基準は 006 のビルドで作り直した（`%TEMP%ase006`）。

**Checkpoint**: `checks.exe` の全項目が PASS（quickstart 手順1）

---

## Phase 3: User Story 1 - ノードの送信フレーム一覧を見る (Priority: P1) 🎯 MVP

**Goal**: 編集ウィンドウでノードを選び、一覧を表示する

**Independent Test**: `sample_appendix_a` で ECU1 を選ぶと 0x100・0x101（10 / 100 ms）が表示される（quickstart 手順 2〜3）

- [X] T009 [US1] `MFCApplication1/MFCApplication1.rc` と `MFCApplication1/Resource.h` を更新する: メニュー「編集(&E)」の「セッティング」の下に「データフレーム編集(&F)...」（`ID_EDIT_FRAMES`）を追加。ダイアログ `IDD_FRAME_EDIT`（ノード `IDC_COMBO_FRAME_NODE`、一覧`IDC_LIST_FRAMES`（`LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS`）、`IDC_BUTTON_FRAME_ADD`/`EDIT`/`DELETE`、閉じる（`IDCANCEL`））。文字列テーブルに列見出し（No. / フレーム名 / ID / DLC / 初回オフセット(ms) / 送信周期(ms)）とウィンドウ用の文字列を追加（contracts/ui-frame-editor.md）。**`.rc` はエンコーディングを明示して編集する**
- [X] T010 [US1] `MFCApplication1/FrameEditDlg.h/.cpp` に `CFrameEditDlg` を実装する: コンストラクタで `CanSim::Network&`（プロジェクトの定義）とECU 数・通信速度を受け取る。ノードのリストに「ECU 01」〜「ECU NN」を入れ、初期選択 ECU 01。選択ノードの一覧を通し番号順に表示（ID は `0x%03X`、周期とオフセットは ms。ms の整数でない値は小数第 3 位まで）。ノード切替で一覧を更新。ECU 数 0 では一覧を空にする（依存: T006, T009）
- [X] T011 [US1] `MFCApplication1/MFCApplication1Dlg.cpp` に `ID_EDIT_FRAMES` のハンドラを追加する（`m_definitions` を渡して `CFrameEditDlg` をモーダルで開く）。`OnInitMenuPopup` で実行中は無効にする（spec FR-001）。ハンドラ冒頭でも実行中なら何もしない（依存: T010）

**Checkpoint**: 一覧の表示と切替ができる

---

## Phase 4: User Story 2 - フレームを追加する (Priority: P1)

**Goal**: 入力画面から検査付きでフレームを追加する

**Independent Test**: 空のノードに 0x200・DLC 4・20 ms を追加し、No.1 で表示される。NG 入力は理由付きで拒否（quickstart 手順 4）

- [X] T012 [US2] `MFCApplication1/MFCApplication1.rc` と `MFCApplication1/Resource.h` に `IDD_FRAME_ENTRY`（フレーム名 `IDC_EDIT_FRAME_NAME`（最大 32 文字）、ID `IDC_EDIT_FRAME_ID`、DLC `IDC_EDIT_FRAME_DLC`、送信周期 `IDC_EDIT_FRAME_PERIOD`、初回オフセット `IDC_EDIT_FRAME_OFFSET`、ビット長・伝送時間の表示 `IDC_STATIC_FRAME_BITS`、OK、キャンセル）と、NG メッセージの文字列（項目名 5 つ、理由 7 つ（未入力、数値ではない、範囲外（範囲を示す）、他のフレームと重複（重複先のノードと No. を示す）、長すぎる、送信周期以上、一覧が上限）、見出し）を追加する（contracts/ui-frame-editor.md）。**`.rc` はエンコーディングを明示して編集する**
- [X] T013 [US2] `MFCApplication1/FrameEntryDlg.h/.cpp` に `CFrameEntryDlg` を実装する: 追加モード（初回オフセットの初期値 0）と編集モード（既存値を入れて開く）。ID・DLC の変更（`EN_CHANGE`）でビット長と伝送時間（`ComputeFrameTiming`、データ 0xFF、現在の通信速度）を「132 ビット / 264 µs」形式で表示し、範囲外なら「-」（spec FR-011）。OK で `FrameEditor::ValidateFrame` を呼び、問題があれば「項目: 理由」を行ごとに並べたメッセージを表示して最初の NG 項目にフォーカスし、閉じない（spec FR-006）。問題がなければ確定した `FrameDefinition` を返して閉じる。フレーム名は UTF-16 → UTF-8 に変換し、文字数は UTF-16 の文字数で数える（依存: T006, T012）
- [X] T014 [US2] `MFCApplication1/FrameEditDlg.cpp` の「追加」: `CFrameEntryDlg` を追加モードで開き、OK なら `FrameEditor::AddFrame`（定義のない ECU は`EnsureNode` で作られる）して一覧を再表示し、追加した行を選択する。一覧が 100 件または ECU 数 0 なら「追加」を無効にする（spec FR-004、FR-007）（依存: T010, T013）

**Checkpoint**: SC-001・SC-002・SC-004 を満たす

---

## Phase 5: User Story 3 - フレームを編集・削除する (Priority: P1)

**Goal**: 選んだフレームの編集と、確認付きの削除、通し番号の振り直し

**Independent Test**: ECU1 の 0x101 を 50 ms に編集、0x100 を削除して No. が振り直される（quickstart 手順 5）

- [X] T015 [US3] `MFCApplication1/FrameEditDlg.cpp` の「編集」と一覧のダブルクリック: 選んだフレームの値を入れた `CFrameEntryDlg` を編集モードで開き（ID の重複検査は自身を除く）、OK なら `ReplaceFrame` して再表示し、同じ行を選択したままにする（spec FR-007）（依存: T014）
- [X] T016 [US3] `MFCApplication1/FrameEditDlg.cpp` の「削除」: 「No.n フレーム名 (0xXXX) を削除しますか？」を表示し、了承されたら `DeleteFrame` して再表示する（通し番号は並び順で振り直される。spec FR-008、FR-009）。行が選ばれていないときは「編集」「削除」を無効にする（`LVN_ITEMCHANGED` で更新。spec FR-004）（依存: T014）

**Checkpoint**: US3 の受け入れシナリオを満たす

---

## Phase 6: User Story 4 - 編集結果をプロジェクトに保存し、シミュレーションで使う (Priority: P2)

**Goal**: 編集結果が保存・読込み・実行に届く

**Independent Test**: 追加して保存→再起動→読込みで一覧が同じ。開始すると追加したフレームがログに出る（quickstart 手順 6）

- [X] T017 [US4] 実機（Debug|x64、一時フォルダのサンプル）で、追加・編集・削除のあと保存し、アプリを再起動して読み込み、一覧が一致することと`.canprj` に `Name=` が書かれ `Data=` がないことを確認する。続けて開始（スピード +10）し、追加したフレームの Tx_Request が「通常動作開始 + 初回オフセット + n × 周期」であることをログで確認する（SC-003、SC-005）（依存: T015, T016, T011）【結果】ECU 02 に「new frame」（0x200・DLC 4・20 ms・オフセット 5 ms）を追加し、No.1「ギア位置」を削除して保存。`.canprj` に`Name=` があり `Data=` はない。再起動後の読込みで一覧が一致。スピード +10 で実行し、0x200 の最初の Tx_Request = 5022 µs（通常動作 22 µs + 5 ms）を確認。

---

## Phase 7: Polish & Cross-Cutting Concerns

- [X] T018 [P] `CLAUDE.md` を更新する（憲章 V）: アーキテクチャ節に `FrameEditor.h/.cpp`、`FrameEditDlg`・`FrameEntryDlg` を追加。メニューに「データフレーム編集」。`.canprj` のフレームは `Name` を持ち、データは常に DLC 分の 0xFF（`Data=` は読まない・書かない）。送信要求の起点は通常動作開始時刻（Doc FR-020a）。検証節の「004 SC-009 の基準 CSV は 006 以前のものなので一致しない」旨
- [X] T019 [P] `specs/004-menu-project-settings/contracts/project-file-format.md` の冒頭の注記に、006 による変更（`Name` 追加、`Data` 廃止、100 上限、オフセットの起点）と `specs/006-frame-editor/contracts/project-file-frames.md` への参照を追記する。`tools/cansim_runner/README.md` の基準比較の説明に、006 で結果が変わったことを追記する
- [X] T020 Debug/Release × x64/x86 の全 4 構成で `msbuild MFCApplication1\MFCApplication1.sln` が警告なしで通ることを確認する（憲章 II）
- [X] T021 quickstart 手順 2〜6 を GUI で通して実施し（一時フォルダのサンプル）、`checks.exe` の全項目 PASS を確認する。16 ECU × 100 フレームのプロジェクトで一覧の切替が即時であることも確認する（依存: T020）【結果】GUI 手順 2〜6 を自動操作で実施（一覧の列、ノード切替、NG 入力の理由表示（ID 重複・DLC 範囲外・周期 0）、追加時のビット長「87 ビット / 174 µs」、編集、削除の確認と No. の振り直し、行未選択で「編集」「削除」が無効、保存→再読込み、実行）。16 ECU × 100 フレームで ECU 01→16 の切替が即時、100 件のノードで「追加」が無効。実行中は「データフレーム編集」がグレー。列幅が足りず「送信周期」列が見切れていたため、`IDD_FRAME_EDIT` を 430 DLU 幅に広げた。checks.exe 全項目 PASS。

---

## Dependencies & Execution Order

- **Setup (T001)** → **Foundational**: T002 → T003 / T004 / T005 / T006（T004・T006 は並行可）→ T007 → T008
- **US1**: T009 → T010 → T011（T010 は T006 も前提）
- **US2**: T012 → T013 → T014
- **US3**: T015・T016（T014 後）
- **US4**: T017（US1〜US3 後）
- **Polish**: T018・T019 並行可、T020 → T021

### Parallel Opportunities

- T004（保存）・T005（起点）・T006（FrameEditor）は、T002 後に並行可（別ファイル）
- T009・T012（`.rc`）は同じファイルなので順に行うが、コアのタスクとは並行可
- T018・T019（文書）は実装と並行可

## Implementation Strategy

1. Foundational でデータ 0xFF・`Name`・100 上限・起点の変更と `FrameEditor` を作り、checks.exe の期待値を新しい動作にそろえる。
2. **MVP = US1**: 編集ウィンドウで一覧が見える状態。
3. US2（追加）→ US3（編集・削除）→ US4（保存・実行の確認）→ Polish。

## Notes

- `CanSim/` の新規ソースに日本語を含めない（ASCII のみ）。
- 3.7a（バッファ 16/8 本、OVERWRITE）と Update_Count への列名変更は本機能の範囲外。
