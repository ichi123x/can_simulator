# Research: ログクリア（ログファイルの削除）

## R1. 削除の処理をどこに置くか

- **Decision**: `CanSim/LogFileIndex.h/.cpp` に次の 3 つを足す。
  - `ResultFilePaths(projectPath)`: 3 つのパス（`.log.csv`・`.summary.csv`・`.startup.csv`）
  - `ExistingResultFiles(projectPath)`: そのうちある通常のファイル
  - `DeleteResultFiles(paths)`: 削除できたもの・できなかったものの一覧
- **Rationale**:
  - 削除の対象を決める処理を UI から切り離し、checks で実際のファイルを使って検証できる（憲章 III）。
  - `LogPathFor` がすでにここにあり、ログのパスの決まりを 1 か所に集められる。
- **Alternatives considered**:
  - ダイアログで `DeleteFile` を直接呼ぶ: 検証できず、パスの決まりが `WriteResults` と 2 か所に分かれる。
  - `ProjectFile` に置く: プロジェクトファイル自体の読み書きとは別の話で、ログの名前は `LogFileIndex` が持っている。

## R2. ファイルの有無と削除に使う関数

- **Decision**: CRT の関数を使う。
  - 有無: `_wstat64`。通常のファイル（`_S_IFREG`）だけを「ある」とする。
  - 削除: `_wremove`。0 なら削除できた、それ以外なら削除できなかったとする。
- **Rationale**:
  - CanSim は MFC に依存しない方針で、`cl.exe` 単体でもビルドする。CRT の関数ならその条件を満たす。
  - `_wremove` は、読み取り専用のファイルとほかのアプリが開いているファイルで失敗する。spec の「属性を変えてまで削除しない」と、「開かれていれば削除できなかったとして通知」にそのまま合う。
  - 同じ名前のフォルダーは「ある」に含めないので、誤って消そうとしない。
- **Alternatives considered**:
  - Win32 の `DeleteFileW` / `GetFileAttributesW`: 動きは同じだが、CanSim が `windows.h` に依存してしまう。
  - `std::filesystem`: C++14 の構成（`/std:c++14`）では使えない。
  - ごみ箱へ移す（`SHFileOperation`）: spec の前提は通常の削除。確認で「元に戻せない」ことを示して補う。

## R3. 確認と通知の文面

- **Decision**:
  - 確認 `IDS_LOG_CLEAR_CONFIRM`（%s = 削除するファイル名を改行区切りで並べたもの）:「次のファイルを削除します。元に戻すことはできません。よろしいですか？」＋ファイル名。`MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2`（既定は「いいえ」）。
  - 完了 `IDS_LOG_CLEAR_DONE`: 「ログファイルを削除しました。」＋ファイル名（`MB_ICONINFORMATION`）。
  - 一部失敗 `IDS_LOG_CLEAR_FAILED`: 「次のファイルは削除できませんでした。ほかのアプリで開いていないか、読み取り専用でないかを確かめてください。」＋ファイル名（`MB_ICONWARNING`）。
  - 表示するのはファイル名だけ（フォルダーは含めない）。プロジェクトファイルの隣にあることは自明。
- **Rationale**: 025 の通知（ファイル名を示す、改行で説明を足す）と同じ書き方にそろえる。既定のボタンを「いいえ」にして、誤操作で消えないようにする（Q1 の趣旨）。

## R4. ログ表示エリアの消去

- **Decision**: 1 つでも削除できたときに、既存の `ClearLog()` を呼ぶ（行・表示数・先頭位置・スクロールバーを空にして描き直す）。`m_runRows` などの実行結果は消さない。
- **Rationale**:
  - 次の「開始」で `ResetDisplaysForRun` が作り直すので、残しても害は無い。
  - 消すと、`m_runRows` を参照するほかの表示（構成図の強調など）にまで影響が広がる。
  - spec の Q3 は「ログ表示エリアの行」だけを対象にし、構成図の強調・時刻の表示は変えないとしている。

## R5. 実行中の扱いとほかのウィンドウ

- **Decision**:
  - 実行中・中断中: 「ログ表示」メニューごとグレー（025 の `UpdateTopMenus`）。項目も `OnInitMenuPopup` で `idleOnly` にし、ハンドラでも `InRun()` を見る。
  - シミュレーションログのウィンドウ: 確認で「はい」の後、削除の前に `CloseLogViewer()` で閉じる（025 の他の経路と同じ）。
  - データフレーム編集ウィンドウ: 閉じない。
- **Rationale**: spec の FR-007 と Edge Cases のとおり。ログのウィンドウは表示中のファイルが無くなると読めなくなるので、先に閉じる。
