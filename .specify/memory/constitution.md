<!--
Sync Impact Report
Version change: 1.1.0 → 1.2.0
Modified principles:
  - I. Spec Kit ワークフロー準拠: 承認ゲート 2 つ・成果物の置き場・overrides テンプレートの扱いを追加
  - II. MFC/C++ ネイティブプラットフォーム規約の遵守: CMake がテスト専用であること、CanSim/ の
    MFC なしビルド（tests/stubs/pch.h）を追加
  - III. テスト可能性の段階的確保 → III. ロジックと UI・外部 I/O の分離（改題・強化）
  - IV. シンプルさ優先 → VI（番号繰り下げ、Complexity Tracking の記載を追加）
  - V. ドキュメントとの同期 → VII（番号繰り下げ、テスト方法の変更を明記）
  - VI. CAN バスドメイン仕様準拠 → VIII（番号のみ変更、内容は不変）
Added sections:
  - IV. 自動テスト必須（GoogleTest / CTest・テストファースト）
  - V. ビルドとテストの実行方法
Removed sections: none
Rationale for MINOR bump: GoogleTest/CTest によるテスト運用の原則を新規追加し、既存の
「テストの段階的確保」を実質的に拡張したため。既存原則の削除・破壊的な再定義は無い。
Follow-up TODOs: none
-->

# CAN Simulator (MFCApplication1) Constitution

## Core Principles

### I. Spec Kit ワークフロー準拠
機能追加や仕様変更は、思いつきでの実装を避け、必ず Spec Kit のワークフロー
（`/speckit-specify` → `/speckit-plan` → `/speckit-tasks` → `/speckit-implement`、
必要に応じて `/speckit-clarify` / `/speckit-analyze` / `/speckit-converge` を併用）に沿って
進めなければならない (MUST)。要求は `spec.md`、設計は `plan.md`、作業分解は `tasks.md` として
`specs/<NNN>-<機能名>/` に明文化してから着手する。人間の承認ゲートは 2 つとする:
`spec.md` の確定時と、PR のマージ時。`.specify/templates/overrides/` のテンプレートは
GoogleTest 連携キットが管理するため、機能の作業の中で変更してはならない (MUST NOT)。
Rationale: 本リポジトリは Spec Kit (SDD) を導入済みであり、CLAUDE.md がこのワークフローを
開発の前提として明記している。仕様策定を経ずに実装すると、仕様と実装の乖離や手戻りが生じる。
テンプレートを個別に書き換えると、キットの更新と食い違う。

### II. MFC/C++ ネイティブプラットフォーム規約の遵守
Visual Studio 2022 (`PlatformToolset` v143) を前提とし、`Win32` / `x64` の両プラットフォーム、
`Debug` / `Release` の両構成で常にビルド可能な状態を維持しなければならない (MUST)。文字セットは
Unicode、MFC の使用方法は動的リンク (`UseOfMfc=Dynamic`) を用いる。プロジェクト設定
(`.vcxproj`) と矛盾する変更を行う場合は、設定ファイル自体を明示的に更新する。CMake はテスト
専用であり、製品のビルドは Visual Studio のソリューション (`.sln` / `.vcxproj`) が正とする。
`CanSim/` は MFC なしでビルドできなければならず (MUST)、CMake では `tests/stubs/pch.h` の
空ヘッダーで `pch.h` の取り込みを満たす。
Rationale: 既存のプロジェクト設定がこれらを前提にしており、逸脱するとビルド不能または未定義動作
を招く。CMake を製品ビルドと分けることで、既存のソリューションを壊さずにテストを導入できる。

### III. ロジックと UI・外部 I/O の分離
CAN フレームの送受信・解析などのビジネスロジックは、UI フレームワーク（MFC）や OS API に依存
しないプレーン C++ として `CanSim/` に実装しなければならない (MUST)。`CMFCApplication1Dlg`
などの UI ハンドラにロジックを埋め込んではならない (MUST NOT)。時刻・乱数・ファイル・
ネットワーク・ハードウェアに触れる処理は、インターフェース越しに差し替えられる形にする (MUST)。
Rationale: ロジックを UI や外部 I/O から切り離すことで、自動テストが決定的に動き、同一構成から
常に同一結果を得る再現性（P③）も保てる。

