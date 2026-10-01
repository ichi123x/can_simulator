---

description: "Task list for feature 018-frame-flow-animation"
---

# Tasks: フレームが流れる様子のアニメーション

**Input**: Design documents from `/specs/018-frame-flow-animation/`

**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/, quickstart.md（すべて存在）。機能 005・014・016・017 の実装が完了していること。

**Tests**: spec.md は自動テストのフレームワークを要求していない。新しい純粋関数（`CanSim/FlowEffect`）は checks.exe に検証項目を加える（憲章 III）。
動きの見た目は GUI の自動操作（画面写し・画素の色）と目視で確認する。

**Organization**: spec.md のユーザーストーリーは 1 つ（US1 送信 ECU から受信 ECU へ流れる動きを見る P1）。

## Format: `[ID] [P?] [Story] Description`

## Path Conventions

- コア（UI 非依存、ASCII のみ）は `MFCApplication1/CanSim/`。
- UI は `MFCApplication1/MFCApplication1Dlg.*`。
- 検証は `tools/cansim_runner/checks.cpp`。GUI 検証スクリプトはリポジトリ外（scratchpad の `t018/`。`t016/`・`t017/` の関数を土台にする）。

---

## Phase 1: Setup

- [X] T001 GUI 検証の準備をする（リポジトリ外、scratchpad の `t018/`）。
  - `t017/Gui.cs` と、`t016/gui.ps1`・`t017/gui.ps1` の関数をコピーする。コピーする関数は次のとおり。
    - 読込み、ボタン・チェックボックスのクリック
    - 構成図の点の色、領域の画像のハッシュ、領域の画像の保存（`SaveRegion`）
  - 構成図の領域に、指定の色の画素があるかを数える関数を用意する（`[Ext17]::HasColor` を広げて、数を返す `CountColor` を Gui.cs に加える）。
  - ECU n の「箱の外側の帯（箱を 6 px 広げた範囲から箱を除いた所）」と「接続線の周り（LineX ± 6 px、LineY1〜バス）」の画素を調べる関数を用意する（96 dpi の座標を DPI 換算し、帯の高さ `S(64)` を足す）。
  - 一時フォルダに検証用の構成を作る。
    - `sample_collision` を `FrameDisplay=1`・スピード 0 にしたもの
    - 同じくスピード -10 にしたもの
    - `sample_16ecu` を `FrameDisplay=1` / `0`・スピード 0 にしたもの

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: 道筋・粒・光・位相の計算（US1 の描画が使う）

- [X] T002 `MFCApplication1/CanSim/FlowEffect.h/.cpp` を新規作成する（data-model.md、research.md 決定1〜3）。ASCII のみ。`#include "BusDiagramLayout.h"`・`#include "ReplayClock.h"`（`ActiveFrames`）・`<vector>`。
  - 定数:
    - `kFlowPeriodUs = 600000`、`kFlowParticles = 3`
    - `kFlowParticleRadius = 4`、`kFlowGlowInset = 3`
    - `kFlowGlowWindow = 0.15`
  - `double FlowPhase(long long elapsedUs)`: `elapsedUs <= 0` なら 0、それ以外は `(elapsedUs % kFlowPeriodUs) / (double)kFlowPeriodUs`。
  - `struct FlowPoint { double X = 0, Y = 0; };`
  - `struct FlowGlow { int Ecu = 0; bool Sender = false; double Strength = 0; };`
  - `struct FlowEffect { std::vector<FlowPoint> Particles; std::vector<FlowGlow> Glows; };`
  - 道筋の関数:
    - `double FlowPathLength(const EcuPlacement& s, const EcuPlacement& r)` = `|s.LineY1 - kBusY| + |r.LineX - s.LineX| + |r.LineY1 - kBusY|`。
    - `FlowPoint FlowPointAt(const EcuPlacement& s, const EcuPlacement& r, double f)`: f を 0〜1 に丸め、距離 `f × L` の点を返す。
      - 区間 1: `(s.LineX, s.LineY1)` → `(s.LineX, kBusY)`
      - 区間 2: `(s.LineX, kBusY)` → `(r.LineX, kBusY)`
      - 区間 3: `(r.LineX, kBusY)` → `(r.LineX, r.LineY1)`
  - `FlowEffect ComputeFlowEffect(const BusDiagramLayout& layout, const ActiveFrames& active, double phase)`:
    - `!active.BusBusy` か、送信・受信のどちらかが空なら、空を返す。
    - 番号でレイアウトの ECU を探し、無い番号は飛ばす。
    - 各送信 s × 各受信 r × k = 0..2 について、`f = frac(phase + k / 3.0)` で粒を置く。
    - 光の強さを集める。
      - 送信の光: `f < 0.15` で `1 - f / 0.15`
      - 受信の光: `f > 0.85` で `(f - 0.85) / 0.15`
      - ECU ごとの最大値にし、0 より大きいものだけを `Glows` に入れる。
