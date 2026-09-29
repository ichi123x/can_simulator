---

description: "Task list for feature 004-menu-project-settings"
---

# Tasks: メニューバー・プロジェクト保存/読込み・設定・フレーム表示・シミュレーション操作

**Input**: Design documents from `/specs/004-menu-project-settings/`

**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/, quickstart.md（すべて存在）。
**機能 003 の実装（`m_ecuCount`／`SetEcuCount`、構成図の描画、ダイアログ寸法）が完了していること。**

**Tests**: spec.md は自動テストを要求していない。plan.md の方針に従い、`CanSim/` のコア（読込み・保存・
実行用ネットワーク生成・フレームイベント抽出）は `tools/cansim_runner/checks.cpp` に検証項目を追加する。
GUI は `quickstart.md` の目視手順で確認する。

**Organization**: タスクは spec.md のユーザーストーリー（US1〜US5）ごとにグループ化する。メニューの
骨格と既存コントロールの削除は全 GUI ストーリーの前提になるため、Foundational に含める（US5 には
その上の白背景・Enter/Esc 抑止・確認を置く）。

## Format: `[ID] [P?] [Story] Description`

- **[P]**: 並行実行可能（異なるファイル、未完了タスクへの依存なし）
- **[Story]**: 対応するユーザーストーリー（US1〜US5）
- 各タスクに正確なファイルパスを含める

## Path Conventions

既存の単一 VS プロジェクト `MFCApplication1/`。コアは `MFCApplication1/CanSim/`（ASCII のみ、MFC 非依存）、
UI は `MFCApplication1Dlg.*`／`SettingsDlg.*`／`MFCApplication1.rc`。`.rc` は UTF-16LE（BOM 付き、CRLF）のため
**Read/Edit ツールを使わず**、PowerShell の `Get-Content -Encoding Unicode` と
`[System.IO.File]::WriteAllText(path, text, (New-Object System.Text.UnicodeEncoding($false,$true)))` で読み書きする
（CLAUDE.md）。UI の日本語文字列は `.rc` の文字列テーブルに置き、コード側は英語の識別子・メッセージのみ。

---

## Phase 1: Setup

- [X] T001 本機能の実装に着手する**前に**、現在のビルドで `MFCApplication1/CanSim/SampleConfigs/` の
  全サンプル（`sample_appendix_a.cfg`、`sample_collision.cfg`、`sample_startup.cfg`、
  `sample_startup_latejoin.cfg`）を 10 秒間実行し、生成された `*.log.csv` `*.summary.csv` `*.startup.csv`
  をスクラッチ領域（リポジトリ外）へ退避する。SC-009（既存機能の結果が変わらない）の基準データとする
- [X] T002 [P] `MFCApplication1/CanSim/ProjectFile.h/.cpp`、`MFCApplication1/CanSim/FrameEvents.h/.cpp`、
  `MFCApplication1/SettingsDlg.h/.cpp` を空の雛形で作成し（`CanSim/` は ASCII のみ、`pch.h` 経由）、
  `MFCApplication1/MFCApplication1.vcxproj` と `.vcxproj.filters` に登録する（全 4 構成）

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: `.canprj` のコア（読込み・保存・実行用ネットワーク）と、メニュー駆動の GUI の骨格

### コア（CanSim）

- [X] T003 `MFCApplication1/CanSim/CanTypes.h` に `ProjectSettings`（`int EcuCount = 2` 「0〜16」、
  `int DurationSec = 10` 「1〜600」、`bool FrameDisplay = false`）を追加し、
  `MFCApplication1/CanSim/ConfigLoader.h` の `ConfigLoadResult` に `ProjectSettings Project` を追加する
  （data-model.md）
