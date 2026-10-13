---

description: "Task list for feature 013-list-dblclick-add"
---

# Tasks: 送信フレーム一覧のダブルクリックでフレーム追加を開く

**Input**: Design documents from `/specs/013-list-dblclick-add/`

**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/, quickstart.md（すべて存在）。機能 006・010 の実装が完了していること。

**Tests**: spec.md は自動テストを要求していない。コアの変更はないので checks.exe は既存項目の PASS と CSV の一致を確認する。ダブルクリックの動作はGUI の自動操作（マウスのダブルクリックを送り、入力画面 `IDD_FRAME_ENTRY` のウィンドウが現れるかを見る）で確認する。

**Organization**: spec.md のユーザーストーリー（US1 一覧のどこをダブルクリックしてもフレームを追加できる P1、US2 追加できないときはダブルクリックしても何も起きない P2）ごと。

## Format: `[ID] [P?] [Story] Description`

## Path Conventions

UI は `MFCApplication1/FrameEditDlg.*`。`CanSim/`・`.rc` の変更はない。GUI 検証スクリプトはリポジトリ外（セッションの scratchpad の `gui.ps1`）。

---

## Phase 1: Setup

- [X] T001 GUI 検証の準備をする（リポジトリ外、セッションの scratchpad）:
  - `gui.ps1` に次の操作を用意する。
    - アプリ（Debug|x64 の `MFCApplication1.exe`）を起動し、プロジェクトを読み込む。「編集 → データフレーム編集」を開く。
    - 編集ウィンドウの一覧（コントロール ID `IDC_LIST_FRAMES`）の中の指定位置に、マウスのダブルクリックを送る。位置の指定は次のとおり。
      - 一覧のクライアント座標
      - 「行 n の中央」
      - 「最後の行より下の空白」
      - 「列見出しの項目 k の中央」
      - 「列見出しの境目 k」
      - 「列見出しの最後の列より右」
      - 「縦スクロールバーの下矢印・つまみ」
    - 500 ms 以内に入力画面（編集ウィンドウを親とする新しいダイアログ。タイトルは追加と同じ）が現れたかを出力し、現れたら Esc で閉じる。入力画面の各入力欄が空欄・既定値か（追加用か、編集用か）も出力する。
    - 一覧の行数・選ばれている行・各列の幅を出力する。
  - 一時フォルダに検証用の `.canprj` を作る。
    - `frames100.canprj`: ECU 01 に 100 件のフレーム（ID 0x100〜0x163、DLC 8、周期 100 ms）。
    - `ecu0.canprj`: `EcuCount=0`。
    - `frames30.canprj`: ECU 01 に 30 件（スクロールバーが出る件数）。

---

## Phase 2: User Story 1 - 一覧のどこをダブルクリックしてもフレームを追加できる (Priority: P1) 🎯 MVP

**Goal**: 送信フレーム一覧の列見出し・行・行の無い空白のどこをダブルクリックしても、「追加」ボタンと同じ追加の入力画面が開く

**Independent Test**: フレームが 0 件の ECU で一覧の中央をダブルクリックして追加の入力画面が開き、OK で 1 行加わる。行の上・列見出しでも追加が開く（quickstart.md 手順 2-1〜2-5）

- [X] T002 [US1] `MFCApplication1/FrameEditDlg.h/.cpp` に、追加できる条件と一覧からの追加の行き先を加える（research.md 決定3、data-model.md）。
  - `bool CanAdd() const`: `SelectedNodeId() > 0 && m_list.GetItemCount() < CanSim::kMaxFramesPerNode` を返す。
    - 条件は「ECU が選ばれていて、その ECU のフレームが 100 件未満」。
  - `UpdateButtons` の「追加」ボタンの `EnableWindow` を `CanAdd()` に置き換える。
  - `void AddFromList()`:
    - `CanAdd()` が偽なら何もせずに戻る。メッセージも出さない。
    - 真なら `OnAdd()` を呼ぶ。
  - コメントは ASCII（英語）で、「feature 013: double-click in the frame list = Add (FR-001, FR-002)」の旨を書く。（spec FR-002、FR-005）
- [X] T003 [US1] `MFCApplication1/FrameEditDlg.cpp` の `OnListDoubleClick`（`NM_DBLCLK`）の中身を `OnEdit()` から `AddFromList()` に変える（research.md 決定1・4・5）。
  - 行の上（`iItem` ≥ 0）でも空白（`iItem` = -1）でも同じく `AddFromList()` を呼ぶ。行の選択には触れない。
  - 機能 006 の「一覧のダブルクリック = 編集」を置き換える旨をコメントに書く。
  - `OnEdit` は「編集」ボタンからだけ呼ばれるようになるが、中身は変えない。（spec FR-001、FR-003）（依存: T002）
