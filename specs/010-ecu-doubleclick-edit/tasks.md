---

description: "Task list for feature 010-ecu-doubleclick-edit"
---

# Tasks: ECU ブロックのダブルクリックでデータフレーム編集を開く

**Input**: Design documents from `/specs/010-ecu-doubleclick-edit/`

**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/, quickstart.md（すべて存在、US3 追加後の改訂版）。機能 003・006・007・009 の実装が完了していること。

**Tests**: spec.md は自動テストを要求していない。plan.md の方針に従い、当たり判定（`FindEcuAt`・`ScaleToDpi`）は `tools/cansim_runner/checks.cpp` で検証し、ダブルクリック・切り替え・待ち時間・メニューの有効／無効は GUI の自動操作（実マウス入力、ウィンドウの列挙、時刻の計測、スクリーンショット）で確認する。

**Organization**: spec.md のユーザーストーリー（US1 ダブルクリックで開く P1、US2 それ以外・実行中は何もしない P2、US3 開いたまま別 ECU に切り替える P2）ごと。当たり判定と DPI 換算は全ストーリーの前提なので Foundational に置く。編集ウィンドウのモードレス化と「編集中」のメニュー制御は、モードレスにした時点で必要になるため US1 に含める。

## Format: `[ID] [P?] [Story] Description`

## Path Conventions

コアは `MFCApplication1/CanSim/`（ASCII のみ）。UI は `MFCApplication1Dlg.*`／`FrameEditDlg.*`。`.rc` の変更はない。GUI 検証スクリプトはリポジトリ外（セッションの scratchpad の `gui.ps1`）。

---

## Phase 1: Setup

- [X] T001 GUI 検証スクリプト（scratchpad の `gui.ps1`、リポジトリ外）に操作を追加する: `classstyle`（メインウィンドウに `GetClassLongPtr(hwnd, GCL_STYLE)` を呼び`CS_DBLCLKS`（0x0008）の有無を出力）、`mdbl x y`（メインウィンドウのクライアント座標 (x, y) を `ClientToScreen` し、`SetCursorPos` + `SendInput` の左ボタン押下・解放 × 2 で実際のダブルクリックを行う）、`mclick x y`（同じく 1 回クリック）、`editor`（プロセスのトップレベルウィンドウのうち、クラス `#32770` でコンボボックス `IDC_COMBO_FRAME_NODE`（1014）を子に持つものを編集ウィンドウとして探し — スクリプトは ASCII のみで日本語のタイトルでは探さない —、ハンドル・ウィンドウ矩形・コンボボックスの選択文字列を出力、無ければ `none`）、`waiteditor ms`（編集ウィンドウが現れるまで待ち、経過 ms を出力）、`waitnoeditor ms`（消えるまで待つ）、`menustate`（「シミュレーション」メニューの「開始」の有効／無効を `GetMenuState` で出力）

---

## Phase 2: Foundational (Blocking Prerequisites)