- [X] T004 `MFCApplication1/CanSim/ConfigLoader.cpp` に `[Project]` セクション（`EcuCount`／`DurationSec`／
  `FrameDisplay`、すべて任意、省略時は T003 の既定値）の読込みを追加する。検証: 「`EcuCount` は 0〜16
  の整数」「`DurationSec` は 1〜600 の整数」「`FrameDisplay` は 0 または 1」。違反時は `Success=false` で
  キー名と理由を含む `ErrorMessage` を返す（FR-010）。あわせて、ノードが 0 個（`[NodeN]` なし）を許容する
  ように「少なくとも 1 つの `[NodeN]`」の拒否を外す（`Config.Nodes` は空でよい）。他の検証規則は変更しない
  （依存: T003）
- [X] T005 `MFCApplication1/CanSim/ProjectFile.h/.cpp` に (a) `bool SaveProject(const std::wstring& path,
  const ProjectSettings&, const Network& definitions)`（contracts/project-file-format.md の形式を UTF-8 で
  書き出す。`[Project]`、`[Network] BitRateBps`、全ノード定義とフレーム定義、`RxFilter`／`ReceiveDelayUs`／
  `PowerOnTimeUs`／`InitDurationUs` を含める。コメントは保存しない）と、(b) `Network BuildRunNetwork(const
  ProjectSettings&, const Network& definitions)`（research.md 決定2: ECU 番号 1〜EcuCount のノードを NodeId
  昇順で作り、同番号の定義があればそれを使い、なければ名前 `ECU<n>`・フレームなしのノードを補う。EcuCount
  を超える番号の定義は捨てる。`BitRateBps` は definitions から引き継ぐ）を実装する（依存: T003）
- [X] T006 サンプルを `.canprj` に移行する: `MFCApplication1/CanSim/SampleConfigs/sample_*.cfg` の 4 ファイルを
  `git mv` で `sample_*.canprj` に改名し、それぞれ先頭に `[Project]`（`EcuCount` = そのファイルの定義ノード数、
  `DurationSec=10`、`FrameDisplay=0`）を追加する（内容は変えない: SC-009）。
  【実施メモ】`sample_collision` は NodeId が 1・3 のため `EcuCount=3` にすると定義のない ECU 2 が補われ、受信行が
  増えて SC-009 が崩れる。そこで ECU 2 を `RxFilter=0x7FF`（何も受信しない）の待機 ECU として明示した。ログ・集計は
  従来と完全一致し、起動記録だけ ECU 2 の行が 1 行増える（checks.exe の比較ではその行を除外）。
  `tools/cansim_runner/runner.cpp` は `BuildRunNetwork` を使って `.canprj` を実行し、`DurationSec` を
  既定にする（引数で上書き可）。`tools/cansim_runner/checks.cpp` の `.cfg` 参照（27 箇所）を `.canprj`
  に置換する。`tools/cansim_runner/README.md` のコマンド例と説明も更新する（依存: T004, T005）
- [X] T007 [P] `MFCApplication1/CanSim/SampleConfigs/sample_16ecu.canprj` を作成する: `EcuCount=16`、
  `sample_appendix_a.canprj` と同じ書式で NodeId 1〜16 のノード（各ノードに CAN ID が重複しない
  1 フレーム以上）、`BitRateBps=500000`、`DurationSec=10`。フレーム再生と 16 ECU 表示の確認に使う
- [X] T008 `tools/cansim_runner/checks.cpp` に検証項目を追加する: `.canprj` の既定値（`[Project]` 省略時 2／10／OFF）、
  範囲外（`EcuCount=17`、`DurationSec=0`／`601`、`FrameDisplay=2`）の拒否、ノード 0 個の許容、保存→再読込みで
  設定と全定義が一致（SC-003）、`BuildRunNetwork` の ECU 増（定義なしの ECU を補う）・減（超過番号の
  定義を捨てる）・通信速度の引き継ぎ（FR-011）、`EcuCount=0` とフレーム定義なしの ECU 2 個でエンジンを実行して
  エラーにならず 3 種の CSV（ヘッダのみ）が出力される（spec US3 AS5。集計 `ComputeMetrics` が空のログで例外を
  出さないことを含む）、移行後 4 サンプルの結果が T001 の基準データと一致（SC-009）
  （依存: T005, T006, T007）

