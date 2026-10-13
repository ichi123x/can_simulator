---

description: "Task list for feature 023-toolbar-file-buttons"
---

# Tasks: ツールバーへのプロジェクト読込み・保存ボタンの追加

**Input**: `/specs/023-toolbar-file-buttons/` の spec.md・plan.md（座標・見た目の表）・research.md・data-model.md・contracts/ui-toolbar-file-buttons.md・quickstart.md

**Tests**: spec はテストの作成を求めていない。座標・当たり判定・有効／無効の検証は、既存の慣習どおり `checks.cpp` に項目として足す（T005）。

## Phase 1: Setup

- [X] T001 [P] `MFCApplication1/Resource.h` と `MFCApplication1/MFCApplication1.rc` にリソースを足す。
  - `Resource.h`:
    - `#define ID_FILE_SAVE_OVERWRITE 32779	// feature 023`（ツールバー専用。メニューには載せない）
    - `#define IDS_FILE_BTN_SAVE 251	// feature 023`
    - `#define IDS_FILE_BTN_LOAD 252`
    - `_APS_NEXT_SYMED_VALUE` を 253、`_APS_NEXT_COMMAND_VALUE` を 32780 にする。
  - `.rc` の文字列テーブル（`IDS_SIM_BTN_*` の近く）に `IDS_FILE_BTN_SAVE "保存"`、`IDS_FILE_BTN_LOAD "読込み"` を足す。UTF-16LE・BOM 付き・CRLF を保つため、PowerShell でエンコーディングを明示して読み書きし、書いた後に読み直して文字化けが無いことを確かめる（CLAUDE.md のビルド節）。

## Phase 2: Foundational（すべてのストーリーの前提）

- [X] T002 `MFCApplication1/CanSim/SimControls.h/.cpp` にツールバーのボタンの通し番号を足す（contracts の「CanSim に足す・変えるもの」）。ASCII のみ。
  - `enum class ToolbarButton { Save, Load, Start, Pause, Stop };`、`const int kToolbarButtonCount = 5;`、`const int kFileButtonCount = 2;`
  - 通し番号 i のボタンの左端（96 dpi）は `kSimButtonLeft(=8) + kSimButtonPitch(=28) × slot`。slot は `i < kFileButtonCount ? i : i + 1`（開始の前にボタン 1 つ分の空き）。大きさ（箱 24 × 18、上 14、名前 y 33〜49）は今の定数のまま。
  - `DiagramRect ToolbarButtonRect(int index, int dpi)`、`ToolbarButtonBoxRect`、`ToolbarButtonLabelRect` を足す。今の `ScaledButtonPart(index, …)` を、通し番号から slot を求める形に変える。
  - `int FindToolbarButtonAt(int x, int y, int dpi)`: 当たり判定 `[Left, Right) × [Top, Bottom)` を含むボタンの通し番号を返す。無ければ -1（空きの上も -1）。
  - `bool IsToolbarButtonEnabled(int index, RunPhase phase, bool editingFrames)`:
    - Save・Load は `phase == RunPhase::Idle`（`editingFrames` に関係しない）。
    - Start・Pause・Stop は `GetSimCommandStates(phase, editingFrames)` の Start・Pause・Stop。
  - 既存の `SimButtonRect(i)`・`SimButtonBoxRect(i)`・`SimButtonLabelRect(i)` は `ToolbarButton…Rect(i + kFileButtonCount)` を返す。`FindSimButtonAt` は開始〜終了だけを 0〜2 で返す（保存・読込みの上は -1）。
  - 既存部品の x の定数を 84 増やす（plan.md の表）:
    - `kFrameDisplayBoxLeft` 188、`kFrameDisplayLabelLeft` 208、`kFrameDisplayLabelRight` 292、`kFrameDisplayHitLeft` 184、`kFrameDisplayHitRight` 296
    - `kSimTimeLeft` 314、`kSimTimeRight` 494、`kSimTimeValueRight` 464、`kSimTimeUnitLeft` 468
    - `kSpeedCaptionLeft` 514、`kSpeedCaptionRight` 584
    - `kSpeedSliderLeft` 604
    - コメントの「x 520」などの数値も合わせて直す。