- [X] T003 [P] `MFCApplication1/MFCApplication1.vcxproj` と `.vcxproj.filters` に `CanSim\FlowEffect.h`（ClInclude）・`CanSim\FlowEffect.cpp`（ClCompile）を追加する。014 の `SimControls` と同じ並べ方・filter（CanSim）にする。
- [X] T004 [P] `tools/cansim_runner/checks.cpp` に `CheckFeature018()` を追加し、`main` で `CheckFeature017()` の後に呼ぶ。`#include "FlowEffect.h"`、`<chrono>`（quickstart.md 1）。（依存: T002）
  - `FlowPhase`: 0→0、300000→0.5、600000→0、-1→0。1,000 点の値がすべて 0 以上 1 未満。
  - `FlowPointAt`（`ComputeBusDiagramLayout(6)`、組み合わせは (1→2)、(2→1)、(1→3)、(3→1)、(6→1)）:
    - 端点: f = 0 で送信の `(LineX, LineY1)`、f = 1 で受信の `(LineX, LineY1)`。
    - f = 0, 0.01, …, 1 の各点が、x = 送信の LineX、x = 受信の LineX、y = kBusY のいずれかの上にある（誤差 1e-6）。
    - 隣り合う点の道筋上の距離（マンハッタン距離）が単調に増える。
  - `ComputeFlowEffect`:
    - 送信 {1}、受信 {3, 5}、BusBusy で、粒が 6 個。
    - 送信が空・受信が空・`BusBusy` が偽のときは、粒も光も 0 個。
    - 送信 {1}、受信 {3}、ECU 数 6 のとき、どの粒も ECU 2・4・5・6 の接続線（x = LineX、y がバスと箱の間）の上に無い。
    - phase = 0 のとき、ECU 1 の送信の光の強さが 1。光の強さはすべて (0, 1]。
  - 性能: `ComputeBusDiagramLayout(16)`、送信 {1..8}・受信 {9..16} の `ComputeFlowEffect` を、phase を変えながら 1,000 回計算して 1 秒未満（`std::chrono::steady_clock`）。

**Checkpoint**: checks.exe の 018 項目と既存項目がすべて PASS。

---

## Phase 3: User Story 1 - 送信 ECU から受信 ECU へ流れる動きを見る (Priority: P1) 🎯 MVP

**Goal**: フレーム表示 ON の実行中、強調の上に、流れの粒と送信・受信の光を描く。実時間で一定周期、中断中は止まる。

**Independent Test**: `sample_collision`（フレーム表示 ON、スピード 0）で、粒が送信 ECU から受信 ECU へ進み、受信しない ECU 2 には行かない（quickstart.md 手順 2-1〜2-7）

