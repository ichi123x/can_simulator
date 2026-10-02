# Data Model: プロジェクトファイル名の表示と関連付けからの起動

## 現在のプロジェクトファイル（`CMFCApplication1Dlg::m_projectPath`、既存）

| 項目 | 内容 |
|---|---|
| 値 | 最後に読込み・保存に成功したプロジェクトファイルの絶対パス。空 = 決まっていない |
| 変える箇所 | `SetProjectPath` だけ（`OnInitDialog` で空、`LoadProjectFrom` の成功、`SaveProjectTo` の成功） |
| 使う箇所（既存） | ツールバーの上書き保存の保存先（023）、「開始」の保存先の確認と CSV の出力先（004） |
| 表示 | `SetProjectPath` のたびにタイトルバーを `ComposeWindowTitle(基のタイトル, ラベル, パス)` に書き直す |

状態の遷移:

```text
空 ──読込み成功 / 保存成功──> パス A
パス A ──読込み成功 / 保存成功（パス B）──> パス B
どの状態でも ──キャンセル・失敗──> 変わらない
```

## 起動時のプロジェクト（`CMFCApplication1Dlg::m_startupProject`、新規）

| 項目 | 内容 |
|---|---|
| 値 | `InitInstance` が `FindStartupProjectArg` で選んだ引数（変換前の文字列）。空 = 引数なし |
| 設定 | `DoModal` の前に `SetStartupProject` |
| 使用 | `WM_APP_LOAD_STARTUP_PROJECT` のハンドラで絶対パスに変換して `LoadProjectFrom` に渡し、空に戻す（1 回だけ） |

## ウィンドウのタイトル

| 部分 | 値 |
|---|---|
| 基のタイトル | `.rc` の `CAPTION`（「CAN Bus Simulator」）。`OnInitDialog` で `GetWindowText` して `m_baseTitle` に保持 |
| 区切り | 半角空白 4 つ |
| ラベル | `IDS_PROJECT_PATH_LABEL`「プロジェクト名：」 |
| パス | `m_projectPath`（空ならここまで） |
