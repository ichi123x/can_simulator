# Research: ウィンドウの最小化・最大化ボタンとスピードの範囲の変更

## 決定1: 最小化・最大化は標準のスタイルで出す

- **Decision**: `.rc` の `IDD_MFCAPPLICATION1_DIALOG` の STYLE に `WS_MINIMIZEBOX | WS_MAXIMIZEBOX` を足す。`WS_THICKFRAME`・`WS_SYSMENU`・`WS_CAPTION` は既にある。
- **Rationale**: Q2 = A（Windows 標準のボタン）。スタイルだけで、ボタンの描画・押下・システムメニュー・ダブルクリック・Win+↑↓・スナップを Windows が行う。システムメニューの「バージョン情報」（`IDM_ABOUTBOX`）は今のまま末尾に付く。
- **Alternatives considered**: `ModifyStyle` を `OnInitDialog` で呼ぶ（リソースエディタで見えず、二か所に分かれる）、独自のタイトルバー（Q2 で不採用）。

## 決定2: 最大化・元に戻すで中身は動かさない

- **Decision**: 描画（`OnPaint`・`OnEraseBkgnd`）と当たり判定はクライアント領域の左上を基準にした固定の座標のまま。ツールバーの帯は今と同じく `GetToolbarRect`（クライアントの幅いっぱい）。ログ表示エリアのスクロールバー（`IDC_SCROLL_LOG`）も今の位置のまま。
- **Rationale**: Q1 = A。今でも端を引っ張って大きさを変えられ、そのときの描画は正しい（余りは `OnEraseBkgnd` で白）。最大化は大きさの変更の一種なので、同じ経路で正しく描かれる。`OnSize` での配置し直しは不要。
- **確かめること**: 最大化・元に戻した直後に全体が描き直される（`CS_HREDRAW | CS_VREDRAW` が無くても、`WM_SIZE` 後の無効化で余りの領域が白く塗られる）。描き残しがあれば `OnSize` で `Invalidate()` を足す。

## 決定3: 最小化中の実行

- **Decision**: コードは変えない。再生のタイマー（16 ms）は最小化中も届き、時刻は `QueryPerformanceCounter` の経過時間から求めるので、戻したときにはその時点の時刻・ログの行になっている。終端に達すれば最小化中でも完了して CSV を書き、完了の扱いは今と同じ。
- **Rationale**: spec US2-1・SC-002（最小化は中断ではない）。最小化中の `Invalidate` は描画されないだけで害は無い。
- **確かめること**: `OnPaint` のウィザード生成の `IsIconic()` の分岐（最小化中はアイコンを描く）が残っていて、最小化中に図の描画を試みないこと。

## 決定4: スライダーの左端を揃える

- **Decision**: -10 の目盛りを今の -13 の位置（`kSpeedSliderLeft` = 604）に置く。目盛りの間隔 13 はそのまま。`SpeedTickX` などの式は `kMinSpeed` 基準なので、定数を変えるだけで右へ詰まる。
- **Rationale**: 「スピード：」との間隔（機能 019）を保つ。右端は 903 → 864 になり、帯の中の余裕が増える。
- **Alternatives considered**: +10 の位置（903）を保って左端を右へずらす（「スピード：」との間が空いて間延びする）、目盛りの間隔を広げて同じ幅にする（数字・つまみの位置の検証を作り直すことになる）。

## 決定5: 以前のファイルのスピード -13〜-11

- **Decision**: `ConfigLoader` は `kLegacyMinSpeed`（-13）〜`kMaxSpeed` を数値として受け入れ、`kMinSpeed`（-10）未満は -10 にする。範囲外（-14 以下・+11 以上）のエラーの文言は `kMinSpeed`〜`kMaxSpeed`（-10〜10）を示す。
- **Rationale**: spec FR-005・SC-004。023・024 などで保存したプロジェクトが開けなくなるのを防ぐ。保存は常に -10〜+10 の値で書く（`SetSpeed` が範囲で切るので -11 以下は作られない）。
- **Alternatives considered**: エラーにする（以前の版のファイルが開けなくなる）、そのまま -13 を保持する（スライダーで表せない値を持つことになる）。