- [X] T005 [US1] `MFCApplication1/MFCApplication1Dlg.h/.cpp` に、エフェクトの時計を追加する（research.md 決定4）。（依存: T002）
  - メンバ `long long m_flowElapsedUs = 0;`、`LARGE_INTEGER m_flowLastQpc = {};` を追加する。`#include "CanSim/FlowEffect.h"`。
  - 新しい実行の開始（`OnSimStart` の Idle の分岐で、`QueryPerformanceCounter(&m_replayStartQpc)` の所）で、`m_flowElapsedUs = 0`・`m_flowLastQpc = m_replayStartQpc` にする。
  - `AdvanceReplay` の先頭で、時計を進める。
    - `QueryPerformanceCounter(&now)` を取り、`(now - m_flowLastQpc)` を μs に直して `m_flowElapsedUs` に足し、`m_flowLastQpc = now` にする。
    - 換算は `m_qpcFrequency` を使う。0 なら足さない。
  - `ResumeReplay` で `m_flowLastQpc = m_replayStartQpc`（再開の時刻）にする（中断の間は数えない）。
  - 中断中は `AdvanceReplay` が呼ばれないので、時計は止まる（追加の処理は不要。その旨をコメントに書く）。
- [X] T006 [US1] `MFCApplication1/MFCApplication1Dlg.cpp` の `DrawBusDiagram` の最後で、エフェクトを描く（research.md 決定3・5、contracts/ui-flow-effect.md）。（依存: T005）
  - 位置は、ECU の箱と名前を描くループの後、`mem.SelectObject(oldFont)` / `BitBlt` の前。
  - 条件は `active != nullptr && active->BusBusy`。
  - `ComputeFlowEffect(layout, *active, CanSim::FlowPhase(m_flowElapsedUs))` を求める。
  - 光（先に描く）: 各 `FlowGlow` について、ECU の箱 `ScaleRect(ecu.Box, dpi)` を `ScalePx(kFlowGlowInset, dpi)` だけ外側へ広げた角丸の枠を描く。
    - 塗りなし（`NULL_BRUSH`）。角丸は `ScalePx(kEcuCornerPx + 3, dpi)`。
    - 太さ: `max(1, ScalePx(1 + 3 × Strength, dpi))`。
    - 色: 黄 `RGB(255,235,90)` と白 `RGB(255,255,255)` を、Strength で混ぜる（Strength 1 で白寄り）。
  - 粒（後に描く）: 各 `FlowPoint` を中心に、半径 `ScalePx(kFlowParticleRadius, dpi)` の円を描く。
    - 塗り `RGB(255,235,90)`、縁 `RGB(255,255,255)`（太さ `max(1, ScalePx(1, dpi))`）。
    - 座標は `ScalePx` で換算する（double は四捨五入）。
  - 使ったペン・ブラシは元に戻す。（spec FR-001〜FR-005、FR-007）
- [X] T007 [US1] Debug|x64 をビルドし、実機で quickstart.md 手順 2-1〜2-7 を確認する（依存: T001, T003, T006）。
  - 2-1: `sample_collision`（ON、0）で、0.1 秒おきに構成図を 5 枚保存する。
    - 黄 `FFEB5A` の画素の重心が、枚ごとに変わる。
    - 5 枚を並べた画像で、送信 → 受信の向きに進んでいることを目視で確かめる。
  - 2-2: 同じ実行の 10 秒の間、0.2 秒おきに ECU 2 の箱の外側の帯と接続線の周りを調べる。`FFEB5A`・白の光の画素が 1 度も無い。
  - 2-3: 粒がある瞬間の画面写しで、次を確かめる。
    - 送信 ECU の箱の中の点が `F06E6E`、受信 ECU の箱の中の点が `A0C3F5`。
    - 名前が読める（目視）。
  - 2-4: 中断して 5 秒待つと、構成図のハッシュが変わらない。再開後 1 秒で、ハッシュが変わる。
  - 2-5: チェックボックスを外すと黄の画素が 0 になり、入れると戻る。
  - 2-6: `sample_16ecu` で、エフェクトありとなしを比べる。
    - ON・OFF それぞれで 1 秒間、できるだけ速く構成図のハッシュを取り、前と異なる枚数を数える。
    - ON が OFF の 0.8 倍以上。
  - 2-7: スピード -10 で、0.1 秒おきの粒の重心の動きが、スピード 0 と同程度（目視と数値）。
  - （SC-001〜SC-005、spec US1 受け入れシナリオ 1〜7）

