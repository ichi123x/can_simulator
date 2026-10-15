<!--
Sync Impact Report
Version change: 2.0.0 → 2.1.0
Modified principles:
  - I. Spec Kit ワークフロー準拠（見出しは不変。人間の承認ゲート 2 つと、overrides/ のテンプレートは
    GoogleTest 連携キットが管理することを追加）
  - III. 自動テスト必須（GoogleTest/CTest・テストファースト）（見出しは不変。sdd-gtest-kit 付録 A に
    合わせて具体化: Red の確認、AC/B/SC [AUTO] との対応、テスト名 F<NNN>_<ID>_<条件>_<結果> と
    RecordProperty、日本語のコメント、EXPECT/ASSERT の使い分け、期待値はリテラル、テストの書き換え・
    無効化の禁止、カバレッジのテストの根拠、GoogleMock/フェイク）
  - IV. シンプルさ優先 (YAGNI)（見出しは不変。テストのための設計の歪みは Complexity Tracking に書く）
Added sections:
  - VII. ロジックと UI・外部 I/O の分離
  - VIII. ビルドとテストの実行方法
Removed sections: none
Other edits: 技術スタック制約の自動テストの記述を VIII（app_core、GoogleTest 1.15.2 を FetchContent、
  tests/stubs の pch.h）に合わせて更新。付録 A の VI（ドキュメント同期）は既存の V、VII（ドメイン固有）は
  既存の VI で満たすため、新しい原則にはしない。既存の原則 I〜VI の番号と見出しは、spec.md・CLAUDE.md の
  参照（「憲章 II」「憲章 III」「憲章 VI」「P①〜P⑤」）を保つため変えない。
Rationale for MINOR bump: 原則の削除・後方互換性のない再定義は無く、新しい原則（VII・VIII）の追加と、
  既存の原則 III のガイダンスの実質的な拡張であるため。
Templates / docs checked:
  ✅ .specify/templates/plan-template.md — Constitution Check は憲章を実行時に読むため変更不要
  ✅ .specify/templates/overrides/spec-template.md・tasks-template.md — キットが管理（I）。AC/B/SC [AUTO] と
     「テスト → 失敗確認 → 実装 → ctest」の並びは既に III・VIII と一致
  ✅ CLAUDE.md「テストの実行」「カバレッジ」節 — setup.ps1 が追記済みで VIII のコマンドと一致
  ⚠ CLAUDE.md の「プロジェクトの現状」の「憲章（v1.1.0）」と Spec Kit 節の「v1.1.0 として確定済み」は
     古い表記のまま（v2.0.0 のときから）。別途 2.1.0 に直す
Follow-up TODOs: none
-->

# CAN Simulator (MFCApplication1) Constitution

## Core Principles

### I. Spec Kit ワークフロー準拠
機能追加や仕様変更は、思いつきでの実装を避け、必ず Spec Kit のワークフロー（`/speckit.specify` → `/speckit.plan` → `/speckit.tasks` → `/speckit.implement`、必要に応じて `/speckit.clarify` / `/speckit.analyze` / `/speckit.converge` を併用）に沿って進めなければならない (MUST)。要求は `spec.md`、設計は `plan.md`、作業分解は `tasks.md` として`specs/<NNN>-<機能名>/` に明文化してから着手する。人間の承認ゲートは 2 つで、spec.md の確定時と PR のマージ時である (MUST)。`.specify/templates/overrides/` のテンプレートは GoogleTest 連携キット（sdd-gtest-kit）が管理し、機能の作業の中で変更してはならない (MUST NOT)。Rationale: 本リポジトリは Spec Kit (SDD) を導入済みであり、CLAUDE.md がこのワークフローを開発の前提として明記している。仕様策定を経ずに実装すると、仕様と実装の乖離や手戻りが生じる。テンプレートの正本をキットの 1 か所にしておくことで、機能ごとにテンプレートが食い違うことを防ぐ。

