# Research: 013 一覧のダブルクリックでフレーム追加

## 決定1: 一覧の本体（行・空白）は `NM_DBLCLK` を使う

- **Decision**: 既存の `ON_NOTIFY(NM_DBLCLK, IDC_LIST_FRAMES, OnListDoubleClick)` をそのまま使い、中身を `OnEdit()` から
  `AddFromList()`（決定3）に変える。
- **Rationale**: レポート表示の一覧は、行の上でも行の無い空白（`iItem` = -1）でもダブルクリックで `NM_DBLCLK` を親に送る。
  フレームが 0 件の一覧でも届く。スクロールバーは一覧の非クライアント領域なので、そこのダブルクリック（`WM_NCLBUTTONDBLCLK`）では
  `NM_DBLCLK` は届かない。これで FR-001（本体）と FR-004（スクロールバーは対象外）を満たす。
- **Alternatives considered**:
  - 一覧をサブクラス化して `WM_LBUTTONDBLCLK` を取る: 一覧の既定の処理（1 回目のクリックでの行の選択など）との関係を自分で
    扱う必要があり、`NM_DBLCLK` で足りるので不要。

## 決定2: 列見出しはサブクラス化して `WM_LBUTTONDBLCLK` を取る

- **Decision**: `CHeaderCtrl` の派生クラス `CFrameListHeader` を `FrameEditDlg.h/.cpp` に置き、`OnInitDialog` で
  `m_list.GetHeaderCtrl()` をサブクラス化する。`OnLButtonDblClk` では既定の処理を呼ばず、編集ウィンドウに
  `WM_APP_FRAME_LIST_ADD`（`WM_APP + 2`、編集ウィンドウ内だけで使う）を `PostMessage` する。
- **Rationale**:
  - 列見出しは一覧の子ウィンドウで、その通知（`HDN_ITEMDBLCLICK` / `HDN_DIVIDERDBLCLICK`）は一覧に届き、編集ウィンドウには
    届かない。また、最後の列より右の空いた列見出しのダブルクリックでは、どちらの通知も出ない。ウィンドウメッセージの
    `WM_LBUTTONDBLCLK` なら、列見出しのどこでも受けられる（列見出しのウィンドウクラスは `CS_DBLCLKS` を持つ）。
  - 既定の処理を呼ばないので、境目のダブルクリックによる列幅の自動調整は行われない（spec Edge Cases）。ドラッグでの列幅の変更は
    `WM_LBUTTONDOWN` / `WM_MOUSEMOVE` / `WM_LBUTTONUP` で行われるので、そのまま残る。
  - `PostMessage` にするのは、モーダルの入力画面を列見出しのメッセージ処理の中から開かないため（列見出しのマウスの捕捉などの
    途中で別のメッセージループに入るのを避ける）。編集ウィンドウが先に閉じられていれば、送ったメッセージは捨てられるだけで害はない。
- **Alternatives considered**:
  - 一覧をサブクラス化して `HDN_ITEMDBLCLICK` / `HDN_DIVIDERDBLCLICK` を `OnNotify` で取る: 最後の列より右の空いた列見出しで
    通知が出ないため、赤枠全体（ユーザー回答）を満たせない。
  - `PreTranslateMessage` で列見出しの `WM_LBUTTONDBLCLK` を見る: モードレスのウィンドウでは呼び出し元（メインのメッセージループ）
    に依存し、仕組みが分かりにくい。サブクラス化の方が直接的。

## 決定3: 行き先を 1 つにまとめ、追加できる条件を共通にする

- **Decision**:
  - `bool CanAdd() const` = ECU が選ばれていて（`SelectedNodeId() > 0`）、一覧の件数が `CanSim::kMaxFramesPerNode`（100）未満。
    `UpdateButtons` の「追加」ボタンの有効／無効はこれを使う。
  - `void AddFromList()` = `CanAdd()` が偽なら何もしない（メッセージも出さない）。真なら `OnAdd()` を呼ぶ。
  - 本体の `NM_DBLCLK` と `WM_APP_FRAME_LIST_ADD` はどちらも `AddFromList()` を呼ぶ。
- **Rationale**: FR-002・FR-005。今の `OnAdd` は ECU が無いときしか拒まない（100 件のときはボタンが無効なので呼ばれない）。
  ダブルクリックはボタンの有効／無効と関係なく起きるので、同じ条件を明示的に確かめる。追加の処理そのものは `OnAdd` のままで、
  入力画面・検査・末尾への追加と選択・オーナーの無効化（機能 010）がボタンとまったく同じになる。

## 決定4: 行の上のダブルクリックで選ばれた行

- **Decision**: 特別な処理はしない。1 回目のクリックで一覧が行を選ぶ（既定の動作）。OK なら `OnAdd` が追加した行を選び直し、
  キャンセルなら選ばれた行が残る。
- **Rationale**: spec Edge Cases のとおり。「編集」「削除」ボタンの有効／無効は、既存の `LVN_ITEMCHANGED` → `UpdateButtons` で
  選択に追従する。

## 決定5: 「編集」ボタンは変えない

- **Decision**: `OnEdit` はボタン（`IDC_BUTTON_FRAME_EDIT`）からだけ呼ばれるようになる。中身は変えない。
- **Rationale**: FR-003。
