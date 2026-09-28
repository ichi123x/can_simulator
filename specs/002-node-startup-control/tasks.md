---

description: "Task list template for feature implementation"
---

# Tasks: ノードのスタートアップ制御（電源投入〜通常動作移行）

**Input**: Design documents from `/specs/002-node-startup-control/`

**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/, quickstart.md（すべて存在）

**Tests**: `spec.md` はテストタスクを明示的に要求していないため、テストタスクは含めない
（`plan.md` の Technical Context に従い、自動テスト基盤は本フェーズでも導入しない）。検証は
`quickstart.md` の手順で行い、必要に応じて `cl.exe` による一時的な検証ハーネス（リポジトリには
含めない）を使う。

**Organization**: タスクは `spec.md` のユーザーストーリー（US1〜US3）ごとにグループ化する。

## Format: `[ID] [P?] [Story] Description`

- **[P]**: 並行実行可能（異なるファイル、未完了タスクへの依存なし）
- **[Story]**: 対応するユーザーストーリー（US1, US2, US3）
- 各タスクに正確なファイルパスを含める

## Path Conventions

既存の単一 VS プロジェクト `MFCApplication1/` 配下。シミュレーションコアは
`MFCApplication1/CanSim/`（機能 001 で作成済み）を拡張する。

---

## Phase 1: Setup

**Purpose**: 挙動変更の前後比較（SC-006）に使う基準データを確保する

- [X] T001 本機能の実装に着手する**前に**、現在のビルドで
  `MFCApplication1/CanSim/SampleConfigs/sample_appendix_a.cfg` を10秒間実行し、生成された
  `sample_appendix_a.cfg.log.csv` をスクラッチ領域（リポジトリ外）へ退避する。SC-006 の
  「全時刻が一律 +22 µs されるだけで、行数・順序・`Status`・`Retry_Count`・`Overwrite_Count` は
  変化しない」を後で機械的に比較するための基準とする

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: US1〜US3 すべてが依存する起動状態の表現・設定読み込み・同期判定・イベント処理を実装する

**⚠️ CRITICAL**: このフェーズが完了するまで、いずれのユーザーストーリーの実装にも着手しない

- [X] T002 `MFCApplication1/CanSim/CanTypes.h` に `data-model.md` の追加定義を実装する:
  - `StartupState` 列挙: 「`PoweredOff` / `Initializing` / `BusSyncDetecting` / `Normal`。この順に
    のみ遷移する」（FR-002）
  - `Node` への追加フィールド: `PowerOnTimeUs`（「整数（0以上、既定0）」）、`InitDurationUs`
    （「整数（0以上、既定0）」）、`StartupState`（初期値 `PoweredOff`）、`InitDoneUs`、
    `NormalEntryUs`（いずれも「整数 or 未設定」）、`SyncTargetNs`（「整数 or 未設定。
    `BusSyncDetecting` のときのみ意味を持つ」）
  - `EventKind` に `PowerOn` / `InitDone` / `SyncComplete` を追加し、同一時刻の処理順序を
    `TxComplete → IntermissionEnd → RxProcess → PowerOn → InitDone → SyncComplete → TxRequest →
    BusIdle → SimulationEnd` となるよう列挙値を並べ替える（既存種別どうしの相対順序は変えない、
    research.md 決定3）

- [X] T003 [P] `MFCApplication1/CanSim/ConfigLoader.cpp` に、`[NodeN]` セクションの
  `PowerOnTimeUs` と `InitDurationUs` の読み込みを追加する（`contracts/config-input-format-delta.md`）。
  いずれも「任意（既定 0）」「0以上の整数」とし、整数として解釈できない値・負の値は既存の
  `ReceiveDelayUs` と同じ書式のエラーメッセージで読み込みを拒否する（FR-001）。既存の構成ファイル
  （キーを持たないもの）がそのまま読めることを壊さないこと

