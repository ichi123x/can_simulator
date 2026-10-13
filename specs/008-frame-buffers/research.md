# Research: 008

## 決定1: 送信バッファの本数の数え方
- **Decision**: 送信要求の時点の使用本数 = そのノードの `PendingFrames` の数 + （そのノードのフレームが送信中なら 1）。既存の実装では、調停に勝ったフレームは `PendingFrames` から外れて送信中になるため、送信中の 1 本を足す（spec Assumptions:送信中も送信完了までバッファを占有）。
- **Decision**: `EnqueueTxRequest` は結果（`Queued` / `Updated` / `Discarded`）を返す。同じフレームが送信待ちなら `Updated`（本数に関係なく更新。Doc FR-092a）。送信中の同じフレームは「送信待ち」ではないので、新しい要求は新しいバッファを使う（既存動作と同じ）。
- **Alternatives**: 送信中はバッファを解放済みとみなす — Doc FR-092「送信が成功するとバッファは空く」と合わない。

## 決定2: 受信 FIFO をイベントなしで表す
- **Decision**: 各ノードに「受信処理時刻の列」（`std::deque<long long>`、µs）を持つ。フレームの受信完了時刻 t（= 送信完了）ごとに、先頭から `≤ t` のものを外し（取り出し済み。同時刻は取り出しが先: spec FR-007）、残りが 8 本なら新しいフレームを捨て、そうでなければ t + 受信処理遅延を末尾に足す。
- **Rationale**: 受信処理遅延はノードごとに一定なので、受信処理時刻は到着順に単調増加し、FIFO の順と一致する。新しいイベント種別（受信処理）を作らずに済み、同時刻の処理順（Doc FR-013）も変えない。
- **影響**: 受信処理遅延 0 なら、受信完了時刻 t に入れた要素は次の受信完了（> t）の時点で必ず外れるので満杯にならない（Doc FR-095）。

## 決定3: OVERWRITE 行の形
- 送信側の破棄: `Timestamp_us` = 要求時刻、`Tx_Request` = 要求時刻、`Rx_Node`・`Tx_Start`・`Tx_End`・`Rx_Start`・`Rx_End`・`Rx_Process` は空欄、`Bus_Load_Pct` はその時刻の値、`Status` = OVERWRITE、`Retry_Count` = 0、`Update_Count` = 0。行はその時刻に出す（ログは行を出した時刻の順に並ぶ既存の方針）。
- 受信側の破棄: 通常の受信行と同じ内容で、`Timestamp_us` = `Rx_End`、`Rx_Process` は空欄、`Status` = OVERWRITE。
- 空欄の表現: `LogRow` の時刻に `-1` を入れ、`Logger` が負の値を空の値として出力する（`Rx_Node` の -1 は既存どおり空欄）。

## 決定4: 集計
- OVERWRITE 行はすべての統計（レイテンシ、送信待ち遅延、伝送時間、バス負荷、ジッター、Count）から除く。
- 見出しの末尾に `TxDiscard_Count,RxDiscard_Count` を追加。Node 行には、そのノードの送信側破棄数（`Tx_Node` 基準）と受信側破棄数（`Rx_Node` 基準）を入れ、ID 行と ALL 行は空の値にする。
- Node 行は従来どおり、送信した行を持つノードに出す。それに加えて、破棄が 1 件以上あるノードにも Node 行を出す（従来の行は変えない）。

## 決定5: 名前の変更
- `PendingFrame::OverwriteCount` / `LogRow::OverwriteCount` を `UpdateCount` に改名し、CSV の列名を `Update_Count` にする（Doc FR-051、v1.8）。checks.cpp の参照も改名する。

## 決定6: 検証用サンプル
- `sample_buffer_full.canprj`: ECU1 に初回オフセット 0・周期 100 ms・DLC 0 のフレームを 20 個（ID 0x100〜0x113）。ECU2 は受信処理遅延2 ms で、ECU3 が 1 ms 周期の短いフレームを送る（受信 FIFO が満杯になる）。スピード +10、10 秒。
