# Feature Specification: フレーム表示の既定値を ON にする

**Feature Branch**: `ver.0.3`（spec ディレクトリ: `021-frame-display-default-on`）

**Created**: 2026-10-01

**Status**: Draft

**Input**: User description: "フレーム表示のチェックはデフォルトはオンにしておいてください。"

**前提となる機能**: 機能 004（プロジェクトの既定値: フレーム表示 OFF）、機能 017（フレーム表示のチェックボックス）。

## User Scenarios & Testing *(mandatory)*

### User Story 1 - 起動直後からフレーム表示が ON (Priority: P1)

ユーザーはアプリを起動すると、ツールバーのフレーム表示のチェックボックスにチェックが入った状態から使い始め、そのまま「開始」すると構成図の強調と流れのエフェクトが表示される。

**Independent Test**: 起動直後のチェックボックスにチェックが入っており、そのまま実行すると強調が出ることを確かめる。

**Acceptance Scenarios**:

1. **Given** 起動直後、**When** ツールバーを見る、**Then** フレーム表示のチェックボックスにチェックが入っている。
2. **Given** 起動直後、**When** そのまま「開始」する、**Then** 構成図の強調・流れのエフェクトが表示される。
3. **Given** `FrameDisplay=0` と書かれたプロジェクト、**When** 読み込む、**Then** チェックは外れる（ファイルの値が優先）。
4. **Given** `FrameDisplay=` の行が無いプロジェクトファイル、**When** 読み込む、**Then** チェックが入る（既定値）。

## Requirements *(mandatory)*

- **FR-001**: プロジェクトの既定値のフレーム表示を ON にしなければならない（起動直後、`FrameDisplay=` の無いファイルの読込み）。
- **FR-002**: ファイルに `FrameDisplay=0` / `1` がある場合はその値に従い、保存の形式は変えてはならない。
- **FR-003**: シミュレーションの計算・CSV は変えてはならない。

## Success Criteria *(mandatory)*

- **SC-001**: 起動直後にチェックが入っている。
- **SC-002**: 既存の検証（004 の既定値の項目は新しい既定値で）がすべて通り、CSV は変わらない。

## Assumptions

- 機能 004 の「起動時はフレーム表示 OFF」を置き換える。同梱のサンプルはすべて `FrameDisplay=` を明記しているので、読み込んだときの状態は変わらない。
