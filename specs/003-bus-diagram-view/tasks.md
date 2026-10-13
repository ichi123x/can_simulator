---

description: "Task list for feature 003-bus-diagram-view"
---

# Tasks: CAN バス構成図の表示（ダイアログ表示部）

**Input**: Design documents from `/specs/003-bus-diagram-view/`

**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/ui-layout.md, quickstart.md（すべて存在）

**Tests**: `spec.md` は自動テストを要求していない。ただし plan.md の方針に従い、座標計算（`BusDiagramLayout`）は `tools/cansim_runner/checks.cpp` の検証項目として追加する。描画と初期サイズは `quickstart.md` の目視手順で確認する。

**Scope**: 本機能は「現在の ECU 数を描く表示部品」に絞る。ECU 数の入力手段（セッティング画面・プロジェクト読込み）、メニュー、既存コントロールの削除、ダイアログ全体の白化、ECU 数の初期値の設定は機能 004 のタスクで扱う。

**Organization**: タスクは spec.md のユーザーストーリー（US1〜US3）ごとにグループ化する。

## Format: `[ID] [P?] [Story] Description`

- **[P]**: 並行実行可能（異なるファイル、未完了タスクへの依存なし）
- **[Story]**: 対応するユーザーストーリー
- 各タスクに正確なファイルパスを含める

## Path Conventions

既存の単一 VS プロジェクト `MFCApplication1/`。幾何計算は `MFCApplication1/CanSim/`（ASCII のみ、MFC 非依存）、描画とリソースは `MFCApplication1Dlg.*` と `MFCApplication1.rc`。

---

## Phase 1: Setup

**Purpose**: 新規ファイルの雛形とプロジェクト登録

- [X] T001 `MFCApplication1/CanSim/BusDiagramLayout.h` と `BusDiagramLayout.cpp` を作成し（ASCII のみのコメント、`pch.h` 経由の include は既存 `CanSim/*.cpp` と同じ流儀）、`MFCApplication1/MFCApplication1.vcxproj` の `ClInclude`/`ClCompile` と`MFCApplication1.vcxproj.filters` に登録する（全4構成で有効）。中身は空の名前空間 `CanSim` のみでよい

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: US1・US3 が共有する座標計算。data-model.md の値型と不変条件を実装する

- [X] T002 `MFCApplication1/CanSim/BusDiagramLayout.h/.cpp` に data-model.md の `DiagramRect`、`EcuPlacement`（Number, OnTop, Box, LineX, LineY1, LineY2）、`BusDiagramLayout`（BusLine, Ecus）と`ComputeBusDiagramLayout(int ecuCount)` を実装する。寸法は research.md 決定3: 図領域 1140×216 px、バス線 y=110 付近・x=10〜1130、ECU 箱 96×34 px、接続線長約 40 px、隣接 ECU の中心間隔56 px（ECU n の中心 x = 150 + (n-1)×56。ECU 01 は x=150、ECU 16 は x=990 で、バス両端から 140 pxの余裕を空け、見出し「CAN BUS」＝x≈14〜118 と接続線を重ねない）。「ecuCount は 0〜16 にクランプする（範囲外は 0 未満→0、17 以上→16）」「Ecus.size() == clamp(ecuCount)」「奇数=OnTop true（バス線より上）、偶数=false（下）」「全 Box が図領域（1140×216）内」「ECU n の座標は ecuCount によらず同一（FR-009）」
- [X] T003 [P] `tools/cansim_runner/checks.cpp` に `ComputeBusDiagramLayout` の検証項目を追加する: ECU 数 0/1/6/16 の個数一致、奇偶の上下、中心間隔 56 px 一定、ECU 01 の LineX=150 かつ 16 ECU 時のECU 16 の LineX=990（左右対称）、全 LineX が見出し右端（118）より右、全 Box が 1140×216 内、ECU 数を変えても ECU 1〜n の座標が不変、-1 と 17 のクランプ（依存: T002）

