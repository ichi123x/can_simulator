---

description: "Task list for feature 011-ecu-feedback-frames"
---

# Tasks: ECU ダブルクリック時のグレー表示と、表示エリアの枠

**Input**: Design documents from `/specs/011-ecu-feedback-frames/`

**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/, quickstart.md（すべて存在）。機能 003・009・010 の実装が完了していること。

**Tests**: spec.md は自動テストを要求していない。plan.md の方針に従い、構成図の枠の位置は `tools/cansim_runner/checks.cpp` で検証し、グレー表示の
色・時間・受け付けない操作・枠の見た目は GUI の自動操作（実マウス入力、画面の画素の色、時刻の計測、スクリーンショット）で確認する。

**Organization**: spec.md のユーザーストーリー（US1 ダブルクリックした ECU が 0.5 秒グレー P1、US2 受け付けないダブルクリックではグレーにしない P2、
US3 構成図とログ表示エリアの枠 P2）ごと。構成図の枠の配置は checks で先に検証できるので Foundational に置く。

## Format: `[ID] [P?] [Story] Description`

## Path Conventions

コアは `MFCApplication1/CanSim/`（ASCII のみ）。UI は `MFCApplication1Dlg.*`。`.rc` の変更はない。GUI 検証スクリプトはリポジトリ外（セッションの
scratchpad の `gui.ps1`）。

---

## Phase 1: Setup

- [X] T001 GUI 検証スクリプト（scratchpad の `gui.ps1`、リポジトリ外）に操作を追加する: `pixel x y`（96 dpi 基準のクライアント座標を `ScaleDpi` で換算し
  `ClientToScreen` した点の画面の色を `RRGGBB` で出力）、`waitcolor x y RRGGBB ms`（その点が指定の色になるまで 5 ms 間隔で待ち、`mark`（最後のクリック）
  からの経過 ms を出力）、`waitnotcolor x y RRGGBB ms`（指定の色でなくなるまで待ち、同様に出力）

---

## Phase 2: Foundational (Blocking Prerequisites)

- [X] T002 `MFCApplication1/CanSim/BusDiagramLayout.h/.cpp` に構成図の枠を追加する（research.md 決定5、data-model.md）: 定数 `kDiagramFrameInset = 4`、
  `BusDiagramLayout` にメンバ `DiagramRect Frame`、`ComputeBusDiagramLayout` で ECU 数によらず `Frame = (4, 4) - (kDiagramWidth - 4, kDiagramHeight - 4)`
  （= (4, 4) - (1136, 212)）を設定する。ASCII のみ
- [X] T003 [P] `tools/cansim_runner/checks.cpp` に `CheckFeature011()` を追加し `main` から呼ぶ（quickstart.md 手順 1）: ECU 数 0〜16 × DPI 96/120/144 で、
  `Frame` を `ScaleToDpi` で換算した矩形が構成図の領域 `[0, ScaleToDpi(1140)) × [0, ScaleToDpi(216))` の内側にあり、枠の線（太さ `max(1, ScaleToDpi(1))`）
  の内側に、換算後のバス線・「CAN BUS」の文字の範囲（x 14〜118、y 80〜106）・各 ECU の箱・各接続線がすべて 1 px 以上離れて収まる。既存の 001〜010
  の項目が PASS のまま（依存: T002）。補足（動作に影響なし）: 文字の範囲は実装では描画枠全体（x 14〜158、y 76〜106）で確かめた（quickstart.md 手順 1 の補足）

**Checkpoint**: checks.exe の 011 項目がすべて PASS

---

## Phase 3: User Story 1 - ダブルクリックした ECU が 0.5 秒グレーになる (Priority: P1) 🎯 MVP

**Goal**: 受け付けた ECU のダブルクリックのたびに、その ECU だけが約 0.5 秒グレーになり、機能 010 の動作は変わらない

**Independent Test**: `sample_appendix_a` を読み込み「ECU 03」をダブルクリックして、ECU 03 だけが 0.1 秒以内にグレーになり 0.4〜0.7 秒で戻る
（quickstart.md 手順 2-1）

- [X] T004 [US1] `MFCApplication1/MFCApplication1Dlg.h/.cpp` にグレー表示の状態を実装する（research.md 決定1・3、data-model.md）: 無名名前空間に
  `kGrayTimerId = 3`・`kGrayMs = 500`、メンバ `int m_grayEcu = 0`（「0 = なし」）、`void StartGray(int ecu)`（`m_grayEcu = ecu`、`SetTimer(kGrayTimerId, kGrayMs, nullptr)`
  （張り直しで数え直す）、`InvalidateRect(GetDiagramRect(), FALSE)`、`UpdateWindow()`）、`void ClearGray()`（`KillTimer(kGrayTimerId)`、`m_grayEcu` が 0 でなければ
  0 にして構成図を無効化）。`OnLButtonDblClk` で「実行中なら戻る」「`FindEcuAt` が 0 なら戻る」の後、機能 010 の分岐（開く・前面に出す・閉じて待つ・
  待ちの間に置き換える）より前に `StartGray(ecu)` を呼ぶ。`OnTimer` の先頭（切替タイマーの分岐の前）で `nIDEvent == kGrayTimerId` なら `ClearGray()` して
  戻る（spec FR-001、FR-002、FR-004、FR-005）