### II. MFC/C++ ネイティブプラットフォーム規約の遵守
Visual Studio 2022 (`PlatformToolset` v143) を前提とし、`Win32` / `x64` の両プラットフォーム、`Debug` / `Release` の両構成で常にビルド可能な状態を維持しなければならない (MUST)。文字セットはUnicode、MFC の使用方法は動的リンク (`UseOfMfc=Dynamic`) を用いる。プロジェクト設定(`.vcxproj`) と矛盾する変更を行う場合は、設定ファイル自体を明示的に更新する。Rationale: 既存のプロジェクト設定がこれらを前提にしており、逸脱するとビルド不能または未定義動作を招く。

### III. 自動テスト必須（GoogleTest/CTest・テストファースト）
`CanSim/` 配下のロジック（CAN フレームの送受信・調停・解析・集計など）の追加・変更には、GoogleTest で書いた自動テストを必ず伴わなければならない (MUST)。ビジネスロジックは MFC ダイアログクラス (`CMFCApplication1Dlg`) の UI ハンドラに埋め込まず、UI から分離したプレーン C++ として実装しなければならない (MUST)。テストについては次を守る。
- 実装より先にテストを書き、失敗（Red）を確認してから実装する (MUST)。Red を確認していないテストは完了とみなさない。
- spec.md の受け入れ基準（AC-xxx）・境界値（B-xx）・成功基準（SC-xxx [AUTO]）は、すべて対応するテストを持たなければならない (MUST)。
- テスト名は `F<機能番号>_<ID>_<条件>_<結果>` とし（英数字と `_` だけ。日本語にすると ctest で 0 件実行のまま成功になる）、`RecordProperty("spec", "<機能番号>:<ID>")` で spec.md の ID を記録する (MUST)。
- テストのコメントと、アサーションに付けるメッセージは日本語で書く（テスト名と ID は英数字のまま）。テストのソースは UTF-8 とし、MSVC では `/utf-8` を付ける (MUST)。`CanSim/` のソースは ASCII のみという規則（CLAUDE.md）とは別である。
- アサーションは `EXPECT_*` を原則とし、以降の検証が無意味になる前提条件にだけ `ASSERT_*` を使う。
- 期待値はリテラルで書き、実装と同じ計算式をテスト内で再現してはならない (MUST NOT)。
- テストを通すためにテストや期待値を書き換えてはならない (MUST NOT)。仕様の誤りが疑われるときは実装を止め、spec.md の修正を人間に提案する。
- `DISABLED_`、削除、理由のない `GTEST_SKIP` によるテストの無効化は禁止する (MUST NOT)。
- カバレッジを上げるテストは、期待値の根拠が仕様（spec.md・data-model.md・contracts/・ヘッダーのコメント）にある行だけ書き、根拠をテストのコメントに残す。根拠の無い行は人が決め、テストしない行は `tests/coverage-hold.md` に理由と一緒に記録する。赤い行を消すためだけのテストは書かない（`/coverage`）。
- 単体テストでは外部依存を GoogleMock / フェイクに差し替え、何度実行しても同じ結果になるようにする (MUST)。

Rationale: UI 非依存の `CanSim/` を自動テストで守ることで、仕様（受け入れ基準・境界値・成功基準）との対応を追跡でき、回帰を早期に検出できる。テストを先に書いて Red を確認することで、テストが実際に仕様を検証していることを保証する。テストの書き換えや無効化を禁じることで、仕様と実装の食い違いが黙って消えることを防ぐ。

### IV. シンプルさ優先 (YAGNI)
現在のダイアログベースアーキテクチャ（単一ダイアログ `IDD_MFCAPPLICATION1_DIALOG` が UI そのもの）を維持し、Document/View アーキテクチャへの移行など、必要性が具体的に生じていない抽象化・再設計を導入してはならない (MUST NOT)。CAN シミュレータ機能は、そのときに必要な範囲で最小限の変更として追加する。テストのために製品コードの設計を歪める場合は、plan.md の Complexity Tracking に理由を書かなければならない (MUST)。Rationale: 将来の要件を先取りした複雑化はメンテナンスコストを増大させ、Spec Kit の「必要になってから作る」という進め方とも矛盾する。