- [X] T003 `MFCApplication1/MFCApplication1Dlg.cpp` のツールバーの描画と押下を、5 つのボタンの通し番号で扱うように変える（依存: T001, T002）。
  - 無名名前空間の表を `CanSim::kToolbarButtonCount` 個にする。
    - 記号の色: 保存 `RGB(120, 70, 180)`、読込み `RGB(210, 150, 30)`、開始・中断・終了は今の色。
    - 名前: `IDS_FILE_BTN_SAVE`、`IDS_FILE_BTN_LOAD`、`IDS_SIM_BTN_START`・`PAUSE`・`STOP`。
    - コマンド: `ID_FILE_SAVE_OVERWRITE`、`ID_FILE_LOAD_PROJECT`、`ID_SIM_START`・`PAUSE`・`STOP`。
  - `IsButtonEnabled(states, index)` を `CanSim::IsToolbarButtonEnabled(index, m_runPhase, IsEditingFrames())` に置き換える（`DrawSimToolbar`・`PressToolbarButton`・`OnLButtonUp` の 3 か所）。
  - `ToolbarButtonAt` は `CanSim::FindToolbarButtonAt` を使う。`m_pressedButton` は通し番号（0〜4、-1 = なし）にする（`MFCApplication1Dlg.h` のコメントも直す）。
  - `DrawSimToolbar` のループを 5 つにして、箱は `ToolbarButtonBoxRect`、名前は `ToolbarButtonLabelRect` で描く。記号は通し番号で分ける（今の `i == 0 / 1 / else` を `ToolbarButton` の値で分ける形に）。
    - 保存（フロッピー）: 記号の色で 10 × 10 の四角（`ScalePx(10)`、中心は箱の中心）。その上辺の中央に白の 5 × 3 の窓（`ScalePx(5)` × `ScalePx(3)`、上端から `ScalePx(1)` 下）。
    - 読込み（フォルダー）: 記号の色で、本体 10 × 7（下寄せ）と、本体の左上に乗るつまみ 4 × 2。
    - 箱・押下中・無効の色、名前の文字（10 px、白／無効は灰）は今のボタンと同じ。

**Checkpoint**: この時点で 5 つのボタンが描かれ、読込みボタンはメニューと同じ処理で動く（`ID_FILE_LOAD_PROJECT` は既存のハンドラ）。保存ボタンはハンドラが無いので、押しても何も起きない。

## Phase 3: User Story 1 - ツールバーからプロジェクトを保存する (Priority: P1) 🎯 MVP

**Goal**: 「保存」ボタンで、ファイルが無ければ保存先を尋ねて保存し、あれば尋ねずに上書きする。

**Independent Test**: quickstart.md の手順 2〜5。

- [X] T004 [US1] `MFCApplication1/MFCApplication1Dlg.h/.cpp` に保存の共通関数とツールバーの保存のハンドラを足す（依存: T003）。
  - `bool SaveProjectTo(const CString& path)`:
    - `CanSim::SaveProject(std::wstring((LPCTSTR)path), m_settings, m_definitions)` が失敗したら、`IDS_SAVE_ERROR` を `path` で整形して `AfxMessageBox(…, MB_OK | MB_ICONWARNING)` で出し、false を返す。このとき `m_projectPath` は変えない。
    - 成功したら `m_projectPath = path` にして true を返す。
  - `OnFileSaveProject`（メニュー。毎回保存先を尋ねる動作は変えない）と、`OnSimStart` の「`m_projectPath` が空なら尋ねて保存」で、今ある同じ処理を `SaveProjectTo` に置き換える。`OnSimStart` は失敗なら return する（今と同じ）。
  - `afx_msg void OnFileSaveOverwrite()` を足し、`ON_COMMAND(ID_FILE_SAVE_OVERWRITE, &CMFCApplication1Dlg::OnFileSaveOverwrite)` をメッセージマップに足す。
    - `InRun()` なら何もしない（US3。ボタンは無効だが、ほかの経路にも備える）。
    - `CString path = m_projectPath;` が空なら `AskSavePath(path)` を呼び、キャンセルなら return する。
    - `SaveProjectTo(path)` を呼ぶ。
    - 編集ウィンドウは閉じない（spec Edge Cases）。ログ表示エリアの行も消さない。

**Checkpoint**: US1 の受け入れシナリオ 1〜4 を手で確かめられる。

## Phase 4: User Story 2 - ツールバーからプロジェクトを読み込む (Priority: P1)

**Goal**: 「読込み」ボタンが、メニューの「プロジェクト読込み」とまったく同じ処理を行う。

**Independent Test**: quickstart.md の手順 6〜8。

