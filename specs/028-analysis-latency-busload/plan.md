# Implementation Plan: 解析メニューの受信完了時間（表・ヒストグラム）と CAN バス負荷

**Branch**: `ver.0.6`（spec ディレクトリ: `028-analysis-latency-busload`） | **Date**: 2026-10-03 | **Spec**: [spec.md](./spec.md)

## Summary

「解析」メニューの「準備中」を「受信完了時間」「CAN バス負荷」の 2 項目に置き換える。どちらも現在のプロジェクトのフレームログ（`.log.csv`）を読んで計算し、モードレスのウィンドウに表とグラフを出す。

- **CanSim**（新規 `LogAnalysis.h/.cpp`。プレーン C++、ASCII のみ）が、計算をすべて担う。
  - ログの読み込み（`ReadLogRecords`）
  - 受信完了時間の集計（送信 ECU × 受信 ECU ごとの件数・最小・平均・最大・95 パーセンタイル）
  - ヒストグラムのビン
  - バス負荷（10 ms の区間・平均・最大・ECU ごとの使用率）
  - グラフの軸の目盛り（`NiceTicks`）と、点の間引き（区間の最大）

  checks で、手で計算した値と比べて検証する（憲章 III）。
- **ダイアログ**（新規 `AnalysisDlg.h/.cpp`）は、計算結果を表示するだけにする。
  - 2 つのウィンドウ `CLatencyDlg`・`CBusLoadDlg`: 一覧（`CListCtrl` のレポート表示）とグラフ（自前描画の `CWnd`）
  - 作り・閉じ方は 025 の `CLogViewDlg` と同じ（`new` + `Open`、✖・Esc で `DestroyWindow`、`PostNcDestroy` でオーナーに通知して `delete this`）
- **メインダイアログ**は、メニューのハンドラ 2 つと、開いているウィンドウの管理を持つ。
  - ハンドラの通知はログ表示と同じ（025: プロジェクトなし・ログなし・読めない）。
  - 開いているウィンドウは、025 でログのウィンドウを閉じていたすべての場所（`CloseLogViewer` の呼び出し 7 か所）と、027 のログクリアで閉じる。

## 計算の決まり（data-model.md に詳細）

- **受信完了時間**: 受信 ECU のある行ごとに `Rx_End − Tx_Request`。
  - 対象: Status が `SUCCESS` か `ARBITRATION_LOST_RETRY` の行（ログの各行は、最終的に送れた 1 回）。
  - `OVERWRITE` の行は集計に含めず、件数だけ数える。
- **95 パーセンタイル**: 最近順位法。小さい順に並べた `ceil(0.95 × n)` 番目（1 始まり）。
- **ヒストグラム**:
  - ビンの数の目安は `clamp(ceil(sqrt(n)), 10, 50)`。
  - 幅は `(最大 − 最小) ÷ 目安` を 1・2・5 × 10^k に切り上げた「きりの良い」値（最小 1 µs）。
  - 始まりは最小値を幅で切り捨てた値。すべて同じ値なら幅 1 のビンが 1 本。
- **バス負荷**:
  - 送信 1 回 = `(Tx_Node, CAN_ID, Tx_Start)` が同じ行の集まり（受信 ECU の数だけ重複する行を 1 回にまとめる。`OVERWRITE` は送信していないので除く）。
  - 使用時間は `[Tx_Start, Tx_End)`。
  - 範囲は 0 から、最大の `Tx_End` を含む 10 ms の区間の終わりまで（送信が無ければ 10 ms）。
  - 区間の負荷 = 区間内の使用時間 ÷ 10 ms。平均 = 使用時間の合計 ÷ 範囲。ECU ごと = その ECU の使用時間 ÷ 範囲。
- **グラフの間引き**: 区間の数がグラフの幅（ピクセル）より多いときは、1 列に入る区間の最大値を描く（山を見落とさない）。

## Technical Context

