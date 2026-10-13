# Contract: 解析・ログ表示メニューとログ表示のウィンドウ

## メニューバー（`IDR_MAINMENU`）

```text
ファイル(&F)  編集(&E)  シミュレーション(&S)  解析(&A)  ログ表示(&L)
                                              └ 準備中（グレー）   └ シミュレーションログ(&L)...
```

| 位置 | メニュー | 実行中・中断中 |
|---|---|---|
| 0 | ファイル | グレー（007） |
| 1 | 編集 | グレー（007） |
| 2 | シミュレーション | 使える |
| 3 | 解析 | グレー（025） |
| 4 | ログ表示 | グレー（025） |

## 「シミュレーションログ」を選んだとき

| 状態 | 結果 |
|---|---|
| プロジェクトファイルが無い | `IDS_LOG_NONE_NOPROJECT`「プロジェクトファイルがないため、シミュレーションログはありません。\n…「開始」でシミュレーションを実行すると、ログが作られます。」 |
| `<プロジェクトファイル>.log.csv` が無い | `IDS_LOG_NONE`「プロジェクト「%s」のシミュレーションログはありません。\n「開始」でシミュレーションを実行すると、ログが作られます。」（%s = プロジェクトのファイル名） |
| あるが開けない・読めない | `IDS_LOG_READ_ERROR`「シミュレーションログを読み込めませんでした。\n%s」（%s = ログのフルパス） |
| ある | ログ表示のウィンドウを開く（開いていれば前面に出す） |

メッセージはすべて `AfxMessageBox(…, MB_OK | MB_ICONINFORMATION)`（読めないときは `MB_ICONWARNING`）。

## ログ表示のウィンドウ（`IDD_LOG_VIEW`、モードレス）

- タイトル: `IDS_LOG_VIEW_TITLE`「シミュレーションログ - %s」（%s = ログのフルパス）。
- 大きさを変えられる（`WS_THICKFRAME`、最大化あり）。初期 900 × 600 px（96 dpi 換算）、メインウィンドウの中央。
- 中身: 白い背景に黒の等幅文字（MS Gothic 13 px）、ファイルのとおりの内容。縦・横のスクロールバー。
- 操作: スクロールバー、ホイール（3 行）、↑↓←→・PgUp/PgDn・Home/End・Ctrl+Home/Ctrl+End、ドラッグ／Shift + キーで選択、Ctrl+A、Ctrl+C、右クリックのメニュー（`IDS_LOG_MENU_COPY`「コピー(&C)」・`IDS_LOG_MENU_SELECT_ALL`「すべて選択(&A)」）。
- 選択が 16 MB を超える Ctrl+C: `IDS_LOG_COPY_TOO_LARGE`「選択範囲が大きすぎるため、コピーできません（16 MB まで）。」
- 編集の操作は無い（文字の入力・削除・貼り付けは何も起きない）。
- 閉じる: ✖・Esc。閉じるとオーナーに `WM_APP_LOG_VIEWER_CLOSED`（`WM_APP + 5`）を送る。

## CanSim に足すもの（`LogFileIndex.h`、ASCII のみ）

```cpp
class LogFileIndex
{
public:
    static const int kIndexStride = 256;

    // Reads the whole file once. False if it cannot be opened or read.
    bool Build(const std::wstring& path);

    long long LineCount() const;
    int MaxLineLength() const;          // characters, without CR / LF

    // Lines [first, first + count) clipped to [0, LineCount()), without CR / LF.
    // Empty when the file cannot be read any more.
    std::vector<std::string> ReadLines(long long first, int count) const;
};
```