### V. ドキュメントとの同期
アーキテクチャ、ビルド手順、テスト方法に実質的な変更が生じた場合は、その変更と同じ作業の中でCLAUDE.md を更新しなければならない (MUST)。Spec Kit の成果物 (`spec.md` / `plan.md` / `tasks.md` など) は、対応する機能ブランチ・`specs/<NNN>-<機能名>/` ディレクトリと一致した状態を保つ。Rationale: CLAUDE.md は Claude Code がこのプロジェクトを理解するための一次情報源であり、実態との乖離は誤った支援や手戻りに直結する。

### VI. CAN バスドメイン仕様準拠
`Doc/CANバスシミュレータ仕様.md` に記録された次の仕様ポイントは、個別機能の `spec.md` や実装がこれを覆してはならない、プロジェクト全体を拘束する決定事項である (MUST)。これらを変更する場合は、当該ドキュメントの改定に加えて本憲章の改定 (`/speckit.constitution`) を伴わなければならない (MUST)。
- **[P①]** プロトコルは CAN 2.0A 標準フォーマット（11ビットID）に準拠する。拡張フォーマット（29ビットID）・CAN FD・リモートフレーム・オーバーロードフレームは初期版のスコープ外とする。
- **[P②]** ノードの状態遷移は、スタートアップ制御・メッセージ送信制御（送信・調停）・エラー状態制御の3系統で構成する。エラー状態制御は初期版では枠（入口・状態遷移の型）のみ用意し、実際のエラー発生・遷移は実装しない。
- **[P③]** シミュレーション方式はイベント駆動型（Discrete Event Simulation）とし、イベントはビット単位ではなくフレーム単位で発生させる。同一時刻のイベントは定められた順序（FR-013）とノード番号順で決定的に処理し、同一構成からは常に同一結果を得る（再現性）。
- **[P④]** 通信仕様とログ仕様は `Doc/CANバスシミュレータ仕様.md` が参照する「CAN通信 性能評価・ログ設計仕様書」に従う。電気的な物理層（差動電圧・終端抵抗・伝搬遅延・ビットタイミング・クロック誤差）は模擬せず、全ノードが理想的に同期しているものとみなす。
- **[P⑤]** 接続ノード数は最大16とし、17ノード以上の構成は読み込み時に拒否する。Rationale: これらはI.Hが確定させた仕様ポイントであり、CAN シミュレータとしての本質的な振る舞い（何を模擬し、何を模擬しないか）を定義する。原則として明文化しておくことで、個別機能の実装や `spec.md` 作成の際に、スコープを逸脱した設計判断（例: CAN FD 対応やビット単位の物理層シミュレーション）が紛れ込むことを防ぐ。

### VII. ロジックと UI・外部 I/O の分離
ビジネスロジックは、UI フレームワーク（MFC）や OS API に依存しないプレーン C++ として実装しなければならない (MUST)。このプロジェクトでは `MFCApplication1/CanSim/` がその置き場であり、`CanSim/` のソースは MFC のヘッダーを取り込まずにビルドできる状態を保つ（テスト用の CMake ビルドでは、`pch.h` を `tests/stubs` の空のヘッダーで満たす）。時刻・乱数・ファイル・ネットワーク・ハードウェアは、インターフェース越しに差し替えられる形にしなければならない (MUST)。Rationale: UI や外部 I/O から切り離しておくことで、ロジックを GUI を操作せずに単体テストでき、テストが環境に左右されず何度実行しても同じ結果になる。