### GUI の骨格（メニュー、既存コントロールの削除）

- [X] T009 `MFCApplication1/MFCApplication1.rc` と `MFCApplication1/Resource.h` を更新する: (a) `IDD_MFCAPPLICATION1_DIALOG`
  の既存コントロール（`IDC_EDIT_CONFIG_PATH`、`IDC_BUTTON_BROWSE`、`IDC_EDIT_DURATION`、`IDC_BUTTON_RUN`、
  `IDC_STATIC_RESULT`、ラベル、`IDOK`、`IDCANCEL`）をすべて削除し、対応する ID 定義も削除する。ダイアログ寸法は
  機能 003 で決めた値（クライアント720×152 DLU）を維持する。(b) `MENU IDR_MAINMENU`（contracts/
  ui-menu-dialogs.md: `ファイル(&F)`〔`プロジェクト読込み(&O)...`=`ID_FILE_LOAD_PROJECT`、`プロジェクト保存(&S)...`=
  `ID_FILE_SAVE_PROJECT`〕、`編集(&E)`〔`セッティング(&T)...`=`ID_EDIT_SETTINGS`、`フレーム表示(&D)`=
  `ID_EDIT_FRAME_DISPLAY`〕、`シミュレーション(&S)`〔`開始(&R)`=`ID_SIM_START`、`終了(&X)`=`ID_SIM_STOP`〕）を追加し、
  ダイアログ定義に `MENU IDR_MAINMENU` を指定する。(c) 文字列テーブルに通知メッセージ（完了〔出力ファイル名
  を含む〕、プロジェクト読込みエラー〔理由〕、保存エラー、セッティング入力エラー、上書き確認、プロジェクトの
  ファイルフィルタ「CANプロジェクト (*.canprj)」）を追加する（依存: 機能 003 の T005）
- [X] T010 `MFCApplication1/MFCApplication1Dlg.h/.cpp` を更新する: 旧ハンドラ `OnBnClickedButtonBrowse`／
  `OnBnClickedButtonRun`／`SetResultText` とメッセージマップ項目を削除する。プロジェクトの状態
  （`CanSim::ProjectSettings m_settings`、`CanSim::Network m_definitions`〔読込んだ全定義〕、`CString
  m_projectPath`）と実行状態（`enum { Idle, Running } m_runState`）を追加し、6 つのメニューコマンドを
  `ON_COMMAND` で空のハンドラとして配線する。`ON_WM_INITMENUPOPUP` で contracts/ui-menu-dialogs.md
  の有効条件（停止中のみ: 読込み・セッティング・開始、実行中のみ: 終了）と「フレーム表示」のチェックを設定する。
  `OnInitDialog` で `m_settings` を既定値（EcuCount=2）にして `SetEcuCount(m_settings.EcuCount)` を呼ぶ。
  `OnOK` を何もしない実装にする（依存: T009）

**Checkpoint**: ビルドして起動 → メニュー 3 項目が表示され、旧コントロールがなく、ECU 2 個の構成図が出る。
`checks.exe` の新規項目が PASS（quickstart 手順1・2-1）

---

## Phase 3: User Story 1 - セッティングで ECU 数・通信速度・シミュレーション時間を選ぶ (Priority: P1) 🎯 MVP

**Goal**: セッティングで 3 つの値を指定し、決定した ECU 数が構成図に反映される

**Independent Test**: quickstart 手順 2（ECU 数 17 通り・通信速度 4 通り、16 で決定→16 個、キャンセル→変化なし、
時間 0 を拒否）

- [X] T011 [US1] `MFCApplication1/MFCApplication1.rc` に `IDD_SETTINGS` ダイアログを追加する: ECU 数
  `IDC_COMBO_ECU_COUNT`（`CBS_DROPDOWNLIST`）、通信速度 `IDC_COMBO_BITRATE`（`CBS_DROPDOWNLIST`）、
  シミュレーション時間 `IDC_EDIT_DURATION_SEC`（`ES_NUMBER`）、ラベル、OK、キャンセル。ラベルと入力エラー
  文字列は日本語（文字列テーブル）。`Resource.h` に ID を追加する
