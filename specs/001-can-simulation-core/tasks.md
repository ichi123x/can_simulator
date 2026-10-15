---

description: "Task list template for feature implementation"
---

# Tasks: CAN バスシミュレーション コア機能（構成・調停・性能評価）

**Input**: Design documents from `/specs/001-can-simulation-core/`

**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/, quickstart.md（すべて存在）

**Tests**: `spec.md` はテストタスクを明示的に要求していないため、テストタスクは含めない（`plan.md` の Technical Context / research.md 決定3 に基づき、自動テスト基盤は本フェーズで導入しない）。検証は Polish フェーズの quickstart.md 手動シナリオ実行で代替する。

**Organization**: タスクは `spec.md` のユーザーストーリー（US1〜US3）ごとにグループ化する。

## Format: `[ID] [P?] [Story] Description`

- **[P]**: 並行実行可能（異なるファイル、未完了タスクへの依存なし）
- **[Story]**: 対応するユーザーストーリー（US1, US2, US3）
- 各タスクに正確なファイルパスを含める

## Path Conventions

すべて既存の単一 VS プロジェクト `MFCApplication1/` 配下（`plan.md` のProject Structure 参照）。新規ソースは `MFCApplication1/CanSim/` に置く。

---

## Phase 1: Setup

**Purpose**: シミュレーションコア用のフォルダ構成を用意する

- [X] T001 `MFCApplication1/CanSim/` ディレクトリを作成し、`MFCApplication1/MFCApplication1.vcxproj.filters` に "CanSim" フィルタ（フォルダ）を追加する（後続タスクで追加するファイルの受け皿とする）

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: US1〜US3 すべてが依存する共通データ型・DES基盤・設定読み込みを実装する

**⚠️ CRITICAL**: このフェーズが完了するまで、いずれのユーザーストーリーの実装にも着手しない

- [X] T002 `MFCApplication1/CanSim/CanTypes.h` に `data-model.md` の値型を実装する: `Network`（`BitRateBps`, `Nodes`）、`Node`（`NodeId`, `Name`, `Frames`, `RxFilter`, `TxState` 列挙 `Idle/ArbitratingTransmitting/Won_Complete/Lost_Waiting/RetryPending`, `PendingFrames`）、`FrameDefinition`（`CanId`, `Dlc`, `Data`, `PeriodUs`, `InitialOffsetUs`）、`PendingFrame`（`Definition`, `TxRequestUs`, `CurrentData`, `RetryCount`, `OverwriteCount`）、`Event`（`TimeNs`, `Kind`, `NodeId`）、`LogRow`（`TimestampUs`, `CanId`, `Dlc`, `TxNodeId`, `RxNodeId`, `TxRequestUs`, `TxStartUs`, `TxEndUs`, `RxStartUs`, `RxEndUs`, `RxProcessUs`, `BusLoadPct`, `Status`, `RetryCount`, `OverwriteCount`)。MFC/GUI型に依存しないプレーン C++ とする。

- [X] T003 [P] `MFCApplication1/CanSim/EventQueue.h/.cpp` に DES イベントキューを実装する（T002 の `Event` を使用）。`(TimeNs, Kind の優先順位, NodeId)` の順で決定的にソートし処理する（FR-006, FR-007: 「同一構成からは必ず同じ結果が得られる」再現性を満たす固定順序とする）。

- [X] T004 [P] `MFCApplication1/CanSim/FrameTiming.h/.cpp` に、ビットスタッフィング（同一レベル5ビット連続で反転ビットを1つ挿入）を適用したビット数からフレーム伝送時間を計算する関数を実装する（FR-008, FR-009）。送信完了後3ビットのフレーム間休止期間（インターミッション）の長さも算出できるようにする（FR-010）。