- [X] T002 `MFCApplication1/CanSim/BusDiagramLayout.h/.cpp` に `int ScaleToDpi(int px, int dpi)` を追加する（research.md 決定2: 「`(px * dpi + 48) / 96`（非負の px に対して`MulDiv(px, dpi, 96)` と同じ四捨五入）」。中間値は `long long`）。ASCII のみ
- [X] T003 `MFCApplication1/CanSim/BusDiagramLayout.h/.cpp` に `int FindEcuAt(const BusDiagramLayout& layout, int x, int y, int dpi)` を追加する（research.md 決定1・3、data-model.md）: 各 `EcuPlacement` の `Box` を `ScaleToDpi` で換算した矩形 `[Left, Right) × [Top, Bottom)` に `(x, y)` が含まれればその `Number`、どれにも含まれなければ 0（依存: T002）
- [X] T004 `MFCApplication1/MFCApplication1Dlg.cpp` の無名名前空間の `ScalePx` を `return CanSim::ScaleToDpi(px, dpi);` に変える（描画と判定の換算を統一。見た目は変わらない）（依存: T002）
- [X] T005 [P] `tools/cansim_runner/checks.cpp` に `CheckFeature010()` を追加し `main` から呼ぶ（quickstart.md 手順 1）: (a) px 0〜1140 × DPI 96/120/144 で `ScaleToDpi` が`(px * dpi + 48) / 96` と一致、(b) ECU 数 1〜16 × DPI 96/120/144 の各 ECU の換算後の箱で、内側の四隅（左上、右 − 1、下 − 1、右下 − 1）と中心がその番号を返し、外側 1 px（左 − 1、上 − 1、右、下）ではその番号を返さない、(c) ECU 数 16・各 DPI で、96 dpi 基準の次の点を `ScaleToDpi` で換算した位置が 0: バス線の中央 (570, 110)、「CAN BUS」の文字の中心 (66, 93)（文字は `kCaptionLeft` 14〜約 118、y 80〜106）、各 ECU の接続線の中点 (`LineX`, `(LineY1 + LineY2) / 2`)、構成図の右下の余白 (1135, 210)、(d) ECU 数 0 では構成図内の 10 px 間隔の格子点すべてで 0、(e) ECU 数 N のとき N を超える番号は返らない。既存の 001〜009 の項目が PASS のまま（依存: T003）

**Checkpoint**: checks.exe の 010 項目がすべて PASS。構成図の見た目は変わらない

---

## Phase 3: User Story 1 - 構成図の ECU をダブルクリックして、その ECU のフレームを編集する (Priority: P1) 🎯 MVP

**Goal**: ECU のブロックのダブルクリックで、その ECU を選んだ編集ウィンドウが開き、開いたままメインウィンドウも操作でき、メニューから開いた場合と同じく編集できる

**Independent Test**: `sample_appendix_a` を読み込み「ECU 03」をダブルクリックして、選択欄が ECU 03 の編集ウィンドウが開く（quickstart.md 手順 2-1）

