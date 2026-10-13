---

description: "Task list for feature 017-toolbar-display-time"
---

# Tasks: ツールバーのフレーム表示チェックボックスとシミュレーション時間の表示

**Input**: Design documents from `/specs/017-toolbar-display-time/`

**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/, quickstart.md（すべて存在）。機能 014・015・016 の実装が完了していること。

**Tests**: spec.md は自動テストのフレームワークを要求していない。新しい純粋関数（`CanSim/SimControls` に追加）は checks.exe に検証項目を加える（憲章 III）。見た目・操作は GUI の自動操作で確認する。

**Organization**: spec.md のユーザーストーリーごと。
- US1: ツールバーのチェックボックスでフレーム表示を切り替える（P1）
- US2: シミュレーション時間を見る（P1）

共通の土台（座標・書式・文字列）は Phase 2 に置く。

## Format: `[ID] [P?] [Story] Description`

## Path Conventions

- コア（UI 非依存、ASCII のみ）は `MFCApplication1/CanSim/`。
- UI は `MFCApplication1/MFCApplication1Dlg.*`。
- リソースは `MFCApplication1/Resource.h`・`MFCApplication1/MFCApplication1.rc`。`.rc` は UTF-16LE で、BOM・CRLF を保って編集する（014・015 と同じ方法）。
- 検証は `tools/cansim_runner/checks.cpp`。GUI 検証スクリプトはリポジトリ外（scratchpad の `t017/`。`t015/`・`t016/` の関数を土台にする）。

---

## Phase 1: Setup

- [X] T001 GUI 検証の準備をする（リポジトリ外、scratchpad の `t017/`）。
  - `t016/Gui.cs` と、`t016/gui.ps1` の関数をコピーする。コピーする関数は次のとおり。
    - 読込み・保存・メッセージボックスの確実な閉じ方
    - ボタンのクリック、画面の色の読み取り
    - バスが青の瞬間に中断する、構成図の点の色
    - 停止時刻の読み取り
  - チェックボックス用の関数を用意する（座標は research.md 決定2 の値を DPI 換算する）。
    - チェック欄・文字の中央をクリックする。
    - チェックの有無を読む（チェック欄の中央付近にレ点の青 `1E6EC8` の画素があるか）。
  - 時刻の欄の画像を保存する関数を用意する（目で数値を読むため）。
  - 「編集」メニューの項目の文字列を数える関数を用意する（`GetMenuItemCount` / `GetMenuString`）。
  - 一時フォルダに検証用の構成を作る。
    - `sample_appendix_a` のコピー（スピード 0 / -10 / +3）
    - 600 秒・スピード +10 の構成

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: 座標・書式・文字列（US1・US2 の両方が使う）

- [X] T002 `MFCApplication1/CanSim/SimControls.h/.cpp` を変更・追加する（data-model.md、research.md 決定2・4）。ASCII のみ。`#include <string>`。
  - `kSpeedSliderLeft` を 200 から 320 に変える。コメントに「017: moved right for the frame display check box」を足す。
  - チェックボックスの定数（96 dpi）:
    - `kFrameDisplayBoxLeft = 186`、`kFrameDisplayBoxTop = 25`、`kFrameDisplayBoxSize = 14`
    - `kFrameDisplayLabelLeft = 206`、`kFrameDisplayLabelRight = 290`、`kFrameDisplayLabelTop = 22`、`kFrameDisplayLabelBottom = 42`
    - `kFrameDisplayHitLeft = 182`、`kFrameDisplayHitRight = 294`、`kFrameDisplayHitTop = 4`、`kFrameDisplayHitBottom = 60`
  - 時刻の欄の定数（96 dpi）:
    - `kSimTimeLeft = 950`、`kSimTimeRight = 1130`
    - `kSimTimeCaptionTop = 8`、`kSimTimeCaptionBottom = 28`
    - `kSimTimeValueTop = 30`、`kSimTimeValueBottom = 54`、`kSimTimeValueRight = 1100`
    - `kSimTimeUnitLeft = 1104`
  - 関数（すべて `ScaleToDpi` で換算した `DiagramRect`）:
    - `FrameDisplayBoxRect(dpi)`、`FrameDisplayLabelRect(dpi)`
    - `bool IsOnFrameDisplayToggle(int x, int y, int dpi)`: 当たり判定の範囲を、換算後の `[Left, Right) × [Top, Bottom)` で判定する。
    - `SimTimeCaptionRect(dpi)`: `[950,1130) × [8,28)`
    - `SimTimeValueRect(dpi)`: `[950,1100) × [30,54)`
    - `SimTimeUnitRect(dpi)`: `[1104,1130) × [30,54)`
    - `SimTimeAreaRect(dpi)`: `[950,1130) × [8,54)`
  - `std::string FormatSimTimeUs(long long us)`: 0 以上の整数を 3 けたごとに `,` で区切った文字列を返す。負の値は `"0"` にする。