- [X] T005 [US1] `MFCApplication1/MFCApplication1Dlg.cpp` の `DrawBusDiagram` で、`ecu.Number == m_grayEcu` の ECU をグレーの配色で描く（research.md 決定4、
  data-model.md「配色」）: 塗り `RGB(205,205,205)`、枠 `RGB(128,128,128)`（太さ 1 を換算）、文字 `RGB(96,96,96)`（その ECU の `DrawText` の前に
  `SetTextColor`、後で `kTextColor` に戻す）。送信・受信の強調より優先する。ほかの ECU・接続線は変えない（spec FR-003）（依存: T004）
- [X] T006 [US1] `MFCApplication1/MFCApplication1Dlg.cpp` で取り消しを実装する（research.md 決定3）: `SetEcuCount` で `m_grayEcu > 新しい ECU 数` なら
  `ClearGray()`、`OnSimStart` の計算の前（`ClearLog()` の近く）で `ClearGray()`、`OnDestroy` で `KillTimer(kGrayTimerId)`（spec FR-007、Edge Cases）（依存: T004）
- [X] T007 [US1] Debug|x64 をビルドし、実機（一時フォルダのサンプル、実マウス入力）で quickstart.md 手順 2-1〜2-4・2-6 を確認する: 色は ECU の箱の左端から
  8 px・上下の中央の点で読む（`pixel`・`waitcolor`・`waitnotcolor`）。2-1 は `waitcolor` が 100 ms 以内、戻るまで 400〜700 ms、ほかの ECU の点の色が
  変わらない。2-3 はグレーが戻る時刻と `waiteditor` の時刻の差が 100 ms 以内、閉じてから開くまで 400〜700 ms（機能 010 と同じ）。2-4 は 2 回目の
  ダブルクリックの直後に ECU 03 が通常色、ECU 05 が 2 回目から約 0.5 秒グレー（SC-001、SC-002、SC-004）（依存: T001, T005, T006）

**Checkpoint**: US1 の受け入れシナリオ 1〜5 を満たす

---

## Phase 4: User Story 2 - 受け付けないダブルクリックではグレーにならない (Priority: P2)

**Goal**: グレー表示を「受け付けた」合図に限る

**Independent Test**: 実行中に ECU をダブルクリックしてグレーにならない（quickstart.md 手順 2-5）

- [X] T008 [US2] 実機で quickstart.md 手順 2-5 を確認する: `sample_startup`（スピード 0）の実行中の ECU のダブルクリック、バス線・余白・ログ表示エリアの
  ダブルクリック、ECU の 1 回クリック、「追加」の入力画面の表示中の別 ECU のダブルクリックで、対象の ECU の点がグレー（`CDCDCD`）にならない（600 ms 監視）。
  実行中は、グレー表示のコードが実行中の分岐より後にあること（T004）で満たされる（SC-003、spec FR-006）（依存: T004）

**Checkpoint**: SC-003 を満たす

---

## Phase 5: User Story 3 - 構成図とログ表示エリアに枠が付く (Priority: P2)

**Goal**: 2 つの表示エリアを枠で囲み、画面の区切りをはっきりさせる

**Independent Test**: 起動直後のスクリーンショットで 2 つの枠が表示されている（quickstart.md 手順 2-7）

- [X] T009 [US3] `MFCApplication1/MFCApplication1Dlg.cpp` の `DrawBusDiagram` で、白で塗った直後（中身を描く前）に `layout.Frame` を `ScaleRect` で換算した
  矩形の線を描く（research.md 決定5・7）: ペン `RGB(160,160,160)`・太さ `max(1, ScalePx(1, dpi))`、ブラシ `NULL_BRUSH`（塗らない）（spec FR-008、FR-010、
  FR-011）（依存: T002）
- [X] T010 [US3] `MFCApplication1/MFCApplication1Dlg.h/.cpp` にログ表示エリアの枠を追加する（research.md 決定6）: ログ表示エリアの定数の隣に
  `kLogFrameGapPx = 3`、`void DrawLogFrame(CDC& dc)` で `GetLogRect()` を `ScaleForDialog(this, kLogFrameGapPx)` だけ外側に広げた矩形に、T009 と同じ色・太さ
  の線を描く（塗らない）。`OnPaint` の `DrawLogView(dc)` の後で呼ぶ。枠はログの書き換え（`GetLogTextRect` の無効化）では消えない位置にある
  （spec FR-009、FR-010）
- [X] T011 [US3] 実機で quickstart.md 手順 2-7 を確認する: ECU 数 0（`zero.canprj`）・6（`sample_appendix_a`）・16（`sample_16ecu`）、`sample_16ecu` の実行中
  （フレーム表示 ON）、ログをスクロールした状態のスクリーンショットで、2 つの枠が同じ位置に欠けずに表示され、ECU・バス線・「CAN BUS」の文字・ログの
  文字・スクロールバーと重ならない。枠の線上の点の色（`pixel`）が `A0A0A0` であることも確かめる（SC-005、spec FR-010、FR-012）（依存: T009, T010）

