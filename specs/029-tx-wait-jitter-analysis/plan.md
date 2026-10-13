# Implementation Plan: 解析メニューの送信待ち時間と送信ジッター

**Branch**: `ver.0.7`（spec ディレクトリ: `029-tx-wait-jitter-analysis`） | **Date**: 2026-10-05 | **Spec**: [spec.md](./spec.md)

## Summary

「解析」メニューに「送信待ち時間(W)」「送信ジッター(J)」を加え、全体を「送信待ち時間・受信完了時間・送信ジッター・区切り線・CAN バス負荷」の順に並べ替える（Q2 = A）。どちらも 028 と同じく、現在のプロジェクトのフレームログ（`.log.csv`）を `CanSim::ReadLogRecords` で読み、CanSim で計算し、モードレスのウィンドウに表とグラフを出す。

- **CanSim**（`LogAnalysis.h/.cpp` に追加。ASCII のみ）:
  - `CollectTransmissions`: ログの行を送信 1 回にまとめる（`(TxNode, CanId, TxStartUs)` が同じ行。OVERWRITE は除き、件数だけ数える）。028 の `AnalyzeBusLoad` と同じまとめ方。
  - `AnalyzeTxWait`: 送信待ち時間（`TxStartUs − TxRequestUs`）の全体と CAN ID ごとの件数・最小・平均・最大・P95（028 の受信完了時間と同じ統計の計算を共通の関数にして使う）。ヒストグラムは 028 の `MakeHistogram` をそのまま使う。
  - `AnalyzeJitter`: CAN ID ごとに、送信開始の順に並べ、連続する 2 回の「送信開始の間隔 − 送信要求の間隔」をずれとする。周期（送信要求の間隔の最頻値）・件数・最小・最大・母標準偏差と、ずれの点の列（時刻 = 後の送信の送信開始）を返す。
  - `MinMaxPerColumn`: ずれの点を画面の列ごとにまとめ、列の最小と最大を残す（Q1 = A の間引き。山も谷も残す）。
- **ダイアログ**（`AnalysisDlg.h/.cpp` に追加）:
  - `CTxWaitDlg`: 028 の `CLatencyDlg` と同じ並び（一覧・説明の行・ヒストグラム）。
  - `CJitterDlg`: 一覧・説明の行・ずれのグラフ。
  - `CAnalysisGraph` に 3 つ目の描き方 `SetDeviation`（横軸 = 時刻 ms、縦軸 = ずれ µs、0 の線、点と折れ線。点が多いときは列ごとの最小〜最大の縦線）を足す。
  - 作り・閉じ方・位置は `CAnalysisDlgBase` をそのまま使う（新しい通知 `WM_APP_TXWAIT_CLOSED` / `WM_APP_JITTER_CLOSED`）。
- **メインダイアログ**: ハンドラ 2 つ（`OnAnalysisTxWait` / `OnAnalysisJitter`。流れは 028 の 2 つと同じ）、`m_txWaitDlg` / `m_jitterDlg`、`CloseResultWindows` で 2 つも閉じる（これで FR-008 の契機はすべて満たす）。`OnInitMenuPopup` で 2 項目を `idleOnly` にする。
- **メニュー**（`.rc`）: 「解析(&A)」を並べ替え、区切り線を入れる。

## 計算の決まり（data-model.md に詳細）

- **送信 1 回**: Status が `OVERWRITE` 以外で、`TxRequestUs >= 0`・`TxStartUs >= 0` の行を、`(TxNode, CanId, TxStartUs)` で 1 つにまとめる。
- **送信待ち時間**: `TxStartUs − TxRequestUs`。全体の行と、CAN ID ごとの行（CAN ID の昇順）。P95 は最近順位法（`ceil(0.95 × n)` 番目）。
- **ジッター**: CAN ID ごとに送信開始の順に並べ、`k ≥ 1` について `(Start[k] − Start[k−1]) − (Request[k] − Request[k−1])`。
  - 破棄が無ければ `Request[k] − Request[k−1]` は周期そのものなので、機能 001 の FR-019（`Start[k] − Start[k−1] − 周期`）と同じ値になる。
  - 周期 = `Request[k] − Request[k−1]` のうち最も多い値（同数なら小さい方）。送信が 1 回だけなら周期・統計は空。
  - 標準偏差は母標準偏差（`sqrt(Σ(x − 平均)² / n)`）。集計 CSV と同じ計算（CSV は小数第 2 位で丸める）。