- [X] T003 [P] リソースを変更する（research.md 決定5・6）。
  - `MFCApplication1/Resource.h`:
    - `#define ID_EDIT_FRAME_DISPLAY 32774` の行を削除する。
    - `IDS_FRAME_DISPLAY_LABEL 245`、`IDS_SIM_TIME_CAPTION 246`、`IDS_SIM_TIME_UNIT 247` を追加する（「// feature 017」）。
    - `_APS_NEXT_SYMED_VALUE` を 248 にする。
  - `MFCApplication1/MFCApplication1.rc`:
    - `IDR_MAINMENU` の「編集」から `MENUITEM "フレーム表示(&D)", ID_EDIT_FRAME_DISPLAY` の行を削除する。
    - 文字列テーブルに `IDS_FRAME_DISPLAY_LABEL "フレーム表示"`、`IDS_SIM_TIME_CAPTION "シミュレーション時間"`、`IDS_SIM_TIME_UNIT "μs"`（U+03BC）を追加する。
    - 編集後に読み直し、日本語・μ が化けていないことを確かめる。
- [X] T004 [P] `tools/cansim_runner/checks.cpp` に `CheckFeature017()` を追加し、`main` で `CheckFeature015()` の後に呼ぶ（quickstart.md 1）。（依存: T002）
  - 並び（DPI 96/120/144）は、次のとおり左から順で、互いに重ならないこと。
    - `SimButtonRect(2).Right`
    - < チェックボックスの当たり判定の左端（`ScaleToDpi(kFrameDisplayHitLeft)`）
    - チェックボックスの当たり判定の右端 ≤ スライダーの当たり判定の左端（`SpeedTickX(-13) - ScaleToDpi(13)`）
    - スライダーの当たり判定の右端（`SpeedTickX(10) + ScaleToDpi(13)`） ≤ `SimTimeAreaRect.Left`
  - 範囲:
    - `SimTimeAreaRect.Right <= ScaleToDpi(kDiagramWidth)`。全部の下端が `ScaleToDpi(kToolbarHeight)` 以下。
    - チェック欄と文字は当たり判定の中にある。数値と μs の欄は重ならない。
  - `IsOnFrameDisplayToggle`:
    - 真になる点: `FrameDisplayBoxRect`・`FrameDisplayLabelRect` の中央。
    - 偽になる点: 終了ボタンの中央、`SpeedTickX(-13)`、帯の下（y = 帯の高さ + 1）。
  - `FormatSimTimeUs`: 0→`0`、999→`999`、1000→`1,000`、1234567→`1,234,567`、10000000→`10,000,000`、600000000→`600,000,000`、-5→`0`。

**Checkpoint**: checks.exe の 017 項目と既存項目（015 のスライダーの項目を含む）がすべて PASS。

---

## Phase 3: User Story 1 - ツールバーのチェックボックスでフレーム表示を切り替える (Priority: P1) 🎯 MVP

**Goal**: 「編集 → フレーム表示」を削除し、ツールバーのチェックボックスでいつでも切り替えられる。

**Independent Test**: チェックの有無と強調の有無が一致し、実行中・中断中もその場で反映される。メニューに項目が無い（quickstart.md 手順 2-1〜2-4・2-8）

