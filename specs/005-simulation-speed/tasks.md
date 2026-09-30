---

description: "Task list for feature 005-simulation-speed"
---

# Tasks: シミュレーションのスピード設定

**Input**: Design documents from `/specs/005-simulation-speed/`

**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/, quickstart.md（すべて存在）。
機能 004 の実装（メニュー、`.canprj`、セッティング、フレーム表示の再生）が完了していること。

**Tests**: spec.md は自動テストを要求していない。plan.md の方針に従い、`CanSim/` のコア（`Speed` の
読み書き、`ReplayClock`）は `tools/cansim_runner/checks.cpp` に検証項目を追加する。所要時間と表示は
`quickstart.md` の GUI 手順（画面操作の自動化とストップウォッチ）で確認する。

**Organization**: spec.md のユーザーストーリー（US1〜US5）ごとにグループ化する。時計と区間抽出
（`ReplayClock`）、`Speed` の設定値は全ストーリーの前提なので Foundational に置く。

## Format: `[ID] [P?] [Story] Description`

- **[P]**: 並行実行可能（異なるファイル、未完了タスクへの依存なし）
- **[Story]**: 対応するユーザーストーリー（US1〜US5）

## Path Conventions

コアは `MFCApplication1/CanSim/`（ASCII のみ、MFC 非依存）、UI は `MFCApplication1Dlg.*`／`SettingsDlg.*`／
`MFCApplication1.rc`。`.rc` は UTF-16LE（BOM 付き、CRLF）のため Read/Edit ツールを使わず、エンコーディングを
明示して読み書きする（CLAUDE.md）。UI の日本語文字列は `.rc` のみに置く。

---

## Phase 1: Setup

- [X] T001 `MFCApplication1/CanSim/ReplayClock.h/.cpp` を空の雛形で作成し（ASCII のみ、`pch.h` 経由）、
  `MFCApplication1/MFCApplication1.vcxproj` と `.vcxproj.filters` に登録する（全 4 構成、フィルタ `CanSim`）

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: `Speed` 設定値と、再生の時計・区間内フレーム抽出のコア

- [X] T002 `MFCApplication1/CanSim/CanTypes.h` の `ProjectSettings` に `int Speed = 0;`（「-13〜+10 の整数」）を追加し、
  定数 `kMinSpeed = -13`、`kMaxSpeed = 10`、`kInstantSpeed = 10`（待ちなし）を定義する（data-model.md）
- [X] T003 `MFCApplication1/CanSim/ConfigLoader.cpp` の `[Project]` 読込みに `Speed` を追加する（任意、省略時 0）。
  符号付き整数として解析し（先頭の `+`／`-` を許容。research.md 決定6）、「-13〜10 の整数」以外（`11`、`-14`、
  `1.5`、`abc`、空）は `Success=false` で `[Project] Speed: ...` の理由を返す（FR-002）（依存: T002）
- [X] T004 [P] `MFCApplication1/CanSim/ProjectFile.cpp` の `SaveProject` が `[Project]` に常に `Speed=<値>` を書くように
  する（`+` は付けない。contracts/project-file-speed.md）（依存: T002）
- [X] T005 `MFCApplication1/CanSim/ReplayClock.h/.cpp` を実装する（data-model.md）: (a) `long long SimTimeAtUs(long long
  elapsedRealUs, int speed)` — speed が -13〜+9 のとき `elapsedRealUs × 2^speed` を整数演算で返す（speed ≥ 0 は左シフト、
  < 0 は右シフト。オーバーフローしそうなら `LLONG_MAX` に飽和）。(b) `struct ActiveFrames { std::vector<int> Senders;
  std::vector<int> Receivers; bool BusBusy = false; }`（`Receivers` には `Senders` に含まれる ECU を入れない、重複なし・
  昇順）。(c) `class ActiveFrameCursor` — コンストラクタで `const std::vector<FrameEvent>&`（TxStart 昇順）を受け取り、
  `ActiveFrames Collect(long long fromUs, long long toUs)` で区間 `(fromUs, toUs]` と送信区間 `[TxStartUs, TxEndUs]` が
  重なるフレームを集める。`TxEndUs <= fromUs` のフレームはカーソルを進めて以後見ない。`toUs` は単調増加を前提とする
  （research.md 決定4）（依存: T001）
