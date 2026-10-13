# Research: 012 開始時の表示の初期化

## 決定1: 表示の初期化を 1 つの関数にまとめ、計測関連の初期化より前に呼ぶ

- **Decision**: `void ResetDisplaysForRun()` を追加し、`OnSimStart` で保存先の確定の後、計算のブロック（`CWaitCursor` と`BuildRunNetwork` 以降）の前に 1 回呼ぶ。中身の順序は次のとおり。
  1. `ClearLog()`: 行・表示済み数・先頭行を 0 にし、スクロールバーを更新する。
  2. `ClearGray()`
  3. `m_active = CanSim::ActiveFrames()`: 強調を空にする。
  4. `InvalidateRect(GetDiagramRect(), FALSE)`・`InvalidateRect(GetLogTextRect(), FALSE)`
  5. `UpdateWindow()`
- **Rationale**: FR-001〜FR-003・FR-005。今の `ClearLog`・`ClearGray` は無効化しかしないため、描き直しは計算が終わってメッセージループに戻ったときになる。`UpdateWindow` は、無効化した領域の `WM_PAINT` をその場で送る。これで計算の前に画面が空になる。
- **Alternatives considered**:
  - 計算を別スレッドにする: 機能 004 の同期計算の設計を変えることになり、要求を超える。
  - 計算の途中でメッセージを処理する: 計算はコアの中の 1 回の呼び出しで、途中に UI の処理を挟めない。

## 決定2: キャンセル・保存失敗では初期化しない

- **Decision**: 呼び出し位置は、保存先を尋ねて保存する処理（`AskSavePath` / `SaveProject`）が成功した後とする（今の `ClearLog` と同じ位置）。
- **Rationale**: FR-006。実行しない場合に表示を消さない。

## 決定3: スクロールバー

- **Decision**: `ClearLog` が呼ぶ `UpdateLogScroll` で、範囲 0・位置 0・無効にする。スクロールバーは子ウィンドウなので、`SetScrollInfo(…, TRUE)` でその場で描き直される。
- **Rationale**: FR-003。前回の実行で遡っていた位置を引き継がない（先頭行 `m_logTop` = 0）。

## 決定4: 完了後の表示位置は変えない

- **Decision**: `SetLogShown` の追従（一番下を見ている間は最新行を一番下に保つ）は変えない。スピード +10 の即時完了も今のまま。
- **Rationale**: ユーザー回答（spec Assumptions）。新しい実行は先頭行 0・表示 0 行から始まるので、行がエリアに収まる間は上から順に並び、超えた後は機能 009 のとおり追従する（FR-004）。
