# Research: 014 中断とツールバー

## 決定1: 実行の状態を CanSim の `RunPhase` にし、有効／無効を 1 つの関数で決める

- **Decision**:
  - `CanSim/SimControls.h` に `enum class RunPhase { Idle, Running, Paused };` を置く。ダイアログの `RunState`（`Idle`/`Running`）はこれに置き換える。
  - `SimCommandStates GetSimCommandStates(RunPhase phase, bool editingFrames)` が、`{ Start, Pause, Stop }` の有効／無効を返す。
    | phase | Start | Pause | Stop |
    |---|---|---|---|
    | Idle | `!editingFrames` | false | false |
    | Running | false | true | true |
    | Paused | true | false | true |
  - `OnInitMenuPopup`（メニュー）とツールバーの描画・クリック判定の両方が、この関数を使う。
- **Rationale**: FR-004（メニューとボタンの一致）。表を 1 か所に置けば、両者がずれることはない。純粋関数なので checks で表全体を検証できる。Paused で editing を見ないのは、中断中は「編集」メニューがグレーで、編集中にならないためである（spec Edge Cases）。
- **Alternatives considered**: メニューとツールバーで別々に判定する方法は、ずれる危険があるので採らない。

## 決定2: ツールバーは自前で描く（`CToolBar` を使わない）

- **Decision**: クライアントの上端に、高さ `kToolbarHeight`（96 dpi で 64 px）の暗い帯を `OnPaint` で描く。
  - 帯の中に 3 つのボタン（角丸の明るい箱に色付きの記号、下に白い名前）を描く。
  - クリックは `OnLButtonDown` / `OnLButtonUp`（と、素早い 2 回目の `OnLButtonDblClk`）で受ける。
  - 押した場所と離した場所が同じ有効なボタンのとき、`PostMessage(WM_COMMAND, ID_SIM_*)` で対応するメニューのコマンドを送る。
- **Rationale**:
  - 添付画像の見た目（暗い帯・角丸・緑／青／赤の記号・名前は下）を、ビットマップのリソース無しで、DPI に合わせて描ける。
  - ダイアログでの `CToolBar` は、`RepositionBars`・`ON_UPDATE_COMMAND_UI` の配送（ダイアログでは来ない）・カスタムドローによる背景色の変更などの手当てが要る。構成図と同じ自前描画の方が単純で、座標を CanSim に置いて検証できる（憲章 III・IV）。
  - コマンドを送る方式にすると、ボタンとメニューの処理が同じ関数を通る（FR-003）。
- **Alternatives considered**:
  - `CToolBar` + 32bpp のイメージリスト: 名前の表示・暗い背景・無効時のグレーの作り込みが多く、リソースのビットマップが要る。
  - ボタンのコントロール（`CButton` のオーナードロー）× 3: 押下の見た目はコントロールが持つが、フォーカス枠・Enter キーの扱い（このダイアログはEnter を握りつぶしている）・タブ順など考えることが増える。

## 決定3: ツールバーの座標（96 dpi、帯の中の座標 = クライアント座標）

- **Decision**:
  - 帯: `(0, 0)`〜`(クライアント幅, 64)`、色 `RGB(45, 45, 48)`。
  - ボタン i（0 = 開始、1 = 中断、2 = 終了）:
    - 箱: 左 `8 + 56 * i`、上 6、幅 48、高さ 36、角丸 8。
    - 名前: 箱の下 `[42, 60)`、箱と同じ左右、中央寄せ、文字の高さ 12。
    - 当たり判定の範囲: 箱と名前を合わせた `[左, 6) 〜 (左 + 48, 60)`。
  - 記号（箱の中央、20 px 角）: 開始 = 右向き三角、中断 = 縦の 2 本線（各幅 6、間 6）、終了 = 正方形 16 px。
  - 色:

    | 状態 | 箱 | 記号 | 名前 |
    |---|---|---|---|
    | 有効 | `RGB(240,240,240)` | 開始 `RGB(30,160,70)`・中断 `RGB(30,110,200)`・終了 `RGB(210,40,40)` | 白 |
    | 押下中 | `RGB(200,200,200)` | 有効と同じ | 白 |
    | 無効 | `RGB(95,95,98)` | `RGB(140,140,140)` | `RGB(140,140,140)` |

  - 換算は `ScaleToDpi`（構成図と同じ）。当たり判定 `FindSimButtonAt(x, y, dpi)` は、換算後の `[Left, Right) × [Top, Bottom)` でボタン番号（0〜2）を返し、どれでもなければ -1 を返す。
- **Rationale**: 画像のイメージ（3 つの角丸ボタンが左から並ぶ暗い帯）に合わせた。高さ 64 px は、箱 36 px と名前 12 px に余白を足した値。描画と判定で同じ換算を使う（機能 010 と同じ考え方）。

## 決定4: コンテンツを帯の高さだけ下げる（`m_contentTop`）

- **Decision**:
  - `int ContentTop() const` = `ScaleToDpi(kToolbarHeight, dpi)` とする。次の箇所でこの値を足す。
    - `GetDiagramRect`（上端）
    - `DrawBusDiagram` の `BitBlt` 先
    - `OnLButtonDblClk` の ECU 判定（`point.y - ContentTop()` で判定）
    - `GetLogRect`（`OffsetRect(0, ContentTop())`。これでスクロールバー・枠・ホイール判定も追従する）
    - 編集ウィンドウを開く位置の y
  - `OnInitDialog` で、ウィンドウの高さを `ContentTop()` だけ広げる（`.rc` の DLU は変えない）。
  - `OnEraseBkgnd` はクライアント全体を白く塗る（今のまま）。帯は `OnPaint` で上から塗る。