- [X] T005 [US2] `ID_FILE_LOAD_PROJECT` の Post で、`OnFileLoadProject` がそのまま呼ばれることを確かめる（依存: T003）。
  - 確かめること: 編集ウィンドウを閉じる（`CloseFrameEditor`。切り替えの待ちも取り消す）、拡張子の検査、`LoadConfig` のエラー表示、`InvalidateToolbar`・`SetEcuCount` による反映。
  - 処理を新しく足す必要が無ければ、コードは変えない。足りない点があれば `MFCApplication1/MFCApplication1Dlg.cpp` の `OnFileLoadProject` を直す。

## Phase 5: User Story 3 - 実行中は押せない (Priority: P2)

**Goal**: 実行中・中断中は保存・読込みがグレーで、押しても何も起きない。

**Independent Test**: quickstart.md の手順 9。

- [X] T006 [US3] 実行の状態が変わるとき（開始・中断・再開・自然終了・即時完了・「終了」）と、編集ウィンドウを開け閉めするときに、保存・読込みが描き直されることを確かめる（依存: T003）。
  - 確かめ方: `MFCApplication1/MFCApplication1Dlg.cpp` で、これらの箇所が `UpdateSimControls`（`InvalidateToolbar`）を呼んでいること。
  - 呼ばない経路があれば、その経路に `UpdateSimControls()` を足す。

## Phase 6: Polish & Cross-Cutting

- [X] T007 [P] `tools/cansim_runner/checks.cpp` に `CheckFeature023()` を足し、`main` から呼ぶ（依存: T002）。DPI 96/120/144 で、次を確かめる。
  - ボタンの並び:
    - 5 つのボタンが左から 保存・読込み・開始・中断・終了 の順で、重ならない。
    - 96 dpi での箱の左端が 8・36・92・120・148 である。
    - 読込みの右端と開始の左端の間は、ボタンの間隔 1 つ分以上空いている。
  - 当たり判定:
    - `FindToolbarButtonAt` が各ボタンの範囲 `[Left, Right) × [Top, Bottom)` と一致する。
    - 空き（96 dpi で x 64〜88 の中央）と帯の外では -1 になる。
  - 既存の関数との対応:
    - `SimButtonRect(i)` が `ToolbarButtonRect(i + 2)` と等しい。
    - `FindSimButtonAt` は、保存・読込みの上で -1 になる。
  - 有効／無効（`IsToolbarButtonEnabled`）:
    - Idle: 保存・読込みは編集中でも true。
    - Running・Paused: 保存・読込みは false。
    - 開始・中断・終了は `GetSimCommandStates` と一致する（3 状態 × 編集中の有無）。
  - 部品の並びと収まり:
    - 終了ボタンの右端 < チェックボックスの当たり判定の左端。
    - +10 の数字の右端が `kDiagramWidth` 以下で、帯の中に収まる。
  - 既存の 014・015・017・019 の項目がそのまま通ることも確かめる（座標の数値を直書きしている項目があれば、新しい値に直す）。
- [X] T008 [P] `CLAUDE.md` と `tools/cansim_runner/README.md` を更新する。
  - `CLAUDE.md`:
    - ツールバーの節（014・017・019）に機能 023 の段落を足す。内容は、ボタンの並び（保存・読込み・空き・開始・中断・終了）、`ToolbarButton` の通し番号、保存ボタンの動作（ファイルがあれば上書き、無ければ保存先を尋ねる。メニューの保存は毎回尋ねる）、`SaveProjectTo`、`ID_FILE_SAVE_OVERWRITE`。
    - アーキテクチャ節の `SimControls.h/.cpp` の説明にも 023 を足す。
    - 検証ツールの対象機能の一覧に 023 を足す。
  - `README.md`: 023 の検証項目を足す。
- [X] T009 Debug/Release × x64/x86 の 4 構成をビルドし、次を確かめる（依存: T001〜T008）。
  - 4 構成とも、警告・エラーなしでビルドできる。
  - checks.exe の全項目が PASS し、サンプルの CSV が基準と一致する（spec SC-005）。
- [X] T010 quickstart.md の手動確認 1〜11 を行い、結果を tasks.md の末尾に記録する（依存: T009）。
  - ユーザーが起動しているアプリのプロセスは終了させない。exe が使用中でビルドできないときは、ユーザーに閉じてもらうよう頼む（019 の作業での反省）。

## Dependencies & Execution Order

