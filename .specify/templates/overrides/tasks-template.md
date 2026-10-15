---

description: "機能実装のタスクリストのテンプレート（C++ / GoogleTest・テストファースト）"
---

# Tasks（タスク）: [機能名]

**Input（入力）**: `/specs/[###-feature-name]/` の設計ドキュメント

**Prerequisites（前提）**: plan.md（必須）、spec.md（ユーザーストーリーに必須）、research.md、data-model.md、contracts/

**Tests（テスト）**: テストは必須（憲章 原則 III）。spec.md の AC / B / SC [AUTO] の ID ごとに GoogleTest の
テストを 1 件書き、そのテストを通すコードより先に書いて、失敗することを確かめる。

**Organization（構成）**: タスクはユーザーストーリーごとにまとめ、ストーリーごとに単独で実装・テストできるようにする。

## Format（書式）: `[ID] [P?] [Story] 説明 (仕様の ID)`

- **[P]**: 並行して実行できる（別のファイルで、依存関係がない）
- **[Story]**: どのユーザーストーリーのタスクか（例: US1、US2、US3）
- **(仕様の ID)**: そのタスクが対応する spec.md の AC / B / SC / FR の ID。例: `(AC-001, B-01〜B-04)`
- 説明には正確なファイルパスを書く

## Test Conventions（テストの決まり）

- テストファイル: `tests/unit/<対象>_test.cpp`、`tests/integration/<流れ>_test.cpp`、`tests/acceptance/<機能>_acceptance_test.cpp`
- テスト名: `F<機能番号>_<ID>_<条件>_<結果>`。例: `F012_AC003_OverStock_FailsAndKeepsStock`
- 各テストで `RecordProperty("spec", "<機能番号>:<ID>")` を呼ぶ
- テストのコメントと、アサーションに付けるメッセージ（`<< "..."`）は日本語で書く。テスト名と ID は英数字のまま
- 境界値の表（B-xx）は、表の 1 行を 1 つのパラメータにした `TEST_P` 1 件にする
- テストの実行ファイルは `gtest_discover_tests(... PROPERTIES LABELS <unit|integration|acceptance>)` で登録する。時間のかかる検証には `slow` も付ける
- 処理時間・速度を測るテストは、ほかのテストと別のファイル（別の `add_gtest`）にして `slow` を付け、Release ビルドで実行する。Debug ビルドの時間で合否を決めない。閾値は spec.md の値から変えない
- 各タスクの後に実行する: `cmake --build build --config Debug && ctest --test-dir build -C Debug --output-on-failure -LE slow`

## Path Conventions（パスの決まり）

- **単一プロジェクト**: リポジトリ直下の `src/`、`tests/`
- 下のパスは単一プロジェクトを想定している。plan.md の構成に合わせて読み替える

<!--
  ============================================================================
  重要: 下のタスクは説明のための「例」である。

  /speckit-tasks コマンドは、これらを次の情報に基づく実際のタスクに置き換えること (MUST)。
  - spec.md のユーザーストーリー（優先度 P1、P2、P3 …）
  - spec.md の AC / B / SC の ID（manual 以外の ID は、それぞれちょうど 1 つのテストタスクに現れること）
  - plan.md の機能要件
  - data-model.md のデータ
  - contracts/ のインターフェース

  ルール:
  - 各ユーザーストーリーでは、「テスト」のまとまりを「実装」のまとまりより前に置く。
  - 各実装タスクには、それによって通るテストタスクを書く（「T010 を通す」）。
  - 各ユーザーストーリーの最後に「ctest を実行する」タスクを置く。
  - 先行するテストタスクのない実装タスクは認めない。

  タスクはユーザーストーリーごとにまとめ (MUST)、各ストーリーが次のようになること。
  - 単独で実装できる
  - 単独でテストできる
  - MVP の増分として提供できる

  生成した tasks.md に、これらの例のタスクを残さないこと。
  ============================================================================
-->

## Phase 1: Setup（共通の準備）

**目的**: プロジェクトの初期化と基本構成

- [ ] T001 実装計画に沿ってプロジェクトの構成を作る
- [ ] T002 CMakeLists.txt に、FetchContent（バージョン固定）で GoogleTest を取り込む設定と `enable_testing()` を追加する
- [ ] T003 tests/CMakeLists.txt を作る。テストの実行ファイルを追加し、テスト対象のライブラリと GTest::gtest_main / GTest::gmock をリンクし、LABELS 付きで gtest_discover_tests を呼ぶ補助関数を用意する
- [ ] T004 `ctest --test-dir build -C Debug` が動くことを確かめる（この時点ではテスト 0 件でよい）

---

## Phase 2: Foundational（すべてのストーリーの前提）

