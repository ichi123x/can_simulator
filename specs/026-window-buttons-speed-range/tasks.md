---

description: "Task list for feature 026-window-buttons-speed-range"
---

# Tasks: ウィンドウの最小化・最大化ボタンとスピードの範囲の変更

**Input**: `/specs/026-window-buttons-speed-range/` の spec.md（Q1 = A、Q2 = A）・plan.md（変わる座標の表）・research.md（決定1〜5）・data-model.md・contracts/ui-window-and-speed.md・quickstart.md

**Tests**: spec はテストの作成を求めていない。スピードの範囲・座標・読込みの互換の検証は、既存の慣習どおり `checks.cpp` の項目を直し・足す（T007）。

## Phase 1: Setup

（なし。新しいファイル・リソース ID は追加しない）

## Phase 2: Foundational

（なし。US1 と US3 は互いに独立している）

## Phase 3: User Story 1 - メインウィンドウを最小化・最大化する (Priority: P1) 🎯 MVP

**Goal**: タイトルバーに Windows 標準の「_」「□」を出し、最小化・最大化・元に戻すができる。最大化しても中身は左上のまま（Q1 = A）。

**Independent Test**: quickstart.md の手順 1〜4。

- [X] T001 [US1] `MFCApplication1/MFCApplication1.rc` の `IDD_MFCAPPLICATION1_DIALOG DIALOGEX` の STYLE に `| WS_MINIMIZEBOX | WS_MAXIMIZEBOX` を足す（research 決定1）。
  - 今の STYLE は 4 行に分かれている（`DS_SHELLFONT | WS_POPUP | WS_VISIBLE | WS_CAPTION` / `| WS_THICKFRAME` / `| WS_SYSMENU`）。`| WS_SYSMENU` の行の後に `   | WS_MINIMIZEBOX | WS_MAXIMIZEBOX` の行を足す。
  - UTF-16LE・BOM 付き・CRLF を保つため、`[IO.File]::ReadAllText(path, [Text.Encoding]::Unicode)` と `New-Object Text.UnicodeEncoding($false,$true)` で読み書きし、書いた後に読み直して `git diff --text` で 1 行だけ増えたことを確かめる。
- [X] T002 [US1] `MFCApplication1/MFCApplication1Dlg.cpp` で、最大化・元に戻すの描画と当たり判定が正しいことを確かめる（依存: T001。research 決定2、spec FR-003）。
  - `OnEraseBkgnd` がクライアント全体を白で塗ること、`GetToolbarRect` がクライアントの幅いっぱいであること、構成図・ログ表示エリア・`IDC_SCROLL_LOG`・ECU の当たり判定（`ContentTop()` 基準）がクライアントの左上を基準にしていることを確かめる。
  - Release x64 を自分で起動して `ShowWindow(SW_MAXIMIZE)` 相当（`WM_SYSCOMMAND` / `SC_MAXIMIZE` を Post）→ 画面写し → `SC_RESTORE` → 画面写しで、描き残し・ずれが無いことを見る。描き残しがあれば、`OnSize` を足して `Invalidate()` する（今は `ON_WM_SIZE` が無い）。
  - 起動したプロセスは、`Start-Process -PassThru` で得た ID だけを、同じコマンドの中で閉じる（他の MFCApplication1 は閉じない）。

**Checkpoint**: タイトルバーに _ □ ✖ が並び、最小化・最大化・元に戻すができる。

## Phase 4: User Story 3 - スピードの範囲を -10〜+10 にする (Priority: P1)

**Goal**: スピードを -10〜+10 にし、以前のファイルの -13〜-11 は -10 として読む。

**Independent Test**: quickstart.md の手順 8〜10。checks の 005・015・019・026 の項目。

- [X] T003 [P] [US3] `MFCApplication1/CanSim/CanTypes.h` の定数を変える（contracts の「CanSim の変更」）。
  - `const int kMinSpeed = -10;  // feature 026 (was -13)`
  - `const int kLegacyMinSpeed = -13;  // feature 026: older project files may say -13..-11; read as kMinSpeed`
  - `ProjectSettings::Speed` のコメントを `-10..10 (feature 026; was -13..10)` にする。
- [X] T004 [US3] `MFCApplication1/CanSim/ConfigLoader.cpp` の `Speed=` の読込みを変える（依存: T003。research 決定5、spec FR-005）。
  - 受け入れる範囲を `kLegacyMinSpeed`〜`kMaxSpeed` にし、`value < kMinSpeed` なら `kMinSpeed` にする。
  - 範囲外のエラーの文言は今の形のまま `kMinSpeed`〜`kMaxSpeed`（"must be an integer between -10 and 10."）。コメントも直す。