- [X] T006 `tools/cansim_runner/checks.cpp` に 005 の検証項目を追加する（`#include "ReplayClock.h"`）:
  `Speed` の既定値 0、`+3`/`-3`/`10`/`-13` の受理と `11`/`-14`/`1.5`/`abc` の拒否、保存→再読込みで `Speed` が一致、
  `SimTimeAtUs`（speed 0 で等倍、+1 で 2 倍、-1 で 1/2、-10 で 1/1024、-13 で 1/8192、+9 で 512 倍）、`ActiveFrameCursor`
  （1 区間に複数フレームがあると送信元・受信者をすべて集める、送信優先で受信から除く、空き区間は `BusBusy=false` で
  空、`sample_appendix_a` の 1 秒分を 16 ms 刻みと 1 ms 刻みのどちらで集めても全フレームが少なくとも 1 回は対象になる）
  （依存: T003, T004, T005）

**Checkpoint**: `checks.exe` の全項目が PASS（quickstart 手順1）

---

## Phase 3: User Story 1 - 実時間に近い速さでシミュレーションを進める (Priority: P1) 🎯 MVP

**Goal**: スピード 0 で、10 秒のシミュレーションが約 10 秒かけて進み、フレーム表示 ON なら強調が時間どおりに動く

**Independent Test**: quickstart 手順 2（スピード 0・10 秒で完了まで 9〜11 秒）

- [X] T007 [US1] `MFCApplication1/MFCApplication1Dlg.h/.cpp` の再生を置き換える: 004 の `kReplayStepMs`（250 ms）と
  `m_playIndex` による 1 フレームずつの再生を削除し、再生の状態として `LARGE_INTEGER m_replayStartQpc`、
  `LARGE_INTEGER m_qpcFrequency`、`long long m_prevSimUs`、`long long m_curSimUs`、`CanSim::ActiveFrames m_active`、
  `std::unique_ptr<CanSim::ActiveFrameCursor> m_cursor` を持つ。`OnSimStart` は計算後（004 と同じ一括計算）、
  スピードが `kInstantSpeed` でなければ（フレーム表示の ON/OFF に関係なく。FR-007。**送信フレームが 0 件でも**
  再生（時間の経過）を行う。004 の「フレームが 0 件なら直ちに完了」の分岐は削除する: spec Edge Cases）`m_events = BuildFrameEvents(...)`、
  カーソル作成、`QueryPerformanceCounter` で開始時刻を記録し、`m_runState = Running`、`SetTimer(kReplayTimerId, 16)`
  で再生を始める（research.md 決定2・3）（依存: T005）
- [X] T008 [US1] `MFCApplication1/MFCApplication1Dlg.cpp` の `OnTimer` を実装し直す: 経過実時間（µs）を QPC から求め、
  `m_curSimUs = min(SimTimeAtUs(elapsed, m_settings.Speed), DurationSec × 10^6)`、`m_active = m_cursor->Collect(m_prevSimUs,
  m_curSimUs)`、`m_prevSimUs = m_curSimUs`、構成図を再描画する。終端に達したら 004 の `FinishReplay`（全結果の CSV 出力と
  完了通知）を呼ぶ。タイマーの回数は時刻計算に使わない（FR-005）（依存: T007）
- [X] T009 [US1] `MFCApplication1/MFCApplication1Dlg.cpp` の `DrawBusDiagram` の強調を、004 の「`m_events[m_playIndex]`
  1 件」から `m_active` に置き換える: `Running` かつ `m_settings.FrameDisplay` のとき、`Senders` の ECU を送信の強調（赤系、
  太枠、接続線を青）、`Receivers` の ECU を受信の強調（緑系）、`BusBusy` ならバスを青系で太く描く（contracts/ui-speed-replay.md）
  （依存: T008）

**Checkpoint**: SC-001 を満たす

---

## Phase 4: User Story 2 - スピードを選んで速く／遅く進める (Priority: P1)

**Goal**: セッティングで -13〜+10 を選べ、+10 は即時、-13 はどの条件でも 1 フレームを目で追える

**Independent Test**: quickstart 手順 1・3・5（24 項目、+1/-1/+3/+10 の所要時間、-13 の見え方）

- [X] T010 [US2] `MFCApplication1/MFCApplication1.rc` と `MFCApplication1/Resource.h` を更新する: `IDD_SETTINGS` に
  ラベル「スピード:」、ドロップダウンリスト `IDC_COMBO_SPEED`（`CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP`、ドロップ高さは
  24 項目が収まる程度）、注記「(+10: 即時 / 0: 実時間 / -13: 1/8192)」を追加し、ダイアログの高さと OK/キャンセルの位置を
  調整する（contracts/ui-speed-replay.md）。`Resource.h` に `IDC_COMBO_SPEED` を追加する。**`.rc` は UTF-16LE のため
  エンコーディングを明示して編集する**