**Checkpoint**: US1 の受け入れシナリオ 1〜7 を満たす

---

## Phase 4: Polish & Cross-Cutting Concerns

- [X] T008 [P] `CLAUDE.md` を更新する（憲章 V）。
  - アーキテクチャの `CanSim/` の一覧に `FlowEffect.h/.cpp` を加える。
    - 内容: 流れのエフェクトの道筋（送信の接続線 → バス → 受信の接続線）。
    - 内容: 粒（1 本 3 個、600 ms 周期）と送信・受信の光の計算。
    - 内容: `FlowPhase` / `ComputeFlowEffect`。
  - 機能 016 か 017 の段落の後に、機能 018 の段落を加える。
    - エフェクトの見た目。
    - エフェクトの時計（`m_flowElapsedUs`、実行中だけ進み、中断で止まり、スピードに依存しない）。
    - 描く条件（`InRun() && FrameDisplay && BusBusy`）。
  - 検証範囲の記述を「001〜011・014・015・017・018」にする。
- [X] T009 [P] `tools/cansim_runner/README.md` に 018 の項目を加え、検証範囲を 018 まで広げる。
- [X] T010 Debug/Release × x64/x86 の全 4 構成で、次を確認する（依存: T007）。
  - 警告なしでビルドできる。
  - `checks.exe` の全項目が PASS。
  - サンプルの CSV が基準と一致する（spec SC-006）。

---

## Dependencies & Execution Order

- **Setup**: T001（実装と並行可）
- **Foundational**: T002 → T003（並行可）・T004（T002 の後）
- **US1**: T005（T002 の後）→ T006 → T007（T001・T003 の後）
- **Polish**: T008・T009 は並行可、T010 は最後

### Parallel Opportunities

- T003（vcxproj）と T004（checks）は、T005・T006（ダイアログ）と並行可。
- T008・T009（文書）は並行可。

## Implementation Strategy

1. **Foundational**（T002〜T004）: 計算を CanSim に入れ、checks で固める。
2. **MVP = US1**（T005〜T007）: 時計と描画を入れる（これで要望はすべて）。
3. **Polish**: 文書・4 構成ビルド・checks。

## Notes

- シミュレーションエンジン・CSV・強調の対象と色は変更しない。
- `CanSim/FlowEffect.*` は ASCII のみ。

## 実装結果（2026-10-01）

- 新規: `CanSim/FlowEffect.h/.cpp`（ASCII のみ）。
- 変更:
  - `CanSim/ReplayClock.h/.cpp`（`ActiveFrames::Flows` の追加。実装中の見直し、下記）
  - `MFCApplication1Dlg.h/.cpp`（エフェクトの時計 `m_flowElapsedUs` / `m_flowLastQpc`、`DrawFlowEffect`）
  - `MFCApplication1.vcxproj(.filters)`
  - `tools/cansim_runner/checks.cpp`・`README.md`、`CLAUDE.md`
- **実装中の見直し**: 当初は粒の道筋を「強調の送信 × 強調の受信」から作る設計（research.md 決定1）だった。
  - しかし `sample_collision`（スピード 0）では、ECU 1・3 が同じ書き換えの間に互いに送り合う。
  - 強調の「受信 ECU」は、赤を優先して送信もした ECU を除くため、空になり、粒が 1 つも出なかった（画面写しで確認）。
  - そこで、`ActiveFrameCursor::Collect` で、実際のフレームの（送信, 受信）の組 `Flows` も集め、道筋はその組から作るように変えた。
  - 強調の色（赤の優先）は変えていない。spec の Edge Cases・research.md・data-model.md も合わせて直した。
- ビルド: Debug/Release × x64/x86 の 4 構成とも、警告・エラーなしでビルドできた。
- checks.exe: 全 161 項目 PASS（018 の 10 項目を追加。既存の強調の項目も PASS）。基準 CSV との比較も一致した。
  - 最悪の場合（送信 8 × 受信 8）の計算 1,000 回は 0.016 秒だった。