**Checkpoint**: `checks.exe` をビルド・実行して追加項目がすべて PASS（quickstart 手順1）

---

## Phase 3: User Story 1 - 現在の ECU 数どおりに CAN バス構成図を見る (Priority: P1) 🎯 MVP

**Goal**: ダイアログ上に白背景の構成図を描画し、現在の ECU 数と同数の ECU を上下交互に表示する

**Independent Test**: `m_ecuCount` を 6・16 にして、ECU の個数・ラベル・配置が ECU 数と一致する（quickstart 手順2〜4）

- [X] T004 [US1] `MFCApplication1/MFCApplication1Dlg.h/.cpp` に `int m_ecuCount`（初期値 2）と`void SetEcuCount(int)`（0〜16 にクランプし、図領域を `InvalidateRect` する）を追加し、`OnPaint` に構成図の描画を実装する。図領域（ダイアログクライアント左上、1140×216 px に DPI/96 を掛けたもの）をメモリ DC 経由で `FillSolidRect(RGB(255,255,255))` で塗り（FR-001）、`ComputeBusDiagramLayout(m_ecuCount)` の結果から、バス線（黒・4 px）、見出し「CAN BUS」（太字、バス線の左上）、ECU の角丸箱（奇数=塗り `RGB(225,213,231)`／枠 `RGB(150,115,166)`、偶数=塗り`RGB(255,230,204)`／枠 `RGB(215,155,0)`）、箱中央の「ECU NN」（NN は2桁ゼロ埋め）、接続線を描く。描画文字列は英字のみ。アイコン描画等の既存 `OnPaint` の分岐は壊さない。`SetEcuCount` は、値が変わらない場合も安全に呼べること。ECU 数の変更がキャンセル・失敗した場合に呼び出し側が`SetEcuCount` を呼ばない、という約束（FR-010）をコメントに明記する（依存: T002）

**Checkpoint**: `m_ecuCount` の初期値を一時的に 6・16 に書き換えて再ビルドし、ECU 数が一致する

---

## Phase 4: User Story 2 - 初期表示で全16 ECU が見切れずに収まる (Priority: P1)

**Goal**: 起動直後のダイアログサイズのまま、16 ECU がスクロール・リサイズなしで見える

**Independent Test**: ECU 数=16 で、初期サイズのままで全 ECU が欠け・重なりなく見える（quickstart 手順3）

- [X] T005 [US2] `MFCApplication1/MFCApplication1.rc` の `IDD_MFCAPPLICATION1_DIALOG` を、図領域が収まるクライアント720×152 DLU（research.md 決定4。図領域 1140×216 px ＋ 左右・下の余白 10 px）に変更する。既存コントロールの削除・再配置は本タスクでは行わない（機能 004 が削除する）。004 の削除が終わるまでは、既存コントロールが図の上に重なって見えるため、目視確認は 004 のコントロール削除と同じ作業で行う。**`.rc` は UTF-16LE（BOM 付き、CRLF）のため、Read/Edit ツールを使わず PowerShell の`Get-Content -Encoding Unicode` と `[System.IO.File]::WriteAllText(path, text, (New-Object System.Text.UnicodeEncoding($false,$true)))` で読み書きすること**（CLAUDE.md）
- [X] T006 [US2] 実機（Debug|x64）で `m_ecuCount` を一時的に 16 にし、初期サイズのまま ECU 01〜16 が欠け・重なりなく収まり、ECU 16 の右端がダイアログ内に入っていることを確認する。あわせて参考画像 2 枚（`Doc/CANバスイメージ(最大時 ECU=16).jpg`、`(最小時 ECU=0).jpg`）と見比べ、見出し「CAN BUS」の文字がECU 01 の接続線と重ならないことを確認する。収まらない場合は T005 の寸法または T004 のスケールを調整する（依存: T004, T005。既存コントロールの重なりは 004 で解消）