- [X] T011 [US2] `MFCApplication1/SettingsDlg.cpp` にスピードの選択を追加する: `OnInitDialog` でリストに `+10`〜`+1`、`0`、
  `-1`〜`-13` の 24 項目を上から降順で入れ、現在値を選択する。`OnOK` で選択値を `Settings.Speed` に入れる（FR-001）。
  `MFCApplication1/MFCApplication1Dlg.cpp` の `OnEditSettings` は `IDOK` のときに `m_settings.Speed` も反映する（依存: T010）
- [X] T012 [US2] `MFCApplication1/MFCApplication1Dlg.cpp` の `OnSimStart` で、スピードが `kInstantSpeed`（+10）のときは再生を
  行わず、直ちに `FinishReplay`（CSV 出力と完了通知）を呼ぶ（フレーム表示 ON でも待たない。FR-004）（依存: T007）

**Checkpoint**: SC-002・SC-003・SC-004 を満たす

---

## Phase 5: User Story 3 - フレーム表示は表示の有無だけを切り替える (Priority: P2)

**Goal**: ON/OFF で所要時間と結果が変わらず、強調の有無だけが変わる

**Independent Test**: quickstart 手順 4（ON と OFF の所要時間差 1 秒以内、OFF は強調なし）

- [X] T013 [US3] `MFCApplication1/MFCApplication1Dlg.cpp` の `OnEditFrameDisplay` から、004 の「再生中に OFF にしたら
  `FinishReplay` で直ちに終了する」処理を削除し、`m_settings.FrameDisplay` の反転と構成図の再描画だけにする（実行は
  継続する。FR-007, FR-010）（依存: T009）

**Checkpoint**: SC-006 を満たす

---

## Phase 6: User Story 4 - 速いスピードでも送受信した ECU がわかる (Priority: P2)

**Goal**: 書き換え 1 回の間の複数フレームをまとめて強調する

**Independent Test**: quickstart 手順 4（スピード 0 で `sample_appendix_a` の強調が途切れずに見える、書き換え 30 回/秒以上）

- [X] T014 [US4] 実機（Debug|x64）で、スピード 0・フレーム表示 ON・`sample_appendix_a.canprj`（一時フォルダにコピー）を実行し、
  実行中のスクリーンショットを複数枚取得して、送信元（赤）と受信（緑）の強調が確認できることを確認する。あわせて
  空き時間に強調が消えること（US1 AS2）を `sample_16ecu.canprj` で確認する。
  `OnTimer` の呼び出し回数を一時的に計測し（確認後に計測コードは削除する）、1 秒間に 30 回以上であることを確認する（SC-009）
  （依存: T009）
  【結果】Release|x64 で 10 秒間に 394 回（約 39 回/秒）。保証値 30 回/秒は満たすが、目標の約 60 回/秒には届かない
  （Windows 標準タイマーの粒度のため。時刻は経過時間で決めているので所要時間と結果には影響しない）。計測コードは削除済み。
  強調は区間ごとにまとめて表示されることをスクリーンショットで確認。空き時間の消灯は `sample_16ecu` の -13（3.3 秒時点）と
  -5 で確認。なお、スピード 0 では空き時間が 1 回の書き換え間隔（約 16〜31 ms）より短いと消灯は見えない（区間でまとめる仕様上）。

**Checkpoint**: SC-009 を満たす

---

## Phase 7: User Story 5 - 実行中はセッティングを変更できない (Priority: P2)

**Goal**: 実行中は設定・読込み・開始ができず、「終了」でその時点までの結果が出る

**Independent Test**: quickstart 手順 2・5（実行中のメニュー状態、途中終了の結果）

- [X] T015 [US5] `MFCApplication1/MFCApplication1Dlg.cpp` の `OnSimStop` を、004 の「表示中フレームの TxStart」から
  「現在の時計の時刻 `m_curSimUs`」を終了時刻にするよう変更する（エンジンを `m_curSimUs` まで再実行して CSV 出力、
  終了通知。FR-012）。フレーム表示 OFF の実行中でも「終了」が選べること（004 の `OnInitMenuPopup` の `Running` 判定で
  満たされる）、実行中に「セッティング」「プロジェクト読込み」「開始」が選べないことを確認する（FR-011）（依存: T008）