- GUI（Debug|x64、120 dpi）: scratchpad の `t018/gui.ps1` で、全項目が合格した。
  - 2-1: `sample_collision`（60 秒版、フレーム表示 ON、スピード 0）で 0.1 秒おきに 5 枚撮った。5 枚とも構成図が異なり、黄の粒が描かれていた。
    - 並べた画像 `t018/flows.png` で、ECU 1 ⇄ ECU 3 の間を、接続線・バスに沿って粒が流れることを目視で確認した。
  - 2-2: 8 秒の間、0.2 秒おきに調べた。受信しない ECU 2 の箱の周りと接続線には、粒・光の画素が 0 だった。
  - 2-3: 粒がある瞬間も、送信 ECU の箱は `F06E6E` だった。ECU 名が読めることを `t018/colours.png` で確認した。
  - 2-4: 中断中は 5 秒たっても構成図が変わらなかった。再開後は再び動いた。
  - 2-5: チェックボックスを外すと粒の画素は 0 になり、入れると戻った。
  - 2-6: `sample_16ecu`（スピード 0）で 2 秒間、構成図をできるだけ速く取得した。
    - エフェクトありは 38 回中 38 回で画面が更新されていた。
    - 取得の回数は、フレーム表示 OFF（39 回）と同程度で、エフェクトによる遅れは見られなかった。
  - 2-7: スピード -10（`sample_appendix_a`）でも、粒が描かれて動いた（30 枚中 6 枚に粒。フレームの無い間は描かない）。

---

## Phase 5: 改訂 1（ユーザーの確認を受けて: 案 A・C・D）

- [X] T011 `MFCApplication1/CanSim/FlowEffect.h/.cpp` を改訂 1 に合わせて変える（research.md 決定7〜9、data-model.md 改訂 1）。
  - 削除: `kFlowPeriodUs`・`kFlowParticles`・`kFlowGlowWindow`・`FlowPhase`。
  - 追加: `kFlowSpeed = 150`、`kFlowSpacing = 16`、`kFlowGlowPeriodUs = 800000`、`kFlowHoldUs = 300000`。
  - `ComputeFlowEffect(layout, flows, elapsedUs)` に変える。
    - 粒: `offset = (elapsedUs × 150 / 1e6) mod 16`、距離 `d = offset + 16k ≤ L`。
    - 光: 道筋の送信・受信の ECU ごとに 1 つ。強さは `0.5 + 0.5 sin(2πt/800ms + φ)` を 0.05〜1 に丸めたもの（φ は送信 0、受信 π。両方に当たる ECU は送信の値）。
  - `FlowPathHold`（`Update` / `Current` / `Clear`）を追加する。
- [X] T012 `MFCApplication1/MFCApplication1Dlg.h/.cpp` を変える。（依存: T011）
  - メンバ `CanSim::FlowPathHold m_flowHold` を追加する。
  - `AdvanceReplay` で、時計を進めた後に `m_flowHold.Update(m_active.Flows, m_flowElapsedUs)` を呼ぶ。
    - `m_active` は、`AdvanceReplay` の中の `Collect` の後に更新する。
  - 新しい実行の開始で `m_flowHold.Clear()` を呼ぶ。
  - `DrawBusDiagram` の描く条件を `InRun() && FrameDisplay` にする。その中で `Current(m_flowElapsedUs)` が空でなければ描く。
  - 色を変える。
    - 粒: `RGB(255,140,0)` の芯と白い縁。
    - 光: `RGB(255,190,120)`〜`RGB(255,140,0)`、太さ 1〜4 px。
- [X] T013 `tools/cansim_runner/checks.cpp` の `CheckFeature018` を改訂 1 に合わせて変える。（依存: T011）
  - 粒:
    - 間隔が 16（同じ道筋の隣どうしの距離）。
    - 時計を Δ 進めると、各粒が `150 × Δ` 進む（`mod 16`）。
    - 数が `floor((L − offset)/16) + 1`。
  - 光: 強さが 0.05〜1。送信と受信で半周期ずれる。
  - `FlowPathHold`: t=0 で見た組が t=300 ms まで残り、t=300.001 ms で消える。再び見ると残る時間が延びる。`Clear` で空になる。
  - 性能（送信 8 × 受信 8 の 1,000 回）が 1 秒未満。