- [X] T005 [P] `MFCApplication1/CanSim/ConfigLoader.h/.cpp` に、`contracts/config-input-format.md` の形式で設定ファイルを読み込み `Network`/`Node`/ `FrameDefinition`（T002）を構築する処理を実装する。以下の検証ルールを満たすこと:
  - ノード数は「1〜16」。17件以上は読み込み時に拒否する（FR-001）。【のちの変更】0 件も読込みエラーにした（FR-001 の追記。機能 004 で一度許容したものを戻した。テストは `F001_FR001_ZeroNodes_Rejected`）。
  - `[Network].BitRateBps` は「125000 / 250000 / 500000 / 1000000 のいずれか」（既定500000）（FR-002）。
  - 「データのバイト数が DLC と合わない構成は拒否する」（FR-003）。
  - 「1つの ID を送信するノードは1つに限る。違反した構成は読み込み時に拒否する」（FR-004）。
  - `RxFilter` 省略時は「全 ID を受信する」（FR-005）。
  - 拒否時は該当箇所とエラー理由を返せるようにする（US1 Acceptance Scenario #2）。

- [X] T006 `MFCApplication1/MFCApplication1.vcxproj` と `.vcxproj.filters` に T002〜T005 のファイルを Debug/Release × Win32/x64 の全構成に追加し、単体でコンパイルが通ることを確認する

**Checkpoint**: Foundational 完了 — US1〜US3 の実装に着手可能

---

## Phase 3: User Story 1 - ネットワークを構成してシミュレーションを実行する (Priority: P1) 🎯 MVP

**Goal**: 設定ファイルからネットワーク/ノード/送信フレーム一覧を読み込み、指定時間シミュレーションを実行して、送受信の記録を CSV ログとして出力する。

**Independent Test**: `quickstart.md` 手順1・2（サンプル構成の実行とログ出力の確認、および17ノード構成・ID重複構成の拒否確認）で単独に検証できる。

### Implementation for User Story 1

- [X] T007 [P] [US1] `MFCApplication1/CanSim/Network.h/.cpp` に、ノードの送信状態遷移（T002 の `TxState`）と、バスアイドル時の調停勝者決定（送信待ちを持つ通常動作中ノードのうち、先頭フレームの ID が最小のノードが勝つ。ビット単位の比較は行わない、FR-012）、ならびに送信成功フレームの受信配送（「通常動作中かつ受信フィルタに合致する送信元以外の全ノードに受信させる」FR-015、受信完了時刻に受信処理遅延を加えて受信処理時刻とする FR-016）を実装する。

- [X] T008 [US1] `MFCApplication1/CanSim/SimulationEngine.h/.cpp` に、T003 の `EventQueue` / T005 の `ConfigLoader` / T007 の `Network` / T004 の `FrameTiming` を統合した DES メインループを実装する。イベント種別（送信要求・バスアイドル・送信完了・インターミッション終了・受信処理・終了）を FR-006 のとおり処理し、利用者が指定したシミュレーション時間で終了する（FR-021）。（依存: T002-T005, T007）

- [X] T009 [P] [US1] `MFCApplication1/CanSim/Logger.h/.cpp` に、`contracts/log-output-format.md` の列定義（`Timestamp_us,CAN_ID,DLC,Tx_Node,Rx_Node, Tx_Request,Tx_Start,Tx_End,Rx_Start,Rx_End,Rx_Process,Bus_Load_Pct,Status,Retry_Count, Overwrite_Count`）に従い、1フレーム×受信ノードごとに1行（受信ノードがないフレームは送信側情報のみで1行）を CSV（UTF-8、ヘッダ行あり）としてファイル出力する処理を実装する（FR-017）。

- [X] T010 [US1] `MFCApplication1/MFCApplication1.rc` と `Resource.h` に、設定ファイル選択（ファイルダイアログ）・シミュレーション時間入力・実行ボタン・結果表示用のコントロールを追加する

- [X] T011 [US1] `MFCApplication1/MFCApplication1Dlg.h/.cpp` に、T010 のコントロールに対応するハンドラを実装する: 設定ファイルを T005 の `ConfigLoader` で読み込み、失敗時はエラー理由（ノード数超過／ID重複等）をメッセージ表示して実行しない（US1 Acceptance Scenario #2）。成功時は T008 の `SimulationEngine` を実行し、完了後に T009 で出力したログファイルのパスを表示する。（依存: T008, T009, T010）

- [X] T012 [US1] `MFCApplication1.vcxproj` / `.vcxproj.filters` に T007・T008・T009 をDebug/Release × Win32/x64 の全構成に追加し、ビルドが通ることを確認する

