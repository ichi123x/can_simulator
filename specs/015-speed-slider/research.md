# Research: 015 スピードスライダー

## 決定1: スライダーは自前で描く（`CSliderCtrl` を使わない）

- **Decision**: 014 のツールバーの帯の中に、溝・目盛りの線・つまみ・目盛りの数字を `DrawSimToolbar` で描く。
  - 操作は、ダイアログの `OnLButtonDown` / `OnMouseMove` / `OnLButtonUp`（014 のボタンと同じ所）で受ける。
- **Rationale**:
  - 暗い帯の上での見た目（棒状のつまみ・24 個の数字すべての表示）を、014 のボタンと揃えて描ける。
  - 座標と丸めを CanSim に置いて checks で検証できる（憲章 III）。
  - 標準のトラックバーは、背景色・つまみの形・目盛りの数字（トラックバーは数字を描かない）の作り込みが要る。また、フォーカスを持つコントロールになり、
    Enter・Esc・矢印キーの扱いがこのダイアログの方針（フォーカスを持つコントロールを置かない）とずれる。
- **Alternatives considered**: `CSliderCtrl`（`TBS_AUTOTICKS`）+ 数字の STATIC × 24。カスタムドローと位置合わせが要り、DPI ごとのずれも出やすい。

## 決定2: スライダーの寸法（96 dpi、クライアント座標 = 帯の中の座標）

- **Decision**:
  - 目盛り: `kSpeedSliderLeft = 200`（-13 の目盛りの x）、`kSpeedTickPitch = 26`。+10 の目盛りの x は 200 + 23 × 26 = 798。
  - 溝: 目盛りの -13〜+10 を結ぶ横線。y 中央 `kSpeedTrackY = 20`、太さ 4。色 `RGB(160,160,165)`。
  - 目盛りの線: 各目盛りの x に、y 28〜33 の縦線（1 px）。色 `RGB(160,160,165)`。
  - つまみ: 目盛りの x を中心に幅 10、y 7〜33（高さ 26）の長方形。
    - 塗り `RGB(240,240,240)`、枠 `RGB(30,110,200)`（2 px）。
    - ドラッグ中は塗りを `RGB(200,200,200)` にする。
  - 数字: 目盛りの x を中心に幅 26、y 38〜56 の範囲に、中央寄せ・文字の高さ 11 で描く。
    - 色は白。今の値だけ太字にし、黄色 `RGB(255,210,60)` にする。
    - 表記は `-13`〜`-1`・`0`・`+1`〜`+10`。
  - 当たり判定（押して操作を始める範囲）: x `[200 - 13, 798 + 13)`、y `[4, 60)`。目盛りの線・数字の上も含む。
  - ボタン（014、終了ボタンの右端は 8 + 56 × 2 + 48 = 168）との間は 19 px（当たり判定の左端 187 との差）。
- **Rationale**:
  - 「-13」を 11 px の文字で描くと幅は約 18 px なので、ピッチ 26 px なら隣の数字と重ならない。
  - 全体の右端は約 811 px で、ウィンドウの幅（約 1140 px）に収まる。帯の高さ（64）も変えずに済む。
- **Alternatives considered**: ピッチ 20 px。120 dpi の換算後の余白が狭く、数字が詰まって読みにくい。

## 決定3: 目盛りへの丸めと座標の関数（CanSim）

- **Decision**: `SimControls.h` に次の関数を加える。換算は `ScaleToDpi`。
  - `int SpeedTickX(int speed, int dpi)`: `ScaleToDpi(kSpeedSliderLeft + (speed - kMinSpeed) * kSpeedTickPitch, dpi)`。
  - `int SpeedAtX(int x, int dpi)`: 各目盛りの `SpeedTickX` との距離が一番小さい speed を返す。
    - 距離が同じなら小さい方の値にする（決定的）。範囲外の x は端の値にする。
  - `bool IsOnSpeedSlider(int x, int y, int dpi)`: 当たり判定（決定2）を、換算後の `[Left, Right) × [Top, Bottom)` で判定する。
  - `DiagramRect SpeedThumbRect(int speed, int dpi)`、`DiagramRect SpeedLabelRect(int speed, int dpi)`、`DiagramRect SpeedTrackRect(int dpi)`（描画用）。
