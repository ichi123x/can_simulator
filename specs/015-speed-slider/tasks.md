---

description: "Task list for feature 015-speed-slider"
---

# Tasks: ツールバーのスピードスライダー

**Input**: Design documents from `/specs/015-speed-slider/`

**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/, quickstart.md（すべて存在）。機能 005・014 の実装が完了していること。

**Tests**: spec.md は自動テストのフレームワークを要求していない。新しい純粋関数（`CanSim/SimControls` に追加）は checks.exe に検証項目を加える（憲章 III）。スライダーの見た目・操作・実行中の変更は GUI の自動操作で確認する。

**Organization**: spec.md のユーザーストーリーごと。
- US1: ツールバーのスライダーでスピードを選ぶ（P1）
- US2: 実行中・中断中にスピードを変える（P1）

共通の土台（スライダーの座標・丸め・変更時の動作の判定）は Phase 2 に置く。

## Format: `[ID] [P?] [Story] Description`

## Path Conventions

- コア（UI 非依存、ASCII のみ）は `MFCApplication1/CanSim/`。
- UI は `MFCApplication1/MFCApplication1Dlg.*`、`MFCApplication1/SettingsDlg.*`。
- リソースは `MFCApplication1/Resource.h`・`MFCApplication1/MFCApplication1.rc`。`.rc` は UTF-16LE で、Python 等で BOM・CRLF を保って編集する（014 と同じ方法）。
- 検証は `tools/cansim_runner/checks.cpp`。GUI 検証スクリプトはリポジトリ外（scratchpad の `t015/`。`t014/` の `Gui.cs`・`gui.ps1` の関数を土台にする）。

---

## Phase 1: Setup

- [X] T001 GUI 検証の準備をする（リポジトリ外、scratchpad の `t015/`）。
  - `t014/Gui.cs` をコピーし、`t014/gui.ps1` の関数を再利用する。再利用する関数は次のとおり。
    - 読込み・メッセージボックスの確実な閉じ方
    - ボタンのクリック、ボタンの状態の読み取り
    - 画面の色の読み取り（PrintWindow）
  - スライダー用の関数を用意する（座標は research.md 決定2 の値を DPI 換算する）。
    - 目盛り s の x を求める。
    - つまみの位置を読む: 各目盛りの x・y = `S(20)` の色が、つまみの塗り `F0F0F0` か溝の色かで判定する。つまみの中心が溝を覆う。
    - 押す・ドラッグする・離す。
  - 停止時刻を読む関数を用意する（「終了」のメッセージの `(\d+) µs` を取り出す。014 の 2-8 と同じ方法）。
  - 一時フォルダに検証用の構成を作る: `sample_appendix_a.canprj` のコピー（スピード 0、10 秒、フレーム表示 ON）。

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: スライダーの座標・丸め・当たり判定・変更時の動作の判定（US1・US2 の両方が使う）

