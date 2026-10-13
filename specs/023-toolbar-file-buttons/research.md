# Research: ツールバーへのプロジェクト読込み・保存ボタンの追加

## R1. ボタンの番号の付け方

- **Decision**: ツールバーのボタンを通し番号 `ToolbarButton { Save, Load, Start, Pause, Stop }` で扱う。座標・当たり判定・有効／無効はこの番号を受け取る関数にする。既存の `SimButtonRect(i)` などは `ToolbarButton` の開始〜終了（i + 2）を返す薄い関数として残す。
- **Rationale**: ダイアログの押下中の状態（`m_pressedButton`）・描画のループ・Post するコマンドの表が 1 本で済む。既存の checks（014・015・017・019）は `SimButtonRect(2)` を「終了」として使っており、それを書き換えずに済む。
- **Alternatives considered**:
  - 保存・読込み用の関数を別系統（`FileButtonRect` など）で足す: ダイアログで押下・描画・Post の処理が 2 系統になり、重複が増える。
  - `SimButtonRect` の番号を 0〜4 に広げる: 既存の checks と呼び出し側の意味（2 = 終了）が変わってしまう。

## R2. 空きと既存部品の移動

- **Decision**: ボタンの左端を `8 + 28 × slot` とし、開始以降は slot を 1 つ飛ばす。チェックボックス・シミュレーション時間・「スピード：」・スライダーは定数を 84 px（28 × 3）ずつ増やす。
- **Rationale**: 「アイコン 1 つ分の空き」（spec Q2）は、隣り合うボタンの左端の間隔 28 px と同じ幅に当たる。足したのはボタン 2 つと空き 1 つ分で、既存部品の間隔はそのまま保つ。+10 の数字の右端は 916 px で、構成図の幅 1140 px に収まる（120・144 dpi でも同じ比率）。
- **Alternatives considered**: 既存部品の間隔を詰めて移動を減らす: 019 で決めた並びと間隔が変わり、検証項目の見直しが増える。

## R3. 保存ボタンのコマンド

- **Decision**: ツールバー専用のコマンド `ID_FILE_SAVE_OVERWRITE`（32779）を足す。メニューには載せない。ハンドラは、実行中なら何もせず、`m_projectPath` が空なら `AskSavePath` で保存先を尋ね、あればそのまま使って保存する。
- **Rationale**: spec はメニューの「プロジェクト保存」（毎回保存先を尋ねる）を変えないと決めている（Assumptions）。そのため同じコマンドは使えない。Post で送る形にすれば、既存のボタン（押して同じボタンの上で離したら `WM_COMMAND` を Post）と同じ流れになる。
- **Alternatives considered**: ボタンの離しで直接メソッドを呼ぶ: 押下の捕捉を外す前にダイアログ（保存先の選択）が開いてしまう。既存のボタンが Post にしている理由と同じ。

## R4. 保存処理の共通化

- **Decision**: 「`SaveProject` で書き、失敗なら `IDS_SAVE_ERROR` を出して false、成功なら `m_projectPath` を更新して true」を`CMFCApplication1Dlg::SaveProjectTo(const CString&)` にまとめる。メニューの保存・ツールバーの保存・「開始」の保存先確認の 3 か所で使う。
- **Rationale**: 同じ処理が 3 か所に並ぶのを避ける。失敗時に現在のプロジェクトファイルを変えないことも、1 か所で保証できる（spec US1 シナリオ 4、FR-005）。

## R5. 有効／無効

- **Decision**: 保存・読込みは `RunPhase::Idle` のときだけ有効にする。編集中（編集ウィンドウが開いている、または切り替えの待ち）でも有効にする。`IsToolbarButtonEnabled(button, phase, editingFrames)` が、保存・読込みはこの規則、開始〜終了は `GetSimCommandStates` の結果を返す。
- **Rationale**: メニューの「ファイル」と同じ規則（007: 実行中・中断中はロック）。編集中は、読込みなら先に編集ウィンドウを閉じ（`OnFileLoadProject` が既に行う）、保存ならそのまま保存できる（spec Edge Cases）。

## R6. 記号

- **Decision**: 保存はフロッピー（紫の四角に白い窓）、読込みはフォルダー（黄土色のつまみと本体）を GDI の四角形だけで描く。
- **Rationale**: 10 px の小さな記号でも見分けがつき、既存の記号（緑の三角・青の 2 本線・赤の四角）と色も形も重ならない。画像リソースを足さずに済む（IV）。