- **Rationale**: FR-002〜FR-005、SC-001。描画と判定が同じ換算を使う（014 と同じ考え方）。距離で丸めると、DPI の換算で目盛りの間隔が
  不揃い（±1 px）になっても、一番近い目盛りに止まる。

## 決定4: スピードを変えたときの動作（CanSim の判定 + ダイアログの処理）

- **Decision**: `enum class SpeedChangeAction { None, Rebase, FinishNow };` と、`SpeedChangeAction GetSpeedChangeAction(RunPhase phase, int newSpeed)` を加える。
  | phase | newSpeed | 動作 |
  |---|---|---|
  | Idle | 任意 | None（値を変えるだけ） |
  | Running | < kInstantSpeed | Rebase |
  | Paused | < kInstantSpeed | None（再開で効く） |
  | Running / Paused | kInstantSpeed（+10） | FinishNow |
  - ダイアログの `SetSpeed(int speed)` は、値が今と同じなら何もしない。違えば、`GetSpeedChangeAction` の結果で次のとおり動く。
    - **Rebase**: 先に、古いスピードで今のシミュレーション時刻 `t = CurrentSimTimeUs()` を求める。
      その後 `m_simBaseUs = t`、`m_replayStartQpc` = 今、`m_settings.Speed = speed` にする。
      `m_curSimUs` / `m_prevSimUs` は変えない（次のタイマーで t 以上から続く）。
    - **FinishNow**: `m_settings.Speed = speed`、ドラッグの捕捉を外し、`FinishReplay()`（全行の表示・全結果の CSV・完了メッセージ・待機へ）。
    - **None**: `m_settings.Speed = speed` だけ。
    - いずれもツールバーを描き直す。
- **Rationale**: FR-007〜FR-009、SC-002。
  - Rebase は 014 の再開と同じ式（`ResumedSimTimeUs(base, 経過, speed)`）なので、変更の瞬間は `ResumedSimTimeUs(t, 0, 新) = t` になる。時刻は連続し、戻らない。
  - 強調の収集とログは、`m_prevSimUs` から続くので欠けも重複もない。
  - CSV は開始時の全計算（またはその場での完了の全結果）で、スピードに関わらない（FR-011）。
- **Alternatives considered**: スピードの変更をタイマーの次の更新まで遅らせる方法。変更の前後どちらのスピードで計算するかが曖昧になるので採らない。

## 決定5: ドラッグ

- **Decision**:
  - `OnLButtonDown` で `IsOnSpeedSlider` なら、`m_draggingSpeed = true`・`SetCapture` してから、`SetSpeed(SpeedAtX(point.x))` を呼ぶ。
  - `OnMouseMove` は、ドラッグ中なら `SetSpeed(SpeedAtX(point.x))` を呼ぶ（y は見ない。帯の外へ出ても横の位置で追う）。
  - `OnLButtonUp` と `OnCaptureChanged` では、ドラッグを終える。
  - 素早い 2 回目のクリック（`OnLButtonDblClk`）は、帯の中なら押下として扱う（014 と同じ）。
  - ボタンの押下とスライダーのドラッグは、どちらか一方だけ（押した場所で決まる）。
- **Rationale**: FR-005（ドラッグ中も目盛りごとに反映、US2 シナリオ 2）。+10 で完了したときは、`FinishReplay` のメッセージボックスの前に捕捉を外す
  （モーダルの間に捕捉が残らないように）。

## 決定6: セッティングからの削除

- **Decision**:
  - `.rc` の `IDD_SETTINGS` から、スピードのラベル・`IDC_COMBO_SPEED`・説明の行を削除する。OK・キャンセルを上へ詰め、ダイアログの高さを 128 → 92 DLU にする。
  - `SettingsDlg.cpp` からスピードの処理を削除する。`Settings`（コピー）の `Speed` は受け取った値のまま返るので、OK でスピードは変わらない。
  - `Resource.h` の `IDC_COMBO_SPEED` を削除する。
- **Rationale**: FR-001。他の項目の位置・動作は変えない。

## 決定7: 読込み・起動時のつまみの位置

- **Decision**:
  - `OnFileLoadProject` の成功時（`m_settings` を置き換えた後）に、`InvalidateToolbar()` を呼ぶ。
  - 起動直後は `m_settings.Speed` = 0（004 の既定値）のつまみを描く。
  - 保存（`SaveProject`）は `m_settings.Speed` をそのまま書く（今のまま）。
- **Rationale**: FR-010、SC-004。