- [X] T012 [US1] `MFCApplication1/SettingsDlg.h/.cpp` に `CSettingsDlg : CDialogEx` を実装する: `OnInitDialog`
  で ECU 数のリストに 0〜16 の 17 項目、通信速度のリストに `125 kbps`／`250 kbps`／`500 kbps`／`1 Mbps`
  の 4 項目（値 125000／250000／500000／1000000）を入れ、現在値を選択する。`OnOK` でシミュレーション時間を
  検証し（「1〜600 の整数」）、範囲外・空欄・数字以外はメッセージを表示してダイアログを閉じない。
  結果は `ProjectSettings`（EcuCount, DurationSec）と `BitRateBps` として取り出せる（依存: T011）
- [X] T013 [US1] `MFCApplication1/MFCApplication1Dlg.cpp` に `ID_EDIT_SETTINGS` のハンドラを実装する: `CSettingsDlg` を
  現在の `m_settings`／`m_definitions.BitRateBps` で初期化して `DoModal`。`IDOK` のときだけ値を
  `m_settings`／`m_definitions.BitRateBps` に反映し、`SetEcuCount(m_settings.EcuCount)` を呼ぶ。`IDCANCEL` では
  何も変更せず `SetEcuCount` も呼ばない（FR-007、003 の FR-010）（依存: T010, T012）

**Checkpoint**: SC-001・SC-002 を満たす

---

## Phase 4: User Story 2 - プロジェクトを保存・読込みする (Priority: P1)

**Goal**: `.canprj` の保存と読込みで、設定とフレーム定義が完全に復元される

**Independent Test**: quickstart 手順 3（保存→再起動→読込みで一致、不正ファイルで現在の内容が不変）

- [X] T014 [US2] `MFCApplication1/MFCApplication1Dlg.cpp` に `ID_FILE_LOAD_PROJECT` のハンドラを実装する:
  `CFileDialog`（開く、既定拡張子 `canprj`、フィルタは文字列テーブル）でパスを得て `CanSim::LoadConfig` を呼ぶ。
  読込み前にパスの拡張子が `.canprj`（大文字小文字を区別しない）であることを検査し、違えば「拡張子が
  .canprj ではない」旨を通知して読み込まない（spec US2 AS3）。`LoadConfig` の失敗時は `ErrorMessage` を含む
  通知を表示し、`m_settings`／`m_definitions`／`m_projectPath` を変更しない（FR-010）。成功時のみこれらを置き換え、`SetEcuCount(m_settings.EcuCount)` を呼ぶ（フレーム表示のチェックは
  メニュー表示時に `m_settings.FrameDisplay` から反映される）（依存: T010）
- [X] T015 [US2] `MFCApplication1/MFCApplication1Dlg.cpp` に `ID_FILE_SAVE_PROJECT` のハンドラを実装する:
  `CFileDialog`（保存、既定拡張子 `canprj`、`OFN_OVERWRITEPROMPT`）でパスを得て、`CanSim::SaveProject(path,
  m_settings, m_definitions)` を呼ぶ。失敗時は保存エラーを通知する。成功時は `m_projectPath` を更新する
  （FR-008）（依存: T010）

**Checkpoint**: SC-003・SC-004 を満たす

---

## Phase 5: User Story 3 - シミュレーションを開始する (Priority: P1)

**Goal**: 「開始」で実行し、3 種の結果を出力して完了を通知する（フレーム表示 OFF の経路）

**Independent Test**: quickstart 手順 4（`sample_appendix_a.canprj` を読込み、開始→完了通知と 3 種の CSV）

