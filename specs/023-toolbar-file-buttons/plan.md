# Implementation Plan: ツールバーへのプロジェクト読込み・保存ボタンの追加

**Branch**: `ver.0.4`（spec ディレクトリ: `023-toolbar-file-buttons`） | **Date**: 2026-10-01 | **Spec**: [spec.md](./spec.md)

## Summary

ツールバーの左端に「保存」「読込み」の 2 つのボタンを足し、ボタン 1 つ分の空きを置いて開始・中断・終了を続ける。
ボタンの座標と有効／無効の判定は `CanSim/SimControls` に置き、ツールバーのボタンを 5 つ（保存・読込み・開始・中断・終了）の
通し番号で扱えるようにする。既存の部品（シミュレーションのボタン・チェックボックス・シミュレーション時間・「スピード：」・スライダー）は
すべて 84 px（ボタンの間隔 28 × 3）右へずらす。

- **読込みボタン**: メニューと同じ `ID_FILE_LOAD_PROJECT` を Post する。
- **保存ボタン**: ツールバー専用の新しいコマンド `ID_FILE_SAVE_OVERWRITE` を Post する。プロジェクトファイルがあればそこへ上書きし、無ければ保存先を尋ねる。
  メニューの保存と「開始」の保存先確認にある「書いて、失敗ならメッセージ、成功なら現在のファイルにする」処理は、共通の 1 関数にまとめる。

## 新しい座標（96 dpi、帯の中）

| 部品 | 変更前 | 変更後 |
|---|---|---|
| 保存ボタン（通し番号 0） | なし | 左 8（箱 24 × 18、上 14。名前 y 33〜49） |
| 読込みボタン（1） | なし | 左 36 |
| 空き | — | 64〜88（ボタン 1 つ分） |
| 開始・中断・終了（2〜4） | 左 `8 + 28i` | 左 `92 + 28i`（i = 0〜2。終了の右端 172） |
| チェックボックス | 欄 104・文字 124〜208・当たり 100〜212 | 欄 188・文字 208〜292・当たり 184〜296 |
| シミュレーション時間 | 230〜410（数値〜380、μs 384〜） | 314〜494（数値〜464、μs 468〜） |
| 「スピード：」 | 430〜500 | 514〜584 |
| スライダー | -13 の目盛り 520（+10 は 819） | -13 の目盛り 604（+10 は 903、数字の右端 916） |

- 縦方向の座標・大きさ、帯の高さ 64、構成図の幅 1140 は変えない。+10 の数字の右端 916 は幅 1140 に収まる。
- 通し番号 i のボタンの左端は `8 + 28 × slot` とする。slot は、保存・読込みでは i、開始・中断・終了では i + 1（空きの分）。

## ボタンの見た目

| ボタン | 記号（10 px の枠の中） | 記号の色 | 名前 |
|---|---|---|---|
| 保存 | フロッピー: 四角 10 × 10、上に白い窓 5 × 3 | 紫 `RGB(120, 70, 180)` | 保存（`IDS_FILE_BTN_SAVE` = 251） |
| 読込み | フォルダー: 左上のつまみ 4 × 2 ＋ 本体 10 × 7 | 黄土 `RGB(210, 150, 30)` | 読込み（`IDS_FILE_BTN_LOAD` = 252） |

箱の色、押下中・無効の色、名前の文字（10 px、白／無効は灰）はシミュレーションのボタンと同じにする。

## Technical Context

- **Language/Version**: C++14 / MFC（Visual Studio 2022、v143）。新規依存なし
- **Primary Dependencies**: MFC・GDI（描画）、CanSim（プレーン C++）
- **Storage**: `.canprj`（形式は変えない）
- **Testing**: `tools/cansim_runner/checks.cpp`（座標・当たり判定・有効／無効を検証）。GUI の動作は [quickstart.md](./quickstart.md) の手順で確認
- **Target Platform**: Windows、Win32 / x64 × Debug / Release
- **Project Type**: デスクトップアプリ（単一ダイアログ）
- **Constraints**: `CanSim/` は ASCII のみ。日本語の名前は `.rc` の文字列テーブル（UTF-16LE）に置く
- **Scale/Scope**: ボタン 2 つの追加と、既存部品の横方向の移動

## Constitution Check

| 原則 | 判定 | 理由 |
|---|---|---|
| I. Spec Kit | PASS | spec → plan → tasks の順に進めている |
| II. ビルド | PASS | 4 構成のビルドを tasks で確認する |
| III. テスト可能性 | PASS | 座標・当たり判定・有効／無効は CanSim の純粋関数に置き、checks で検証する。ダイアログは描画と Post だけを担う |
| IV. シンプルさ | PASS | ボタンの仕組みは既存のもの（押して同じボタンで離すと Post）を通し番号で広げるだけ。新しいウィンドウやコントロールは足さない |
| V. ドキュメント | PASS | CLAUDE.md のツールバーの節と checks の README を更新する |
| VI. ドメイン仕様 | PASS | UI だけの変更で、シミュレーションの計算・CSV に影響しない |

設計後の再確認: 変更なし（PASS）。

## Project Structure

```text
specs/023-toolbar-file-buttons/
  spec.md / plan.md / research.md / data-model.md / quickstart.md
  contracts/ui-toolbar-file-buttons.md
  checklists/requirements.md

MFCApplication1/CanSim/SimControls.h/.cpp   ToolbarButton（通し番号）、座標・当たり判定・有効／無効、既存部品の定数を +84
MFCApplication1/MFCApplication1Dlg.h/.cpp   5 つのボタンの描画・押下、OnFileSaveOverwrite、保存処理の共通化
MFCApplication1/Resource.h, MFCApplication1.rc   ID_FILE_SAVE_OVERWRITE、IDS_FILE_BTN_SAVE / LOAD
tools/cansim_runner/checks.cpp, README.md   023 の検証項目
CLAUDE.md                                   ツールバーの節
```

**Structure Decision**: 既存の構成のまま。ドメインロジックは増えず、UI の座標と判定だけを CanSim に足す。

## Complexity Tracking

なし。