### IV. 自動テスト必須（GoogleTest / CTest・テストファースト）
ロジックの追加・変更には、GoogleTest で書いた自動テストを必ず伴わなければならない (MUST)。
- 実装より先にテストを書き、失敗（Red）を確認してから実装する。Red を確認していないテストは
  完了とみなさない。
- `spec.md` の受け入れ基準（AC-xxx）・境界値（B-xx）・成功基準（SC-xxx [AUTO]）は、すべて
  対応するテストを持つ。
- テスト名は `F<機能番号>_<ID>_<条件>_<結果>` とし（英数字と `_` だけ。日本語にすると ctest で
  0 件実行のまま成功になる）、`RecordProperty("spec", "<機能番号>:<ID>")` で ID を記録する。
- テストのコメントとアサーションに付けるメッセージは日本語で書く。ソースは UTF-8 とし、
  MSVC では `/utf-8` を付ける。
- アサーションは `EXPECT_*` を原則とし、以降の検証が無意味になる前提条件にだけ `ASSERT_*` を使う。
- 期待値はリテラルで書き、実装と同じ計算式をテスト内で再現してはならない (MUST NOT)。
- テストを通すためにテストや期待値を書き換えてはならない (MUST NOT)。仕様の誤りが疑われる
  ときは実装を止め、`spec.md` の修正を人間に提案する。
- テストの無効化（`DISABLED_`、削除、理由のない `GTEST_SKIP`）は禁止する (MUST NOT)。
- カバレッジを上げるテストは、期待値の根拠が仕様（`spec.md`・`data-model.md`・`contracts/`・
  ヘッダーのコメント）にある行だけ書き、根拠をテストのコメントに残す。根拠の無い行は人が決め、
  テストしない行は `tests/coverage-hold.md` に理由と一緒に記録する（`/coverage`）。赤い行を
  消すためだけのテストは書かない (MUST NOT)。
- 単体テストでは外部依存を GoogleMock / フェイクに差し替え、何度実行しても同じ結果になるようにする。
Rationale: 仕様と実装の乖離を自動で検出するには、仕様の ID に結びついたテストが先にあり、
それを書き換えずに守ることが必要。日本語のテスト名は ctest で見えなくなり、検証が空振りする。

### V. ビルドとテストの実行方法
テストは CMake + CTest で構成し、GoogleTest は `FetchContent` でバージョン（v1.15.2）を固定して
取得する (MUST)。ラベルは `unit` / `integration` / `acceptance` とし、時間のかかるものには
`slow` を付ける。
- タスク完了の前に `cmake --build build --config Debug && ctest --test-dir build -C Debug
  --output-on-failure -LE slow` を実行し、全件成功を確認する (MUST)。
- マージ前は Release ビルドで `slow` を含めて全件実行する
  （`cmake --build build --config Release && ctest --test-dir build -C Release --output-on-failure`）。
- 処理時間・速度を測るテストは別のファイルにして `slow` を付け、Release ビルドで合否を決める。
  Debug ビルドの時間で判定してはならない (MUST NOT)。閾値は `spec.md` の値から変えない。
- Claude Code の Stop フック（`.claude/hooks/run-tests.ps1`）が、作業の終わりにビルドとテストを
  自動で実行する。フックを外したり、失敗を無視して作業を終えたりしてはならない (MUST NOT)。
- 実行コマンドは CLAUDE.md に記載し、変わったら同じ作業の中で更新する。
Rationale: 実行方法を固定することで、誰が（どの AI が）実行しても同じ基準で合否が決まる。
Debug の時間は実測の根拠にならない。

### VI. シンプルさ優先 (YAGNI)
現在のダイアログベースアーキテクチャ（単一ダイアログ `IDD_MFCAPPLICATION1_DIALOG` が UI そのもの）
を維持し、Document/View アーキテクチャへの移行など、必要性が具体的に生じていない抽象化・
再設計を導入してはならない (MUST NOT)。CAN シミュレータ機能は、そのときに必要な範囲で
最小限の変更として追加する。テストのために製品コードの設計を歪める場合は、`plan.md` の
Complexity Tracking に理由を書かなければならない (MUST)。
Rationale: 将来の要件を先取りした複雑化はメンテナンスコストを増大させ、Spec Kit の「必要に
なってから作る」という進め方とも矛盾する。