- [X] T005 [US1] `MFCApplication1/MFCApplication1Dlg.h/.cpp` から「編集 → フレーム表示」の処理を削除し、`ToggleFrameDisplay` を追加する（research.md 決定5）。（依存: T003）
  - 削除するもの:
    - `ON_COMMAND(ID_EDIT_FRAME_DISPLAY, ...)`
    - `OnEditFrameDisplay` の宣言と定義
    - `OnInitMenuPopup` の `CheckMenuItem(ID_EDIT_FRAME_DISPLAY, ...)`
  - `void ToggleFrameDisplay()` を追加する。
    - `m_settings.FrameDisplay = !m_settings.FrameDisplay;`
    - 構成図（`GetDiagramRect`）とツールバーを無効化する。
    - `InRun()` による拒否はしない（017 FR-004、Q1: A）。
  - `DrawBusDiagram` の強調の条件（`InRun() && m_settings.FrameDisplay`）は変えない。その上の「実行中は切り替えられない」旨のコメントを、017 の内容に直す。
- [X] T006 [US1] `MFCApplication1/MFCApplication1Dlg.cpp` の `DrawSimToolbar` に、チェックボックスの描画を追加する（research.md 決定2、contracts）。（依存: T002, T005）
  - チェック欄: `FrameDisplayBoxRect` を、塗り `RGB(240,240,240)`・枠 `RGB(160,160,165)`（1 px）で描く。
  - `m_settings.FrameDisplay` のとき、欄の中に青 `RGB(30,110,200)`・太さ `max(1, ScalePx(2))` のレ点を描く。
    - 欄の左上を (0,0)、幅を s として、(0.2s, 0.5s) → (0.42s, 0.75s) → (0.82s, 0.25s) の折れ線。
  - 文字: `IDS_FRAME_DISPLAY_LABEL` を `FrameDisplayLabelRect` に、白・高さ `ScalePx(12)`・Meiryo UI で、`DT_LEFT | DT_VCENTER | DT_SINGLELINE` で描く。
- [X] T007 [US1] `MFCApplication1/MFCApplication1Dlg.cpp` の押下に、チェックボックスを加える（research.md 決定1）。（依存: T006）
  - `OnLButtonDown` で、ボタン → チェックボックス → スライダーの順に試す。
  - チェックボックスは `IsOnFrameDisplayToggle(point.x, point.y, dpi)` なら `ToggleFrameDisplay()` して戻る。
  - `OnLButtonDblClk` の、帯の中の素早い 2 回目のクリックでも、同じ順で試す。
  - `OnFileLoadProject` の成功時の `InvalidateToolbar()`（015）で、読込み後のチェックの状態も描かれることを確かめる。（spec FR-002、FR-003、FR-005）
- [X] T008 [US1] Debug|x64 をビルドし、実機で quickstart.md 手順 2-1〜2-4・2-8 を確認する（依存: T001, T007）。
  - 2-1: 起動直後の画面写しで、並びとチェックなしを確かめる。「編集」メニューの項目の文字列に「フレーム表示」が無い。
  - 2-2: チェック欄のクリックでチェックが入り、文字のクリックで外れる。
  - 2-3: チェックありで、バスが青の時点で中断すると、送信 ECU が `F06E6E`。
    - チェックを外すと、すぐ全 ECU が元の色になる。
    - 入れると戻る。
  - 2-4: チェックなしで実行し、実行中にチェックを入れると、0.5 秒以内に赤・青の ECU が現れる（バスが青の瞬間を待って判定）。
  - 2-8: チェックありで保存すると、ファイルに `FrameDisplay=1` がある。外してから読み込み直すと、チェックが入る。
  - （SC-001〜SC-003、spec US1 受け入れシナリオ 1〜7）

**Checkpoint**: US1 の受け入れシナリオ 1〜7 を満たす

---

## Phase 4: User Story 2 - シミュレーション時間を見る (Priority: P1)

**Goal**: ツールバーの右端に「シミュレーション時間」・今のシミュレーション時刻（3 けた区切り）・「μs」を表示する。

**Independent Test**: スピード 0 で実行すると数値が増え、中断で止まり、「終了」の値が終了メッセージの時刻と一致する（quickstart.md 手順 2-5〜2-7）