### VIII. ビルドとテストの実行方法
テストは CMake + CTest で構成し、GoogleTest は `FetchContent` でバージョンを固定して（1.15.2）取得しなければならない (MUST)。`CanSim/` のソースは移動せず、静的ライブラリ `app_core` としてビルドする（MFC アプリ本体のビルドは `.sln` / `.vcxproj` のまま）。テストにはラベル `unit` / `integration` / `acceptance` を付け、時間のかかるものには `slow` を付ける。
- タスク完了の前に必ず `cmake --build build --config Debug && ctest --test-dir build -C Debug --output-on-failure -LE slow` を実行し、全件成功を確認する (MUST)。
- マージ前には Release ビルドで `slow` を含めて全件実行する（`cmake --build build --config Release && ctest --test-dir build -C Release --output-on-failure`）(MUST)。
- 処理時間・速度を測るテストは別のファイルにして `slow` を付け、Release ビルドで合否を決める。Debug ビルドの時間で判定してはならず、閾値は spec.md の値から変えてはならない (MUST NOT)。
- Claude Code の Stop フック（`.claude/hooks/run-tests.ps1`）が、作業の終わりにビルドとテストを自動で実行する。フックを外したり、失敗を無視して作業を終えたりしてはならない (MUST NOT)。
- 実行コマンドは CLAUDE.md に記載し、変わったら同じ作業の中で更新する (MUST)。

Rationale: 取得するバージョンと実行方法を固定することで、誰がいつ実行しても同じ結果になる。Debug と Release で役割を分けることで、最適化の無い Debug の時間のぶれで合否が変わることを防ぐ。フックで作業の終わりに必ずテストを走らせることで、壊れた状態のまま作業を終えることを防ぐ。

## CAN バスシミュレータ ドメイン仕様

`Doc/CANバスシミュレータ仕様.md`（状態: Draft v1.0、2026-09-28）が、CAN シミュレータ機能のドメイン仕様における一次情報源である。同ドキュメントは「プロジェクト全体の仕様であり、本憲章と各機能の `spec.md`（`/speckit.specify` で作成）の参照元とする」ことを明記しており、個別機能の詳細な機能要件（FR-xxx）・ユーザーシナリオ・成功基準・未決事項は同ドキュメントおよび今後作成される各 `spec.md` 側で管理する（本憲章には転記しない）。

### 参照資料
機能実装・仕様策定にあたっては、次の資料を一次情報源として参照する。

| 略称 | 資料 | リポジトリ内の場所 |
|---|---|---|
| [図] | CANバスイメージ図（ECU 6台、CAN BUS 500 kbps、両端120Ω終端） | `Doc/references/CANバスイメージ図.jpg` |
| [Bosch] | Bosch CAN Specification 2.0 Part A | `Doc/references/Bosch_CAN20A.pdf` |
| [入門] | ベクター・ジャパン「はじめてのCAN/CAN FD」 | `Doc/references/For_Beginners_CAN_CANFD.pdf` |
| [状態遷移] | CANプロトコル状態遷移（送信・調停／スタートアップ／エラー状態） | `Doc/references/CANプロトコル状態遷移.pdf` |
| [性能・ログ] | CAN通信 性能評価・ログ設計仕様書 | `Doc/references/CAN通信 性能評価・ログ設計仕様書.pdf` |

### スコープ境界（初期版）
`Doc/CANバスシミュレータ仕様.md` §1.3 に基づき、初期版のスコープを次のとおり定める。個別機能の`spec.md` はこの境界の中で詳細化するものとし、境界を拡張する場合は本憲章の改定を伴う (MUST)。

| 区分 | 対象 |
|---|---|
| 対象（初期版） | CAN 2.0A 標準フォーマットのデータフレーム、ID による調停、自動再送信、スタートアップ制御、ビットスタッフィングを含む伝送時間の計算、[性能・ログ] 形式のログ、評価指標の算出 |
| 枠だけ用意（中身は初期版では搭載しない） | エラー状態制御（エラーアクティブ／パッシブ／バスオフ、TEC/REC）、エラー発生の入口 |
| 対象外（初期版） | 拡張フォーマット（29ビットID）、CAN FD、リモートフレーム、オーバーロードフレーム、物理層、実機との接続 |