- [X] T013 [P] [US1] `MFCApplication1/CanSim/SampleConfigs/sample_appendix_a.cfg`として、ドメイン仕様書 付録A 相当（6ノード・8フレーム・500 kbps）のサンプル構成を`contracts/config-input-format.md` の形式で作成する（`quickstart.md` 手順1で使用）

**Checkpoint**: User Story 1 が単独で完全に機能し、テスト可能な状態

---

## Phase 4: User Story 2 - 調停と自動再送信を確認する (Priority: P2)

**Goal**: 複数ノードが同時に送信要求を出した場合に、ID の小さいフレームが優先されて先に送信され、負けたフレームは自動的に再送信されることを、ログ上のステータス・再送信回数で確認できるようにする。

**Independent Test**: `quickstart.md` 手順3（`CAN_ID=0x080` と `0x100` の `Tx_Start` 比較、`Status=ARBITRATION_LOST_RETRY` と `Retry_Count>=1` の確認）で単独に検証できる。

### Implementation for User Story 2

- [X] T014 [US2] `CanSim/Network.cpp`（T007）を拡張し、調停に負けたノードについて「そのフレームの再送信回数を1増やして待機に入る」（FR-013）、「勝ったフレームの EOF を検出した」後に「インターミッション後、バスがアイドルになった」時点で自動的に再送信を試みる状態遷移（`Lost_Waiting → RetryPending → ArbitratingTransmitting`、`data-model.md` の TxState 遷移図）を実装する。1ノードに送信待ちフレームが複数ある場合は「ID の小さい順に調停へ出す」（FR-014）。

- [X] T015 [US2] `CanSim/Network.cpp`（T007/T014）に、送信待ちの間に同じフレームの次周期の送信要求が来た場合の上書き処理を実装する:「データを最新値で上書きし、上書き回数を数える。送信要求時刻（Tx_Request）は最初の要求の時刻のまま残す」（FR-011）。

- [X] T016 [US2] `CanSim/Logger.cpp`（T009）を拡張し、ログ行の `Status` 列に`SUCCESS` または `ARBITRATION_LOST_RETRY` を、`Retry_Count` / `Overwrite_Count` 列にT014/T015 で記録した再送信回数・上書き回数を正しく反映する。

- [X] T017 [P] [US2] `CanSim/SampleConfigs/sample_collision.cfg` を、T013 のサンプルをベースに ECU3 (`0x080`) と ECU1 (`0x100`) の `InitialOffsetUs` を同じ値に揃えた構成として作成する（`quickstart.md` 手順3で使用する衝突誘発構成）

- [X] T018 [US2] `quickstart.md` 手順3を T017 の構成で手動実行し、`0x080` の `Tx_Start` が`0x100` より早いこと、`0x100` 側の行が `Status=ARBITRATION_LOST_RETRY` かつ`Retry_Count>=1` であることを確認して結果を記録する（依存: T014-T017）

  実装メモ: T014/T015（調停・自動再送信・上書き）は、DES ループが1つのまとまった機能であるため、実際には T007（`Network.cpp`）の時点で完全実装済み。T016（Status/Retry_Count/ Overwrite_Count のログ反映）も `SimulationEngine.cpp` の `HandleTxComplete` に実装済み。T018 の検証は、GUI 手動操作の代わりに、CanSim を UI 非依存にプレーン C++ として実装した利点を活かし、`cl.exe` で直接ビルドした検証ハーネスで自動化して実施した（当初はスクラッチ領域のみに置いていたが、機能 002 完了後に `tools/cansim_runner/` としてリポジトリへ移した。research.md 決定3 の改定を参照）。結果: `sample_appendix_a.cfg`（6ノード・10秒）で0x080 の初回 `Tx_Start`=140us、0x100 の初回 `Tx_Start`=576us で 0x080 が先行し、0x100 側に`Status=ARBITRATION_LOST_RETRY` と `Retry_Count>=1` を確認した（21件のチェックすべて PASS。再現性 SC-001、調停順 SC-002、送信回数 SC-004、ビットスタッフィング SC-003、構成拒否US1 AS#2、上書き FR-011 も同ハーネスで確認済み）。

**Checkpoint**: User Story 1 と 2 がともに単独で機能する状態

---

