# Data Model: 018 フレームが流れる様子のアニメーション

新しい保存データは無い。プロジェクトファイル・CSV は変わらない。

## CanSim（`CanSim/FlowEffect.h`、UI 非依存、96 dpi のコンテンツ座標）

| 要素 | 内容 |
|---|---|
| `kFlowPeriodUs = 600000` | 1 周期（実時間） |
| `kFlowParticles = 3` | 道筋 1 本あたりの粒の数 |
| `kFlowParticleRadius = 4` | 粒の半径（96 dpi） |
| `kFlowGlowInset = 3` | 光の枠を箱から外側へ広げる量 |
| `kFlowGlowWindow = 0.15` | 送信の光（出たばかり）・受信の光（届く直前）とみなす進みの幅 |
| `double FlowPhase(long long elapsedUs)` | `(elapsedUs mod 600000) / 600000`（負は 0）。0 以上 1 未満 |
| `struct FlowPoint { double X, Y; }` | 粒の中心 |
| `struct FlowGlow { int Ecu; bool Sender; double Strength; }` | ECU の光（強さ 0〜1。0 は描かない） |
| `struct FlowEffect { std::vector<FlowPoint> Particles; std::vector<FlowGlow> Glows; }` | 描く内容 |
| `FlowEffect ComputeFlowEffect(const BusDiagramLayout&, const ActiveFrames&, double phase)` | 下の規則で計算 |
| `ActiveFrames::Flows`（`ReplayClock.h` に追加） | 書き換え 1 回分に送られたフレームの（送信, 受信）の組。昇順・重複なし・送信 ≠ 受信。`Receivers` と違い、送信もした ECU を含む |
| `double FlowPathLength(const EcuPlacement& s, const EcuPlacement& r)` / `FlowPoint FlowPointAt(s, r, double f)` | 道筋の長さと、進み f（0〜1）の点（checks 用にも公開） |

### `ComputeFlowEffect` の規則

- `Flows` が空、または `BusBusy` が偽なら、空。
- レイアウトに無い ECU 番号は無視する。
- `Flows` の各組 (s, r) について、粒 k = 0..2 を `FlowPointAt(s, r, frac(phase + k/3))` に置く。
- 光:
  - 送信 s の強さは、s から出る粒のうち `f < 0.15` のものの `1 − f/0.15` の最大値。
  - 受信 r の強さは、r へ向かう粒のうち `f > 0.85` のものの `(f − 0.85)/0.15` の最大値。
  - 強さ 0 の光は入れない。

## ダイアログ（`CMFCApplication1Dlg`）

| 要素 | 内容 |
|---|---|
| `m_flowElapsedUs` | エフェクトの時計（実行中だけ進む実時間の累積） |
| `m_flowLastQpc` | 最後に時計を進めた時刻 |
| 描画 | `DrawBusDiagram` の最後で、`active && active->BusBusy` のとき、光の枠 → 粒 |

## 改訂 1（案 A・C・D）

| 要素 | 内容 |
|---|---|
| `kFlowSpeed = 150` / `kFlowSpacing = 16` | 粒の速さ（px/秒、96 dpi）と間隔（px）。`kFlowPeriodUs`・`kFlowParticles`・`FlowPhase` は使わない（削除） |
| `kFlowGlowPeriodUs = 800000` | 光の明滅の周期 |
| `kFlowHoldUs = 300000` | 道筋を残す時間 |
| `FlowEffect ComputeFlowEffect(const BusDiagramLayout&, const std::vector<std::pair<int,int>>& flows, long long elapsedUs)` | `flows` の各組の道筋に、距離 `d = offset + 16k ≤ L` の粒を置く（`offset = (elapsedUs × 150 / 1e6) mod 16`）。光は道筋の送信・受信の ECU ごとに 1 つ。強さは `0.5 + 0.5 sin(2πt/800ms + φ)`（送信 φ=0、受信 φ=π）を 0.05〜1 に丸めたもの |
| `class FlowPathHold { void Update(const std::vector<std::pair<int,int>>&, long long nowUs); std::vector<std::pair<int,int>> Current(long long nowUs); void Clear(); }` | 道筋を最後に見えてから 300 ms 残す |

- 色: 粒は `RGB(255,140,0)` の芯と白い縁。光の枠は `RGB(255,190,120)`〜`RGB(255,140,0)`、太さは 1〜4 px（96 dpi）。