- [X] T006 [US1] `MFCApplication1/FrameEditDlg.h/.cpp` をモードレスに対応させる（research.md 決定5・8、data-model.md）: (a) コンストラクタに `int initialEcu = 1` を足し`m_initialEcu` に保持、`OnInitDialog` で ECU 数が 1 以上なら `m_node.SetCurSel(clamp(m_initialEcu, 1, m_ecuCount) - 1)`（従来の `SetCurSel(0)` を置き換え、ECU 数 0 なら選択なし）、(b) `int CurrentEcu() const`（= `SelectedNodeId()`）を public にする、(c) `OnOK`・`OnCancel` をオーバーライドして `DestroyWindow()`（Enter・Esc・「閉じる」・✖）、(d) `PostNcDestroy` で `GetOwner()` に `SendMessage(WM_APP_FRAME_EDITOR_CLOSED)`（`WM_APP + 1`、`FrameEditDlg.h` に定義）してから `delete this`、(e) `OnAdd`・`OnEdit`・`OnDelete` の入れ子のモーダル（`CFrameEntryDlg::DoModal`、`AfxMessageBox`）の間、オーナー（メインダイアログ）を `EnableWindow(FALSE)` し、戻った後に必ず `EnableWindow(TRUE)`する（小さな RAII 構造体で）（spec FR-002、FR-003、FR-015、FR-016）
- [X] T007 [US1] `MFCApplication1/MFCApplication1Dlg.h/.cpp` に編集ウィンドウの保持を実装する（research.md 決定5・7・9、data-model.md「メインダイアログの状態」）:メンバ `CFrameEditDlg* m_frameEditor = nullptr`・`int m_pendingEcu = 0`、`bool IsEditingFrames() const`（`m_frameEditor != nullptr || m_pendingEcu != 0`）、`void OpenFrameEditor(int ecu)`（`new CFrameEditDlg(m_definitions, m_ecuCount, ecu, this)` → `Create(IDD_FRAME_EDIT, this)` → 位置決め → `ShowWindow(SW_SHOW)`。構成図と選択欄のECU 数を揃えるため `m_settings.EcuCount` ではなく `m_ecuCount` を渡す。位置決めは research.md 決定10: 左上 = メインのクライアント座標`(ScaleToDpi(150), ScaleToDpi(kDiagramHeight + 8))` を `ClientToScreen`、`MonitorFromWindow` の作業領域の右端・下端からはみ出す分は左・上へずらし、左端・上端より外には出さない、`SetWindowPos(SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE)`（spec FR-017））、`void CloseFrameEditor()`（切替タイマー `kSwitchTimerId` = 2 を `KillTimer`、`m_pendingEcu = 0`、`m_frameEditor` があれば `DestroyWindow()`）、`ON_MESSAGE(WM_APP_FRAME_EDITOR_CLOSED, ...)` で `m_frameEditor = nullptr`、`ON_WM_DESTROY` の `OnDestroy` で `CloseFrameEditor()`。`OnEditFrames` は、実行中なら何もしない（従来）、`m_frameEditor` があれば `SetForegroundWindow` のみ、`m_pendingEcu != 0` なら何もしない、どちらでもなければ`OpenFrameEditor(1)`（spec FR-008、FR-009）（依存: T006）
- [X] T008 [US1] `MFCApplication1/MFCApplication1Dlg.cpp` で「編集中」のメニューを制御する（research.md 決定7、contracts「メインウィンドウのメニュー」）: `OnInitMenuPopup` で `IsEditingFrames()` なら「開始」（`ID_SIM_START`）を `MF_GRAYED`、`OnSimStart` の先頭で `IsEditingFrames()` なら何もしない（FR-013）。`OnFileLoadProject`・`OnEditSettings` の先頭（実行中チェックの後、ダイアログを出す前）で `CloseFrameEditor()` を呼ぶ（FR-014）。保存・フレーム表示は変えない（依存: T007）
- [X] T009 [US1] `MFCApplication1/MFCApplication1Dlg.h/.cpp` に `afx_msg void OnLButtonDblClk(UINT nFlags, CPoint point)` と `ON_WM_LBUTTONDBLCLK()` を追加する（contracts「判定」1〜3 の「閉」「開・同じ ECU」まで）: 実行中なら基底を呼んで戻る。`CClientDC` の `LOGPIXELSX` を DPI とし`k = CanSim::FindEcuAt(CanSim::ComputeBusDiagramLayout(m_ecuCount), point.x, point.y, dpi)`。0 なら基底のみ。`m_frameEditor` も `m_pendingEcu` も無ければ `OpenFrameEditor(k)`、`m_frameEditor` があり `CurrentEcu() == k` なら `SetForegroundWindow`。`WM_LBUTTONDOWN` は処理しない（spec FR-001、FR-002、FR-007、FR-011）（依存: T003, T007）
- [X] T010 [US1] Debug|x64 をビルドし、T001 の `classstyle` でメインダイアログのクラスが `CS_DBLCLKS` を持つことを確認する（research.md 決定4）。持たない場合は、実マウスのダブルクリックが届かない旨を報告して対処（ダイアログテンプレートのクラス登録）を検討する（依存: T001, T009）
- [X] T011 [US1] 実機（Debug|x64、一時フォルダのサンプル、実マウス入力）で quickstart.md 手順 2-1・2-6（開始がグレー、閉じると選べる）・2-7・2-9・2-13・2-14・2-15 を確認する。2-15 は、ECU 数 16 で編集ウィンドウの矩形（`editor` の出力）が全 ECU の箱の換算後の矩形と重ならないこと、編集ウィンドウで Tab が効き Esc で閉じること、メインウィンドウはEnter・Esc で閉じないこと（FR-017、contracts「編集ウィンドウ」）。2-14 は拡大率 100% の実機で ECU 03 の箱の縁の内側 1 px（左上から (+1, +1)）で開き外側 1 px（(−1, −1)）で開かないこと。125%・150% は T005 の計算検証で代える（描画と判定が同じ `ScaleToDpi` を使うため）（SC-001、SC-002、SC-004、SC-005、SC-007）（依存: T008, T010）

