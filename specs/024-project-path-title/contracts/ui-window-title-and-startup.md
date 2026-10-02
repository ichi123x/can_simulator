# Contract: ウィンドウのタイトルと起動時の引数

## タイトルバー

```text
CAN Bus Simulator    プロジェクトファイル名：C:\work\a.canprj
CAN Bus Simulator    プロジェクトファイル名：          ← プロジェクトファイルが無いとき
```

- 更新するのは、読込み・保存の成功のときだけ（spec FR-003・FR-004）。
- 長くて収まらない部分は Windows が省略して描く。記録しているパスは省略しない。

## 起動時のコマンドライン

```text
MFCApplication1.exe                       → 単体起動（読み込まない）
MFCApplication1.exe "C:\work\a.canprj"    → a.canprj を読み込む
MFCApplication1.exe a.canprj              → カレントディレクトリ基準の絶対パスにして読み込む
MFCApplication1.exe a.canprj b.canprj     → a.canprj だけを読み込む
```

- 読込みの検査・エラー表示・反映は、メニューの「プロジェクト読込み」と同じ（拡張子 → `IDS_BAD_EXTENSION`、内容・存在 → `IDS_LOAD_ERROR`）。
- 失敗しても起動は続け、単体起動と同じ状態になる（spec FR-006）。
- 関連付けの登録はしない。エクスプローラーからの起動は、ユーザーが「プログラムから開く」でこの exe を選んだ場合の、引数 1 つの起動と同じ。

## CanSim に足す・変えるもの（`ProjectFile.h`、ASCII のみ）

```cpp
// appTitle + "    " + label + projectPath (label ends the title when the path is empty)
std::wstring ComposeWindowTitle(const std::wstring& appTitle, const std::wstring& label,
                                const std::wstring& projectPath);

// args[0] is the program; returns the first non-empty argument after it, or an empty string
std::wstring FindStartupProjectArg(const std::vector<std::wstring>& args);

// true when the path ends with ".canprj" (case-insensitive); moved from the dialog
bool HasProjectExtension(const std::wstring& path);
```

## ダイアログ（`MFCApplication1Dlg.h`）

| 名前 | 役割 |
|---|---|
| `void SetStartupProject(const CString&)` | `InitInstance` から、`DoModal` の前に呼ぶ |
| `void SetProjectPath(const CString&)` | `m_projectPath` を変える唯一の箇所。タイトルも書き直す |
| `bool LoadProjectFrom(const CString&)` | 拡張子の検査 → `LoadConfig` → エラー表示 or 反映。メニュー・ツールバー・起動時が使う |
| `WM_APP_LOAD_STARTUP_PROJECT`（`WM_APP + 4`） | `OnInitDialog` が Post。ハンドラが絶対パスに変換して `LoadProjectFrom` |
| `IDS_PROJECT_PATH_LABEL` | 「プロジェクトファイル名：」 |
