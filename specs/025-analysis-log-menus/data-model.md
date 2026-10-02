# Data Model: 解析・ログ表示メニューの追加とシミュレーションログの表示

## LogFileIndex（CanSim、新規）

ログファイルの行の索引。ファイルを開いたままにはしない（読むたびに開いて閉じる）。

| 項目 | 型 | 内容 |
|---|---|---|
| `m_path` | `std::wstring` | ログファイルのパス |
| `m_checkpoints` | `std::vector<long long>` | `kIndexStride`（256）行ごとの行頭のバイト位置。`m_checkpoints[k]` = 行 `k × 256` の先頭 |
| `m_lineCount` | `long long` | 行数。最後の文字が改行でなければ、その残りも 1 行と数える。空ファイルは 0 |
| `m_maxLineLength` | `int` | 最も長い行の文字数（改行の CR・LF は含まない） |

規則:

- 行の区切りは `\n`。行末の `\r` は取り除く（CRLF・LF のどちらでも同じ内容になる）。
- `ReadLines(first, count)` は `[first, first + count)` と `[0, lineCount)` の重なりを返す（範囲外は返さない）。
- ファイルが開けない・読み込みの途中で失敗した → `Build` は false（`LogFileIndex` は空のまま）。

## ログ表示（`CLogViewDlg` / `CLogTextView`、新規）

| 項目 | 内容 |
|---|---|
| 索引 | `LogFileIndex`（開くときに 1 回作る） |
| 先頭の行・先頭の列 | 縦・横のスクロール位置（行・文字） |
| アンカー・キャレット | 選択の始点と終点の (行, 列)。同じなら選択なし |
| 表示行のキャッシュ | 直前に描いた行の範囲と文字列（同じ範囲の描き直しでファイルを読まない） |

状態:

```text
（無い）──「シミュレーションログ」でログあり──> 開いている
開いている ──もう一度選ぶ──> 開いている（前面に出す）
開いている ──✖ / Esc / 開始 / 読込み / セッティング / プロジェクトファイルのパスの変更 / メインを閉じる──> （無い）
```

## メインダイアログ（`CMFCApplication1Dlg`、追加）

| 項目 | 内容 |
|---|---|
| `m_logViewer` | 開いているログ表示（`CLogViewDlg*`）。閉じた通知（`WM_APP_LOG_VIEWER_CLOSED`）で nullptr |
| `LogPathFor(projectPath)` | `projectPath + ".log.csv"`（`WriteResults` と共通） |