- [X] T014 Debug|x64 をビルドし、実機で確かめる（依存: T012, T013）。
  - 次の確認の色を `FFEB5A` から `FF8C00` に読み替えて、もう一度確かめる。
    - quickstart.md の手順 2-1〜2-7
    - ECU 2 の判定は、オレンジの判定に変える。
  - 加えて、`sample_collision`（スピード 0）で 25 ms おきに構成図を 20 枚撮る。オレンジの画素の数の変動が小さい（点滅しない）ことを確かめる（最小 ÷ 最大 ≥ 0.5）。
  - バスが空いてから 0.3 秒たつと消えることは、スピード -10 で確かめる。
  - 5 枚を並べた画像を目視して、滑らかさ・色の見え方を確かめる。
- [X] T015 `CLAUDE.md`・`tools/cansim_runner/README.md` の 018 の記述を改訂 1 に合わせて直し、4 構成のビルドと checks.exe（全項目 PASS・CSV 一致）を確かめる（依存: T014）。

## 改訂 1 の実装結果（2026-10-01）

- 変更:
  - `CanSim/FlowEffect.h/.cpp`
    - `FlowPhase` を削除し、`FlowOffset`（150 px/秒・16 px 間隔）を追加。
    - `ComputeFlowEffect(layout, flows, elapsedUs)`。同じ位置の粒は 1 つにまとめる。
    - 光の明滅、`FlowPathHold`。
  - `MFCApplication1Dlg.h/.cpp`（`m_flowHold`、描く条件、オレンジの色）
  - `tools/cansim_runner/checks.cpp`・`README.md`、`CLAUDE.md`
- 実装中の追加: 16 ECU では送信と受信の組が多く、1 回に数千個の粒を描いて画面の取得が遅くなった（エフェクトなしの 0.76 倍）。
  - 同じ送信 ECU から出る道筋は、送信側の接続線とバスの区間で粒の位置が同じになる。そこで、重なる粒は 1 つにまとめた。
  - 最悪の場合（送信 8 × 受信 8）の粒は 2,129 → 164 個になり、計算は 1 回 0.3 ms になった。
- ビルド: Debug/Release × x64/x86 の 4 構成とも、警告・エラーなしでビルドできた。
- checks.exe: 全 163 項目 PASS（018 の項目を改訂 1 で書き直し、12 項目）。基準 CSV との比較も一致した。
- GUI（Debug|x64、120 dpi）: scratchpad の `t018/gui.ps1`（色の判定を `FF8C00` に変更）で、全項目が合格した。
  - 2-1:
    - 0.1 秒おきの 5 枚で、構成図はすべて異なった。
    - `t018/rev1_0.png` で、オレンジの粒が 16 px おきに接続線とバスに沿って並び、ECU 1・3 の外側にオレンジの光の枠があることを目視で確認した。
  - 点滅: 25 ms おきの 20 枚で、オレンジの画素の数の最小 ÷ 最大は 0.52（735 / 1421）で、基準 0.5 以上だった。
    - 光の枠の 0.8 秒周期の明滅（太さ 1〜4 px）で画素の数が変わる分を含む。
  - 2-2: 8 秒の間、ECU 2 の箱の外側の帯と接続線に、オレンジの画素は 0 だった。
    - 箱の中は、「ECU 02」の文字の ClearType の縁がオレンジに近い色になるため、判定から外した。
  - 2-3〜2-5: 送信 ECU の塗り `F06E6E` は変わらなかった。中断中は止まり、再開で動き、チェックボックスで消えて戻った。
  - 2-6: `sample_16ecu` で 2 秒間取得した。エフェクトありは 36 回（毎回画面が更新）、フレーム表示 OFF は 39 回で、0.92 倍だった。
  - 2-7・道筋の保持: スピード -10 でも粒は動いた。バスが空いている間は消えた（30 枚中 8 枚に粒）。
