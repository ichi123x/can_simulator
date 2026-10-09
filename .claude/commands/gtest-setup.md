---
description: GoogleTest 連携の仕上げ（憲章の作成と最終確認）。setup.ps1 を実行した後に使う
argument-hint: "概要: <何を作るか> ドメイン原則: <準拠する規格など。なければ なし>"
---

# GoogleTest 連携の仕上げ

決まった作業（テンプレートのコピー、CMakeLists.txt の作成、フックの登録など）は `setup.ps1` が済ませている。
このコマンドでは、AI でなければできない作業だけを行う。

入力: $ARGUMENTS

## 手順 1. setup.ps1 の結果を確かめる

- `build/gtest-setup-report.txt` を読む。無ければ「先にキットの `setup.ps1` をプロジェクトのルートで実行してください」と伝えて止まる。
- 次がそろっていることを確かめる。足りないものがあれば、setup.ps1 の再実行をユーザーに頼んで止まる。
  - `.specify/templates/overrides/spec-template.md` と `tasks-template.md`
  - `CMakeLists.txt` と `tests/CMakeLists.txt`
  - `.claude/settings.json` の `hooks.Stop` に `run-tests.ps1` を実行する設定
  - `.claude/commands/coverage.md`、`.claude/commands/spec-tests.md`、`.claude/scripts/coverage.ps1`、`tests/coverage-hold.md`
- テンプレートのファイルは書き換えない（キットが正本）。

## 手順 2. プロジェクトに合わせて調整する（必要なときだけ）

報告に次のどちらかがあるときだけ行う。どちらも無ければ手順 3 へ進む。

**既存の CMakeLists.txt を変更しなかった場合**
既存の内容を壊さずに、次の 4 つを追加する。既に同じ働きの記述があれば追加しない。
- テスト対象のソースを `app_core` という静的ライブラリにまとめる（`add_library(app_core STATIC ...)`、`target_include_directories(app_core PUBLIC ...)`）
- GoogleTest の取得: `include(FetchContent)`、`FetchContent_Declare(googletest URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.zip DOWNLOAD_EXTRACT_TIMESTAMP TRUE)`、`set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)`、`FetchContent_MakeAvailable(googletest)`
- `enable_testing()`
- `add_subdirectory(tests)`

**既存の tests/CMakeLists.txt を変更しなかった場合**
`tests/support/console_utf8.cpp`（日本語の出力を Windows のコンソールで文字化けさせないファイル）を、すべてのテストの実行ファイル（`add_executable`）のソースに加える。既に `SetConsoleOutputCP(CP_UTF8)` を呼んでいるテストの実行ファイルには加えない。

**ビルドかテストが失敗した場合**
原因を調べて直す。よくある原因と直し方:
- ソースが MFC などの UI フレームワークや、プリコンパイル済みヘッダー（`pch.h`、`stdafx.h`）に依存している
  → UI に依存するファイルを `app_core` から外す、またはテスト用の空のヘッダーを `tests/stubs/` に置いてインクルードパスの先頭に入れる
- GoogleTest をダウンロードできない（社内プロキシなど） → ユーザーに伝えて止まる

守ること:
- 既存の Visual Studio のソリューション（`.sln` / `.vcxproj`）は変更しない。
- `tests/unit/smoke_test.cpp` を書き換えてテストを通してはならない。
- 直したら `cmake -S . -B build`、`cmake --build build --config Debug`、`ctest --test-dir build -C Debug --output-on-failure` を実行し、成功するまで繰り返す。

## 手順 3. 憲章を作る

`/speckit-constitution` を実行する。入力には、次のプロジェクト情報と「付録 A」の原則を渡す。

- プロジェクト名と C++ 規格: `CMakeLists.txt` の `project()` と `CMAKE_CXX_STANDARD` から取る
- 概要・ドメイン固有の原則: 上の「入力」から取る。無ければリポジトリ（README など）から推定し、推定したことを手順 4 で報告する。推定できなければユーザーに質問する
- 対象 OS / ツールチェーン: `build/gtest-setup-report.txt` のツールの確認結果から取る

`.specify/memory/constitution.md` が既に中身のある憲章なら、作り直さず、付録 A の原則を追加・改定する形にする（バージョンは Spec Kit の決まりに従って上げる）。

## 手順 4. 最終確認と報告

- `cmake --build build --config Debug` と `ctest --test-dir build -C Debug --output-on-failure` を実行し、全件成功を確かめる。
- 次を短く報告する: 変えたファイル、ctest の結果、推定した項目、残っている確認事項。
- フックは Claude Code を再起動するか `/hooks` を開くと有効になる、と伝える。
- `specs/` に作成済みの仕様があれば、次は `/spec-tests` でその仕様のテストを作れる、と伝える（機能の一覧と未テストの数も添える）。
- カバレッジを上げるときは `/coverage` を使える（OpenCppCoverage が必要）、と伝える。
- コミットはしない（ユーザーが確認してから行う）。