- **Language/Version**: C++14 / MFC（Visual Studio 2022、v143）。新規依存なし（グラフは GDI で自前描画）
- **Primary Dependencies**: MFC（`CListCtrl`・`CDialogEx`）、CanSim
- **Storage**: 読むだけ（`<プロジェクト>.log.csv`）。形式は機能 001・008 のとおり（列名はヘッダーで探す）
- **Testing**: `tools/cansim_runner/checks.cpp` に 028 の項目。
  - 小さなログを文字列で作って、受信完了時間・パーセンタイル・ビン・負荷を手計算と照合する。
  - 16 ECU の車両サンプルを 5 秒実行したログで、件数の一致と、使用率の合計 = 平均を確かめる。
  - GUI は [quickstart.md](./quickstart.md) の手順で確かめる。
- **Target Platform**: Windows、Win32 / x64 × Debug / Release
- **Project Type**: デスクトップアプリ（単一ダイアログ + モードレスのウィンドウ）
- **Performance Goals**: 16 ECU・5 秒（約 4,000 フレーム、受信の行は約 2 万）で、1 秒以内に開く（SC-004）。600 秒・数百万行でも数秒で開く（1 回の読み込みで集計）
- **Constraints**: `CanSim/` は ASCII のみ。日本語の見出し・メッセージは `.rc`（UTF-16LE）
- **Scale/Scope**: CanSim の新規 1 組、ダイアログの新規 1 組（2 クラス + グラフの部品）、リソース（ダイアログ 2・メニュー 2・文字列約 12）

## Constitution Check

| 原則 | 判定 | 理由 |
|---|---|---|
| I. Spec Kit | PASS | spec（確認 3 件回答済み）→ plan → tasks の順 |
| II. ビルド | PASS | 新しいファイルを `.vcxproj`・`.filters` に登録し、4 構成のビルドを tasks で確かめる |
| III. テスト可能性 | PASS | 読み込み・集計・ビン・負荷・目盛り・間引きはすべて CanSim の純粋関数にし、checks で検証する。ダイアログは表示だけ |
| IV. シンプルさ | PASS | グラフのライブラリは入れず、GDI の線・矩形・文字だけで描く。2 つのウィンドウは 025 のログのウィンドウと同じ作りを繰り返す。書き出しや設定の操作は足さない |
| V. ドキュメント | PASS | CLAUDE.md（アーキテクチャ・025 の節に 028）と checks の README を更新する |
| VI. ドメイン仕様 | PASS | 出力済みのログを読むだけで、シミュレーションの計算と CSV の形式は変えない。バス負荷・レイテンシは [性能・ログ] の評価指標の見せ方を足すもの |

設計後の再確認: 変更なし（PASS）。

## Project Structure

```text
specs/028-analysis-latency-busload/
  spec.md / plan.md / research.md / data-model.md / quickstart.md
  contracts/ui-analysis.md
  checklists/requirements.md

MFCApplication1/CanSim/LogAnalysis.h/.cpp    新規: ReadLogRecords / AnalyzeLatency / MakeHistogram / AnalyzeBusLoad / NiceTicks / MaxPerColumn
MFCApplication1/AnalysisDlg.h/.cpp           新規: CLatencyDlg / CBusLoadDlg / CAnalysisGraph（自前描画のグラフ）
MFCApplication1/MFCApplication1Dlg.h/.cpp    OnAnalysisLatency / OnAnalysisBusLoad、m_latencyDlg / m_busLoadDlg、CloseResultWindows
MFCApplication1/FrameEditDlg.h               WM_APP_LATENCY_CLOSED (WM_APP + 6)、WM_APP_BUSLOAD_CLOSED (WM_APP + 7)
MFCApplication1/Resource.h, MFCApplication1.rc   ダイアログ 2、メニュー、文字列
MFCApplication1/MFCApplication1.vcxproj(.filters) 新規ファイルの登録
tools/cansim_runner/checks.cpp, README.md   028 の項目
CLAUDE.md
```

**Structure Decision**: 計算は CanSim の新しいモジュール、表示は新しいダイアログのファイルに分ける。メインダイアログはグルーだけ（憲章 III）。

## Complexity Tracking

なし。