- [X] T004 [US1] `MFCApplication1/FrameEditDlg.h/.cpp` に、列見出しの派生クラス `CFrameListHeader : public CHeaderCtrl` を加え、一覧の列見出しをサブクラス化する（research.md 決定2、data-model.md）。
  - `FrameEditDlg.h` に `const UINT WM_APP_FRAME_LIST_ADD = WM_APP + 2;` を定義する（編集ウィンドウ内だけで使う。`WM_APP_FRAME_EDITOR_CLOSED` = `WM_APP + 1` と重ならない値）。
  - `CFrameListHeader::OnLButtonDblClk` は既定の処理（`CHeaderCtrl::OnLButtonDblClk`）を呼ばない。境目のダブルクリックによる列幅の自動調整を行わないためである。
    - `GetParent()`（一覧）の `GetParent()`（編集ウィンドウ）に、`::PostMessage(hwnd, WM_APP_FRAME_LIST_ADD, 0, 0)` を送るだけにする。
    - 送り先の `HWND` が無効なら何もしない。
  - `CFrameEditDlg` にメンバ `CFrameListHeader m_header;` を追加する。`OnInitDialog` で列の追加の後に `m_header.SubclassWindow(m_list.GetHeaderCtrl()->GetSafeHwnd())` を呼ぶ。
  - `ON_MESSAGE(WM_APP_FRAME_LIST_ADD, OnListAddRequest)` を追加する。`LRESULT OnListAddRequest(WPARAM, LPARAM)` は `AddFromList()` を呼んで 0 を返す。
  - `OnLButtonDown` / `OnMouseMove` / `OnLButtonUp` は上書きしない。列幅のドラッグでの変更は今までどおり。
  - 決定2の注意をコメントに書く（列見出しの通知は一覧に届き、最後の列より右では通知が出ない。モーダルを列見出しの処理の中で開かないため Post にする）。（spec FR-001、FR-005）（依存: T002）
- [X] T005 [US1] Debug|x64 をビルドし、実機で quickstart.md 手順 2-1〜2-6・2-9・2-10 を確認する（依存: T001, T003, T004）。
  - 2-1: 0 件の一覧の中央 → 追加の入力画面。OK で 1 行目に加わって選ばれる。
  - 2-2: 行の上 → 追加の入力画面が開き、入力欄は空欄・既定値（編集用ではない）。キャンセルで行数は変わらず、その行が選ばれたまま。
  - 2-3: 最後の行より下の空白 → 追加。
  - 2-4: 列見出しの項目・境目・右の空き → どれも追加。境目のダブルクリックの前後で列幅は変わらない。
  - 2-5: 境目のドラッグで列幅が変わる。
  - 2-6: `frames30.canprj` で縦スクロールバーの下矢印・つまみをダブルクリック → 入力画面は開かない。
  - 2-9: ボタンからの追加と、ダブルクリックからの追加を同じ値で行い、プロジェクト保存後のフレームの行が同じ形になる。
  - 2-10: 「編集」ボタンは選んだ行の編集の入力画面を開く。
  - 各場所を 10 回ずつダブルクリックして、10 回とも期待どおりになることを確認する。（SC-001、SC-002、SC-004）

**Checkpoint**: US1 の受け入れシナリオ 1〜6 を満たす

---

## Phase 3: User Story 2 - 追加できないときはダブルクリックしても何も起きない (Priority: P2)

**Goal**: 「追加」ボタンが無効のとき（ECU なし・100 件）は、一覧のどこをダブルクリックしても入力画面が開かない

**Independent Test**: `frames100.canprj` の ECU 01 で一覧の行・空白・列見出しをダブルクリックしても何も開かない（quickstart.md 手順 2-7・2-8）

- [X] T006 [US2] 実機で quickstart.md 手順 2-7・2-8 を確認する（依存: T005）。
  - 2-7: `frames100.canprj` の ECU 01 で、次をそれぞれ 10 回ダブルクリックする。
    - 行・空白（スクロールして最後の行の下）・列見出しの項目・境目
    - 入力画面（追加・編集とも）もメッセージボックスも開かない。
  - 2-8: `ecu0.canprj` で一覧・列見出しをダブルクリックしても何も開かない。
  - コードの変更は T002 の `CanAdd()` で足りる見込み。満たさない場合は原因を報告する。（SC-003、spec FR-002）

**Checkpoint**: SC-003 を満たす

---

## Phase 4: Polish & Cross-Cutting Concerns

- [X] T007 [P] `CLAUDE.md` の `FrameEditDlg.h / .cpp` の説明に追記する（憲章 V）。
  - 送信フレーム一覧は、列見出し・行・空白のどこをダブルクリックしても「追加」と同じ動作になる（機能 013）。追加できないとき（ECU なし・100 件）は何もしない。
  - 列見出しは `CFrameListHeader` でサブクラス化して、`WM_APP_FRAME_LIST_ADD` を編集ウィンドウに Post する。
  - 行のダブルクリックで編集を開く動作（機能 006）は無くなり、編集は「編集」ボタンから行う。