**Checkpoint**: SC-005 を満たす

---

## Phase 6: Polish & Cross-Cutting Concerns

- [X] T012 [P] `CLAUDE.md` を更新する: 受け付けた ECU のダブルクリックでその ECU を 500 ms グレー表示（`m_grayEcu`、タイマー ID 3、最後の 1 つだけ、
  ECU 数の変更・開始で取り消し）、構成図の枠（`BusDiagramLayout::Frame`、領域の 4 px 内側）とログ表示エリアの枠（3 px 外側）（憲章 V）
- [X] T013 Debug/Release × x64/x86 の全 4 構成で警告なしのビルドと `checks.exe` の全項目 PASS を確認し、サンプルの CSV が基準と一致すること
  （004 SC-009）を確かめる（依存: T003, T007, T008, T011）

---

## Dependencies & Execution Order

- **Setup**: T001（GUI 確認の前提。実装と並行可）
- **Foundational**: T002 → T003（checks）、T002 → T009
- **US1**: T004 → T005、T004 → T006 → T007
- **US2**: T008（T004 の後）
- **US3**: T009、T010 → T011
- **Polish**: T012 は並行可、T013 は最後

### Parallel Opportunities

- T001（検証スクリプト）は T002〜T006 と並行可
- T003（checks.cpp）は T004〜T006（UI）と並行可
- T012（文書）は実装と並行可

## Implementation Strategy

1. **MVP = US1**（T001〜T007）: 受け付けた ECU のダブルクリックでその ECU が 0.5 秒グレーになる。
2. US2 で「グレーにならない」場合の確認、US3 で 2 つの枠。
3. Polish で文書・4 構成ビルド。

## Notes

- `CanSim/` の変更に日本語を含めない（ASCII のみ）。
- シミュレーションエンジン・CSV・`.rc`・当たり判定（`FindEcuAt`）は変更しない。
- 機能 010 の切替タイマー（ID 2）・再生タイマー（ID 1）の処理は変えない。

## 実装結果（2026-09-30）

- checks.exe: 011 の 2 項目を含む全項目 PASS（サンプルの CSV は基準と一致）。4 構成（Debug/Release × x64/x86）とも警告なし。
- GUI（Debug|x64、拡大率 125% の実機、実マウス入力、画面の色の読み取り）:
  - 2-1〜2-3: ダブルクリックからグレーになるまで 37 ms（開く）・16 ms（前面に出す）・18 ms（切り替え）。戻るまで 516〜569 ms。ほかの ECU の色は変わらない。切り替えでは、グレーが戻ってから 2 ms で新しい編集ウィンドウが開いた（閉じてから開くまで 571 ms）。
  - 2-4: 2 回目のダブルクリックの直後に ECU 03 は通常色、ECU 05 がグレー。ECU 05 は 2 回目から 519 ms で戻った。
  - 2-5: バス線・「CAN BUS」の文字・接続線・余白・ログ表示エリアのダブルクリック、ECU の 1 回クリック、入力画面の表示中、実行中のいずれでもグレーの ECU は 0。
  - 2-6: ECU 06 のダブルクリックの直後に ECU 数を 2 → 6 にすると、ECU 06 は通常色で表示された。
  - 2-7: 起動直後に、構成図の枠とログ表示エリアの枠（左・上）の画素がどちらも `A0A0A0`。ECU 数 0・6・16、実行中（フレーム表示 ON）、ログをスクロールした状態のウィンドウ画像で、2 つの枠は同じ位置に欠けずに表示され、中身と重ならない。
- analyze の指摘 U1 は、今の設計のまま（待ちの間に置き換えた場合、グレーは最後のダブルクリックから 0.5 秒続く）とした。

## Phase 7: Convergence

- [X] T014 ログ表示エリアの位置（96 dpi で左 150・上 260・850×260 px、スクロールバー込み）と枠の間隔 `kLogFrameGapPx` = 3 を、`MFCApplication1Dlg.cpp` の無名名前空間から `MFCApplication1/CanSim/` の UI 非依存な定数・関数（例: `BusDiagramLayout.h` に `LogViewRect()` と `LogFrameRect()`）へ移し、ダイアログの `GetLogRect`・`DrawLogFrame` はそれを換算して使うようにしたうえで、`tools/cansim_runner/checks.cpp` の `CheckFeature011` に、DPI 96/120/144 で換算したログ表示エリアの枠が、灰色の領域とスクロールバーから 1 px 以上外側にあり、構成図の下端（`kDiagramHeight`）より下にあることの検証を追加する（見た目・位置は変えない。ASCII のみ） per FR-011, SC-005 (partial)

  T014 の結果: checks.exe に 011 の 2 項目（ログ表示エリアの枠が DPI 96/120/144 で灰色の領域・スクロールバー・構成図と重ならない、ログ表示エリアの位置が 009 と同じ）を追加し全項目 PASS。4 構成とも警告なし。実機（125%）で枠と灰色の領域の画素の位置が変更前と同じ。
