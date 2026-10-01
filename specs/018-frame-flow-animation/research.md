# Research: 018 フレームが流れる様子のアニメーション

## 決定1: 道筋（96 dpi のコンテンツ座標、`BusDiagramLayout` から）

- **Decision**: 送信 ECU s → 受信 ECU r の道筋は、次の 3 区間をつないだ折れ線とする。
  1. s の接続線: `(s.LineX, s.LineY1)`（箱の縁）→ `(s.LineX, kBusY)`（バスの中心）
  2. バス: `(s.LineX, kBusY)` → `(r.LineX, kBusY)`（左右どちらへも）
  3. r の接続線: `(r.LineX, kBusY)` → `(r.LineX, r.LineY1)`（箱の縁）
  - 道筋の長さ L = |s.LineY1 − kBusY| + |r.LineX − s.LineX| + |r.LineY1 − kBusY|。
  - 対にするのは、書き換え 1 回分に送られたフレームの（送信 ECU, 受信 ECU）の組（`ActiveFrames.Flows`、実装中に追加）。
    - 当初は `Senders × Receivers` としたが、`Receivers` は送信もした ECU を除く（赤を優先する 005 の作り）。
    - そのため、スピード 0 で全 ECU が同じ書き換えの間に送信すると、受信が空になり、粒が出なかった（`sample_collision` で確認）。
    - `Flows` は `ActiveFrameCursor::Collect` で、強調の集まりと同時に集める。強調の色（赤の優先）は変えない。
  - 構成図に無い ECU 番号（ECU 数の外）は使わない。
- **Rationale**:
  - FR-001・FR-005。粒が通るのは、強調されている送信・受信の接続線と、強調されているバスだけになる。
  - 複数送信のまとめ方は spec Edge Cases に従う。

## 決定2: 粒と周期

- **Decision**:
  - 周期 `kFlowPeriodUs = 600,000`（実時間）。位相 `phase = (elapsed mod period) / period`（0 以上 1 未満）。
  - 粒は道筋 1 本につき `kFlowParticles = 3` 個。粒 k の進み `f_k = frac(phase + k / 3)`。位置は道筋上の距離 `f_k × L` の点。
  - 粒の大きさは半径 4 px（96 dpi）。色は塗り `RGB(255,235,90)`（明るい黄）、縁 `RGB(255,255,255)`（1 px）。
- **Rationale**:
  - FR-002。600 ms は、スピード 0 で目で追える速さとした。構成図の端から端（約 1000 px）でも、1 周期で届く。
  - 粒を 3 個にすると、どの瞬間も道筋上に粒があり、「流れている」ことが伝わる。
  - 黄色の粒は、赤・青・黒のどれとも区別できる。
- **Alternatives considered**:
  - 粒の速さを一定（px/s）にする: 遠い受信 ECU ほど届くのが遅れ、受信の光の時期がばらつく。1 周期で必ず届く方が、一定のパターンとして分かりやすい。
  - バスの区間だけに流す: 送信 → 受信の向きが ECU まで伝わらない。

## 決定3: 送信・受信の光（縁の輝き）

- **Decision**:
  - 送信 ECU の光の強さ = 道筋 1 本でも、粒が出たばかり（`f_k < 0.15`）のものがあれば、`1 − f_k / 0.15` の最大値。
  - 受信 ECU の光の強さ = 粒が届く直前（`f_k > 0.85`）のものがあれば、`(f_k − 0.85) / 0.15` の最大値。
  - 光は、箱を外側へ 3 px 広げた角丸の枠として描く。
    - 太さ: `1 + 3 × 強さ` px（96 dpi）。
    - 色: 黄 `RGB(255,235,90)` と白の間で、強さに応じて混ぜる。
    - 強さ 0 なら描かない。
- **Rationale**:
  - FR-003。箱の塗り（赤・青）と ECU 名には重ねない（枠だけ）ので、強調の色と文字は読める（FR-004、SC-003）。
  - GDI の半透明（`AlphaBlend`）は使わず、太さと色の変化で光を表す。描画が軽く、PrintWindow での確認も安定する。
  - 箱の外側へ 3 px 広げても、構成図の領域・枠の内側に収まる（上段の箱の上端 34 px、下段の箱の下端 186 px、枠は 4〜212）。

## 決定4: エフェクトの時計

- **Decision**: ダイアログのメンバ `m_flowElapsedUs`（累積の実時間）と `m_flowLastQpc` を使う。
  - **進める**: 再生のタイマー処理（`AdvanceReplay`）のたびに、`now − m_flowLastQpc` を `m_flowElapsedUs` に足し、`m_flowLastQpc = now` にする。
  - **止める**: 中断（`OnSimPause`）では、`AdvanceReplay` の後、時計を止める（以後は足さない）。
  - **再開**: 再開（`ResumeReplay`）では `m_flowLastQpc = now` にする（中断の間は数えない）。
  - **新しい実行**: 新しい実行の開始では、`m_flowElapsedUs = 0`、`m_flowLastQpc = now` にする。
  - 位相は `CanSim::FlowPhase(m_flowElapsedUs)` で求める。
- **Rationale**:
  - FR-006・FR-007。強調の対象が書き換えごとに入れ替わっても、位相は連続して進む。中断中は止まる（SC-004）。
  - スピード・スピード変更（015）に関係しない（FR-002）。

## 決定5: 描画の場所と順序

- **Decision**: `DrawBusDiagram` の最後（ECU の箱・名前を描いた後、`BitBlt` の前）で、次のときだけ描く。
  - 条件: `active != nullptr`（= `InRun() && FrameDisplay`）かつ `active->BusBusy`。
  - 描く順:
    1. 光の枠
    2. 粒
  - 座標は 96 dpi の値を `ScalePx` で換算する。メモリ DC は構成図の領域なので、はみ出しは切られる。
- **Rationale**: FR-001・FR-005・FR-007。強調の上に重ね（FR-004）、構成図の外には描かない（spec Edge Cases）。

## 決定6: 性能の確かめ方

- **Decision**: 16 ECU の構成（`sample_16ecu`、フレーム表示 ON、スピード 0）で比べる。
  - 1 秒間に撮った画面写しの中で、構成図が前の 1 枚と異なる回数を数える。
  - エフェクトありとなしで、同じ程度であることを確かめる。
  - あわせて、checks で最悪の場合（16 ECU、送信 8 × 受信 8）の `ComputeFlowEffect` を 1,000 回計算して、合計の時間が 1 秒未満であることを確かめる。
- **Rationale**: SC-005。書き換えの回数を直接数える手段がないので、計算の重さと見た目の更新で代える。