- [X] T016 [US3] `MFCApplication1/MFCApplication1Dlg.cpp` に結果出力の補助関数
  `bool WriteResults(const CString& projectPath, const CanSim::Network& network, const CanSim::Logger&,
  const CanSim::SimulationEngine&)` を実装する。旧 `OnBnClickedButtonRun` の処理を移す: `<projectPath>.log.csv`
  （`Logger::WriteCsv`）、`.summary.csv`（`ComputeMetrics`＋`WriteMetricsCsv`）、`.startup.csv`
  （`BuildStartupRecords`＋`WriteStartupCsv`）。出力内容は従来と同一にする（SC-009）
- [X] T017 [US3] `MFCApplication1/MFCApplication1Dlg.cpp` に `ID_SIM_START` のハンドラを実装する: (1) `m_projectPath`
  が空なら保存ダイアログ（T015 と同じ `OFN_OVERWRITEPROMPT`）で場所を求め、`SaveProject` して `m_projectPath` を更新する（キャンセルなら開始しない、
  FR-017）。(2) `BuildRunNetwork(m_settings, m_definitions)` から `SimulationEngine(network,
  m_settings.DurationSec * 1000000LL)` を作って `Run` する。実行と CSV 出力の間は待機カーソル（`CWaitCursor`）を
  表示する（同期実行のため UI が反応しない。research.md 決定4、上限 600 秒で約 5 秒）。(3) `m_settings.FrameDisplay` が OFF のとき
  `WriteResults` を呼び、完了通知（出力ファイル名を含む）を表示する。ON のときの経路は US4（T021）で追加する。
  ECU 数 0 やフレームなしでもエラーにせず、ヘッダのみの結果を出力する（依存: T014, T015, T016）

**Checkpoint**: SC-008 の前半（開始から指定時間経過で 3 種が出力される）を満たす

---

## Phase 6: User Story 4 - フレームの送受信の様子をビジュアルで確認する (Priority: P2)

**Goal**: フレーム表示 ON のとき、実行結果をフレームごとに強調表示で再生する。実行中に「終了」できる

**Independent Test**: quickstart 手順 5・6（ON で再生、途中終了、ON/OFF で CSV 同一）

- [X] T018 [US4] `MFCApplication1/CanSim/FrameEvents.h/.cpp` に `struct FrameEvent { long long TxStartUs;
  long long TxEndUs; int TxNodeId; std::vector<int> RxNodeIds; unsigned CanId; }` と
  `std::vector<FrameEvent> BuildFrameEvents(const std::vector<LogRow>&)` を実装する（data-model.md:
  `(TxNodeId, TxStartUs)` が同じ行を 1 件にまとめ、`RxNodeId != -1` のみ受信者に集め、`TxStartUs` 昇順）
  （依存: T002）
- [X] T019 [P] [US4] `tools/cansim_runner/checks.cpp` に `BuildFrameEvents` の検証項目を追加する（受信ノードなしの行は
  受信者が空、複数受信者の集約、送信開始時刻の昇順）。あわせて、終了時刻 `t` で再実行した結果が「`t` までに
  送信を完了したフレームだけ」になること（SC-008、research.md 決定4 の要確認事項: 終了時刻をまたぐ
  送信中フレームがログに含まれないこと）を検証し、含まれる場合は結果を報告して設計を見直す（依存: T018）
- [X] T020 [US4] `MFCApplication1/MFCApplication1Dlg.cpp` に `ID_EDIT_FRAME_DISPLAY` のハンドラを実装する:
  `m_settings.FrameDisplay` を反転する（チェック表示は T010 の `WM_INITMENUPOPUP` が反映）。実行中でも切替可能
  （表示のみが変わる。結果には影響しない: FR-015）（依存: T010）
- [X] T021 [US4] `MFCApplication1/MFCApplication1Dlg.h/.cpp` に再生を実装する: T017 の ON 経路で `BuildFrameEvents`
  の結果を保持し（`std::vector<CanSim::FrameEvent> m_events; size_t m_playIndex`）、`m_runState = Running`、
  `SetTimer`（250 ms 固定、定数）で開始。`OnTimer` で `m_playIndex` を進めて再描画し、末尾まで達したら
  タイマーを止めて `WriteResults` を呼び、完了通知を出して `Idle` に戻し強調を消す。イベントが 0 件なら
  即座に完了する（依存: T017, T018）
