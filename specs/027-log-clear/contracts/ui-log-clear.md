# UI Contract: ログクリア

## メニュー

```text
ログ表示(L)
  シミュレーションログ(L)...   ID_LOG_VIEW_SIMLOG（025）
  ログクリア(C)...             ID_LOG_CLEAR（027、新規）
```

- `OnInitMenuPopup` で、`ID_LOG_CLEAR` は実行中・中断中グレー（`idleOnly`。「ログ表示」メニュー自体も 025 の `UpdateTopMenus` でグレー）。
- 待機中は、プロジェクトファイルやログの有無に関係なく選べる（選んだときに理由を通知する）。

## CanSim（`LogFileIndex.h`）

| 名前 | 内容 |
|---|---|
| `std::vector<std::wstring> ResultFilePaths(const std::wstring& projectPath)` | `{ LogPathFor(p), p + L".summary.csv", p + L".startup.csv" }` |
| `std::vector<std::wstring> ExistingResultFiles(const std::wstring& projectPath)` | 上のうち通常のファイルとしてあるもの（順はそのまま）。`projectPath` が空なら空 |
| `struct LogClearResult { std::vector<std::wstring> Deleted; std::vector<std::wstring> Failed; }` | 削除の結果 |
| `LogClearResult DeleteResultFiles(const std::vector<std::wstring>& paths)` | 各パスを削除し、できた・できなかったに分ける（属性は変えない） |

`WriteResults` の 3 つのパスも `ResultFilePaths` から取る（書く名前と消す名前を 1 か所で決める）。

## ダイアログ（`CMFCApplication1Dlg::OnLogClear`）

[plan.md の「流れ」](../plan.md) のとおり。

| メッセージ | 種類 | %s |
|---|---|---|
| `IDS_LOG_NONE_NOPROJECT`（既存） | 情報 | — |
| `IDS_LOG_NONE`（既存） | 情報 | プロジェクトのファイル名 |
| `IDS_LOG_CLEAR_CONFIRM`（新規） | 警告・はい／いいえ（既定はいいえ） | 削除するファイル名（改行区切り） |
| `IDS_LOG_CLEAR_DONE`（新規） | 情報 | 削除したファイル名（改行区切り） |
| `IDS_LOG_CLEAR_FAILED`（新規） | 警告 | 削除できなかったファイル名（改行区切り） |

## リソース

| ID | 値 |
|---|---|
| `IDS_LOG_CLEAR_CONFIRM` | 262 |
| `IDS_LOG_CLEAR_DONE` | 263 |
| `IDS_LOG_CLEAR_FAILED` | 264 |
| `ID_LOG_CLEAR` | 32784 |

`_APS_NEXT_SYMED_VALUE` は 265、`_APS_NEXT_COMMAND_VALUE` は 32785 にする。