## Phase 5: User Story 3 - 性能指標を評価する (Priority: P3)

**Goal**: シミュレーション実行後のログから、総送信レイテンシ・送信待ち遅延・フレーム伝送時間・バス負荷率・ジッターを ID ごと・ノードごと・全体で算出できるようにする。

**Independent Test**: `quickstart.md` 手順4（集計サマリ出力の確認、`Bus_Load_Pct` が付録Aの試算値 約7.8% に近いことの確認）で単独に検証できる。

### Implementation for User Story 3

- [X] T019 [P] [US3] `MFCApplication1/CanSim/MetricsAggregator.h/.cpp` に、T009 の `LogRow` 群から `data-model.md` の `MetricsSummary` を算出する処理を実装する:総送信レイテンシ `RxProcessUs - TxRequestUs`、送信待ち・調停遅延`TxStartUs - TxRequestUs`、フレーム伝送時間 `TxEndUs - TxStartUs`（以上 FR-018）、バス負荷率「ΣTx_Duration / T_window × 100」（既定ウィンドウ100ms、1フレームの Tx_Duration は重複カウントしない、インターミッションは含めない）（FR-018, FR-019 該当部）、ID ごとのジッター「連続する Tx_Start の間隔 − 送信周期」の最大・最小・標準偏差（FR-019）。集計はID ごと・ノードごと・全体で算出する（FR-020）。

- [X] T020 [US3] `CanSim/SimulationEngine.cpp`（T008）と `CanSim/Logger.cpp`（T009）を拡張し、シミュレーション終了時に T019 の `MetricsAggregator` を呼び出し、`contracts/log-output-format.md` の「集計サマリ出力」（CSV）をログと同じ出力先に書き出す。（依存: T019）

- [X] T021 [US3] `MFCApplication1.vcxproj` / `.vcxproj.filters` に T019 を Debug/Release ×Win32/x64 の全構成に追加し、ビルドが通ることを確認する

- [X] T022 [US3] `MFCApplication1Dlg.cpp`（T011）の完了メッセージに、T020 で出力した集計サマリファイルのパスも表示するよう拡張する（依存: T020）

**Checkpoint**: User Story 1〜3 すべてが単独で機能する状態

---

## Phase 6: Polish & Cross-Cutting Concerns

**Purpose**: 複数ユーザーストーリーにまたがる仕上げ

- [X] T023 [P] `CLAUDE.md` の「アーキテクチャ（MFC アプリケーション）」節を更新し、`CanSim/` 配下のシミュレーションコア構成と `MFCApplication1Dlg` からの呼び出し方を追記する（憲章 V、`plan.md` Constitution Check の条件付きPASS事項への対応）

  併せて、実装中に判明した「ソリューション構成の Win32 は msbuild 上では `x86`」「`framework.h`に `NOMINMAX` を追加」「`CanSim/` は ASCII のみ・日本語 UI 文字列は `.rc` 側に閉じ込める」「`.rc` は UTF-16LE のためエンコーディングを明示して編集する」を CLAUDE.md のビルド節に追記した。

- [X] T024 `quickstart.md` の手順1〜5をすべて通しで手動実行し、再現性（SC-001, 手順5）を含めて結果を記録する

  実施内容: GUI を手動クリックする代わりに、ビルド済み `MFCApplication1.exe`（Debug|x64）を実際に起動し、Win32 API（`GetDlgItem` + `SendMessage(WM_SETTEXT/BM_CLICK)`）でコントロールID（`IDC_EDIT_CONFIG_PATH`=1000, `IDC_EDIT_DURATION`=1002, `IDC_BUTTON_RUN`=1003）を直接操作し、`sample_collision.cfg`（5秒）を実行させた。結果、実ファイルとして`sample_collision.cfg.log.csv`（1000行 + ヘッダ）と `.summary.csv` が生成され、内容は期待どおり: 0x080 が `Tx_Start=0` で即勝利、0x100 が `Tx_Start=258`・`Status=ARBITRATION_LOST_RETRY`・`Retry_Count=1` で追従、これが10ms周期で500回ずつ完全に再現された（ジッター0、QueueingDelay/TxDuration/BusLoad も手計算と一致）。これは US1〜US3のquickstart手順1〜4に相当する経路を実アプリ経由で検証したものであり、手順5の再現性は`specs/001-can-simulation-core/tasks.md` T018 の自動化ハーネス（同一構成2回実行でログ完全一致）で別途確認済み。検証に使ったテスト出力ファイルはリポジトリから削除済み。ウィンドウのタイトルが "CAN Bus Simulator" で正しく表示されることも確認した（T010 のリソース変更が正しく反映されている証拠）。