### VII. ドキュメントとの同期
アーキテクチャ、ビルド手順、テスト方法に実質的な変更が生じた場合は、その変更と同じ作業の中で
CLAUDE.md を更新しなければならない (MUST)。Spec Kit の成果物 (`spec.md` / `plan.md` /
`tasks.md` など) は、対応する機能ブランチ・`specs/<NNN>-<機能名>/` ディレクトリと一致した
状態を保つ。
Rationale: CLAUDE.md は Claude Code がこのプロジェクトを理解するための一次情報源であり、
実態との乖離は誤った支援や手戻りに直結する。

### VIII. CAN バスドメイン仕様準拠
`Doc/CANバスシミュレータ仕様.md` に記録された次の仕様ポイントは、個別機能の `spec.md` や
実装がこれを覆してはならない、プロジェクト全体を拘束する決定事項である (MUST)。これらを変更
する場合は、当該ドキュメントの改定に加えて本憲章の改定 (`/speckit.constitution`) を伴わなければ
ならない (MUST)。
- **[P①]** プロトコルは CAN 2.0A 標準フォーマット（11ビットID）に準拠する。拡張フォーマット
  （29ビットID）・CAN FD・リモートフレーム・オーバーロードフレームは初期版のスコープ外とする。
- **[P②]** ノードの状態遷移は、スタートアップ制御・メッセージ送信制御（送信・調停）・エラー
  状態制御の3系統で構成する。エラー状態制御は初期版では枠（入口・状態遷移の型）のみ用意し、
  実際のエラー発生・遷移は実装しない。
- **[P③]** シミュレーション方式はイベント駆動型（Discrete Event Simulation）とし、イベントは
  ビット単位ではなくフレーム単位で発生させる。同一時刻のイベントは定められた順序（FR-013）と
  ノード番号順で決定的に処理し、同一構成からは常に同一結果を得る（再現性）。
- **[P④]** 通信仕様とログ仕様は `Doc/CANバスシミュレータ仕様.md` が参照する「CAN通信 性能評価・
  ログ設計仕様書」に従う。電気的な物理層（差動電圧・終端抵抗・伝搬遅延・ビットタイミング・
  クロック誤差）は模擬せず、全ノードが理想的に同期しているものとみなす。
- **[P⑤]** 接続ノード数は最大16とし、17ノード以上の構成は読み込み時に拒否する。
Rationale: これらはI.Hが確定させた仕様ポイントであり、CAN シミュレータとしての本質的な
振る舞い（何を模擬し、何を模擬しないか）を定義する。原則として明文化しておくことで、個別機能の
実装や `spec.md` 作成の際に、スコープを逸脱した設計判断（例: CAN FD 対応やビット単位の物理層
シミュレーション）が紛れ込むことを防ぐ。

## CAN バスシミュレータ ドメイン仕様

`Doc/CANバスシミュレータ仕様.md`（状態: Draft v1.0、2026-09-28）が、CAN シミュレータ機能の
ドメイン仕様における一次情報源である。同ドキュメントは「プロジェクト全体の仕様であり、本憲章と
各機能の `spec.md`（`/speckit.specify` で作成）の参照元とする」ことを明記しており、個別機能の
詳細な機能要件（FR-xxx）・ユーザーシナリオ・成功基準・未決事項は同ドキュメントおよび今後作成
される各 `spec.md` 側で管理する（本憲章には転記しない）。

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
`Doc/CANバスシミュレータ仕様.md` §1.3 に基づき、初期版のスコープを次のとおり定める。個別機能の
`spec.md` はこの境界の中で詳細化するものとし、境界を拡張する場合は本憲章の改定を伴う (MUST)。