**目的**: どのユーザーストーリーを実装する前にも完了していなければならない基盤

**⚠️ 重要**: この段階が終わるまで、ユーザーストーリーの作業は始めない

基盤タスクの例（プロジェクトに合わせて調整する）:

- [ ] T005 外部依存（時刻・ファイル・乱数・ハードウェア）のインターフェースを定義し、テストで差し替えられるようにする
- [ ] T006 [P] 共通のモック・フェイクを tests/mocks/ に作る（例: MockClock）
- [ ] T007 すべてのストーリーが使う基本の型・データを作る

**Checkpoint（確認点）**: 基盤ができた。ここからユーザーストーリーを並行して実装できる

---

## Phase 3: User Story 1 - [タイトル] (Priority: P1) 🎯 MVP

**目標**: [このストーリーで何ができるようになるか]

**単独でのテスト方法**: [このストーリーだけで動くことをどう確かめるか]

### User Story 1 のテスト（先に書き、失敗することを確かめる） ⚠️

- [ ] T010 [P] [US1] tests/unit/[対象]_test.cpp に F###_AC001_[条件]_[結果] を書く (AC-001)
- [ ] T011 [P] [US1] tests/unit/[対象]_test.cpp にパラメータ化テスト F###_B01_B04_[名前] を書く (B-01〜B-04)
- [ ] T012 [US1] tests/acceptance/[機能]_acceptance_test.cpp に F###_AC002_[条件]_[結果] を書く (AC-002)
- [ ] T013 [US1] ビルドして ctest を実行し、T010〜T012 が想定どおりの理由で失敗することを確かめる（コンパイルエラーではないこと）

### User Story 1 の実装

- [ ] T014 [P] [US1] src/[データ1].h/.cpp に [データ1] を作る
- [ ] T015 [US1] src/[サービス].cpp に [サービス] を実装する — T010、T011 を通す (FR-001)
- [ ] T016 [US1] src/[ファイル].cpp に [機能] を実装する — T012 を通す (FR-002)
- [ ] T017 [US1] `ctest --test-dir build -C Debug --output-on-failure -LE slow` を実行し、以前の機能のテストも含めて全件成功することを確かめる

**Checkpoint（確認点）**: この時点で、User Story 1 が単独で動き、テストできる

---

## Phase 4: User Story 2 - [タイトル] (Priority: P2)

**目標**: [このストーリーで何ができるようになるか]

**単独でのテスト方法**: [このストーリーだけで動くことをどう確かめるか]

### User Story 2 のテスト（先に書き、失敗することを確かめる） ⚠️

- [ ] T018 [P] [US2] tests/unit/[対象]_test.cpp に F###_AC003_[条件]_[結果] を書く (AC-003)
- [ ] T019 [US2] ビルドして ctest を実行し、T018 が想定どおりの理由で失敗することを確かめる

### User Story 2 の実装

- [ ] T020 [US2] src/[サービス].cpp に [サービス] を実装する — T018 を通す (FR-003)
- [ ] T021 [US2] `ctest --test-dir build -C Debug --output-on-failure -LE slow` を実行し、全件成功することを確かめる

**Checkpoint（確認点）**: この時点で、User Story 1 と 2 がどちらも単独で動く

---

## Phase 5: User Story 3 - [タイトル] (Priority: P3)

**目標**: [このストーリーで何ができるようになるか]

**単独でのテスト方法**: [このストーリーだけで動くことをどう確かめるか]

### User Story 3 のテスト（先に書き、失敗することを確かめる） ⚠️

- [ ] T022 [P] [US3] tests/integration/[流れ]_test.cpp に F###_AC004_[条件]_[結果] を書く (AC-004)
- [ ] T023 [US3] ビルドして ctest を実行し、T022 が想定どおりの理由で失敗することを確かめる

### User Story 3 の実装

- [ ] T024 [US3] src/[ファイル].cpp に [機能] を実装する — T022 を通す (FR-004)
- [ ] T025 [US3] `ctest --test-dir build -C Debug --output-on-failure -LE slow` を実行し、全件成功することを確かめる

**Checkpoint（確認点）**: すべてのユーザーストーリーが単独で動く

---

[必要に応じて、同じ形でユーザーストーリーの段階を追加する]

---

## Phase N: Polish & Verification（仕上げと最終確認）

**目的**: 複数のストーリーにまたがる改善と、spec.md に対する最終確認

