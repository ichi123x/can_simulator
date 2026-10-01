# UI Contract: ツールバーの保存・読込みボタン

## 並び（左から、96 dpi）

```text
[保存 8] [読込み 36] (空き 64) [開始 92] [中断 120] [終了 148]  [□ フレーム表示 188〜296]  [シミュレーション時間 314〜494]  スピード： 514〜584  [スライダー 604〜916]
```

DPI 120・144 では `ScaleToDpi` で換算する。部品どうしは重ならず、帯（高さ 64）と幅 1140 に収まる。

## CanSim（`SimControls.h`）に足す・変えるもの

| 名前 | 内容 |
|---|---|
| `enum class ToolbarButton { Save, Load, Start, Pause, Stop }`、`kToolbarButtonCount = 5` | 通し番号 |
| `kFileButtonCount = 2` | 保存・読込みの数（slot の空きの計算に使う） |
| `DiagramRect ToolbarButtonRect(int index, int dpi)` / `ToolbarButtonBoxRect` / `ToolbarButtonLabelRect` | 通し番号 0〜4 の当たり判定・箱・名前 |
| `int FindToolbarButtonAt(int x, int y, int dpi)` | 点を含むボタンの通し番号。無ければ -1（空きの上も -1） |
| `bool IsToolbarButtonEnabled(int index, RunPhase phase, bool editingFrames)` | 保存・読込みは `phase == Idle`、開始〜終了は `GetSimCommandStates` |
| `SimButtonRect(i)` など（既存） | `ToolbarButtonRect(i + 2)` などと同じ値。`FindSimButtonAt` は開始〜終了だけを 0〜2 で返す |
| チェックボックス・シミュレーション時間・「スピード：」・スライダーの x の定数 | +84 |

## ダイアログ（`CMFCApplication1Dlg`）

| 操作 | 動作 |
|---|---|
| 有効なボタンを押す | 押下の色にして捕捉する（既存と同じ） |
| 同じボタンの上で離す | そのボタンのコマンドを Post する（保存 → `ID_FILE_SAVE_OVERWRITE`、読込み → `ID_FILE_LOAD_PROJECT`） |
| 別の場所で離す | 何もしない |
| 無効なボタンを押す | 何もしない（押下の色にもならない） |
| `ID_FILE_SAVE_OVERWRITE` | 実行中・中断中は何もしない。`m_projectPath` が空なら `AskSavePath`（キャンセルで終了）、あればそのパスで `SaveProjectTo` |
| `SaveProjectTo(path)` | `SaveProject` が失敗なら `IDS_SAVE_ERROR` を出して false（`m_projectPath` は変えない）。成功なら `m_projectPath = path` で true |

- 有効／無効が変わる契機（実行の開始・中断・終了、編集ウィンドウの開閉）は既存の `UpdateSimControls` でツールバーを描き直す。保存・読込みの状態もこれで更新される。
- メニュー「ファイル → プロジェクト保存」は毎回保存先を尋ねる（変更なし。内部では `SaveProjectTo` を使う）。

## リソース

| ID | 値 | 内容 |
|---|---|---|
| `ID_FILE_SAVE_OVERWRITE` | 32779 | ツールバーの保存（メニューには無い） |
| `IDS_FILE_BTN_SAVE` | 251 | 保存 |
| `IDS_FILE_BTN_LOAD` | 252 | 読込み |

`_APS_NEXT_SYMED_VALUE` は 253、`_APS_NEXT_COMMAND_VALUE` は 32780 にする。
