# Implementation Plan: シミュレーションの中断とツールバー（開始・中断・終了）

**Branch**: `ver.0.3`（spec ディレクトリ: `014-sim-pause-toolbar`） | **Date**: 2026-10-01 | **Spec**: [spec.md](./spec.md)

## Summary

次の 3 つを加える。

1. **中断（一時停止）**: 実行の状態に「中断中」を足す（待機・実行中・中断中）。
   - 「中断」では、その時点まで表示を進めてから再生の時計を止める。
   - 中断中の「開始」で、中断した時点のシミュレーション時刻から再生を続ける。
   - 中断中の「終了」は、中断した時刻で機能 005 の「終了」を行う。
   - 計算・CSV は変えない（再生の時計だけの変更）。
2. **メニュー**: 「シミュレーション」メニューを「開始」「中断」「終了」にする。
3. **ツールバー**: メニューの下に、自前で描く暗い帯のツールバーを置く。
   - 帯には、開始（緑の三角）・中断（青の 2 本線）・終了（赤の四角）の角丸ボタンが並ぶ。
   - ボタンを押すと、対応するメニューのコマンドを送る。
   - ツールバーの分だけウィンドウを縦に広げ、構成図とログ表示エリアは帯の下へそのままずらす。

ボタンとメニューの有効／無効は、1 つの純粋関数（`CanSim::GetSimCommandStates`）で決めて両方に使う（FR-004 の一致を構造で保証する）。
ツールバーの座標と当たり判定・再開後の時刻の計算は、`CanSim/` の UI 非依存な関数にして checks.exe で検証する。

## Technical Context

**Language/Version**: C++14相当（機能 001〜013 と同じ）

**Primary Dependencies**: MFC / GDI（自前描画のツールバー、`RoundRect` / `Polygon`、メモリ DC）。新規外部依存なし。ツールバーのビットマップ
リソースは使わない（図形を描く）。

**Storage**: なし（プロジェクトファイルの形式は変えない）

**Testing**:
- `CanSim/SimControls.*` の純粋関数を checks.exe で検証する。
  - 状態 × 編集中の有効／無効の表（FR-004）
  - ツールバーの座標（帯の中に収まる・重ならない・DPI 96/120/144）
  - 当たり判定（描画と同じ換算）
  - 再開後の時刻（`ResumedSimTimeUs`: 基準時刻 + 経過 × 2^スピード、飽和）
- 既存の検証項目（001〜013）の PASS とサンプルの CSV の一致を確認する。
- GUI は自動操作で確認する（quickstart.md）。
  - ボタンの有効／無効は、画面の色とメニュー項目の状態で確かめる。
  - 中断中にログの行数・時刻が増えないこと、再開後に続くこと、中断中の「終了」の CSV の最後の時刻を確かめる。

**Target Platform**: Windows 10/11（Win32/x64、Debug/Release）

**Project Type**: desktop-app

**Performance Goals**: 「中断」から表示が止まるまで 0.1 秒以内（SC-002。中断の処理の中で、タイマーを待たずに止める）。

**Constraints**:
- `CanSim/` の新規ソースは ASCII のみ。ボタンの名前（開始・中断・終了）は `.rc` の文字列テーブルに置く。
- `.rc` は UTF-16LE。PowerShell でエンコーディングを明示して編集する（CLAUDE.md）。
- 構成図・ログ表示エリアの大きさと、その中の座標（`BusDiagramLayout` の値、checks 011 の位置の検証）は変えない。帯の高さ分だけ全体を下げる。
- シミュレーションの計算・CSV・ログの行は変えない（FR-010）。

**Scale/Scope**:
- 新規: `CanSim/SimControls.h/.cpp`。
- 変更:
  - `MFCApplication1Dlg.h/.cpp`
  - `Resource.h`、`MFCApplication1.rc`（メニュー項目と文字列）
  - `MFCApplication1.vcxproj(.filters)`
  - `tools/cansim_runner/checks.cpp`、`tools/cansim_runner/README.md`
  - `CLAUDE.md`

## Constitution Check

| 原則 | 判定 | 根拠 |
|---|---|---|
| I. Spec Kit ワークフロー準拠 | PASS | spec.md（014、Q1 回答済み）→ 本 plan.md。 |
| II. MFC/C++ ネイティブ規約 | PASS | 新規ファイルは vcxproj/filters に追加し、4 構成のビルドを維持する。リソースは `.rc` の直接編集（エンコーディング明示）。 |
| III. テスト可能性 | PASS | 有効／無効の決定・ツールバーの座標と当たり判定・再開後の時刻を `CanSim/SimControls` の純粋関数にし、checks.exe で検証する。ダイアログは描画とコマンドの送出だけ。 |
| IV. YAGNI | PASS | 既存のツールバーの仕組み（`CToolBar`）は使わず、構成図と同じ自前描画にする（暗い帯・角丸・色付きの記号を、リソースのビットマップ無しで描ける。research.md 決定2）。ツールチップ・ショートカットは入れない。 |
| V. ドキュメント同期 | PASS（条件付き） | CLAUDE.md（アーキテクチャ・実行の状態・ツールバー）と checks の README を更新するタスクを含める。 |
| VI. ドメイン仕様準拠 | PASS | 中断は再生（表示）の時計だけを止め、シミュレーションの計算（DES）・結果は変えない。 |

違反なし。Complexity Tracking は該当なし。

**Post-design re-check**: research.md・data-model.md・contracts の設計後も判定は変わらない。

## Project Structure

### Documentation (this feature)

```text
specs/014-sim-pause-toolbar/
├── spec.md / plan.md / research.md / data-model.md / quickstart.md
├── contracts/ui-sim-controls.md
├── checklists/requirements.md
└── tasks.md（/speckit-tasks で作成）
```

### Source Code (repository root)

```text
MFCApplication1/
├── CanSim/SimControls.h / .cpp        # 新規: RunPhase、GetSimCommandStates、ツールバーの座標・当たり判定、ResumedSimTimeUs
├── MFCApplication1Dlg.h / .cpp        # 変更: RunState → CanSim::RunPhase（Paused を追加）、OnSimPause、再開、
│                                      #       ツールバーの描画・クリック、コンテンツの縦のずらし（m_contentTop）
├── Resource.h / MFCApplication1.rc    # 変更: ID_SIM_PAUSE、メニュー「中断(&P)」、IDS_SIM_BTN_START/PAUSE/STOP
└── MFCApplication1.vcxproj(.filters)  # 変更: SimControls の追加
tools/cansim_runner/checks.cpp, README.md  # 変更: 014 の検証項目
CLAUDE.md                              # 変更
```

**Structure Decision**:
- 「何が押せるか」と「どこがボタンか」は `CanSim/SimControls` に置き、メニューとツールバーの両方が同じ関数を使う。
- ダイアログは、状態の保持・描画・コマンドの送出だけを行う（憲章 III）。
- 構成図・ログ表示エリアの座標は、従来どおりコンテンツの左上を原点とする値のままにし、ダイアログの中で帯の高さ（`m_contentTop`）を
  足す 1 か所の換算にまとめる。

## Complexity Tracking

該当なし。
