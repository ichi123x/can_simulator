# Implementation Plan: 送信フレーム一覧のダブルクリックでフレーム追加を開く

**Branch**: `ver.0.3`（spec ディレクトリ: `013-list-dblclick-add`） | **Date**: 2026-10-01 | **Spec**: [spec.md](./spec.md)

## Summary

データフレーム編集ウィンドウ（`CFrameEditDlg`）の送信フレーム一覧（`IDC_LIST_FRAMES`）で、列見出し・行・行の無い空白のどこを
ダブルクリックしても、「追加」ボタンと同じ処理（`OnAdd`）を行う。入口は 2 つある。

1. **一覧の本体（行・空白）**: 一覧が送ってくる `NM_DBLCLK` の処理先を、今の `OnEdit` から追加に変える。`NM_DBLCLK` は行の上でも
   空白でも届き、スクロールバー（非クライアント領域）のダブルクリックでは届かない。
2. **列見出し**: 列見出しは一覧の子ウィンドウなので、そのダブルクリックは編集ウィンドウに届かない。列見出しを小さな派生クラスで
   サブクラス化し、`WM_LBUTTONDBLCLK` を受けたら既定の処理（境目のダブルクリックによる列幅の自動調整）を行わずに、編集ウィンドウへ
   追加の要求を `PostMessage` で送る。

追加できる条件（ECU があり、フレームが 100 件未満）は `CanAdd()` にまとめ、ボタンの有効／無効とダブルクリックの両方で使う。
`CanSim/`・`.rc`・シミュレーション結果は変えない。

## Technical Context

**Language/Version**: C++14相当（機能 001〜012 と同じ）

**Primary Dependencies**: MFC（`CListCtrl` の `NM_DBLCLK`、`CHeaderCtrl` のサブクラス化）。新規外部依存なし。

**Storage**: なし（追加したフレームは既存どおりプロジェクトの定義 `m_definitions` に入り、保存はプロジェクト保存）

**Testing**:
- UI だけの変更なので、GUI の自動操作で確認する（quickstart.md）。行・空白・列見出し・列見出しの境目・スクロールバーを
  ダブルクリックし、追加の入力画面が開く／開かないを確かめる。
- コアの変更はないので、checks.exe は既存項目の PASS と CSV の一致だけを確認する。

**Target Platform**: Windows 10/11（Win32/x64、Debug/Release）

**Project Type**: desktop-app

**Performance Goals**: ダブルクリックから入力画面が開くまでの時間は「追加」ボタンと同じ（体感で即時）。

**Constraints**:
- 「追加」ボタンからの追加と同じ処理・同じ画面を使う（FR-005）。別の入力画面は作らない。
- 追加できないとき（100 件・ECU なし）はメッセージも出さない（FR-002）。
- 入力画面（モーダル）を列見出しのメッセージ処理の中から開かない（`PostMessage` で編集ウィンドウに処理を移す）。
- `CanSim/` の変更なし、`.rc` の変更なし。新しいソースファイルは作らない（派生クラスは `FrameEditDlg.h/.cpp` に置く）。

**Scale/Scope**: 変更: `FrameEditDlg.h/.cpp`、CLAUDE.md。

## Constitution Check

| 原則 | 判定 | 根拠 |
|---|---|---|
| I. Spec Kit ワークフロー準拠 | PASS | spec.md（013）→ 本 plan.md。 |
| II. MFC/C++ ネイティブ規約 | PASS | 既存ファイルの変更のみで 4 構成のビルドを維持。新しいファイル・プロジェクト設定の変更なし。 |
| III. テスト可能性 | PASS | ドメインロジックは変えない（追加・入力検査は既存の `CanSim::AddFrame` / `ValidateFrame`）。UI の入口の追加だけなので、GUI 自動操作で確認する。 |
| IV. YAGNI | PASS | 列見出しの小さな派生クラス 1 つと、条件の関数 1 つだけ。右クリックメニューなどは入れない。 |
| V. ドキュメント同期 | PASS（条件付き） | CLAUDE.md の `FrameEditDlg` の説明に「一覧のダブルクリックで追加」を追記するタスクを含める。機能 006 の「一覧のダブルクリック = 編集」の記述は置き換わる。 |
| VI. ドメイン仕様準拠 | PASS | シミュレーションの挙動は変えない。 |

違反なし。Complexity Tracking は該当なし。

**Post-design re-check**: research.md・contracts の設計後も上記の判定は変わらない（UI 側だけの変更、コアの変更なし）。

## Project Structure

### Documentation (this feature)

```text
specs/013-list-dblclick-add/
├── spec.md / plan.md / research.md / data-model.md / quickstart.md
├── contracts/ui-frame-list-dblclick.md
├── checklists/requirements.md
└── tasks.md（/speckit-tasks で作成）
```

### Source Code (repository root)

```text
MFCApplication1/
└── FrameEditDlg.h / .cpp   # 変更: CFrameListHeader（列見出しのダブルクリックを編集ウィンドウへ）、
                            #       CanAdd()、OnListDoubleClick を追加に、WM_APP_FRAME_LIST_ADD の処理
CLAUDE.md                   # 変更
```

**Structure Decision**: 入口（本体の `NM_DBLCLK`、列見出しの `WM_LBUTTONDBLCLK`）は違っても、行き先は 1 つの関数（`AddFromList`
→ `CanAdd()` を確かめて `OnAdd()`）にまとめる。こうすると「追加ボタンと同じ」（FR-005）と「追加できないときは何もしない」（FR-002）を
コード上の 1 か所で確認できる。

## Complexity Tracking

該当なし。