**Checkpoint**: US1 の受け入れシナリオ 1〜4 と、編集中の「開始」無効・読込み／セッティング前の自動クローズを満たす

---

## Phase 4: User Story 2 - ECU ブロック以外や実行中のダブルクリックでは何も起きない (Priority: P2)

**Goal**: 誤操作や実行中の編集を防ぐ

**Independent Test**: 余白・バス線のダブルクリック、実行中の ECU のダブルクリックで何も開かない（quickstart.md 手順 2-10・2-11）

- [X] T012 [US2] 実機で quickstart.md 手順 2-10・2-11 を確認する: バス線・「CAN BUS」の文字・接続線・構成図の余白・ログ表示エリアのダブルクリック（`mdbl`）と ECU の 1 回クリック（`mclick`）で編集ウィンドウが現れない（`editor` が `none`）。ECU 数 0 ではどこでも開かない。`sample_startup`（スピード 0）の実行中に ECU をダブルクリックしても開かず実行・ログ表示が続き、完了後は開ける（SC-003、spec FR-005〜FR-007）（依存: T009）

**Checkpoint**: SC-003 を満たす

---

## Phase 5: User Story 3 - 編集ウィンドウを開いたまま、別の ECU をダブルクリックして切り替える (Priority: P2)

**Goal**: 閉じて約 0.5 秒おいてから新しい ECU で開き、切り替わったことが分かる

**Independent Test**: ECU 03 の編集ウィンドウを開いたまま ECU 05 をダブルクリックし、閉じてから 0.4〜0.7 秒後に ECU 05 で開く（quickstart.md 手順 2-2）

- [X] T013 [US3] `MFCApplication1/MFCApplication1Dlg.h/.cpp` に切り替えを実装する（research.md 決定6、data-model.md の状態遷移）: `OnLButtonDblClk` で、`m_frameEditor` があり`CurrentEcu() != k` なら `CloseFrameEditor()` の後に `m_pendingEcu = k`、`SetTimer(kSwitchTimerId, kSwitchDelayMs = 500, nullptr)`。`m_frameEditor` が無く `m_pendingEcu != 0`（待ち）なら `m_pendingEcu = k` だけ（タイマーは延長しない）。`OnTimer` で `nIDEvent == kSwitchTimerId` なら `KillTimer`、`ecu = m_pendingEcu`、`m_pendingEcu = 0`、実行中でなければ `OpenFrameEditor(ecu)`（再生タイマーの処理より前に分岐し、既存の再生処理は変えない）（spec FR-010〜FR-012）（依存: T009）
- [X] T014 [US3] 実機で quickstart.md 手順 2-2〜2-5・2-8・2-12 を確認する: ECU 03 → ECU 05 のダブルクリックで、`waitnoeditor` から `waiteditor` までが 400〜700 ms、選択欄がECU 05。同じ ECU のダブルクリックではハンドルが変わらない。選択欄を ECU 01 に変えてから ECU 01 は閉じ直さず、ECU 03 は開き直す。待ちの間に ECU 06 をダブルクリックすると開くのは ECU 06 の 1 つだけ。待ちの間も「開始」はグレー。「追加」の入力画面中の別 ECU のダブルクリックでは何も起きず入力画面が残り、入力画面を閉じた後はメインウィンドウが再び操作できる（手順 2-16）。「セッティング」で編集ウィンドウが閉じた後にセッティングをキャンセルしても、編集ウィンドウは開かない（FR-014）。待ちの間の ✖ で新しいウィンドウは開かずプロセスが終了する（SC-006、SC-007、SC-008、spec FR-010〜FR-016）（依存: T001, T013）