- [X] T004 [P] `MFCApplication1/CanSim/StartupControl.h/.cpp` を新規作成し、次を実装する:
  - 同期完了時刻の算出: `windowStart = max(検出開始時刻, RecessiveRunStartNs)`、
    `syncComplete = windowStart + 11 * ビット時間`（research.md 決定1、FR-005/FR-006）
  - `StartupRecord` 構造体（`data-model.md`）: `NodeId` / `NodeName` / `PowerOnUs` /
    `InitDoneUs`（未到達なら空）/ `NormalEntryUs`（未到達なら空）/ `ReachedNormal`
  - `Network` から `StartupRecord` 一覧を生成する関数と、`contracts/startup-log-format.md` の列
    `Node_Id,Node_Name,Power_On_us,Init_Done_us,Normal_Entry_us,Reached_Normal` に従った CSV
    （UTF-8、ヘッダ行あり、ノード番号昇順）の書き出し関数（FR-011, FR-012, FR-013, FR-014）。
    `Node_Name` にカンマが含まれる場合は二重引用符で囲む
  - MFC/GUI 型に依存しないプレーン C++ とし、ファイル内は ASCII のみで記述する（CLAUDE.md の
    エンコーディング方針）

- [X] T005 `MFCApplication1/CanSim/SimulationEngine.h/.cpp` に起動シーケンスを組み込む
  （依存: T002, T004）:
  - 実行開始時に全ノードの `PowerOn` イベントを `PowerOnTimeUs` の時刻で登録する
  - `PowerOn` 処理: 状態を `Initializing` にし、`InitDurationUs` 後の `InitDone` を登録
  - `InitDone` 処理: 状態を `BusSyncDetecting` にし、`InitDoneUs` を記録。T004 の式で同期完了
    予定時刻を算出して `SyncComplete` を登録し、ノードの `SyncTargetNs` に保持
  - `SyncComplete` 処理: 「ノードがまだ `BusSyncDetecting`」かつ「イベント時刻が
    `SyncTargetNs` と一致する」場合のみ状態を `Normal` にして `NormalEntryUs` を記録する
    （不一致なら古いイベントとして破棄、research.md 決定2）
  - `SyncComplete` で通常動作に入った際、バスが空いていてそのノードに送信待ちがあれば `BusIdle`
    を積む（research.md 決定3の実装上の注意。これがないと初回オフセット0の構成で1フレームも
    送信されない）
  - 送信開始時に `RecessiveRunStartNs` を「送信開始時刻 + 伝送時間 − 8ビット時間」へ更新する
    （`data-model.md`。シミュレーション開始時の初期値は 0）

- [X] T006 `MFCApplication1/MFCApplication1.vcxproj` と `.vcxproj.filters` の `CanSim` フィルタに
  `CanSim\StartupControl.h` / `CanSim\StartupControl.cpp` を追加し、Debug/Release × x64/x86 の
  全4構成でビルドが通ることを確認する

**Checkpoint**: Foundational 完了 — US1〜US3 の実装に着手可能

---

## Phase 3: User Story 1 - ノードごとの起動シーケンスを模擬して通常動作開始時刻を確認する (Priority: P1) 🎯 MVP

**Goal**: ノードごとに電源投入時刻・初期化時間を設定して実行すると、各ノードが4状態を遷移し、
通常動作開始時刻が起動記録として出力される。

**Independent Test**: `quickstart.md` 手順1・2（`sample_startup.cfg` の実行で
`Normal_Entry_us` が `Power_On_us + Init_Duration + 22` と一致すること、シミュレーション時間より
後に電源投入されるノードが `Reached_Normal=FALSE` になり1件も送信しないこと）で単独に検証できる。

### Implementation for User Story 1

- [X] T007 [US1] `MFCApplication1/CanSim/Network.cpp` の `RunArbitration` を変更し、調停候補を
  `StartupState == Normal` のノードに限定する（FR-008: 「通常動作状態でないノードを調停に参加させて
  はならず、そのノードのフレームを送信してはならない」）。通常動作前のノードの送信待ちフレームは
  破棄せずキューに残すこと（FR-010）

- [X] T008 [US1] `MFCApplication1/CanSim/SimulationEngine.cpp` の `HandleTxComplete` の受信配送を
  変更し、受信対象を `StartupState == Normal` のノードに限定する（FR-009: 「通常動作状態でない
  ノードを受信側とするログ行を生成してはならない」）

- [X] T009 [US1] `MFCApplication1/MFCApplication1Dlg.cpp` の実行ハンドラに、T004 の起動記録を
  `<設定ファイル名>.startup.csv` として書き出す数行を追加し、完了メッセージに出力パスを加える
  （新しいコントロール・操作は追加しない）。（依存: T004, T005）