- [X] T002 `MFCApplication1/CanSim/SimControls.h/.cpp` にスライダーの定数と関数を追加する（data-model.md、research.md 決定2〜4）。ASCII のみ。`#include "CanTypes.h"`（`kMinSpeed` / `kMaxSpeed` / `kInstantSpeed`）。
  - 定数（96 dpi）:
    - `kSpeedSliderLeft = 200`、`kSpeedTickPitch = 26`
    - `kSpeedTrackY = 20`、`kSpeedTrackThickness = 4`
    - `kSpeedThumbWidth = 10`、`kSpeedThumbTop = 7`、`kSpeedThumbBottom = 33`
    - `kSpeedTickTop = 28`、`kSpeedTickBottom = 33`
    - `kSpeedLabelTop = 38`、`kSpeedLabelBottom = 56`
    - `kSpeedHitTop = 4`、`kSpeedHitBottom = 60`
  - 目盛りの位置と丸め:
    - `int SpeedTickX(int speed, int dpi)`: `ScaleToDpi(kSpeedSliderLeft + (speed - kMinSpeed) * kSpeedTickPitch, dpi)`。speed は `kMinSpeed`〜`kMaxSpeed` に丸めてから使う。
    - `int SpeedAtX(int x, int dpi)`: `kMinSpeed`〜`kMaxSpeed` の各 `SpeedTickX` との距離が最小の speed を返す（同距離は小さい方）。範囲外の x は端の値になる。
  - 当たり判定と描画用の範囲:
    - `bool IsOnSpeedSlider(int x, int y, int dpi)`: 範囲は x `[ScaleToDpi(200 - 13), ScaleToDpi(798 + 13))`、y `[ScaleToDpi(4), ScaleToDpi(60))`。13 は `kSpeedTickPitch / 2`、798 は +10 の目盛りの 96 dpi の x。
    - `DiagramRect SpeedTrackRect(int dpi)`: 左右は -13 と +10 の `SpeedTickX`、上下は `kSpeedTrackY ∓ kSpeedTrackThickness/2` を換算したもの。
    - `DiagramRect SpeedThumbRect(int speed, int dpi)`: 左右は `SpeedTickX ∓ ScaleToDpi(kSpeedThumbWidth)/2`、上下は `kSpeedThumbTop`〜`kSpeedThumbBottom` を換算したもの。
    - `DiagramRect SpeedLabelRect(int speed, int dpi)`: 左右は `SpeedTickX ∓ ScaleToDpi(kSpeedTickPitch)/2`、上下は `kSpeedLabelTop`〜`kSpeedLabelBottom`。
  - 変更時の動作の判定:
    - `enum class SpeedChangeAction { None, Rebase, FinishNow };`
    - `SpeedChangeAction GetSpeedChangeAction(RunPhase phase, int newSpeed)`:
      - newSpeed ≥ `kInstantSpeed` で Running または Paused なら FinishNow。
      - Running で newSpeed < `kInstantSpeed` なら Rebase。
      - それ以外（Idle、または Paused で +10 未満）は None。
- [X] T003 [P] `tools/cansim_runner/checks.cpp` に `CheckFeature015()` を追加し、`main` で `CheckFeature014()` の後に呼ぶ（quickstart.md 1）。（依存: T002）
  - 目盛り（DPI 96/120/144）:
    - 24 個の `SpeedTickX` が単調に増え、隣との差が `ScaleToDpi(26) ± 1`。
    - `IsOnSpeedSlider` の左端（`SpeedTickX(-13) - ScaleToDpi(13)` 付近）が `SimButtonRect(2).Right` より右にある。
    - `SpeedLabelRect(+10).Right <= ScaleToDpi(kDiagramWidth)`。
    - 全範囲の下端が `ScaleToDpi(kToolbarHeight)` 以下。
  - 範囲どうしの関係: 隣どうしの `SpeedLabelRect` が重ならない。各 `SpeedThumbRect` が、その `SpeedTickX` を `[Left, Right)` に含む。
  - `SpeedAtX`:
    - 各 `SpeedTickX` でその値になる。
    - 隣り合う目盛りの間の点（左寄り・右寄り）で近い方になる。
    - x = 0 で -13、x = `ScaleToDpi(2000)` で +10 になる。
  - `IsOnSpeedSlider`:
    - 偽になる点: `SimButtonRect(2)` の中央、y = `ScaleToDpi(kToolbarHeight) + 1`、左端より 1 px 左。
    - 真になる点: 各目盛りの (x, つまみの中央)、(x, 数字の中央)。
  - `GetSpeedChangeAction`: Idle・Running・Paused × {-13, 0, +9, +10} の 12 通りが data-model.md の表と一致する。

**Checkpoint**: checks.exe の 015 項目と既存項目がすべて PASS。

---

## Phase 3: User Story 1 - ツールバーのスライダーでスピードを選ぶ (Priority: P1) 🎯 MVP

