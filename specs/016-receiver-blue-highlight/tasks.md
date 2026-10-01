---

description: "Task list for feature 016-receiver-blue-highlight"
---

# Tasks: 受信中の ECU と接続線を青で強調する

**Input**: Design documents from `/specs/016-receiver-blue-highlight/`

**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/, quickstart.md（すべて存在）。機能 005・014 の実装が完了していること。

**Tests**: spec.md は自動テストを要求していない。描画の色だけの変更なので、GUI の自動操作（画面の色の読み取り）で確認する。checks.exe は既存項目の PASS と CSV の一致を確認する。

**Organization**: spec.md のユーザーストーリーは 1 つ（US1 受信中の ECU を青で見分ける P1）。

## Format: `[ID] [P?] [Story] Description`

## Path Conventions

UI は `MFCApplication1/MFCApplication1Dlg.cpp`。`CanSim/`・`.rc` の変更はない。GUI 検証スクリプトはリポジトリ外（scratchpad の `t016/`。`t015/` の `Gui.cs`・`gui.ps1` の関数を土台にする）。

---

## Phase 1: Setup

- [X] T001 GUI 検証の準備をする（リポジトリ外、scratchpad の `t016/`）。
  - `t015/Gui.cs` と、`t015/gui.ps1` の関数をコピーする。コピーする関数は次のとおり。
    - 読込み・メッセージボックスの確実な閉じ方
    - ボタンのクリック、画面の色の読み取り
  - 構成図の点の色を読む関数を用意する（座標は 96 dpi のコンテンツ座標を DPI 換算し、帯の高さ `S(64)` を足す）。
    - ECU n の箱の中の点: 箱の左上から (8, 8) 内側。
    - 接続線の中点: `LineX` と、`(LineY1 + LineY2) / 2`。
    - バスの点: (600, 110)。
    - 位置は `BusDiagramLayout` の規則に従う。
      - 上段（奇数）の箱は y 34〜68、下段（偶数）の箱は y 152〜186。
      - 箱の中心 x は `150 + 56 × (n − 1)`、幅は 96。
  - 一時フォルダに、`sample_appendix_a`・`sample_collision` を `FrameDisplay=1`・`Speed=-10` にしたコピーを作る。
  - 「バスが青の瞬間に中断する」関数を用意する: 開始後、バスの点が青 `1E5ADC` になったら、すぐ中断ボタンを押す。
    - 中断後にバスが黒なら再開して、もう一度試す（最大 20 回）。

---

## Phase 2: User Story 1 - 受信中の ECU を青で見分ける (Priority: P1) 🎯 MVP

**Goal**: フレーム表示 ON の実行中、受信 ECU の箱を青（塗り `RGB(160,195,245)`・枠 `RGB(30,90,220)` 2 px）、接続線を青の太線で描く。

**Independent Test**: `sample_collision` をフレーム表示 ON で実行し、バスが青のときに受信 ECU が青、受信しない ECU 2 が元の色である（quickstart.md 手順 2-1〜2-5）

- [X] T002 [US1] `MFCApplication1/MFCApplication1Dlg.cpp` の `DrawBusDiagram` で、受信 ECU の描き方を変える（research.md 決定1・2、data-model.md）。
  - 色の定数:
    - `kReceiverFill` を `RGB(170, 225, 170)` から `RGB(160, 195, 245)` に変える。
    - `kReceiverFrame` を `RGB(40, 140, 40)` から `RGB(30, 90, 220)`（= `kActiveBusColor`）に変える。
    - コメントを「016: blue, the frame and the stub line in the bus colour」とする。
  - 接続線のペンの選択を `isSender ? &activeLinePen : &linePen` から `isSender || isReceiver ? &activeLinePen : &linePen` に変える（spec FR-002）。
  - 受信 ECU の枠の太さを 2 にする（`else if (isReceiver)` の中で `frameWidth = 2;`）。
  - グレー表示（機能 011）の上書き・送信 ECU の処理・強調を描く条件（`InRun() && m_settings.FrameDisplay`）は変えない。（spec FR-001〜FR-005）