| 区分 | 対象 |
|---|---|
| 対象（初期版） | CAN 2.0A 標準フォーマットのデータフレーム、ID による調停、自動再送信、スタートアップ制御、ビットスタッフィングを含む伝送時間の計算、[性能・ログ] 形式のログ、評価指標の算出 |
| 枠だけ用意（中身は初期版では搭載しない） | エラー状態制御（エラーアクティブ／パッシブ／バスオフ、TEC/REC）、エラー発生の入口 |
| 対象外（初期版） | 拡張フォーマット（29ビットID）、CAN FD、リモートフレーム、オーバーロードフレーム、物理層、実機との接続 |

## 技術スタック制約

- 言語・フレームワーク: ネイティブ C++（C++14）/ MFC（.NET や他の GUI フレームワークへの置き換えは
  本憲章の改定なしに行わない）。
- 対象 OS: Windows のみを前提とする。クロスプラットフォーム対応は現時点でスコープ外。
- 製品のビルドツール: Visual Studio または `MSBuild`（コマンド例は CLAUDE.md 参照）。
- テストのビルドツール: CMake 3.14 以降 + CTest + GoogleTest v1.15.2（`FetchContent`）。
  テスト対象は `MFCApplication1/CanSim/`（静的ライブラリ `app_core`）。
- Lint / CI の仕組みは未整備であり、導入した場合は CLAUDE.md にコマンドを追記する。
- リソース変更（ダイアログレイアウト、アイコン、文字列テーブル等）は Visual Studio の
  リソースエディタまたは `.rc` ファイルの直接編集で行う。

## 開発ワークフロー

- 新機能は Spec Kit のスラッシュコマンド群（Core Principles I 参照）を経て実装する。
- `.specify/memory/constitution.md` は本ファイルであり、`.specify/templates/` 以下の
  テンプレート（`overrides/` を含む）やコアスクリプトは Spec Kit ツール本体として扱い、
  機能実装作業では変更しない。
- 各タスクの後に Debug ビルドと `-LE slow` のテストを実行し、全件成功を確認する。作業の終わりには
  Stop フックが同じ確認を自動で行う。マージ前は Release で `slow` を含む全件を確認する。
- 実装が完了した機能について、仕様に対する未完了作業がないかを `/speckit-converge` で
  確認することを推奨する（任意）。カバレッジを上げるときは `/coverage` を使う。
- コードレビュー・品質チェックの仕組み（Lint/CI 等）を追加する場合は、本憲章および
  CLAUDE.md の両方を更新する。

## Governance

本憲章はプロジェクトにおける他の慣行・判断に優先する (Constitution supersedes all other
practices)。Spec Kit のテンプレートやコマンドはこの憲章を実行時に参照するが、本憲章自体は
`/speckit.constitution` コマンドを通じてのみ改定する。

- **改定手続き**: 改定は `/speckit.constitution` コマンドで行い、変更内容を Sync Impact
  Report（本ファイル冒頭のコメント）に記録する。
- **バージョニング方針**: セマンティックバージョニングに従う。
  - MAJOR: 原則の後方互換性のない削除・再定義。
  - MINOR: 新しい原則やセクションの追加、既存ガイダンスの実質的拡張。
  - PATCH: 文言修正・明確化など非意味的な変更。
- **コンプライアンスレビュー**: `/speckit.plan` の Constitution Check、および `/speckit.analyze`
  によるクロスアーティファクト整合性チェックの際に、本憲章との整合性を確認する。矛盾する
  複雑さの導入は、代替案がない理由を `plan.md` 等に明記して正当化しなければならない (MUST)。
- **ドメイン仕様の位置付け**: `Doc/CANバスシミュレータ仕様.md` は本憲章および各機能 `spec.md`
  のドメイン知識の一次情報源として扱う。同ドキュメントの仕様ポイント①〜⑤・スコープ境界を
  変更する場合は、同ドキュメントの改定に加えて本憲章の改定を伴わなければならない (MUST)。
  FR-xxx 単位の詳細要件・未決事項 (Q-1〜) は `/speckit.specify` を通じて各機能の `spec.md` に
  引き継ぐ。

**Version**: 1.2.0 | **Ratified**: 2026-09-28 | **Last Amended**: 2026-10-09