**Goal**: セッティングからスピードを削除し、ツールバーのスライダーでスピードを選べる。保存・読込みでつまみの位置が保たれる。

**Independent Test**: 待機中につまみを -3 にして「開始」→ 時刻の進みが 1/8。セッティングにスピードが無い。保存・読込みでつまみの位置が戻る（quickstart.md 手順 2-1〜2-4・2-9・2-10）

- [X] T004 [P] [US1] セッティングからスピードを削除する（research.md 決定6）。
  - `MFCApplication1/MFCApplication1.rc`（UTF-16LE BOM・CRLF を保つ）の `IDD_SETTINGS`:
    - 削除: `LTEXT "スピード:"`・`COMBOBOX IDC_COMBO_SPEED`・`LTEXT "(+10: 即時 / 0: 等倍 / -13: 1/8192)"` の 3 行。
    - `DEFPUSHBUTTON "OK",IDOK,82,106,50,14` → `82,70,50,14`、`PUSHBUTTON "キャンセル",IDCANCEL,136,106,50,14` → `136,70,50,14`。
    - ダイアログの大きさ `196, 128` → `196, 92`。
    - 編集後に読み直し、日本語が化けていないことを確かめる。
  - `MFCApplication1/Resource.h`: `#define IDC_COMBO_SPEED 1013 // feature 005` の行を削除する（`_APS_NEXT_CONTROL_VALUE` は変えない）。
  - `MFCApplication1/SettingsDlg.cpp`: `OnInitDialog` のスピードのコンボの初期化と、`OnOK` の `IDC_COMBO_SPEED` の読み取りを削除する。`Settings.Speed` は受け取った値のまま返す。
  - `MFCApplication1/SettingsDlg.h` の冒頭のコメントに、スピードはツールバーで変える旨（機能 015）を加える。（spec FR-001）
- [X] T005 [US1] `MFCApplication1/MFCApplication1Dlg.cpp` の `DrawSimToolbar` に、スライダーの描画を追加する（research.md 決定2、contracts/ui-speed-slider.md）。（依存: T002）
  - 描画の順: 溝 → 目盛りの線 → 数字 → つまみ。
  - 溝: `SpeedTrackRect` を `RGB(160,160,165)` で塗る。
  - 目盛りの線: 各 speed で、x = `SpeedTickX`、y `ScaleToDpi(kSpeedTickTop)`〜`ScaleToDpi(kSpeedTickBottom)` に幅 1 の縦線。色は溝と同じ。
  - 数字: 各 speed の `SpeedLabelRect` に、`DT_CENTER | DT_SINGLELINE | DT_VCENTER` で描く。
    - 文字の高さ `ScaleToDpi(11)`、表記は `s > 0 ? "+%d" : "%d"`。
    - 色は白。今の値（`m_settings.Speed`）は太字のフォントで、色 `RGB(255,210,60)`。
  - つまみ: `SpeedThumbRect(m_settings.Speed)` を描く。
    - 塗り `RGB(240,240,240)`。ドラッグ中（`m_draggingSpeed`）は `RGB(200,200,200)`。
    - 枠は `RGB(30,110,200)`、太さ `max(1, ScaleToDpi(2))`。
  - フォントは 014 のボタンの名前と同じ書体（Meiryo UI）で、通常と太字の 2 つを作る。（spec FR-002〜FR-004）