- [X] T005 [P] [US3] `MFCApplication1/CanSim/SimControls.h/.cpp` と `MFCApplication1/CanSim/ReplayClock.h` のコメントの「-13」を直す（依存: T003）。
  - `kSpeedSliderLeft` のコメント: `x of the kMinSpeed (-10) tick`。`IsSpeedLabelShown` のコメント: `-10 / -5 / 0 / +5 / +10`。
  - 式は `kMinSpeed` 基準なので変えない（`IsSpeedLabelShown` の `speed == kMinSpeed ||` は -10 が 5 刻みなので結果は変わらない。そのまま残す）。
- [X] T006 [US3] `MFCApplication1/MFCApplication1Dlg.cpp` の `kMinSpeed` を使う箇所（スライダーの描画のループ・`SetSpeed` の範囲の切り詰め）が新しい範囲で正しく動くことを確かめる（依存: T003）。コードの変更が要らなければ変えない。

**Checkpoint**: スライダーが -10〜+10 になり、以前のファイルの -13 が -10 で読める。

## Phase 5: User Story 2 - 実行中・編集中でも最小化・最大化できる (Priority: P2)

**Goal**: 実行中・中断中・編集中の最小化・最大化で、状態が壊れない。

**Independent Test**: quickstart.md の手順 5〜7。

- [X] T007 [US2] `MFCApplication1/MFCApplication1Dlg.cpp` で、最小化中も実行が進むことと最小化中の描画を確かめる（依存: T001。research 決定3、spec US2・SC-002）。
  - 再生のタイマー（`OnTimer` → `AdvanceReplay`）が最小化中も時刻を `QueryPerformanceCounter` で進め、終端で `FinishReplay`（CSV の書き出し）まで行うこと。
  - `OnPaint` の `IsIconic()` の分岐が残っていて、最小化中に図を描かないこと。
  - 中断中（`RunPhase::Paused`）はタイマーが止まっていて、最小化しても時刻が進まないこと。
  - 編集ウィンドウ（`CFrameEditDlg`）・ログ表示（`CLogViewDlg`）はメインをオーナーにして作られていて、メインの最小化で一緒に隠れること。
  - コードの変更が要らなければ変えない。

## Phase 6: Polish & Cross-Cutting

- [X] T008 [P] `tools/cansim_runner/checks.cpp` のスピードの範囲に関わる項目を新しい範囲に直し、`CheckFeature026()` を足して `main` から呼ぶ（依存: T003〜T005）。
  - 005（750 行目付近）: 受け入れる値の例 `-13` は読込みの互換（-10 として読む）の確認に変え、保存→再読込みの往復は `-10` で行う。`SimTimeAtUs(e, -13)` の検証は式そのものの検証なので残してよい（コメントで範囲外の値でも式は成り立つことを示す）、または `-10` に替える。
  - 015（1550〜1630 行付近）: `speeds[] = { -13, 0, 5 }` を `{ -10, 0, 5 }` に、`GetSpeedChangeAction` の表の `-13` を `-10` に、「24 ticks from -13 to +10」を「21 ticks from -10 to +10」に直す。
  - 019（1830〜1850 行付近）: `IsSpeedLabelShown` の期待値を新しい範囲で書き直す: `shown(0) == {-10, -5, 0, 5, 10}`、`shown(-9) == {-9, -5, 0, 5, 10}`（-10 は -9 から 1 目盛りなので隠れる）、`shown(9) == {-10, -5, 0, 5, 9}`、`shown(3) == {-10, -5, 0, 3, 10}`。
  - `CheckFeature026()`:
    - `kMinSpeed == -10 && kMaxSpeed == 10`、`SpeedTickX(-10, 96) == 604`、`SpeedTickX(10, 96) == 864`、目盛りは 21 個で間隔 13、`SpeedAtX(0, dpi) == -10`、`SpeedAtX(大きな x) == 10`（DPI 96/120/144）。
    - `SpeedLabelRect(10, dpi).Right <= kDiagramWidth` と帯の中、`SpeedCaptionRect(dpi).Right <= SpeedTickX(-10, dpi) - 刻みの半分`。
    - 読込み: `Speed=-13`・`-12`・`-11` は成功して `-10`、`Speed=-10`・`10` はそのまま、`Speed=-14`・`11` はエラーで文言に `-10 and 10` を含む（一時ディレクトリにプロジェクトを書いて `LoadConfig`）。