**Checkpoint**: SC-006〜SC-008 を満たす

---

## Phase 6: Polish & Cross-Cutting Concerns

- [X] T015 [P] `CLAUDE.md` を更新する: 構成図の ECU ブロックのダブルクリックで、その ECU を選んだ編集ウィンドウを開く（実行中は無効）、`BusDiagramLayout` の `ScaleToDpi`・`FindEcuAt`（描画と当たり判定で換算を共有）、`CFrameEditDlg` のモードレス化（`initialEcu`・`CurrentEcu`、閉じた通知、入力画面の間はメインを無効化）、別 ECU のダブルクリックでの切り替え（閉じて 500 ms 後に開く）、編集中は「開始」不可・読込み／セッティングは先に閉じる（憲章 V）
- [X] T016 Debug/Release × x64/x86 の全 4 構成で警告なしのビルドと `checks.exe` の全項目 PASS を確認し、サンプルの CSV が基準と一致すること（004 SC-009）を確かめる（依存: T005, T011, T012, T014）

---

## Dependencies & Execution Order

- **Setup**: T001（GUI 確認の前提。実装と並行可）
- **Foundational**: T002 → T003 → T005（checks）、T002 → T004
- **US1**: T006 → T007 → T008、T007 + T003 → T009 → T010 → T011
- **US2**: T012（T009 の後。US1 の確認と並行可）
- **US3**: T013（T009 の後）→ T014
- **Polish**: T015 は並行可、T016 は最後

### Parallel Opportunities

- T001（検証スクリプト）は T002〜T009 と並行可
- T005（checks.cpp）は T004・T006〜T008（UI）と並行可
- T015（文書）は実装と並行可

## Implementation Strategy

1. **MVP = US1**（T001〜T011）: ECU のダブルクリックでその ECU の編集ウィンドウが開き、開いたままメインウィンドウを操作でき、編集中は「開始」不可。
2. US2 で「開かない」場合の確認、US3 で開いたままの切り替え（閉じて 0.5 秒後に開く）。
3. Polish で文書・4 構成ビルド。

## Notes

- `CanSim/` の変更に日本語を含めない（ASCII のみ）。
- シミュレーションエンジン・CSV・`.rc` は変更しない。
- 編集ウィンドウはその場で `m_definitions` を書き換える（機能 006 と同じ）。どの経路で閉じても編集内容は失われない。

## 実装結果（2026-09-30）

- checks.exe: 010 の 6 項目を含む全項目 PASS（サンプルの CSV は基準と一致）。4 構成（Debug/Release × x64/x86）とも警告なし。
- GUI（Debug|x64、拡大率 125% の実機、実マウス入力）: quickstart.md 手順 2-1〜2-16 をすべて確認。
  - 2-2・2-4: ダブルクリックから新しい編集ウィンドウが開くまで 532 ms・539 ms（閉じるのはダブルクリックの直後）。
  - 2-14: 拡大率 125% で ECU 03 の箱の左上 (268, 43) に対し、(267, 42) では開かず (269, 44) で開く。100% と 150% は checks の計算検証で代えた。
  - 2-15: ECU 数 16 で、編集ウィンドウの上端（クライアント座標 280 px）が下段の ECU の箱の下端（233 px）より下。Tab でフォーカスが選択欄からフレーム一覧へ移り、Esc で閉じる。
- quickstart.md 手順 5 の「ECU 07」は、`sample_appendix_a` の ECU 数が 6 なので「ECU 06」に直した（T014 も同様）。
- 自動操作では、ダイアログを閉じた直後の最初のダブルクリックがまれに届かないことがあった。待ちを入れて再実行すると 1 回目から開いた。アプリの動作ではなく自動操作のタイミングによるものと判断した。