- [X] T010 [P] [US1] `MFCApplication1/CanSim/SampleConfigs/sample_startup.cfg` を作成する:
  6ノードの `PowerOnTimeUs` を 0 / 10000 / 20000 / 30000 / 40000 / 50000、`InitDurationUs` は
  すべて 0、各ノードに周期 100000 µs・初回オフセット 0・DLC 8 のフレームを1つ（CAN ID は重複
  しない昇順）、`BitRateBps=500000` とする（`quickstart.md` 手順1で使用）

- [X] T011 [US1] `quickstart.md` 手順1・2を実行して結果を記録する: 6ノードの `Normal_Entry_us` が
  22 / 10022 / 20022 / 30022 / 40022 / 50022 µs と一致すること（SC-001, SC-003）、および1ノードの
  `PowerOnTimeUs` をシミュレーション時間より後に変更した場合に `Reached_Normal=FALSE`・
  `Init_Done_us`/`Normal_Entry_us` が空欄・そのノード発の送信が0件であること（FR-013）
  （依存: T007-T010）

**Checkpoint**: User Story 1 が単独で完全に機能し、テスト可能な状態

---

## Phase 4: User Story 2 - 起動前の送信要求が通常動作後に送信されることを確認する (Priority: P2)

**Goal**: 通常動作に入る前に発生した送信要求が破棄されず、送信要求時刻を保持したまま通常動作後に
送信される。

**Independent Test**: `quickstart.md` 手順3（フレームログの各行の `Tx_Start` が送信ノードの
`Normal_Entry_us` 以上であること、電源投入を遅らせたノードの最初の行の `Tx_Request` が 0 のまま
であること）で単独に検証できる。

### Implementation for User Story 2

- [X] T012 [US2] `quickstart.md` 手順3を `sample_startup.cfg` の実行結果に対して実施し、
  結果を記録する: フレームログ全行で `Tx_Start >= 送信ノードの Normal_Entry_us`（SC-002、
  通常動作前の送信が0件）、かつ電源投入を遅らせたノードの最初の行の `Tx_Request` が最初に要求が
  発生した時刻（0）のまま保持され、`Tx_Start − Tx_Request` に起動待ち時間が含まれること（FR-010）
  （依存: T007, T011）

- [X] T013 [US2] 起動待ち中の上書き挙動（`spec.md` エッジケース）を確認して記録する:
  `sample_startup.cfg` の電源投入 50000 µs のノードのフレーム周期を 10000 µs に変更した構成で実行し、
  そのノードの最初のログ行の `Overwrite_Count` が保留中に発生した周期回数ぶん累積していること、
  それでも送信が1回だけ行われることを確認する（機能 001 の上書き規則が起動待ちにもそのまま適用
  されること）

- [X] T014 [US2] 性能指標が送信済みフレームのみを対象としていること（FR-015）を
  `sample_startup.cfg` の `summary.csv` で確認する: 通常動作に到達しなかったノード・未送信の保留
  フレームが `Count` に含まれていないこと（依存: T011）

**Checkpoint**: User Story 1 と 2 がともに単独で機能する状態

---

## Phase 5: User Story 3 - 送信中のバスに後から参加するノードの同期を確認する (Priority: P3)

**Goal**: 他ノードの送信中に同期検出を始めたノードが、そのフレームのフレーム間休止期間の終了時点で
通常動作に入る。

**Independent Test**: `quickstart.md` 手順4（後発ノードの `Normal_Entry_us` が進行中フレームの
`Tx_End + 6 µs` と一致すること）で単独に検証できる。

### Implementation for User Story 3

- [X] T015 [US3] `MFCApplication1/CanSim/SimulationEngine.cpp` に遅延無効化による再スケジュールを
  実装する（research.md 決定2）: 送信が開始されるたびに、`StartupState == BusSyncDetecting` の
  全ノードについて T004 の式で同期完了予定時刻を再計算し、新しい `SyncComplete` イベントを積んで
  ノードの `SyncTargetNs` を更新する。これにより、レセシブ期間が途切れたノードは計数をやり直す
  （FR-007）。`EventQueue` には取り消し機構を追加しないこと（依存: T005）