- [X] T006 [US1] `MFCApplication1/MFCApplication1Dlg.h/.cpp` に、スライダーの操作と `SetSpeed` を追加する（research.md 決定4・5）。（依存: T005）
  - `bool m_draggingSpeed = false;` を追加する。
  - `OnLButtonDown`: 先にボタン（`PressToolbarButton`）を試し、違えば次を行う。
    - `IsOnSpeedSlider(point.x, point.y, dpi)` なら、`m_draggingSpeed = true`・`SetCapture()`・ツールバーを無効化してから、`SetSpeed(SpeedAtX(point.x, dpi))`。
  - `OnLButtonDblClk` の、帯の中の素早い 2 回目のクリックでも、同じ順で試す（014 の処理を、ボタン → スライダーに広げる）。
  - `OnMouseMove`: ドラッグ中なら `SetSpeed(SpeedAtX(point.x, dpi))`。y は見ない。
  - ドラッグを終える（`m_draggingSpeed = false`、捕捉があれば `ReleaseCapture`、ツールバーを無効化）:
    - `OnLButtonUp`
    - `OnCaptureChanged`
    - `EndRun`
  - `void SetSpeed(int speed)`:
    - 範囲を丸める。今と同じなら何もしない。
    - `GetSpeedChangeAction(m_runPhase, speed)` で分ける（Rebase・FinishNow の中身は T009 で実装。この時点では None と同じに値だけ変える）。
    - `m_settings.Speed = speed` にして、ツールバーを無効化する。
  - `OnFileLoadProject` の成功時（`m_settings = loaded.Project;` の後）に `InvalidateToolbar()` を呼ぶ。（spec FR-005、FR-006、FR-010）
- [X] T007 [US1] Debug|x64 をビルドし、実機で quickstart.md 手順 2-1〜2-4・2-9・2-10 を確認する（依存: T001, T004, T006）。
  - 2-1: 起動直後、つまみは 0 の目盛りにある（つまみの位置の読み取り）。帯のスクリーンショットで数字が 24 個並ぶ。
  - 2-2: セッティングのダイアログに `IDC_COMBO_SPEED`（1013）が無い。OK で閉じても、つまみは動かない。
  - 2-3: つまみを、-3 と -2 の中間より少し -3 寄りへドラッグして離すと、-3 に止まる。溝の +4 の目盛りをクリックすると、+4 に移る。
  - 2-4: -3 で `sample_appendix_a` を開始し、2 秒後に「終了」する。停止時刻が 200,000〜300,000 µs になる。
  - 2-9: +5 で保存する。保存したファイルに `Speed=5` がある。0 にして読込み直すと、+5 に戻る。
  - 2-10: データフレーム編集を開いたまま、スライダーを動かせる。
  - （SC-001、SC-004、spec US1 受け入れシナリオ 1〜6）

**Checkpoint**: US1 の受け入れシナリオ 1〜6 を満たす

---

## Phase 4: User Story 2 - 実行中・中断中にスピードを変える (Priority: P1)

**Goal**: 実行中にスライダーを動かすと、その場から新しい速さで進む。中断中は再開から効く。+10 ではその場で完了する。

**Independent Test**: スピード 0 で実行し、2 秒後に +2 にすると、それ以降の進みが 4 倍になり、時刻が戻らない（quickstart.md 手順 2-5〜2-8）

- [X] T008 [US2] `MFCApplication1/MFCApplication1Dlg.cpp` の `SetSpeed` に、Rebase を実装する（research.md 決定4）。（依存: T006）
  - Running のとき、`m_settings.Speed` を変える前に `const long long t = CurrentSimTimeUs();`（古いスピードでの今の時刻）を求める。
  - 続けて `m_simBaseUs = t`、`QueryPerformanceCounter(&m_replayStartQpc)`、`m_settings.Speed = speed` にする。
  - `m_curSimUs` / `m_prevSimUs` は変えない。
  - 「変更の瞬間の時刻を基準にする（015 FR-007）」旨のコメントを付ける。（spec FR-007、FR-008）
- [X] T009 [US2] `MFCApplication1/MFCApplication1Dlg.cpp` の `SetSpeed` に、FinishNow を実装する（research.md 決定4・5、spec Q1: A）。（依存: T008）
  - `m_settings.Speed = speed` にする。
  - ドラッグを終える（`m_draggingSpeed = false`、`ReleaseCapture`）。
  - `FinishReplay()` を呼ぶ（全行の表示・全結果の CSV・完了メッセージ・待機へ）。
  - 中断中（Paused）からも同じ処理になることを確かめる（`FinishReplay` → `EndRun` は Paused でも動く）。（spec FR-009）