- [X] T008 Debug/Release × x64/x86 の全 4 構成で、次を確認する（依存: T005, T006）。
  - 警告なしでビルドできる。
  - `checks.exe` の全項目が PASS（001〜012）。
  - サンプルの CSV が基準と一致する（004 SC-009、spec SC-005）。

---

## Dependencies & Execution Order

- **Setup**: T001（GUI 確認の前提。実装と並行可）
- **US1**: T002 → T003・T004（どちらも T002 の後。同じファイルを触るため順に行う）→ T005
- **US2**: T006（T005 の後。コードは US1 の `CanAdd()` を使う）
- **Polish**: T007 は並行可、T008 は最後

### Parallel Opportunities

- T001（検証スクリプト）は T002〜T004 と並行可
- T007（文書）は実装と並行可
- T003 と T004 は同じファイル（`FrameEditDlg.cpp`）を変更するため並行にしない

## Implementation Strategy

1. **MVP = US1**（T001〜T005）: 一覧のどこをダブルクリックしても追加が開く。
2. US2 で追加できないときに何も起きないことを確認する。
3. Polish で文書・4 構成ビルド。

## Notes

- シミュレーションエンジン・CSV・`.rc`・`CanSim/` は変更しない。
- `FrameEditDlg.*` の追加コードは ASCII のみ（CLAUDE.md のエンコーディング方針）。
- 自動操作の注意: マウスのダブルクリックは、OS のダブルクリック時間内に 2 回のクリックを同じ位置へ送る。画面の座標は、一覧のクライアント座標から `ClientToScreen` で求める。

## 実装結果（2026-10-01）

- 変更: `MFCApplication1/FrameEditDlg.h/.cpp`。
  - 追加したもの: `CFrameListHeader`、`WM_APP_FRAME_LIST_ADD`、`CanAdd()`、`AddFromList()`、`OnListAddRequest`。
  - `OnListDoubleClick` の行き先を `AddFromList()` に変えた。
  - あわせて `CLAUDE.md` を更新した。`CanSim/`・`.rc` は変更なし。
- ビルド: Debug/Release × x64/x86 の 4 構成とも、警告・エラーなしでビルドできた。
- checks.exe: 全 132 項目 PASS。基準 CSV（runner.exe で各サンプルを 10 秒実行したもの。`CanSim/` は HEAD から変更なし）との比較でも一致した。
- GUI（Debug|x64、拡大率 125%、ダブルクリック時間 500 ms）: scratchpad の `t013/gui.ps1` で確認した。各場所は 30 回ずつくり返し（最終回は 10 回）、すべて合格した。
  - 2-1: フレーム 0 件の一覧の中央で追加の入力画面（タイトル「データフレームの追加」、空欄、オフセット 0）が開いた。OK で 1 行目に加わり、その行が選ばれた。
  - 2-2: 行の上でも追加の入力画面が開いた（編集は開かない）。キャンセル後も行数は変わらず、その行が選ばれたままだった。
  - 2-3: 最後の行より下の空白で、追加の入力画面が開いた。
  - 2-4: 列見出しの項目・境目・最後の列より右で、どれも追加の入力画面が開いた。境目のダブルクリックの前後で列幅は変わらなかった。
  - 2-5: 境目のドラッグで列幅が変わり（175 → 205）、入力画面は開かなかった。
  - 2-6: `frames30.canprj` で縦スクロールバーの下矢印をダブルクリックした。先頭行が 10 行進み（スクロールバーに当たっていることを確認）、入力画面は開かなかった。つまみの上の空きでも開かなかった。
  - 2-9: 「追加」ボタンの入力画面は、ダブルクリックのものと同じだった。保存したプロジェクトのフレームの行は、名前・ID 以外が同じ形だった。
  - 2-10: 「編集」ボタンは、選んだ行の値の入った編集の入力画面（タイトル「データフレームの編集」）を開いた。
  - 2-7: `frames100.canprj` では「追加」ボタンが無効だった。行・列見出しの項目・境目・最後の列より右の空きのどれでも、入力画面もメッセージも出なかった。
  - 2-8: `ecu0.canprj` では、一覧の中央・列見出しのどちらでも何も開かなかった。
- 自動操作の注意: 最初の 10 回の試行で、列見出しの右の空きが 1 回だけ開かなかった。その後の 30 回 × 2 回の試行では再現しなかった。そのため、自動操作の入力のタイミング（前面化の直後のクリックなど）によるものとみている。
- 手操作の確認（ユーザー、2026-10-01）: データフレーム編集の気になる所をすべてダブルクリックした。一覧の外では追加が開かず、一覧の中では追加が開いた。