## 技術スタック制約

- 言語・フレームワーク: ネイティブ C++ / MFC（.NET や他の GUI フレームワークへの置き換えは本憲章の改定なしに行わない）。C++ の規格は C++14（`CMakeLists.txt` の `CMAKE_CXX_STANDARD`）。
- 対象 OS: Windows のみを前提とする。クロスプラットフォーム対応は現時点でスコープ外。
- ビルドツール: Visual Studio 2022（MSVC）または `MSBuild`（コマンド例は CLAUDE.md 参照）。自動テストは CMake + GoogleTest（1.15.2、`FetchContent`）+ CTest で、`CanSim/` を静的ライブラリ `app_core` としてビルド・実行する（原則 VIII。MFC アプリ本体のビルドは `.sln` / `.vcxproj` のままとする）。カバレッジの測定には OpenCppCoverage を使う（`/coverage`）。CI/Lint の仕組みは未整備であり、導入した場合はCLAUDE.md にコマンドを追記する。
- リソース変更（ダイアログレイアウト、アイコン、文字列テーブル等）は Visual Studio のリソースエディタまたは `.rc` ファイルの直接編集で行う。

## 開発ワークフロー

- 新機能は Spec Kit のスラッシュコマンド群（Core Principles I 参照）を経て実装する。
- `.specify/memory/constitution.md` は本ファイルであり、`.specify/templates/` 以下のテンプレートやコアスクリプトは Spec Kit ツール本体として扱い、機能実装作業では変更しない。`.specify/templates/overrides/` は GoogleTest 連携キットが管理する（原則 I）。
- 実装が完了した機能について、仕様に対する未完了作業がないかを `/speckit.converge` で確認することを推奨する（任意）。
- 作成済みの仕様のテストをそろえるときは `/spec-tests`、テストで実行されていない行を減らすときは `/coverage` を使う。
- コードレビュー・品質チェックの仕組み（Lint/CI 等）を追加する場合は、本憲章およびCLAUDE.md の両方を更新する。

## Governance

本憲章はプロジェクトにおける他の慣行・判断に優先する (Constitution supersedes all other practices)。Spec Kit のテンプレートやコマンドはこの憲章を実行時に参照するが、本憲章自体は`/speckit.constitution` コマンドを通じてのみ改定する。

- **改定手続き**: 改定は `/speckit.constitution` コマンドで行い、変更内容を Sync Impact Report（本ファイル冒頭のコメント）に記録する。
- **バージョニング方針**: セマンティックバージョニングに従う。
  - MAJOR: 原則の後方互換性のない削除・再定義。
  - MINOR: 新しい原則やセクションの追加、既存ガイダンスの実質的拡張。
  - PATCH: 文言修正・明確化など非意味的な変更。
- **コンプライアンスレビュー**: `/speckit.plan` の Constitution Check、および `/speckit.analyze`によるクロスアーティファクト整合性チェックの際に、本憲章との整合性を確認する。矛盾する複雑さの導入は、代替案がない理由を `plan.md` 等に明記して正当化しなければならない (MUST)。
- **ドメイン仕様の位置付け**: `Doc/CANバスシミュレータ仕様.md` は本憲章および各機能 `spec.md`のドメイン知識の一次情報源として扱う。同ドキュメントの仕様ポイント①〜⑤・スコープ境界を変更する場合は、同ドキュメントの改定に加えて本憲章の改定を伴わなければならない (MUST)。FR-xxx 単位の詳細要件・未決事項 (Q-1〜) は `/speckit.specify` を通じて各機能の `spec.md` に引き継ぐ。

**Version**: 2.1.0 | **Ratified**: 2026-09-28 | **Last Amended**: 2026-10-15