- [X] T010 [US2] Debug|x64 をビルドし、実機で quickstart.md 手順 2-5〜2-8 を確認する（依存: T001, T009）。
  - 2-5: スピード 0 で開始し、2 秒後に +2 にする。
    - 変更の直前と直後の時刻を比べる。中断して停止時刻の代わりに読むか、ログ表示の最後の行の時刻で比べ、直後 ≥ 直前であること。
    - さらに 1 秒後に「終了」し、停止時刻が約 2,000,000 + 4,000,000 µs（± 10%）になること。
  - 2-6: 実行中にスピードを 5 回変えて完了させる（0 → -1 → +1 → 0 → +3 → -2、+10 は使わない）。CSV 3 種が、スピード 0 のまま実行したものと一致すること。
  - 2-7: 中断中に -5 にして再開し、再開から 2 秒後に「終了」する。停止時刻の増分（停止時刻 − 中断の時刻）が約 62,500 µs（2 秒 / 32、± 20%）になること。
  - 2-8: 実行中に +10 へドラッグする。
    - 0.5 秒以内に完了メッセージが出て、待機に戻ること。CSV が最後まで実行したものと一致すること。
    - 中断中に +10 にしても同じであること。
  - （SC-002、SC-003、spec US2 受け入れシナリオ 1〜5）

**Checkpoint**: US2 の受け入れシナリオ 1〜5 を満たす

---

## Phase 5: Polish & Cross-Cutting Concerns

- [X] T011 [P] `CLAUDE.md` を更新する（憲章 V）。
  - `SettingsDlg` の説明から「スピードは +10〜-13 のドロップダウンリスト」を削除し、スピードはツールバーのスライダーで変える旨を書く。
  - `SimControls` の説明に、スライダーの座標（`SpeedTickX` / `SpeedAtX` / `IsOnSpeedSlider` / 範囲）と `GetSpeedChangeAction` を加える。
  - 「中断とツールバー（機能 014）」の段落の後に、機能 015 の段落を加える。
    - スライダーの位置と表示。
    - いつでも操作できる。
    - 実行中は変更の瞬間の時刻を基準に新しいスピードで続ける。中断中は再開で効く。+10 はその場で完了。
    - スピードは `m_settings.Speed` に 1 つだけ持ち、保存される。
  - 「プロジェクトと実行（機能 004）」の「起動時は … スピード 0」の記述は残す。
- [X] T012 [P] `tools/cansim_runner/README.md` の 014 の項目の後に、015 の項目（目盛りの座標・丸め・当たり判定・変更時の動作の判定）を加え、検証範囲を 015 まで広げる。`CLAUDE.md` の「機能 001〜011・014」の記述も「001〜011・014・015」にする。
- [X] T013 Debug/Release × x64/x86 の全 4 構成で、次を確認する（依存: T007, T010）。
  - 警告なしでビルドできる。
  - `checks.exe` の全項目が PASS。
  - サンプルの CSV が基準と一致する（spec SC-005）。

---

## Dependencies & Execution Order

- **Setup**: T001（実装と並行可）
- **Foundational**: T002 → T003
- **US1**:
  - T004 は T002 と並行可（別ファイル）。
  - T005（T002 の後）→ T006 → T007。
- **US2**: T008（T006 の後）→ T009 → T010
- **Polish**: T011・T012 は並行可、T013 は最後

### Parallel Opportunities

- T004（セッティング・.rc・Resource.h）は、T002・T003（CanSim・checks）と並行可。
- T011・T012（文書）は並行可。
- `MFCApplication1Dlg.cpp` を変更するタスク（T005・T006・T008・T009）は順に行う。

## Implementation Strategy

