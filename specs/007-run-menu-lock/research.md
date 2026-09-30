# Research: 007

## 決定1: トップレベルメニューを位置指定でグレーにする
- **Decision**: `GetMenu()->EnableMenuItem(0, MF_BYPOSITION | (running ? MF_GRAYED : MF_ENABLED))`（ファイル）と位置 1（編集）を
  設定し、`DrawMenuBar()` で再描画する。呼び出しは、実行状態が Running になった直後（`OnSimStart`）と、Idle に戻る `EndRun`。
- **Rationale**: グレーのトップレベル項目は、クリックしても Alt+ニーモニックでもドロップダウンが開かない（Windows の標準動作）。
  既存の `OnInitMenuPopup` による項目単位の制御と併用する。
- **Alternatives**: `OnInitMenuPopup` で配下の項目をすべてグレーにする — メニュー自体は開いてしまい、spec FR-001（開けない）を
  満たさない。メニューバーを外す — 画面のレイアウトが変わる。
- **確認事項（実装時）**: Alt+F で、グレーのトップレベル項目にハイライトが移るだけで開かないことを GUI で確かめる。

## 決定2: 実行中のフレーム表示の切替・保存を防ぐガード
- **Decision**: `OnEditFrameDisplay` と `OnFileSaveProject` の冒頭に「実行中なら何もしない」を追加する（メニューが無効でも、
  コマンドが直接送られる場合に備える。既存の読込み・セッティング・データフレーム編集と同じ扱い）。

## 決定3: インターミッションの検証（エンジンは変更しない）
- **Decision**: `sample_collision`（毎周期 2 フレームが同時に要求）を 4 つの通信速度で実行し、Tx_Start 順に並べた連続する
  2 フレーム（同じフレームの複数の受信行はまとめる）について:
  - 後のフレームの Tx_Request ≤ 前のフレームの Tx_End + 3 ビット時間 なら、Tx_Start − 前の Tx_End = 3 ビット時間
  - それ以外でも Tx_Start − 前の Tx_End ≥ 3 ビット時間
  - Tx_End − Tx_Start = `ComputeFrameTiming(ID, DLC, 0xFF データ).TxDurationNs`（インターミッションを含まない）
- **Rationale**: spec SC-003・SC-004。3 ビット時間は 125k: 24 µs、250k: 12 µs、500k: 6 µs、1M: 3 µs で、すべて µs の整数。