**Checkpoint**: SC-001・SC-005・SC-006 を目視で満たす

---

## Phase 5: User Story 3 - ECU が0個のときはバスだけを表示する (Priority: P2)

**Goal**: ECU 数が 0 のとき「CAN BUS」の見出しとバス線のみ

**Independent Test**: ECU 数=0 で ECU も接続線も表示されない（quickstart 手順1）

- [X] T007 [US3] `MFCApplication1/MFCApplication1Dlg.cpp` の `OnPaint` で、`m_ecuCount == 0` のとき見出しとバス線のみを描画し、ECU と接続線を一切描かないことを確認する。不足があれば T004 の描画ループを修正する。`SetEcuCount(0)` → `SetEcuCount(3)` → `SetEcuCount(0)` の切り替えで、残像が出ないことも確認する（依存: T004）

**Checkpoint**: SC-003 を満たす

---

## Phase 6: Polish & Cross-Cutting Concerns

- [X] T008 quickstart.md 手順6を確認する: Windows をダークテーマにして表示部が白のままであること（SC-004）。ダークテーマで背景が変わる場合は T004 の塗りつぶしを修正する（依存: T006）結果（2026-09-29、利用者が実機で確認）: 通常モード・ダークモードのどちらでも、メニュー行より下の表示部は白のままで、16 ECU の構成図が表示された。
- [X] T009 [P] `CLAUDE.md` のアーキテクチャ節を更新する: `CanSim/BusDiagramLayout.h/.cpp` の追加、`MFCApplication1Dlg` が構成図の描画（GDI）と `m_ecuCount`／`SetEcuCount` の保持を担うこと、ダイアログの初期サイズ変更。UI 固有ロジックの制約（グルーコード方針）との整合を保つ（憲章 V）
- [X] T010 Debug/Release × x64/x86 の全4構成で `msbuild MFCApplication1\MFCApplication1.sln` が警告 C4819 なしで通ることを確認する（憲章 II）
- [X] T011 既存機能への回帰確認: `checks.exe` の全項目が PASS のままであること、既存の実行機能（004 がコントロールを削除するまでの間）で `sample_appendix_a.cfg` の結果 CSV が従来どおり出力されることを確認する（依存: T010）結果: `checks.exe` は 001・002・003 の全項目 PASS。実行ハンドラ（`OnBnClickedButtonRun`）は変更していない。

---

## Dependencies & Execution Order

- **Setup (T001)** → **Foundational (T002, T003)** → 各ユーザーストーリー
- **US1 (T004)**: T002 完了後。MVP
- **US2 (T005, T006)**: T005 は `.rc` のみで T001〜T004 と独立に着手可能。T006 は T004・T005 後
- **US3 (T007)**: T004 完了後
- **Polish**: T008 は T006 後、T009 は並行可、T010→T011
- **機能 004 との関係**: 既存コントロールが図に重なるため、T006・T011 の実機確認は、機能 004 のコントロール削除（004 US5）と同じ作業で行うのが効率的。ECU 数の入力（セッティング）は 004 US1

### Parallel Opportunities

- T003（checks.cpp）は T002 後に T004 と並行可
- T005（.rc 編集）は他と独立
- T009（CLAUDE.md）は実装と並行可

## Implementation Strategy

1. **MVP = US1**: T001〜T004 で、`m_ecuCount` どおりの構成図が描画される状態を先に作る。
2. US2（T005〜T006）で初期サイズを整え、US3（T007）で 0 個表示を確認する。
3. Polish で SC-004・ドキュメント・4構成ビルド・回帰を確認する。
4. 機能 004 の実装で、ECU 数の入力・メニュー・コントロール削除を行い、003 の目視確認を完了する。

## Notes

- `CanSim/` の新規ソースに日本語コメント・リテラルを含めない（ASCII のみ）。
- `.rc` は必ずエンコーディングを明示して編集する。
- 各チェックポイントで quickstart.md の該当手順を実施する。