## Technical Context

- **Language/Version**: C++14 / MFC（Visual Studio 2022、v143）。新規依存なし
- **Primary Dependencies**: MFC（`CListCtrl`・`CDialogEx`）、CanSim（028 の `LogAnalysis`）
- **Storage**: 読むだけ（`<プロジェクト>.log.csv`）
- **Testing**: `tools/cansim_runner/checks.cpp` に 029 の項目。
  - 小さなログを文字列で作って、送信待ち時間・ジッター・周期・間引きを手計算と照合する。
  - 16 ECU の車両サンプルを 5 秒実行したログで、件数の一致と、ジッターが集計 CSV（`.summary.csv`）の CAN ID ごとの値と一致することを確かめる（SC-003）。
  - GUI は [quickstart.md](./quickstart.md) の手順で確かめる。
- **Target Platform**: Windows、Win32 / x64 × Debug / Release
- **Project Type**: デスクトップアプリ（単一ダイアログ + モードレスのウィンドウ）
- **Performance Goals**: 16 ECU・5 秒で 1 秒以内に開く（SC-004）。600 秒・数百万行でも 028 と同程度（読み込み 1 回 + 並べ替え 1 回）
- **Constraints**: `CanSim/` は ASCII のみ。日本語の見出し・メッセージは `.rc`（UTF-16LE）
- **Scale/Scope**: CanSim の関数 4 つ、ダイアログ 2 クラス + グラフの描き方 1 つ、リソース（ダイアログ 2・メニュー 2 + 区切り線・文字列 7）

## Constitution Check

| 原則 | 判定 | 理由 |
|---|---|---|
| I. Spec Kit | PASS | spec（確認 2 件回答済み）→ plan → tasks の順 |
| II. ビルド | PASS | 新しいファイルは無く、既存のファイルに足す。4 構成のビルドを tasks で確かめる |
| III. テスト可能性 | PASS | まとめ方・待ち時間・ジッター・周期・間引きはすべて CanSim の純粋関数にし、checks で検証する。ダイアログは表示だけ |
| IV. シンプルさ | PASS | 028 の部品（読み込み・統計・ヒストグラム・ウィンドウの共通部分・グラフ）を使い回し、新しく作るのはジッターの計算とずれのグラフだけ |
| V. ドキュメント | PASS | CLAUDE.md（解析の節・アーキテクチャ）と checks の README を更新する |
| VI. ドメイン仕様 | PASS | 出力済みのログを読むだけ。ジッターは機能 001 の FR-019 の定義を引き継ぐ |

設計後の再確認: 変更なし（PASS）。

## Project Structure

```text
specs/029-tx-wait-jitter-analysis/
  spec.md / plan.md / research.md / data-model.md / quickstart.md
  contracts/ui-analysis-wait-jitter.md
  checklists/requirements.md

MFCApplication1/CanSim/LogAnalysis.h/.cpp    追加: CollectTransmissions / AnalyzeTxWait / AnalyzeJitter / MinMaxPerColumn（統計の共通化）
MFCApplication1/AnalysisDlg.h/.cpp           追加: CTxWaitDlg / CJitterDlg、CAnalysisGraph::SetDeviation
MFCApplication1/MFCApplication1Dlg.h/.cpp    OnAnalysisTxWait / OnAnalysisJitter、m_txWaitDlg / m_jitterDlg、CloseResultWindows
MFCApplication1/FrameEditDlg.h               WM_APP_TXWAIT_CLOSED (WM_APP + 8)、WM_APP_JITTER_CLOSED (WM_APP + 9)
MFCApplication1/Resource.h, MFCApplication1.rc   ダイアログ 2、メニューの並べ替えと区切り線、文字列
tools/cansim_runner/checks.cpp, README.md   029 の項目
CLAUDE.md
```

**Structure Decision**: 028 と同じファイルに足す（新しいファイルは作らない）。計算は CanSim、表示はダイアログ、メインはグルーだけ（憲章 III）。

## Complexity Tracking

なし。