- [X] T016 [P] [US3] `MFCApplication1/CanSim/SampleConfigs/sample_startup_latejoin.cfg` を作成する:
  ノードA（`PowerOnTimeUs=0`）が周期 300 µs・DLC 8 のフレームを送信し続け、ノードB
  （`PowerOnTimeUs=100`、`InitDurationUs=0`）の同期検出開始がノードAのフレーム送信中になる構成
  （`BitRateBps=500000`）（`quickstart.md` 手順4で使用）

- [X] T017 [US3] `quickstart.md` 手順4を実行して結果を記録する: ノードBの `Normal_Entry_us` が、
  検出開始時刻を含んでいたノードAのフレームの `Tx_End + 6 µs`（フレーム間休止期間3ビット分の終了
  時刻）と一致すること（SC-004）。想定値: ノードAが 22〜274 µs で送信していれば、ノードBは 280 µs
  （依存: T015, T016）

**Checkpoint**: User Story 1〜3 すべてが単独で機能する状態

---

## Phase 6: Polish & Cross-Cutting Concerns

**Purpose**: 再現性・後方互換の確認と、複数ユーザーストーリーにまたがるドキュメント整備

- [X] T018 `quickstart.md` 手順5（SC-005）を実施する: `sample_startup.cfg` を同一構成・同一時間で
  2回実行し、`startup.csv` と `log.csv` の両方が1バイト単位で完全に一致することを確認する

- [X] T019 `quickstart.md` 手順6（SC-006）を実施する: `sample_appendix_a.cfg` を10秒間実行し、
  T001 で退避した基準ログと比較して、`Tx_Start` / `Tx_End` / `Rx_Start` / `Rx_End` / `Rx_Process` /
  `Timestamp_us` が一律 +22 µs されているだけで、行数・行の順序・`CAN_ID`・`Tx_Node` / `Rx_Node`・
  `Status`・`Retry_Count`・`Overwrite_Count` が変化していないことを確認する（想定: `0x080` の初回
  `Tx_Start` が 140 µs → 162 µs）（依存: T001, T011）

  **実測結果と仕様の訂正**: 行数（16750行）・行の順序・`CAN_ID`・`DLC`・`Tx_Node`/`Rx_Node`・
  `Status`・`Retry_Count`・`Overwrite_Count` は**1件も変化せず**（差分0件）、`0x080` の初回
  `Tx_Start` は 140 → 162 µs、`0x100` は 576 → 598 µs で想定どおりだった。一方、時刻列の差分は
  「一律 +22 µs」ではなく **0 または +22 µs の2値**だった（16750行中、起動直後の連続送信区間の
  40行のみ +22 µs、残り 16710 行は差分 0）。バスが一度空くと送信は要求時刻どおりに再開されるため、
  ずれが起動直後の区間に限定されるのが理由。`Tx_Request` は送信要求が起動状態と無関係に発生する
  ため常に差分 0。この実測に合わせて `spec.md` の SC-006 と `quickstart.md` 手順6 を訂正した
  （当初の「一律シフト」という予測より影響範囲が限定的であることが判明した形）。

- [X] T020 [P] `specs/001-can-simulation-core/quickstart.md` の手順3・4の期待値を、起動オフセットを
  含んだ実挙動の値へ書き換える。あわせて
  `specs/001-can-simulation-core/contracts/log-output-format.md` の例示 CSV には「機能 002 適用後の
  実行では各時刻に起動待ち時間（11ビット時間）が加わる」旨の注記を追加する（例示値そのものは
  ドメイン仕様書 FR-054 の引用のため書き換えない）（research.md 決定6、憲章 V）（依存: T019）

- [X] T021 [P] `CLAUDE.md` のアーキテクチャ節に `CanSim/StartupControl.h/.cpp` を追記し、起動記録
  `<設定ファイル名>.startup.csv` の存在と、既定構成でも全時刻が11ビット時間ぶん後ろへずれる仕様
  （SC-006）を明記する（憲章 V、`plan.md` Constitution Check の条件付きPASS事項への対応）