- [ ] TXXX 上で扱っていない SC [AUTO] のテストを書く（時間のかかるものには `slow` を付ける） (SC-001, SC-002)
- [ ] TXXX spec.md の manual 以外の AC / B / SC の ID が、それぞれちょうど 1 件のテストを持つことを確かめる（テスト名を ID で検索する）
- [ ] TXXX Release ビルドで、slow を含むすべてのラベルを実行する: `cmake --build build --config Release && ctest --test-dir build -C Release --output-on-failure`
- [ ] TXXX `GTEST_SHUFFLE=1` を付けて、順番を入れ替えながら 2 回実行する: `ctest --test-dir build -C Debug --output-on-failure --repeat until-fail:2`
- [ ] TXXX [P] ビルドやテストのコマンドが変わった場合は CLAUDE.md を更新する
- [ ] TXXX コードの整理とリファクタリング（テストは成功したままにする）
- [ ] TXXX quickstart.md の手順で確かめ、[MANUAL] の SC を人が確認する項目として一覧にする

---

## Dependencies & Execution Order（依存関係と実行順）

### 段階の依存関係

- **Setup（Phase 1）**: 依存なし。すぐに始められる
- **Foundational（Phase 2）**: Setup の完了が前提。すべてのユーザーストーリーを止める
- **ユーザーストーリー（Phase 3 以降）**: すべて Foundational の完了が前提
  - その後は並行して進めてもよい（人手があれば）
  - または優先度の順に 1 つずつ進める（P1 → P2 → P3）
- **Polish（最後の段階）**: 対象とするユーザーストーリーがすべて完了していることが前提

### ユーザーストーリー間の依存関係

- **User Story 1（P1）**: Foundational（Phase 2）の後に始められる。ほかのストーリーへの依存なし
- **User Story 2（P2）**: Foundational（Phase 2）の後に始められる。US1 と連携してもよいが、単独でテストできること
- **User Story 3（P3）**: Foundational（Phase 2）の後に始められる。US1 / US2 と連携してもよいが、単独でテストできること

### 各ユーザーストーリーの中での順序

- テストは実装より先に書き、失敗することを確かめる (MUST)（Red → Green → Refactor）
- 失敗したテストは実装を直して通す。期待値を書き換えて通してはならない
  （仕様が誤っていそうなときは作業を止め、spec.md の修正を人に相談する）
- データ → サービス → インターフェースの順に作る
- 中心の実装 → 連携の順に作る
- 次の優先度に進む前に、そのストーリーの最後の ctest が全件成功していること

### 並行して進められるところ

- [P] の付いた Setup のタスクは、すべて並行して実行できる
- [P] の付いた Foundational のタスクは、すべて並行して実行できる（Phase 2 の中で）
- Foundational が終われば、すべてのユーザーストーリーを並行して始められる（人手があれば）
- [P] の付いた同じストーリーのテストは、すべて並行して書ける
- [P] の付いた同じストーリーのデータは、並行して作れる
- 別々のユーザーストーリーは、別の担当者が並行して進められる

---

## 並行の例: User Story 1

```bash
# User Story 1 のテストをまとめて書く:
Task: "tests/unit/[対象]_test.cpp に F###_AC001_... を書く"
Task: "tests/unit/[対象]_test.cpp に F###_B01_B04_... を書く"

# その後、失敗することを確かめる:
cmake --build build --config Debug && ctest --test-dir build -C Debug --output-on-failure -L unit
```

---

## Implementation Strategy（実装の進め方）

### MVP を先に（User Story 1 だけ）

1. Phase 1: Setup を終える
2. Phase 2: Foundational を終える（重要: すべてのストーリーの前提）
3. Phase 3: User Story 1 を終える（テストが先、実装が後）
4. **止めて確かめる**: ctest が全件成功し、User Story 1 が単独で動く
5. 準備ができていればリリース・デモする

### 少しずつ届ける

1. Setup と Foundational を終える → 基盤ができる
2. User Story 1 を追加 → ctest が全件成功 → リリース・デモ（MVP）
3. User Story 2 を追加 → ctest が全件成功（US1 のテストも成功のまま） → リリース・デモ
4. User Story 3 を追加 → ctest が全件成功（それまでのテストもすべて成功のまま） → リリース・デモ
5. 各ストーリーは、以前のストーリーを壊さずに価値を足す

---

## Notes（注意）

- [P] のタスク = 別のファイルで、依存関係がない
- [Story] と (仕様の ID) で、各タスクを spec.md にたどれるようにする
- 各ユーザーストーリーは、単独で完了・テストできるようにする
- 実装の前に、テストが失敗することを確かめる
- 成功させるためにテストを無効化・削除・スキップしない（`DISABLED_`、理由のない `GTEST_SKIP`）
- タスクごと、またはまとまりごとにコミットする
- どの確認点で止めても、そのストーリーを単独で確かめられるようにする
- 避けること: あいまいなタスク、同じファイルの取り合い、ストーリーの独立性を壊す依存関係