- [X] T022 [US4] `MFCApplication1/MFCApplication1Dlg.cpp` の `OnPaint`（機能 003 の構成図描画）に強調表示を追加する
  （contracts/ui-menu-dialogs.md）: `Running` かつ `m_playIndex` が有効なとき、送信元 ECU を赤系の濃い塗り＋太枠、
  受信 ECU を緑系の塗り、バス線と送信元の接続線を青系の太線にする。表示外のフレームは通常色。強調は
  `Idle` に戻ったとき消える。強調するのは `Running` **かつ** `m_settings.FrameDisplay` が ON のときだけとし、再生
  中に OFF に切り替えたら強調を消す（再生は継続して結果を出力する。FR-014）。再描画でちらつかない
  （メモリ DC。機能 003 T004 の描画を再利用）。
  【実施メモ】再生中に OFF に切り替えたときは、見えない再生を続けるのではなく、その場で全結果を出力して終了する
  （`OnEditFrameDisplay` → `FinishReplay`）。出力内容は最後まで再生した場合と同一（SC-006 で確認）。ECU 数 0 でも例外を出さない（依存: T021）
- [X] T023 [US4] `MFCApplication1/MFCApplication1Dlg.cpp` に `ID_SIM_STOP` のハンドラを実装する: `Running` のとき、
  タイマーを止め、`t = m_events[m_playIndex].TxStartUs` を終了時刻として `SimulationEngine(同じ network, t)` を
  再実行し、`WriteResults` で `t` までの結果を出力して完了通知を出し、`Idle` に戻して強調を消す（FR-018）
  （依存: T021, T019）
- [X] T024 [US4] `MFCApplication1/MFCApplication1Dlg.cpp` で、`Running` 中に ✖ で閉じたとき（`OnCancel` 経路）は
  タイマーを止め、結果を出力せずに閉じる。`Running` 中の「プロジェクト読込み」「セッティング」「開始」が
  受け付けられないこと（T010 の有効条件と、ハンドラ冒頭の二重ガード）を確認する（FR-019）（依存: T021）

**Checkpoint**: SC-005・SC-006・SC-008 を満たす

---

## Phase 7: User Story 5 - メニューバーと、白いだけの作業領域 (Priority: P2)

**Goal**: メニュー行の下のダイアログ全体が白で、構成図以外に表示されるものがない。Enter/Esc で閉じない

**Independent Test**: quickstart 手順 2-1・7

- [X] T025 [US5] `MFCApplication1/MFCApplication1Dlg.h/.cpp` に `ON_WM_ERASEBKGND` を追加し、`OnEraseBkgnd` で
  クライアント領域全体を `RGB(255,255,255)` で塗る（システムテーマに依存しない。FR-004）。`PreTranslateMessage`
  で `WM_KEYDOWN` の `VK_ESCAPE`／`VK_RETURN` を握りつぶす（`OnCancel` は残す: ✖ の `WM_CLOSE` 経路のため。
  FR-020）。既存の `OnPaint` の描画順（背景→構成図）が崩れないこと、ちらつきがないこと（メモリ DC）を
  確認する（依存: T010）
- [X] T026 [US5] 実機（Debug|x64）で quickstart 手順 2-1・7 を実施する: メニュー 3 項目がこの順で表示、Alt+F／
  Alt+E／Alt+S で開く、メニューの下がすべて白で構成図以外に何もない、Enter／Esc で閉じない、✖ で閉じる
  （SC-007）（依存: T025）

**Checkpoint**: SC-007 を満たす

---

## Phase 8: Polish & Cross-Cutting Concerns

- [X] T027 [P] `CLAUDE.md` を更新する（憲章 V）: アーキテクチャ節に `ProjectFile`／`FrameEvents`／`SettingsDlg`、
  ダイアログがメニュー駆動になり旧コントロールがないこと、`.canprj` が `.cfg` の後継であること、
  `SampleConfigs/` の拡張子変更（`.canprj`）と `sample_16ecu.canprj` を反映。ビルド・検証節の
  `checks.exe`／`runner.exe` のコマンド例と、出力 CSV の命名（`<プロジェクトファイル名>.log.csv` 等）も更新する