1. **Foundational**（T002〜T003）: 座標と判定を CanSim に入れ、checks で固める。
2. **MVP = US1**（T004〜T007）: セッティングから外し、スライダーで選べるようにする（実行中の変更は、値が変わるだけ）。
3. **US2**（T008〜T010）: 実行中の変更（基準の付け替え）と、+10 でのその場での完了。
4. **Polish**: 文書・4 構成ビルド・checks。

## Notes

- シミュレーションエンジン・CSV・プロジェクトファイルの形式は変更しない。
- `CanSim/SimControls.*` は ASCII のみ。
- `.rc` を編集した後は、日本語の文字列が化けていないことを必ず読み直して確認する。

## 実装結果（2026-10-01）

- 変更:
  - `CanSim/SimControls.h/.cpp`（スライダーの座標・丸め・当たり判定・`GetSpeedChangeAction`）
  - `MFCApplication1Dlg.h/.cpp`（描画・ドラッグ・`SetSpeed`・読込み後の再描画）
  - `SettingsDlg.h/.cpp`、`MFCApplication1.rc`（`IDD_SETTINGS` は 3 項目・高さ 92 DLU。日本語を読み直して確認済み）、`Resource.h`（`IDC_COMBO_SPEED` を削除）
  - `tools/cansim_runner/checks.cpp`・`README.md`、`CLAUDE.md`
- ビルド: Debug/Release × x64/x86 の 4 構成とも、警告・エラーなしでビルドできた。
- checks.exe: 全 147 項目 PASS（015 の 7 項目を追加）。基準 CSV との比較も一致した。
- GUI（Debug|x64、120 dpi）: scratchpad の `t015/gui.ps1` で、全項目が合格した。
  - 2-1: 起動直後、つまみは 0 にあった。帯に 24 個の数字が並び、今の値「0」は黄色の太字だった。
  - 2-2: セッティングに `IDC_COMBO_SPEED` が無かった。OK で閉じても、スピードは 0 のままだった。
  - 2-3: -3 寄りで離すと -3 に止まった。+4 の目盛りをクリックすると +4 に移った。
  - 2-4: -3 で 2 秒実行して「終了」した。停止時刻は 244,194 µs（約 1/8）だった。
  - 2-5: 0 で開始し、約 2 秒後に +2 にして、約 1 秒後に「終了」した。
    - 停止時刻は 7,407,618 µs だった。クリックした時刻から求めた期待値 7,264,838 µs（変更前 × 1、変更後 × 4）との差は 2%。
    - 変更時刻より後だった（戻らない）。
  - 2-6: 実行中にスピードを 5 回変えて完了させた。CSV 3 種は、変えなかった実行（+10 の即時完了）と一致した。
  - 2-7: 中断中に -5 にした。状態は中断のまま変わらなかった。再開して 6 秒後に「終了」すると、停止時刻は 648,016 µs（約 0.5 秒 + 6 秒 / 32）だった。
  - 2-8: +10 にしたときの動きを確かめた。
    - 実行中に +10 の目盛りをクリックすると、その場で完了した。CSV は全結果と一致し、待機に戻ってつまみは +10 にあった。
    - 600 秒の構成で、実行中に +10 までドラッグすると、離した直後には実行が止まっていて、つまみは +10 にあった。完了メッセージは、全結果の CSV の書き出し（約 30 秒）の後に出た。
    - 中断中に +10 にしても、その場で完了した。CSV は全結果と一致した。
  - 2-9: +5 で保存すると、ファイルに `Speed=5` があった。0 にしてから読み込み直すと、つまみは +5 に戻った。
  - 2-10: データフレーム編集を開いたまま、スライダーを -1 に動かせた。
- 動作上の注意: 10 秒の構成で 0 から +10 へゆっくりドラッグすると、+7（128 倍）付近を通る間に残りの実行が自然に終わることがある。その場合は、+10 に届く前に完了メッセージが出て、つまみはその位置に残る。どちらも完了で、結果の CSV は同じなので、仕様上の問題はない。