- **Rationale**: FR-009。CanSim の構成図・ログ表示エリアの座標（checks 011 の位置の検証）はコンテンツ内の座標のまま変えずに済む。ずらしをダイアログの 1 つの関数にまとめるので、漏れがあれば ECU のダブルクリックの位置ずれとして GUI 確認で見つかる。高さを実行時に広げるのは、DLU とピクセルの換算（フォント依存）を避けて、帯の高さぴったりにするためである。
- **Alternatives considered**: `SetViewportOrg` で描画だけずらす方法は、無効化の矩形・当たり判定・子ウィンドウ（スクロールバー）の位置は別にずらす必要があり、まとまらない。

## 決定5: 中断と再開の時計

- **Decision**:
  - メンバ `m_simBaseUs`（再開の基準のシミュレーション時刻。開始時は 0）を追加する。
  - `CurrentSimTimeUs()` は次のとおりにする。
    - Paused なら `m_curSimUs` を返す。
    - それ以外は `min(ResumedSimTimeUs(m_simBaseUs, 経過実時間, Speed), 終端)` を返す。
  - `ResumedSimTimeUs(baseUs, elapsedRealUs, speed)` = `baseUs + SimTimeAtUs(elapsedRealUs, speed)`（加算は飽和）。CanSim に置き、checks で検証する。
  - **中断**（`OnSimPause`、Running のときだけ）:
    1. 今のタイマー処理と同じ更新（`m_curSimUs` = 現在時刻、強調の収集、`ShowLogUpTo`）を 1 回だけ行う。
    2. 終端に達していれば `FinishReplay()`（自然終了）にする。
    3. そうでなければ、タイマーを止めて Paused にする。強調 `m_active` はそのまま残して描く。
    4. `UpdateSimControls()` で表示を直す。
  - **再開**（`OnSimStart` で Paused のとき）:
    1. `m_simBaseUs = m_curSimUs`、`m_replayStartQpc` = 今、`m_prevSimUs = m_curSimUs` にする。
    2. Running に戻してタイマーを再び動かす。
    3. 計算・`ResetDisplaysForRun`・保存先の確認はしない。
  - **終了**（`OnSimStop`、Running または Paused）: 停止時刻は `CurrentSimTimeUs()`。Paused なら中断した時刻になる。それ以降は今の処理のまま。
- **Rationale**: FR-005〜FR-007、SC-002〜SC-004。
  - 中断の処理の中で表示を中断の時刻まで進めてから止めるので、0.1 秒以内に止まり、止まった表示は中断の時刻と一致する。
  - `m_prevSimUs = m_curSimUs` から再開するので、強調の収集（`Collect` は `toUs` が減らない前提）とログの行は欠けも重複もない。
  - CSV は開始時の全計算（`m_runRows`）か、「終了」時の再計算のどちらかで、中断はどちらにも関わらないので変わらない（FR-010）。
- **Alternatives considered**: 中断中もタイマーを動かして時刻を止めておく方法は、無駄な再描画が続き、0.1 秒以内という条件をタイマーの間隔に依存させることになる。

## 決定6: 「実行中か」の判定を 2 種類に分ける

- **Decision**: 今 `m_runState == RunState::Running` と書いている箇所を、意味で 2 つに分ける。
  - **実行が終わっていない（Running または Paused）**: `InRun()` で判定する。
    - 「ファイル」「編集」メニューのグレー（`UpdateTopMenus`、`OnInitMenuPopup` の idleOnly）
    - 読込み・保存・セッティング・フレーム表示・データフレーム編集の拒否
    - ECU のダブルクリックの拒否、切り替え待ちのタイマーで開かない条件
    - 構成図の強調を描く条件
    - `OnSimStop` の受付
  - **時計が進んでいる（Running だけ）**: 再生のタイマー処理、`OnSimPause` の受付。
  - `OnSimStart` は Idle なら開始、Paused なら再開、Running なら何もしない。
- **Rationale**: FR-008（中断中も機能 007・010 の制限を保つ）、spec Edge Cases（中断中も強調は残す）。

## 決定7: ツールバーの再描画のきっかけ

- **Decision**: `void UpdateSimControls()` を追加する。中身は `UpdateTopMenus()` と、ツールバーの帯の `InvalidateRect` である。
  - 呼ぶのは、状態が変わる所（開始・中断・再開・`EndRun`）と、編集中が変わる所（`OpenFrameEditor` の後、`OnFrameEditorClosed`、切り替え待ちの開始と終了、`CloseFrameEditor`）。
  - 押下中の見た目は、`m_pressedButton`（-1 = なし）で持つ。押下中にボタンの外へ出たら、押下の見た目を外す（`OnMouseMove`、マウスの捕捉）。
- **Rationale**: 「開始」は編集中にグレーになる（FR-004）ので、編集ウィンドウの開閉でも描き直す必要がある。

## 決定8: メニューとリソース

- **Decision**:
  - `Resource.h` に次を追加する。
    - `ID_SIM_PAUSE` = 32778。`_APS_NEXT_COMMAND_VALUE` は 32779 にする。
    - `IDS_SIM_BTN_START` = 242、`IDS_SIM_BTN_PAUSE` = 243、`IDS_SIM_BTN_STOP` = 244。`_APS_NEXT_SYMED_VALUE` は 245 にする。
  - `.rc` を次のとおり変える。
    - メニューは「開始(&R)」「中断(&P)」「終了(&X)」の順にする。
    - 文字列テーブルに「開始」「中断」「終了」を加える。
  - `ON_COMMAND(ID_SIM_PAUSE, OnSimPause)` を追加する。
- **Rationale**: FR-001、spec Assumptions（名前はメニューに揃える）。日本語は `.rc` に閉じ込める（CLAUDE.md）。