- [X] T009 [US2] `MFCApplication1/MFCApplication1Dlg.h/.cpp` に、`m_displaySimUs` と時刻の欄の描画を追加する（research.md 決定2・3）。（依存: T002, T003）
  - `long long m_displaySimUs = 0;` と `void SetDisplaySimUs(long long us)` を追加する。
    - 値が変わったときだけ、`m_displaySimUs = us` にして、`SimTimeAreaRect` を無効化する。
  - `DrawSimToolbar` に、時刻の欄の描画を追加する。
    - `IDS_SIM_TIME_CAPTION` を `SimTimeCaptionRect` に描く（`RGB(200,200,205)`、高さ 12、左寄せ）。
    - `FormatSimTimeUs(m_displaySimUs)` を `SimTimeValueRect` に描く（白、太字、高さ 16、`DT_RIGHT`）。
    - `IDS_SIM_TIME_UNIT` を `SimTimeUnitRect` に描く（`RGB(200,200,205)`、高さ 12、左寄せ）。
    - いずれも `DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX`。
    - フォントは Meiryo UI。
- [X] T010 [US2] `MFCApplication1/MFCApplication1Dlg.cpp` の各所で、`SetDisplaySimUs` を呼ぶ（research.md 決定3）。（依存: T009）
  - 呼ぶ所と値:
    - `ResetDisplaysForRun`（新しい実行の開始）: 0。
    - `AdvanceReplay`（書き換え・中断）: `m_curSimUs`。
    - `FinishReplay` の先頭（自然終了・即時完了・実行中の +10）: `m_settings.DurationSec * 1000000LL`。
    - `OnSimStop`（`stopUs` を求めた直後、`EndRun` の前）: `stopUs`。
  - `FinishReplay`・`OnSimStop` では、メッセージボックスの前に `UpdateWindow()` を呼び、表示を描き終えてから出す。
  - 再開・スピードの変更では呼ばない（次の書き換えで進む）。（spec FR-006、FR-007）
- [X] T011 [US2] Debug|x64 をビルドし、実機で quickstart.md 手順 2-5〜2-7 を確認する（依存: T001, T010）。
  - 2-5: スピード 0 で開始し、1 秒後と 2 秒後の時刻の欄の画像が異なる（増えている）。
    - 3 秒で中断すると、5 秒後も画像が同じ。
    - 「終了」すると、時刻の欄の画面写しの数値が、終了メッセージの時刻を 3 けた区切りにしたものと一致する（目で確認）。
  - 2-6: スピード +3 で自然終了すると、`10,000,000` になる（画面写し）。スピード +10 で開始しても `10,000,000`。次の「開始」の直後は `0`。
  - 2-7: 600 秒・+10 で `600,000,000` が欄からはみ出さない（画面写し）。
  - （SC-004、spec US2 受け入れシナリオ 1〜3）

**Checkpoint**: US2 の受け入れシナリオ 1〜3 を満たす

---

## Phase 5: Polish & Cross-Cutting Concerns

- [X] T012 [P] `CLAUDE.md` を更新する（憲章 V）。
  - `MFCApplication1Dlg` の説明のメニュー「編集（セッティング／フレーム表示）」を、「編集（セッティング／データフレーム編集）」に直す。
  - 「プロジェクトと実行（機能 004）」の「フレーム表示は … 開始時の設定のまま。実行中は切り替えられない」を、機能 017 から「ツールバーのチェックボックスでいつでも切り替え、実行中・中断中もその場で反映」に直す。
  - `SimControls` の説明に、チェックボックス・時刻の欄の座標と `FormatSimTimeUs` を加える。
  - 機能 015 の段落の後に、機能 017 の段落を加える。
    - 並び（ボタン → チェックボックス → スライダー → シミュレーション時間）。
    - `ToggleFrameDisplay`。
    - `m_displaySimUs` の値の規則。
  - 検証範囲の記述を「001〜011・014・015・017」にする。
- [X] T013 [P] `tools/cansim_runner/README.md` に 017 の項目を加え、検証範囲を 017 まで広げる。
- [X] T014 Debug/Release × x64/x86 の全 4 構成で、次を確認する（依存: T008, T011）。
  - 警告なしでビルドできる。
  - `checks.exe` の全項目が PASS。
  - サンプルの CSV が基準と一致する（spec SC-005）。

---

## Dependencies & Execution Order