```text
T001 ─┐
T002 ─┴─> T003 ─┬─> T004 (US1) ─┐
                ├─> T005 (US2) ─┼─> T009 ─> T010
                └─> T006 (US3) ─┤
T002 ─> T007 ───────────────────┤
T008 ───────────────────────────┘
```

- **ストーリーの依存**: US1・US2・US3 はどれも T003（5 つのボタンの描画と押下）の後に進められ、互いには依存しない。
- **並行できるもの**:
  - T001 と T002（ファイルが別）。
  - T002 の後の T007（checks.cpp）と、T003 以降のダイアログの作業。
  - T008（ドキュメント）は、いつでも。

### 並行の例

```text
# Phase 1・2 の初め
T001 Resource.h / .rc        |  T002 SimControls.h/.cpp

# T002 の後
T003 → T004 → T005 → T006 (MFCApplication1Dlg.cpp)  |  T007 checks.cpp  |  T008 CLAUDE.md / README.md
```

## Implementation Strategy

- **MVP**: T001〜T004（US1: 保存ボタン）。保存は要望の中心で、読込みは既存のハンドラを使うため T003 の時点でほぼ動く。
- **順に足す**: US2 の確認（T005）→ US3 の確認（T006）→ checks（T007）→ ドキュメント（T008）→ 4 構成のビルドと検証（T009）→ 手動確認（T010）。
- T004〜T006 は同じファイル（`MFCApplication1Dlg.cpp`）を触るので、順番に行う。

## 実装結果（2026-10-01）

- 変更:
  - `CanSim/SimControls.h/.cpp`（`ToolbarButton`、`ToolbarButtonRect` / `BoxRect` / `LabelRect`、`FindToolbarButtonAt`、`IsToolbarButtonEnabled`、既存部品の x を +84）
  - `MFCApplication1Dlg.h/.cpp`（5 つのボタンの描画・押下、保存・読込みの記号、`OnFileSaveOverwrite`、`SaveProjectTo`）
  - `Resource.h`・`MFCApplication1.rc`（`ID_FILE_SAVE_OVERWRITE`、`IDS_FILE_BTN_SAVE` / `LOAD`。.rc は読み直して確認済み）
  - `tools/cansim_runner/checks.cpp`・`README.md`、`CLAUDE.md`
- T005・T006: コードの変更は不要だった。読込みは既存の `OnFileLoadProject` がそのまま動き、実行の状態・編集ウィンドウの開閉はどれも `UpdateSimControls` を呼んでいる。
- 画面写しで見つけて直したこと: 「読込み」（3 文字、約 30 px）がボタンの幅（24 px）の名前の範囲で切られ、左端が欠けていた。名前を `DT_NOCLIP` で描くようにした。中央揃えのまま隣の隙間に少しはみ出すだけで、「保存」とは重ならない。
- T009（ビルド）:
  - Release x64・Debug x86・Release x86: 警告・エラーなしでビルドできた。
  - Debug x64: 初回はユーザーが起動中の Debug 版が exe を使っていてリンク（LNK1168）で止まった（プロセスは終了させていない）。ユーザーが閉じた後にビルドし直し、警告・エラーなしで通った。
- checks.exe: 全 175 項目 PASS（023 の 6 項目を追加）。
  - HEAD のソースの runner で作った基準 CSV（各サンプル 10 秒）とも一致した（004 SC-009）。
- T010（GUI、Release x64）:
  - quickstart 手順 1（並び）: 画面写しで確認した。保存・読込み・空き・開始・中断・終了 → フレーム表示 → シミュレーション時間 → スピード：→ スライダーの順で、重なりは無い。起動直後は 保存・読込み・開始が有効、中断・終了がグレー。
  - 手順 2〜11（保存・読込みのダイアログ操作、実行中のグレー、押し方など）は未実施。ユーザーの手動確認を待つ。

## T010 の手動確認の結果（2026-10-02、ユーザー）

| 手順 | 結果 |
|------|------|
| 1. 並び | 基本的に問題なし。終了ボタンの箱とフレーム表示のチェックボックスの高さを揃えると見栄えがよくなる（→ 改訂 1 の A） |
| 2. 保存（ファイルなし） | 正常 |
| 3. 保存（ファイルあり） | 正常 |
| 4. 保存の失敗 | 動作は正常。メッセージが不親切なので、プロジェクトファイル名を示し、読み取り専用のため保存できなかったと伝える（→ 改訂 1 の B） |
| 5. メニューの保存は変わらない | 正常 |
| 6. 読込み | 正常 |
| 7. 読込みのキャンセル・エラー | 3 点とも正常 |
| 8. 編集中 | 2 点とも正常 |
| 9. 実行中・中断中 | 2 点とも正常 |
| 10. 押し方 | 正常 |
| 11. 既存の動作 | 正常 |

