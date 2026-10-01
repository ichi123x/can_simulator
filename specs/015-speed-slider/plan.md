# Implementation Plan: ツールバーのスピードスライダー

**Branch**: `ver.0.3`（spec ディレクトリ: `015-speed-slider`） | **Date**: 2026-10-01 | **Spec**: [spec.md](./spec.md)

## Summary

次の 2 つを行う。

1. セッティング（`IDD_SETTINGS`）からスピードの項目を削除する。
2. 機能 014 のツールバーの、開始・中断・終了のボタンの右に、自前で描くスピードのスライダーを置く。
   - 目盛りは -13〜+10 の 24 個。つまみは縦長の長方形で、下に目盛りの数字を並べる。

スライダーの操作:
- 押した位置・ドラッグ中の位置から一番近い目盛りを求め、値が変わるたびに `m_settings.Speed` を変える。
- 実行中は、変更の瞬間のシミュレーション時刻を新しい基準（`m_simBaseUs`）にして、新しいスピードで続ける（014 の再開と同じ仕組み）。
- 中断中は値だけ変え、再開で効く。
- +10 になったら、実行中・中断中はその場で完了にする（`FinishReplay`、Q1: A）。

スライダーの座標・目盛りへの丸め・変更時の動作の判定は、`CanSim/SimControls` の純粋関数にして checks.exe で検証する。
計算・CSV・プロジェクトファイルの形式は変えない。

## Technical Context

**Language/Version**: C++14相当（機能 001〜014 と同じ）

**Primary Dependencies**: MFC / GDI（014 のツールバーの描画に追加）。新規外部依存なし。

**Storage**: プロジェクトファイル（`.canprj`）の `[Project] Speed=` を今までどおり使う（形式は変えない）。

**Testing**:
- checks.exe に 015 の項目を加える。
  - 目盛りの座標（等間隔、ボタンの右、帯の中、数字が重ならない。DPI 96/120/144）
  - `SpeedAtX` の丸め（各目盛りの上・中間点・両端の外）
  - スライダーの当たり判定
  - `GetSpeedChangeAction`（状態 × 新しい値）
- 既存の 001〜014 の PASS と CSV の一致を確認する。
- GUI は自動操作で確認する（quickstart.md）。

**Target Platform**: Windows 10/11（Win32/x64、Debug/Release）

**Project Type**: desktop-app

**Performance Goals**: スピードの変更から新しい進み方になるまで 0.1 秒以内（SC-002。変更の処理の中で基準を付け替える）。

**Constraints**:
- `CanSim/` は ASCII のみ。
- `.rc` は UTF-16LE で、エンコーディングを明示して編集する。
- 構成図・ログ表示エリアの位置は機能 014 のまま（FR-012）。帯の高さは変えない。

**Scale/Scope**:
- 変更:
  - `CanSim/SimControls.h/.cpp`
  - `MFCApplication1Dlg.h/.cpp`
  - `SettingsDlg.h/.cpp`
  - `MFCApplication1.rc`（セッティングのダイアログ）
  - `Resource.h`（`IDC_COMBO_SPEED` の削除）
  - `tools/cansim_runner/checks.cpp`・`README.md`
  - `CLAUDE.md`

## Constitution Check

| 原則 | 判定 | 根拠 |
|---|---|---|
| I. Spec Kit ワークフロー準拠 | PASS | spec.md（015、Q1 回答済み）→ 本 plan.md。 |
| II. MFC/C++ ネイティブ規約 | PASS | 既存ファイルの変更のみ（新規ファイルなし）。4 構成のビルドを維持する。 |
| III. テスト可能性 | PASS | スライダーの座標・丸め・当たり判定・変更時の動作の判定を CanSim の純粋関数にし、checks.exe で検証する。ダイアログは描画と状態の更新だけ。 |
| IV. YAGNI | PASS | 014 の自前描画のツールバーに描き足す。標準のトラックバー（`CSliderCtrl`）は使わない（暗い帯・棒状のつまみ・24 個の数字を揃えるため。research.md 決定1）。キーボード・ホイール操作は入れない。 |
| V. ドキュメント同期 | PASS（条件付き） | CLAUDE.md（セッティング・ツールバー・スピード）と checks の README を更新するタスクを含める。 |
| VI. ドメイン仕様準拠 | PASS | スピードは再生（表示）の時計だけに効き、DES の計算・結果は変えない。 |

違反なし。Complexity Tracking は該当なし。

**Post-design re-check**: 設計後も判定は変わらない。

## Project Structure

### Documentation (this feature)

```text
specs/015-speed-slider/
├── spec.md / plan.md / research.md / data-model.md / quickstart.md
├── contracts/ui-speed-slider.md
├── checklists/requirements.md
└── tasks.md（/speckit-tasks で作成）
```

### Source Code (repository root)

```text
MFCApplication1/
├── CanSim/SimControls.h / .cpp      # 追加: スライダーの寸法・SpeedTickX・SpeedAtX・当たり判定・つまみ／数字の範囲、GetSpeedChangeAction
├── MFCApplication1Dlg.h / .cpp      # 変更: スライダーの描画・ドラッグ、SetSpeed（基準の付け替え・+10 で完了）、読込み後の再描画
├── SettingsDlg.h / .cpp             # 変更: スピードの項目を削除
├── MFCApplication1.rc / Resource.h  # 変更: IDD_SETTINGS からスピードのコンボと説明を削除、ダイアログを縮める
tools/cansim_runner/checks.cpp, README.md
CLAUDE.md
```

**Structure Decision**: スライダーは 014 のツールバーの一部として、同じ `SimControls`（座標）と `DrawSimToolbar`（描画）・マウス処理に足す。
スピードの値は `m_settings.Speed` の 1 か所に持ち、スライダーはそれを描くだけにする（保存・読込み・開始の判定は今のまま使える）。

## Complexity Tracking

該当なし。