- **Setup**: T001（実装と並行可）
- **Foundational**: T002・T003 は並行可 → T004（T002 の後）
- **US1**: T005（T003 の後）→ T006 → T007 → T008
- **US2**: T009（T002・T003 の後。`MFCApplication1Dlg.cpp` を触るので T005〜T007 の後に行う）→ T010 → T011
- **Polish**: T012・T013 は並行可、T014 は最後

### Parallel Opportunities

- T002（CanSim）と T003（リソース）は並行可。T004（checks）はダイアログのタスクと並行可。
- T012・T013（文書）は並行可。
- `MFCApplication1Dlg.*` を変更するタスク（T005〜T007・T009・T010）は順に行う。

## Implementation Strategy

1. **Foundational**（T002〜T004）: 座標・書式・文字列。スライダーが右へずれる。
2. **MVP = US1**（T005〜T008）: チェックボックスでフレーム表示をいつでも切り替えられる。
3. **US2**（T009〜T011）: シミュレーション時間の表示。
4. **Polish**: 文書・4 構成ビルド・checks。

## Notes

- シミュレーションエンジン・CSV・プロジェクトファイルの形式は変更しない。
- `CanSim/SimControls.*` は ASCII のみ。日本語と μ は `.rc` の文字列テーブル。
- `.rc` を編集した後は、文字列が化けていないことを必ず読み直して確認する。

## 実装結果（2026-10-01）

- 変更:
  - `CanSim/SimControls.h/.cpp`（`kSpeedSliderLeft` 200 → 320、チェックボックス・時刻の欄の範囲、`IsOnFrameDisplayToggle`、`FormatSimTimeUs`）
  - `MFCApplication1Dlg.h/.cpp`（`OnEditFrameDisplay` を削除して `ToggleFrameDisplay`、チェックボックス・時刻の描画と押下、`m_displaySimUs` / `SetDisplaySimUs`）
  - `Resource.h`（`ID_EDIT_FRAME_DISPLAY` を削除し、`IDS_FRAME_DISPLAY_LABEL` / `IDS_SIM_TIME_CAPTION` / `IDS_SIM_TIME_UNIT` を追加）
  - `MFCApplication1.rc`（「編集」は 2 項目、文字列 3 つ。日本語と μ を読み直して確認済み）
  - `tools/cansim_runner/checks.cpp`・`README.md`、`CLAUDE.md`
- ビルド: Debug/Release × x64/x86 の 4 構成とも、警告・エラーなしでビルドできた。
- checks.exe: 全 151 項目 PASS（017 の 4 項目を追加。015 のスライダーの項目は、ずらした後の座標で PASS）。基準 CSV との比較も一致した。
- GUI（Debug|x64、120 dpi）: scratchpad の `t017/gui.ps1` で、全項目が合格した。
  - 2-1: 「編集」メニューは「セッティング」「データフレーム編集」の 2 項目。起動直後はチェックなし。帯の並びは、画面写し `t017/idle17.png` で確認した。
  - 2-2: チェック欄のクリックでチェックが入り、文字のクリックで外れた。
  - 2-3: チェックありでバスが青の瞬間に中断すると、送信が赤・受信が青だった。チェックを外すとすぐ強調が消え、入れると中断時点の強調が戻った。
  - 2-4: チェックなしで実行中は強調が無く、実行中にチェックを入れると強調が現れた。
  - 2-5: 時刻は実行中に増えた。中断中は 5 秒たっても変わらなかった。「終了」後は 3,011,305 と表示され、終了メッセージの 3011305 µs と一致した。
  - 2-6: 自然終了（+3）・即時完了（+10）とも 10,000,000 だった。次の「開始」の直後は 6,607（0 から進んでいる）だった。
  - 2-7: 600 秒・+10 で、600,000,000 が欄からはみ出さなかった。2-5〜2-7 は、時刻の欄の画面写し `t017/times.png` で確認した。
  - 2-8: チェックありで保存すると `FrameDisplay=1` で、外してから読み込み直すとチェックが戻った。
- 自動操作の注意: 1 回目の試行では、ファイル読込み直後の 1 回目のクリックが取りこぼされて 2-3・2-4 が失敗した。単独の再現と 2 回目の全体の実行では、どちらも正しく動いた。そのため、自動操作のタイミング（前面化の直後のクリック）によるものとみている。