## Phase 7: 改訂 1（ユーザーの確認を受けて）

spec.md の「改訂 1」の A（チェックボックスの高さ）・B（読み取り専用のメッセージ）を行う。

- [X] T011 [P] `MFCApplication1/CanSim/SimControls.h` のフレーム表示のチェックボックスの y を、終了ボタンの箱の中心（96 dpi で y 23）に揃える。
  - `kFrameDisplayBoxTop` 25 → 16（箱 16〜30）、`kFrameDisplayLabelTop` 22 → 13、`kFrameDisplayLabelBottom` 42 → 33。x・当たり判定（y 4〜60）は変えない。
- [X] T012 [P] 保存の失敗が読み取り専用のファイルによるときのメッセージを足す。
  - `Resource.h`: `#define IDS_SAVE_READONLY 253	// feature 023 revision 1`、`_APS_NEXT_SYMED_VALUE` を 254 にする。
  - `.rc`（UTF-16LE・BOM 付き・CRLF を保つ）: `IDS_SAVE_READONLY "プロジェクトファイル「%s」は読み取り専用のため、保存できませんでした。\n読み取り専用を外してから、もう一度保存してください。\n\n%s"`（1 つ目の `%s` はファイル名、2 つ目はフルパス）。
  - `MFCApplication1Dlg.cpp` の `SaveProjectTo`: 失敗したとき、`GetFileAttributes(path)` が `INVALID_FILE_ATTRIBUTES` でなく `FILE_ATTRIBUTE_READONLY` を含めば `IDS_SAVE_READONLY`、それ以外は今の `IDS_SAVE_ERROR`。`m_projectPath` を変えないことは同じ。
- [X] T013 `tools/cansim_runner/checks.cpp` に、チェックボックスの箱の縦の中心が終了ボタンの箱の縦の中心と ±1 px 以内で一致すること（DPI 96/120/144）、チェックボックスの箱・文字が当たり判定と帯の中に収まることを足す（依存: T011）。
- [X] T014 `CLAUDE.md` の 023 の段落に改訂 1 を足し、4 構成のビルドと checks.exe（全項目 PASS・CSV 一致）を確かめる（依存: T011〜T013）。
- [X] T015 手動確認: quickstart.md の手順 1（高さ）と 4（メッセージ）をもう一度確かめる（依存: T014）。

## 改訂 1 の実装結果（2026-10-02）

- 変更:
  - `CanSim/SimControls.h`: `kFrameDisplayBoxTop` 16、`kFrameDisplayLabelTop` 13、`kFrameDisplayLabelBottom` 33（箱・文字の縦の中心が終了ボタンの箱と同じ y 23）
  - `Resource.h`（UTF-8 BOM のまま）・`.rc`（UTF-16LE のまま。読み直して確認済み）: `IDS_SAVE_READONLY`（253）
  - `MFCApplication1Dlg.cpp` の `SaveProjectTo`: 失敗時に `GetFileAttributes` で読み取り専用なら `IDS_SAVE_READONLY`（ファイル名とフルパス）、それ以外は `IDS_SAVE_ERROR`
  - `checks.cpp`: 023 改訂 1 A の項目を追加、`CLAUDE.md`: 023 の段落に改訂 1、「未完了の作業」の節を削除
- 手動確認の手順 6 でサンプル `sample_16ecu.canprj` が上書き保存され、説明のコメントが消えていた（中身は既定値の書き出しのみ）。`git restore` でコミット済みの内容に戻した。
- T014（ビルド）: Release x64・Debug x86・Release x86 は警告・エラーなし。Debug x64 は初めユーザーが起動中だった（プロセスは終了させていない）。ユーザーが閉じた後にビルドし、警告・エラーなしで通った。
- checks.exe: 全 181 項目 PASS。HEAD のソースの runner で作った基準 CSV（各サンプル 10 秒）とも一致した。
- T015（2026-10-02、ユーザー）: 手順 1（チェックボックスの高さ）・手順 4（読み取り専用のメッセージと、外した後の保存）とも問題なし。
