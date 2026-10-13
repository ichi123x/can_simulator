# Implementation Plan: フレームが流れる様子のアニメーション

**Branch**: `ver.0.3`（spec ディレクトリ: `018-frame-flow-animation`） | **Date**: 2026-10-01 | **Spec**: [spec.md](./spec.md)

## Summary

フレーム表示 ON の実行中（中断中を含む）、構成図の強調の上に「流れのエフェクト」を重ねて描く。

- **粒の道筋**: 強調されている各送信 ECU から各受信 ECU へ、次の道筋を光の粒（1 本につき 3 個）が一定周期（実時間 600 ms）で進む。
  - 送信 ECU の箱の縁 → 接続線 → バス → 受信 ECU の接続線 → 受信 ECU の箱の縁
- **ECU の光**: 送信 ECU には粒が出るときの光、受信 ECU には粒が届くときの光を、箱の縁に重ねて描く。

役割の分け方:
- 道筋と粒の位置・光の強さは、新しい `CanSim/FlowEffect` の純粋関数で計算し、checks.exe で検証する。
- 描画はダイアログの `DrawBusDiagram` の最後で行う。
- 周期の位相は、実行中（Running）だけ実時間で進む「エフェクトの時計」として、ダイアログが持つ。
  - 中断中は止まる。書き換えのたびにやり直さない。

計算・CSV・強調の対象（`ActiveFrameCursor`）・強調の色は変えない。

## Technical Context

**Language/Version**: C++14相当（機能 001〜017 と同じ）

**Primary Dependencies**: MFC / GDI（`Ellipse`・`RoundRect`、メモリ DC 上の描画）。新規外部依存なし。

**Storage**: なし

**Testing**:
- checks.exe に 018 の項目を加える。
  - 道筋の端点（送信 ECU の箱の縁・受信 ECU の箱の縁）
  - 道筋が接続線とバスの上だけを通ること
  - 粒の数（送信 × 受信 × 3）、受信しない ECU を通らないこと
  - 位相 0〜1 で粒が単調に進むこと
  - 光の強さの範囲
  - 送信または受信が空ならエフェクトが空であること
  - 位相の計算（実時間の経過 → 0〜1、600 ms 周期）
- 既存の 001〜017 の PASS と CSV の一致を確認する。
- GUI は自動操作で確認する（quickstart.md）。

**Target Platform**: Windows 10/11（Win32/x64、Debug/Release）

**Project Type**: desktop-app

**Performance Goals**: 16 ECU で、書き換えの回数がエフェクト無しと同程度（SC-005）。粒の数は「送信 ECU の数 × 受信 ECU の数 × 3」。
- 上限は 16 × 15 × 3 = 720 だが、実際の送信 ECU は書き換え 1 回分で数個である。
- 描くのは小さな円なので、GDI で十分速い。

**Constraints**:
- `CanSim/` は ASCII のみ。
- エフェクトは構成図の領域（メモリ DC）の中だけに描く。
- 強調の色（赤・青）は変えない。

**Scale/Scope**:
- 新規: `CanSim/FlowEffect.h/.cpp`。
- 変更:
  - `MFCApplication1Dlg.h/.cpp`（エフェクトの時計、描画）
  - `MFCApplication1.vcxproj(.filters)`
  - `tools/cansim_runner/checks.cpp`・`README.md`
  - `CLAUDE.md`

## Constitution Check

| 原則 | 判定 | 根拠 |
|---|---|---|
| I. Spec Kit ワークフロー準拠 | PASS | spec.md（018）→ 本 plan.md。 |
| II. MFC/C++ ネイティブ規約 | PASS | 新規ファイルを vcxproj/filters に追加し、4 構成のビルドを維持する。 |
| III. テスト可能性 | PASS | 道筋・粒の位置・光の強さ・位相を CanSim の純粋関数にし、checks.exe で検証する。ダイアログは時計の保持と描画だけ。 |
| IV. YAGNI | PASS | 粒（円）と縁の光だけ。半透明合成（`AlphaBlend`）・エフェクトの設定画面は入れない（research.md 決定3）。 |
| V. ドキュメント同期 | PASS（条件付き） | CLAUDE.md（`CanSim/` の一覧、フレーム表示の段落）と checks の README を更新するタスクを含める。 |
| VI. ドメイン仕様準拠 | PASS | 表示だけの変更。フレーム単位 DES・結果は変えない（エフェクトはビットの進みを模擬しない、決まった周期の飾り）。 |

違反なし。Complexity Tracking は該当なし。

**Post-design re-check**: 設計後も判定は変わらない。

## Project Structure

### Documentation (this feature)

```text
specs/018-frame-flow-animation/
├── spec.md / plan.md / research.md / data-model.md / quickstart.md
├── contracts/ui-flow-effect.md
├── checklists/requirements.md
└── tasks.md（/speckit-tasks で作成）
```

### Source Code (repository root)

```text
MFCApplication1/
├── CanSim/FlowEffect.h / .cpp        # 新規: FlowPhase、ComputeFlowEffect（道筋・粒・光）
├── MFCApplication1Dlg.h / .cpp       # 変更: エフェクトの時計（m_flowElapsedUs・m_flowLastQpc）、DrawBusDiagram の最後でエフェクトを描く
└── MFCApplication1.vcxproj(.filters) # 変更: FlowEffect の追加
tools/cansim_runner/checks.cpp, README.md
CLAUDE.md
```

**Structure Decision**: 「どこに何を描くか」は `FlowEffect`（96 dpi のコンテンツ座標、`BusDiagramLayout` と `ActiveFrames` から計算）に置き、ダイアログは DPI 換算して描くだけにする（機能 003/010 の構成図と同じ分け方）。

## Complexity Tracking

該当なし。