- [X] T028 [P] `specs/001-can-simulation-core/contracts/config-input-format.md` の冒頭に、後継の契約
  （`specs/004-menu-project-settings/contracts/project-file-format.md`、拡張子 `.canprj`、`[Project]` セクション
  追加）への参照を追記する（原文は書き換えない）
- [X] T029 Debug/Release × x64/x86 の全 4 構成で `msbuild MFCApplication1\MFCApplication1.sln` が警告 C4819 なし
  で通ることを確認する（憲章 II）
- [X] T030 回帰確認: `checks.exe` の全項目が PASS、T001 の基準データと移行後サンプルの結果 CSV が一致
  （SC-009）、フレーム表示 ON と OFF で 3 種の CSV が完全一致（SC-006）することを確認する（依存: T029）
- [X] T031 `quickstart.md` の GUI 手順（2〜7）を実機で通しで実施し、Windows をダークテーマにしても表示部が白の
  ままであることも確認する（機能 003 の SC-004 と 004 の SC-007）（依存: T030）
  【実施状況】GUI 手順 2〜7 は自動操作スクリプトで実施済み（起動直後の表示、Enter/Esc で閉じない、セッティングの
  選択肢・キャンセル・時間 0 の拒否、保存→読込み、拡張子違い・壊れた内容・EcuCount=17 の拒否と内容維持、未保存で
  開始→保存先の指定、ON の再生と途中終了、ON/OFF で CSV 完全一致、再生中の ✖、Alt+S）。ダークテーマでの確認は
  利用者が実機で実施し（2026-09-29）、ダークモードでもメニュー行より下が白のままであることを確認した（SC-007、003 SC-004）。

---

## Dependencies & Execution Order

- **機能 003** の実装完了 → 本機能
- **Setup (T001, T002)** → **Foundational**: コア（T003→T004/T005→T006→T008、T007 は並行）と GUI 骨格
  （T009→T010）。GUI 骨格はコアの `ProjectSettings`（T003）に依存
- **US1 (T011〜T013)**: T010 完了後 → MVP
- **US2 (T014, T015)**: T010 完了後。US1 と独立（並行可）
- **US3 (T016, T017)**: T014・T015 後（プロジェクト保存と読込みが前提）
- **US4 (T018〜T024)**: T018 はコアのみで独立。T021 以降は T017 後。T023 は T019（再実行の検証）後
- **US5 (T025, T026)**: T010 後いつでも可（骨格の上の見た目）
- **Polish**: T027・T028 は並行可。T029→T030→T031

### Parallel Opportunities

- T002（雛形）と T001（基準データ）は並行可
- T007（サンプル）は T008 まで独立
- US1（T011〜T013）と US2（T014, T015）と US5（T025）は、Foundational 完了後に並行可
- T018・T019（コア側）は GUI 作業と並行可
- T027・T028 は並行可

## Implementation Strategy

1. **MVP = US1**: Foundational（T003〜T010）＋ US1（T011〜T013）で、メニューとセッティングによる ECU 数の
   変更が構成図に反映される状態を先に作る。
2. US2（保存・読込み）→ US3（開始）で、従来の実行機能をメニューで再現する。ここで 001・002 の結果との
   一致（SC-009）を確認する。
3. US4（フレーム再生・終了）→ US5（白背景・Enter/Esc）で仕上げ、Polish で文書・4 構成ビルド・回帰・通し確認。

## Notes

- `CanSim/` の新規ソースに日本語コメント・リテラルを含めない（ASCII のみ）。
- `.rc` は必ずエンコーディングを明示して編集する。
- 各チェックポイントで quickstart.md の該当手順を実施する。
- US3 の受け入れシナリオ 2（実行中の「終了」）は US4 の再生（T021〜T023）が入って初めて確認できる。