- [X] T016 [US5] `MFCApplication1/MFCApplication1Dlg.cpp` の `OnCancel`（✖）が、実行中なら再生を止めて結果を出さずに閉じる
  ことを確認する（004 と同じ。`m_cursor` 等の後始末を含む）（依存: T015）

**Checkpoint**: SC-007・SC-008 を満たす

---

## Phase 8: Polish & Cross-Cutting Concerns

- [X] T017 [P] `CLAUDE.md` を更新する（憲章 V）: アーキテクチャ節に `ReplayClock.h/.cpp` を追加し、「プロジェクトと実行（機能 004）」
  の再生の説明（1 フレーム 250 ms、再生中の OFF で即終了）を、スピードによる経過時間ベースの再生（約 16 ms 書き換え、
  区間内のフレームをまとめて強調、+10 は即時、OFF でも時間は流れる）に書き換える。`.canprj` の `Speed` キーにも触れる
- [X] T018 [P] 機能 004 の `specs/004-menu-project-settings/contracts/project-file-format.md` と `contracts/ui-menu-dialogs.md` の冒頭に、
  005 で `Speed` キーの追加と再生表示の置き換えがあったことと、`specs/005-simulation-speed/contracts/` への参照を追記する
  （原文は書き換えない）
- [X] T019 Debug/Release × x64/x86 の全 4 構成で `msbuild MFCApplication1\MFCApplication1.sln` が警告なしで通ることを確認する（憲章 II）
  【結果】全 4 構成で警告なしに成功（Debug|x64 は、利用者が起動中のアプリを閉じた後に再ビルドして確認）。
- [X] T020 GUI で quickstart 手順 2〜6 を実施する（一時フォルダのサンプルを使用）: スピード 0・10 秒の所要時間 9〜11 秒、
  +1/-1/+3/+10 の所要時間、ON と OFF の所要時間差、-13 で 1 フレームの強調が 0.2 秒以上続くこと（1 Mbps でも）、途中終了の結果、
  スピード 0（ON/OFF）・+3・+10 で最後まで実行した 3 種の CSV が完全一致（SC-001〜SC-008）。`checks.exe` の全項目 PASS も
  確認する（依存: T019）
  【結果】`sample_appendix_a`・10 秒（Release|x64）: スピード 0 = 10.16 秒、0 で表示 ON = 10.13 秒（差 0.03 秒）、+1 = 5.13 秒、
  -1 = 20.13 秒、+3 = 1.38 秒、+10 = 0.11 秒。6 通りの 3 種 CSV はすべて完全一致し、004 以前の基準とも一致。-13・`sample_16ecu`
  で ECU 01 の 1 フレームの強調が 2 秒前後続くことを確認。実行中は「開始」「セッティング」がグレー、「終了」「フレーム表示」は
  選択可。途中終了でその時点（例: 744 µs）までの結果を出力。`checks.exe` 全項目 PASS。

---

## Dependencies & Execution Order

- **Setup (T001)** → **Foundational**（T002→T003/T004、T005→T006）
- **US1 (T007→T008→T009)**: T005 後。MVP
- **US2**: T010→T011（UI）、T012（T007 後）
- **US3 (T013)**: T009 後
- **US4 (T014)**: T009 後の確認
- **US5 (T015→T016)**: T008 後
- **Polish**: T017・T018 は並行可。T019→T020

### Parallel Opportunities

- T004（保存）は T003（読込み）と並行可
- T010（.rc）は T005〜T009 と並行可
- T017・T018（文書）は実装と並行可

## Implementation Strategy

1. **MVP = Foundational + US1**: `Speed`（既定 0）と時計・区間抽出を作り、スピード 0 で実時間の再生ができる状態にする。
2. US2 でスピードの選択と +10 の即時完了、US3〜US5 で表示の独立・まとめ強調の確認・実行中の制限と終了を仕上げる。
3. Polish で文書、4 構成ビルド、GUI の通し確認（所要時間と CSV の一致）。

## Notes

- `CanSim/` の新規ソースに日本語を含めない（ASCII のみ）。
- 所要時間の確認は、再生開始（完了通知の直前までの計算待ちを除く）からの時間で判断する（research.md 決定1）。
- 004 の tasks.md は変更しない（再生方式の置き換えは本機能の T007〜T009・T013・T015 で行う）。