- [X] T025 Debug/Release × Win32/x64 の全4構成でソリューションがビルドできることを確認する（憲章 II）

  `msbuild MFCApplication1\MFCApplication1.sln /p:Configuration={Debug,Release} /p:Platform={x64,x86}` の4組み合わせすべてで警告・エラーなしにビルドが成功することを確認した（ソリューション構成上の Win32 プラットフォーム名は `x86`）。

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: 依存なし。即着手可能
- **Foundational (Phase 2)**: Setup 完了後。全ユーザーストーリーをブロックする
- **User Story 1 (Phase 3)**: Foundational 完了後に着手可能。他ストーリーへの依存なし
- **User Story 2 (Phase 4)**: Foundational 完了後に着手可能だが、`Network.cpp`/`Logger.cpp`を User Story 1（T007, T009）と共有するため、実質的に User Story 1 完了後に着手する
- **User Story 3 (Phase 5)**: Foundational 完了後に着手可能だが、集計対象のログを生成するUser Story 1（T008, T009）完了後に着手する
- **Polish (Phase 6)**: 実施したいユーザーストーリーすべての完了後

### Within Each User Story

- モデル/基盤（Foundational）→ サービス（Network/SimulationEngine/Logger）→ UI 統合→ ビルド設定への登録、の順で進める

### Parallel Opportunities

- Foundational: T003, T004, T005 は T002 完了後に並行実施可能
- User Story 1: T007 と T009 は並行実施可能（依存関係なし）。T013（サンプル構成ファイル）も他タスクと並行実施可能
- User Story 2: T017（サンプル構成ファイル）は T014-T016 と並行実施可能
- User Story 3: T019 は他ストーリーの完了を待たずに着手可能（依存は T009 の `LogRow` 定義のみ）

---

## Parallel Example: Foundational

```bash
# T002 完了後、以下を並行実施:
Task: "CanSim/EventQueue.h/.cpp に DES イベントキューを実装"
Task: "CanSim/FrameTiming.h/.cpp に伝送時間計算を実装"
Task: "CanSim/ConfigLoader.h/.cpp に設定ファイル読み込み・検証を実装"
```

## Parallel Example: User Story 1

```bash
Task: "CanSim/Network.h/.cpp に調停・受信配送ロジックを実装"
Task: "CanSim/Logger.h/.cpp に CSV ログ出力を実装"
Task: "CanSim/SampleConfigs/sample_appendix_a.cfg を作成"
```

---

## Implementation Strategy

### MVP First (User Story 1 のみ)

1. Phase 1: Setup を完了
2. Phase 2: Foundational を完了（全ストーリーをブロックするため必須）
3. Phase 3: User Story 1 を完了
4. **STOP と検証**: `quickstart.md` 手順1・2で User Story 1 を単独確認
5. 必要ならここでいったんデモ・共有

### Incremental Delivery

1. Setup + Foundational → 基盤完了
2. User Story 1 追加 → 単独テスト → MVP としてデモ可能
3. User Story 2 追加 → 単独テスト（`quickstart.md` 手順3）
4. User Story 3 追加 → 単独テスト（`quickstart.md` 手順4）
5. Polish（CLAUDE.md 更新・全構成ビルド確認・再現性確認）

---

## Notes

- `[P]` タスク = 異なるファイル、未完了タスクへの依存なし
- `[Story]` ラベルはトレーサビリティのためユーザーストーリーに対応させている
- User Story 2・3 は User Story 1 が構築する `Network`/`Logger` を拡張する形であり、完全に独立したコードベースではないが、`quickstart.md` の対応手順により単独で検証できる
- 各タスク（または論理的なまとまり）ごとにコミットする
- 各チェックポイントで、該当ユーザーストーリーが単独で動作することを確認してから次へ進む