- [X] T022 Debug/Release × x64/x86 の全4構成でソリューションがビルドできることを確認する
  （憲章 II。ソリューション構成上の Win32 プラットフォーム名は `x86`）

  4構成すべてで警告・エラー0件のビルド成功を確認。あわせて、ビルド済み `MFCApplication1.exe`
  （Debug|x64）を Win32 API 経由で実際に操作し（機能 001 の T024 と同じ手法）、`sample_startup.cfg`
  の実行で `.log.csv` / `.summary.csv` / `.startup.csv` の3ファイルが生成され、`startup.csv` の
  `Normal_Entry_us` が 22 / 10022 / 20022 / 30022 / 40022 / 50022 になることを実アプリ経由でも
  確認した（検証用の出力ファイルは削除済み）。

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: 依存なし。ただし T001 は**挙動を変更する前**に実施する必要がある（最優先）
- **Foundational (Phase 2)**: Setup 完了後。全ユーザーストーリーをブロックする
- **User Story 1 (Phase 3)**: Foundational 完了後に着手可能。他ストーリーへの依存なし
- **User Story 2 (Phase 4)**: 検証中心のため User Story 1（T007, T011）の完了後に実施する
- **User Story 3 (Phase 5)**: Foundational 完了後に着手可能。T015 は T005 の同期処理を前提とする
- **Polish (Phase 6)**: 実施したいユーザーストーリーすべての完了後。T019 は T001 の基準データが
  前提、T020 は T019 の実測結果が前提

### Within Each User Story

- コア（`CanSim/`）→ UI グルー（`MFCApplication1Dlg.cpp`）→ サンプル構成 → 検証、の順で進める

### Parallel Opportunities

- Foundational: T003（ConfigLoader）と T004（StartupControl）は T002 完了後に並行実施可能
- User Story 1: T010（サンプル構成ファイル）は T007〜T009 と並行実施可能
- User Story 3: T016（サンプル構成ファイル）は T015 と並行実施可能
- Polish: T020（機能 001 のドキュメント更新）と T021（CLAUDE.md）は互いに独立

---

## Parallel Example: Foundational

```bash
# T002 完了後、以下を並行実施:
Task: "CanSim/ConfigLoader.cpp に PowerOnTimeUs / InitDurationUs の読み込みを追加"
Task: "CanSim/StartupControl.h/.cpp に同期完了時刻の算出と起動記録 CSV 出力を実装"
```

## Parallel Example: User Story 1

```bash
Task: "CanSim/Network.cpp の調停候補を通常動作ノードに限定"
Task: "CanSim/SampleConfigs/sample_startup.cfg を作成"
```

---

## Implementation Strategy

### MVP First (User Story 1 のみ)

1. Phase 1: Setup（T001 の基準ログ退避は**必ず実装前に**）
2. Phase 2: Foundational を完了（全ストーリーをブロックするため必須）
3. Phase 3: User Story 1 を完了
4. **STOP と検証**: `quickstart.md` 手順1・2で起動記録が期待どおりか単独確認
5. この時点で「起動シーケンスが模擬され、通常動作開始時刻が記録される」価値が提供できる

### Incremental Delivery

1. Setup + Foundational → 基盤完了
2. User Story 1 追加 → 単独テスト → MVP
3. User Story 2 追加 → 単独テスト（`quickstart.md` 手順3）
4. User Story 3 追加 → 単独テスト（`quickstart.md` 手順4）
5. Polish（再現性 SC-005・後方互換 SC-006・ドキュメント同期・全構成ビルド）

---

## Notes

- `[P]` タスク = 異なるファイル、未完了タスクへの依存なし
- `[Story]` ラベルはトレーサビリティのためユーザーストーリーに対応させている
- User Story 2 は実装タスクを持たず検証タスクのみで構成される。これは、起動前の送信要求の保留が
  「調停候補を通常動作ノードに限定する（T007）」＋「機能 001 の既存の送信待ちキュー」の組み合わせ
  で自然に成立するため。機能 001 で調停・再送信を人為的に別タスクへ分割して実装が前倒しになった
  反省を踏まえ、本機能では実装の実態に合わせてタスクを配置している
- 本機能は憲章 [P②] の3系統のうち「エラー状態制御の枠」を対象としない（`/speckit-analyze` の指摘
  C1 として未起票のまま残る）
- 各タスク（または論理的なまとまり）ごとにコミットする