- [X] T003 [US1] Debug|x64 をビルドし、実機で quickstart.md 手順 2-1〜2-5 を確認する（依存: T001, T002）。
  - 2-1: `sample_appendix_a` で、バスが青の瞬間に中断する。
    - 送信 ECU（箱の点が `F06E6E`）が 1 つある。
    - 受信 ECU の箱の点が `A0C3F5`、接続線の中点が `1E5ADC`。
    - 送信 ECU の接続線も `1E5ADC`。
  - 2-2: `sample_collision` で同じく中断する。ECU 2 の箱の点は下段の元の色 `FFE6CC`、接続線は黒 `000000`。
  - 2-3: バスが黒（`000000`）の時点で中断した画面では、どの ECU の箱の点も `A0C3F5`・`F06E6E` でなく、どの接続線も `1E5ADC` でない。
  - 2-4: `FrameDisplay=0` のコピーで実行・中断すると、どの ECU も元の色になる。
  - 2-5: 2-1 の画面写しを保存し、受信 ECU の名前が読めることを目で確認する。
  - （SC-001〜SC-003、spec US1 受け入れシナリオ 1〜5）

**Checkpoint**: US1 の受け入れシナリオ 1〜5 を満たす

---

## Phase 3: Polish & Cross-Cutting Concerns

- [X] T004 [P] `CLAUDE.md` の「プロジェクトと実行（機能 004）」の段落にある「送信元 ECU（赤）・受信 ECU（緑）・バス（青）」を、「送信元 ECU（赤）・受信 ECU（青。機能 016 から。接続線も送信と同じ青の太線）・バス（青）」に直す（憲章 V）。
- [X] T005 [P] `specs/004-menu-project-settings/contracts/ui-menu-dialogs.md` の「受信 ECU | 緑系の塗り」の行に、「（機能 016 で青系の塗り・青い枠・青い接続線に変更）」を追記する。
- [X] T006 Debug/Release × x64/x86 の全 4 構成で、次を確認する（依存: T003）。
  - 警告なしでビルドできる。
  - `checks.exe` の全項目が PASS（001〜011・014・015）。
  - サンプルの CSV が基準と一致する（spec SC-004）。

---

## Dependencies & Execution Order

- T001（検証の準備）と T002（実装）は並行可。T003 は両方の後。
- T004・T005（文書）は並行可。T006 は最後。

## Implementation Strategy

1. **MVP = US1**（T001〜T003）: 受信 ECU を青にする（これで要望はすべて）。
2. Polish で文書・4 構成ビルド。

## Notes

- 強調の対象（`ActiveFrameCursor`）・シミュレーション・CSV は変更しない。

## 実装結果（2026-10-01）

- 変更:
  - `MFCApplication1Dlg.cpp`
    - `kReceiverFill` = `RGB(160,195,245)`、`kReceiverFrame` = `RGB(30,90,220)`
    - 受信 ECU の枠を 2 px にした。
    - 接続線のペンを、`isSender || isReceiver` のとき青の太線にした。
  - `CLAUDE.md`、`specs/004-menu-project-settings/contracts/ui-menu-dialogs.md`
- ビルド: Debug/Release × x64/x86 の 4 構成とも、警告・エラーなしでビルドできた。
- checks.exe: 全 147 項目 PASS（コアは変更なし。基準 CSV と一致）。
- GUI（Debug|x64、120 dpi、スピード -10）: scratchpad の `t016/gui.ps1` で、全項目が合格した。
  - 2-1: `sample_appendix_a` で、バスが青の瞬間に中断した。
    - 送信 ECU 03 は `F06E6E`・線 `1E5ADC`。
    - 受信 ECU 01・02・04・05・06 は箱 `A0C3F5`・線 `1E5ADC`。
  - 2-2: `sample_collision` では、ECU 3 が赤、ECU 1 が青だった。何も受信しない ECU 2 は `FFE6CC`・黒い線のままだった。
  - 2-3: バスが黒の時点で中断すると、全 ECU が元の色（`E1D5E7` / `FFE6CC`）・黒い線だった。
  - 2-4: フレーム表示 OFF では、強調は無かった。
  - 2-5: 画面写し（`t016/busy_appendix.png`）で、青い ECU の名前が読めることを確認した。
