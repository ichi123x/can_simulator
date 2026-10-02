# Research: プロジェクトファイル名の表示と関連付けからの起動

## 決定1: 表示する場所はタイトルバー（`SetWindowText`）

- **Decision**: ダイアログのキャプション（`.rc` の `CAPTION "CAN Bus Simulator"`）を基にして、`SetWindowText` で「CAN Bus Simulator␣␣␣␣プロジェクト名：<パス>」に書き換える。
  基のタイトルは `OnInitDialog` で `GetWindowText` して保持する（文字列を二重に持たない）。
- **Rationale**: 要望の「アプリのタイトルである CAN Bus Simulator と同じライン」はタイトルバーそのもの。タスクバー・Alt+Tab にも同じ文字列が出て、どのファイルかがわかる。
  クライアント領域に描くと、ツールバー・構成図の配置（機能 014・019・023 の座標）を動かす必要がある。
- **Alternatives considered**: ツールバーの右端に描く（場所が足りない。+10 の数字の右端 916 から幅 1140 まで約 220 px でフルパスが入らない）、ステータスバーを足す（ウィンドウの高さが変わり、構成図・ログ表示エリアの座標に影響）。

## 決定2: 区切りとラベル

- **Decision**: 基のタイトルとラベルの間は半角空白 4 つ、ラベルとパスの間は区切りなし（ラベルが全角の「：」で終わる）。ラベルは `IDS_PROJECT_PATH_LABEL "プロジェクト名："`。
  組み立ては `CanSim::ComposeWindowTitle(appTitle, label, path)`（`appTitle + L"    " + label + path`）。パスが空ならラベルで終わる。
- **Rationale**: 全角空白は CanSim（ASCII のみ）に書けない。半角 4 つで全角 2 つ分の見た目になる。
- **Alternatives considered**: 「 - 」で区切る（Windows の慣例「ファイル名 - アプリ名」とは順序が逆で、ラベル付きの要望に合わない）。

## 決定3: 現在のプロジェクトファイルを変える箇所を 1 つにする

- **Decision**: `m_projectPath` への代入を `SetProjectPath(const CString&)` だけにし、そこで `m_projectPath` とタイトルを同時に更新する。呼ぶのは読込みの成功（`LoadProjectFrom`）と保存の成功（`SaveProjectTo`）と `OnInitDialog`（空）。
- **Rationale**: 表示と実体のずれ（SC-001・SC-002）を構造的に防ぐ。今は代入が 2 か所（`OnFileLoadProject`・`SaveProjectTo`）しかなく、変更は小さい。
- **Alternatives considered**: 描画のたびに `m_projectPath` から作る（タイトルバーは非クライアント領域で、`OnPaint` では書けない）。

## 決定4: 起動時の引数の取り出し

- **Decision**: `InitInstance` で CRT の `__argc` / `__wargv`（引用符は CRT が外している）を `std::vector<std::wstring>` にし、`CanSim::FindStartupProjectArg(args)` が
  `args[0]`（exe）を除いた最初の空でない引数を返す（無ければ空）。2 つ目以降は無視する（spec Edge Cases）。ダイアログには `SetStartupProject(path)` で渡す。
- **Rationale**: Unicode ビルドで `__wargv` は常に使える。MFC の `ParseCommandLine` / `CCommandLineInfo` は `/` や `-` で始まる引数をフラグとして扱い、`ProcessShellCommand` と組み合わせる前提でドキュメント／ビュー向け。
  ここでは「最初の引数 = 開くファイル」だけでよい。
- **Alternatives considered**: `CCommandLineInfo::m_strFileName`（動作は同等だが、フラグの解釈が入る）、`CommandLineToArgvW(GetCommandLineW())`（`__wargv` と同じ結果で、解放の手間が増える）。

## 決定5: 絶対パスへの変換

- **Decision**: 起動時の引数は、ダイアログで `GetFullPathNameW`（相対パス → 絶対パス。カレントディレクトリ基準）→ `GetLongPathNameW`（8.3 形式 → 長い名前。ファイルが無ければ失敗するので、そのときは `GetFullPathNameW` の結果を使う）の順に変換してから `LoadProjectFrom` に渡す。
  ファイル選択の結果（`CFileDialog::GetPathName`）は既に絶対パスなので変換しない。
- **Rationale**: spec FR-007。関連付けの起動では通常フルパスが渡るが、コマンドラインからの起動では相対パスもあり得る。ファイルが無い場合も、エラーメッセージには絶対パスが出る。
- **Alternatives considered**: `std::filesystem::absolute`（C++17。プロジェクトは C++14）。

## 決定6: 起動時の読込みのタイミング

- **Decision**: `OnInitDialog` の最後に、引数があれば `PostMessage(WM_APP_LOAD_STARTUP_PROJECT)`（`WM_APP + 4`）。ハンドラで 1 回だけ読み込む（読んだら引数のパスを空にする）。
- **Rationale**: `OnInitDialog` の中ではまだウィンドウが表示されていない。メッセージボックス（拡張子・内容のエラー）を表示済みのメインウィンドウの上に出すため。読み込み後の再描画（`SetEcuCount`・`InvalidateToolbar`）もふつうに効く。
- **Alternatives considered**: `OnInitDialog` の中で直接読む（エラー時にウィンドウの無い状態でメッセージが出る）、`InitInstance` で読んでから渡す（エラー表示と反映の処理が二重になる）。

## 決定7: 拡張子の判定を CanSim に移す

- **Decision**: ダイアログの無名名前空間にある `HasProjectExtension` を `CanSim::HasProjectExtension(const std::wstring&)`（`.canprj` を大文字小文字を区別せずに末尾で判定）に移し、checks で検証する。
- **Rationale**: 起動時の引数で拡張子違いのファイルが渡されるケース（spec US2-4）を、GUI を使わずに確かめられる。動作は今と同じ。