---

## 付録 A. 憲章に入れる原則

各原則に MUST / MUST NOT と Rationale を付けること。

### I. Spec Kit ワークフロー準拠
- 機能追加・仕様変更は `/speckit-specify` → `/speckit-plan` → `/speckit-tasks` → `/speckit-implement` の順に進める。必要に応じて `/speckit-clarify` / `/speckit-analyze` を使う。
- 成果物は `specs/<NNN>-<機能名>/` に置く。
- 人間の承認ゲートは 2 つ: spec.md の確定時と、PR のマージ時。
- `.specify/templates/overrides/` のテンプレートは GoogleTest 連携キットが管理する。機能の作業の中では変更しない。

### II. ロジックと UI・外部 I/O の分離
- ビジネスロジックは UI フレームワークや OS API に依存しないプレーン C++ として実装する。
- 時刻・乱数・ファイル・ネットワーク・ハードウェアは、インターフェース越しに差し替えられる形にする。

### III. 自動テスト必須（GoogleTest / CTest・テストファースト）
- ロジックの追加・変更には、GoogleTest で書いた自動テストを必ず伴う。
- 実装より先にテストを書き、失敗（Red）を確認してから実装する。Red を確認していないテストは完了とみなさない。
- spec.md の受け入れ基準（AC-xxx）・境界値（B-xx）・成功基準（SC-xxx [AUTO]）は、すべて対応するテストを持つ。
- テスト名は `F<機能番号>_<ID>_<条件>_<結果>` とし（英数字と _ だけ。日本語にすると ctest で 0 件実行のまま成功になる）、`RecordProperty("spec", "<機能番号>:<ID>")` で ID を記録する。
- テストのコメントと、アサーションに付けるメッセージは日本語で書く（テスト名と ID は英数字のまま）。ソースは UTF-8 とし、MSVC では `/utf-8` を付ける。
- アサーションは `EXPECT_*` を原則とし、以降の検証が無意味になる前提条件にだけ `ASSERT_*` を使う。
- 期待値はリテラルで書き、実装と同じ計算式をテスト内で再現しない。
- テストを通すためにテストや期待値を書き換えてはならない。仕様の誤りが疑われるときは実装を止め、spec.md の修正を人間に提案する。
- テストの無効化（`DISABLED_`、削除、理由のない `GTEST_SKIP`）は禁止。
- カバレッジを上げるテストは、期待値の根拠が仕様（spec.md・data-model.md・contracts/・ヘッダーのコメント）にある行だけ書き、根拠をテストのコメントに残す。根拠の無い行は人が決め、テストしない行は `tests/coverage-hold.md` に理由と一緒に記録する。赤い行を消すためだけのテストは書かない（`/coverage`）。
- 単体テストでは外部依存を GoogleMock / フェイクに差し替え、何度実行しても同じ結果になるようにする。

### IV. ビルドとテストの実行方法
- テストは CMake + CTest で構成し、GoogleTest は `FetchContent` でバージョンを固定して取得する。
- ラベル: `unit` / `integration` / `acceptance`。時間のかかるものには `slow` を付ける。
- タスク完了の前に必ず `cmake --build build --config Debug && ctest --test-dir build -C Debug --output-on-failure -LE slow` を実行し、全件成功を確認する。マージ前には Release ビルドで `slow` を含めて全件実行する（`cmake --build build --config Release && ctest --test-dir build -C Release --output-on-failure`）。
- 処理時間・速度を測るテストは別のファイルにして `slow` を付け、Release ビルドで合否を決める。Debug ビルドの時間で判定しない。閾値は spec.md の値から変えない。
- Claude Code の Stop フック（`.claude/hooks/run-tests.ps1`）が、作業の終わりにビルドとテストを自動で実行する。フックを外したり、失敗を無視して作業を終えたりしてはならない。
- 実行コマンドは CLAUDE.md に記載し、変わったら同じ作業の中で更新する。

### V. シンプルさ優先（YAGNI）
- 必要になってから作る。テストのために製品コードの設計を歪める場合は、plan.md の Complexity Tracking に理由を書く。

### VI. ドキュメントとの同期
- アーキテクチャ・ビルド手順・テスト方法が変わったら、同じ作業の中で CLAUDE.md を更新する。

### VII. ドメイン固有の原則
- 入力の「ドメイン原則」を入れる。「なし」なら、この原則は作らない。

憲章には「技術スタック制約」「開発ワークフロー」「Governance」の節も作ること。新規の憲章ならバージョンは 1.0.0 から始める。
