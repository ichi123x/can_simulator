# Implementation Plan: シミュレーションのスピード設定

**Branch**: `ver.0.2`（spec ディレクトリ: `005-simulation-speed`） | **Date**: 2026-09-29 | **Spec**: [spec.md](./spec.md)

## Summary

プロジェクト設定に「スピード」（-13〜+10）を追加し、機能 004 の「1 フレーム 250 ms 固定・空き時間スキップ」の再生を、**実経過時間 × 2^スピード** でシミュレーション時刻を進める再生に置き換える。計算そのものは004 と同じく「開始」時に一括で行い（決定性を保つ）、その結果を実時間に合わせて見せながら完了を待つ。再生はフレーム表示の ON/OFF に関係なく行い、ON のときだけ強調を描く。約 16 ms ごとの書き換えで、直前の書き換えからの時刻区間に送信中だったフレームの送信元・受信 ECU をまとめて強調する。時刻計算と区間内フレームの抽出は `CanSim/` の純粋関数（`ReplayClock`）にし、UI はタイマー・経過時間の計測・描画だけを持つ。

## Technical Context

**Language/Version**: C++14相当（機能 001〜004 と同じ）

**Primary Dependencies**: MFC（動的リンク）、Win32（`SetTimer`、`QueryPerformanceCounter`）、C++ 標準ライブラリのみ。新規の外部ライブラリなし。

**Storage**: `.canprj` の `[Project]` に `Speed` キーを追加（任意、省略時 0）。結果の CSV は変更なし。

**Testing**: 機能 001〜004 と同じ方針。`ReplayClock`（倍率、経過時間→シミュレーション時刻、区間内フレームの抽出）と `Speed` の読込み・保存は `tools/cansim_runner/checks.cpp` で検証する。所要時間・表示は`quickstart.md` の手順（ストップウォッチと画面操作の自動化）で確認する。

**Target Platform**: Windows 10/11（Win32/x64、Debug/Release）

**Project Type**: desktop-app（既存 `MFCApplication1` を拡張）

**Performance Goals**: 画面の書き換えは約 16 ms 間隔を目標（1 秒間に 30 回以上を保証、SC-009）。1 回の書き換え（構成図の描画）は数 ms。スピード 0・10 秒の所要時間誤差 ±1 秒以内（SC-001）。

**Constraints**: 結果（3 種の CSV）はスピード・表示設定に依存しない（FR-013）。シミュレーションの計算方法は変えない（憲章 P③）。`CanSim/` は ASCII のみ。UI 文字列は `.rc` のみ。

**Scale/Scope**: 変更対象は `CanTypes.h`（設定 1 項目）、`ConfigLoader`・`ProjectFile`（`Speed` の読み書き）、新規 `ReplayClock`、`SettingsDlg`（リスト 1 つ）、`MFCApplication1Dlg`（再生ロジックの置き換え）、`.rc`。

## Constitution Check

| 原則 | 判定 | 根拠 |
|---|---|---|
| I. Spec Kit ワークフロー準拠 | PASS | spec.md（005）の後に本 plan.md を作成。 |
| II. MFC/C++ ネイティブ規約 | PASS | 新規ソース 1 組を 4 構成に登録するのみ。 |
| III. テスト可能性 | PASS | 時刻計算と区間内フレーム抽出を `CanSim/ReplayClock` に分離し、checks.exe で検証。UI はタイマーと描画だけ。 |
| IV. YAGNI | PASS | 計算は一括のまま（エンジンのステップ実行化やスレッド化はしない）。一時停止・早送り・実行中の速度変更は入れない。高精度タイマー（マルチメディアタイマー）も使わず、経過時間で補正する。 |
| V. ドキュメント同期 | PASS（条件付き） | CLAUDE.md（再生方式の記述、`ReplayClock`、`.canprj` の `Speed`）と 004 の契約（`project-file-format.md`・`ui-menu-dialogs.md` の再生表示）に後継の参照を追記するタスクが必須。 |
| VI. ドメイン仕様準拠 | PASS | 計算方式（フレーム単位 DES、決定性）を変えない。見せ方だけの変更。 |

違反なし。Complexity Tracking は該当なし。

**Post-Design Re-check**: Phase 1 の成果物は新規依存・新規 VS プロジェクトを含まない。全項目 PASS。

## Project Structure

### Documentation (this feature)

```text
specs/005-simulation-speed/
├── plan.md / research.md / data-model.md / quickstart.md
├── contracts/
│   ├── project-file-speed.md   # .canprj の Speed キー
│   └── ui-speed-replay.md      # セッティングのスピード欄、再生と強調の規則
└── tasks.md                    # /speckit-tasks で生成
```

### Source Code (repository root)

```text
MFCApplication1/
├── MFCApplication1.rc / Resource.h   # 変更: IDD_SETTINGS にスピードのリストを追加（UTF-16LE 注意）
├── SettingsDlg.h / .cpp              # 変更: スピード（-13〜+10）の選択
├── MFCApplication1Dlg.h / .cpp       # 変更: 250 ms 固定の再生を、経過時間ベースの再生（約 16 ms 書き換え）に置き換え
└── CanSim/
    ├── CanTypes.h                    # 変更: ProjectSettings に Speed
    ├── ConfigLoader.cpp              # 変更: [Project] Speed（符号付き整数）の読込み
    ├── ProjectFile.cpp               # 変更: Speed の保存
    └── ReplayClock.h / .cpp          # 新規: スピード→倍率、経過時間→シミュレーション時刻、区間内フレームの抽出
tools/cansim_runner/checks.cpp        # 変更: 005 の検証項目
CLAUDE.md                             # 変更
```

**Structure Decision**: 004 の「開始時に一括計算 → 結果を見せる」構成を維持し、見せ方の時計だけを差し替える。こうすることで、エンジンに手を入れずに FR-013（結果の不変）と「終了」時の結果（004 と同じ再実行方式）を保てる。スピード +10 は再生を行わず、004 の「フレーム表示 OFF」と同じ即時完了の経路を通る。

## Complexity Tracking

該当なし。