- [X] T009 [P] `CLAUDE.md` と `tools/cansim_runner/README.md` を更新する。
  - `CLAUDE.md`: スピードの範囲の記述（「-13〜+10」「-13 = 1/8192」「-13 の目盛りは x 520 / x 604」など）を -10〜+10・x 604（-10）に直し、以前のファイルの -13〜-11 は -10 として読むことを足す。メインダイアログの説明に、タイトルバーの最小化・最大化（標準のボタン、最大化しても中身は左上の固定位置、最小化中も実行は進む）を足す。検証ツールの対象機能の一覧に 026 を足す。
  - `README.md`: 026 の検証項目を足し、005・015・019 の説明の「-13」を直す。
- [X] T010 Debug/Release × x64/x86 の 4 構成をビルドし、次を確かめる（依存: T001〜T009）。
  - 4 構成とも、警告・エラーなしでビルドできる。
  - checks.exe の全項目が PASS し、サンプルの CSV が基準と一致する（spec SC-005。`sample_buffer_full.canprj` の `Speed=10` は範囲内のまま）。
  - ユーザーが起動しているアプリのプロセスは終了させない。exe が使用中でビルドできないときは、ユーザーに閉じてもらうよう頼む。
- [X] T011 quickstart.md の手動確認 1〜11 をユーザーに依頼し、結果を tasks.md の末尾に記録する（依存: T010）。

## Dependencies & Execution Order

```text
T001 (US1) ─> T002 (US1) ─────────────┐
T001 ─> T007 (US2) ───────────────────┤
T003 ─┬─> T004 (US3) ─────────────────┤
      ├─> T005 (US3) ─────────────────┼─> T010 ─> T011
      ├─> T006 (US3) ─────────────────┤
      └─> T008 (checks、T004・T005 の後) ┤
T009 ─────────────────────────────────┘
```

- **ストーリーの依存**: US1（.rc のスタイル）と US3（スピードの定数）は独立。US2 は US1 のボタンがあって初めて確かめられる。
- **並行できるもの**:
  - T001（.rc）と T003（CanTypes.h）。
  - T003 の後の T005（SimControls・ReplayClock のコメント）と T004（ConfigLoader）。
  - T009（ドキュメント）は、いつでも。
- T002・T006・T007 は同じファイル（`MFCApplication1Dlg.cpp`）の確認なので、順番に行う。

### 並行の例

```text
T001 MFCApplication1.rc   |  T003 CanTypes.h
T002 → T007 (MFCApplication1Dlg.cpp の確認)   |  T004 ConfigLoader.cpp  |  T005 SimControls / ReplayClock
T008 checks.cpp  |  T009 CLAUDE.md / README.md
```

## Implementation Strategy

- **MVP**: T001・T002（US1: 最小化・最大化のボタン）。スタイル 1 行で要望の前半が満たせる。
- **順に足す**: US3（T003〜T006: スピードの範囲）→ US2（T007: 実行中の確認）→ checks（T008）→ ドキュメント（T009）→ 4 構成のビルドと検証（T010）→ 手動確認（T011）。

## 実装結果（2026-10-02）

- 変更:
  - `MFCApplication1.rc`: `IDD_MFCAPPLICATION1_DIALOG` の STYLE に `| WS_MINIMIZEBOX | WS_MAXIMIZEBOX` の 1 行（UTF-16LE のまま。読み直して確認済み）
  - `CanSim/CanTypes.h`: `kMinSpeed` = -10、`kLegacyMinSpeed` = -13。`ConfigLoader.cpp`: -13〜-11 を -10 として読む（範囲外のエラーは -10〜10）。`SimControls.h`: コメント
  - `tools/cansim_runner/checks.cpp`: 005・015・019 のスピードの項目を新しい範囲に、`CheckFeature026()` を追加。`README.md`、`CLAUDE.md`
- T002・T006・T007: コードの変更は不要だった。
  - Release x64 を自分で起動し、`WM_SYSCOMMAND` の最大化 → 元に戻す → 最小化 → 元に戻すを送った（閉じたのは自分で起動した ID だけ）。スタイルに最小化・最大化のボタンがあり、最大化で画面いっぱい（タイトルバーは _ ・元に戻す・✖）、中身は左上のまま・余りは白・帯は幅いっぱい、元に戻すで描き残しなし（画面写しで確認）。`OnSize` の追加は不要。
  - ダイアログの `kMinSpeed` の使用箇所（スライダーの描画のループ・`SetSpeed` の切り詰め）は式のままで新しい範囲になる。`OnPaint` の `IsIconic()` の分岐は残っている。
- T010（ビルド）: Debug/Release × x64/x86 の 4 構成とも警告・エラーなし。
- checks.exe: 全 194 項目 PASS（026 の 4 項目を追加。005・015・019 の項目を新しい範囲に修正）。基準 CSV とも一致した。
- T011（2026-10-02、ユーザー）: quickstart の手順 1〜11 すべて正常に動作した。
