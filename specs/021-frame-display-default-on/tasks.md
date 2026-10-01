---

description: "Task list for feature 021-frame-display-default-on"
---

# Tasks: フレーム表示の既定値を ON にする

**Plan（要約）**: 既定値は `CanSim::ProjectSettings::FrameDisplay` の 1 か所（起動時の `m_settings` と、ファイル読込みの既定値の両方の元）。これを `true` にする。憲章 I〜VI は PASS（値 1 つの変更、checks で検証、CLAUDE.md を更新）。

- [X] T001 `MFCApplication1/CanSim/CanTypes.h` の `ProjectSettings::FrameDisplay` の既定値を `true` にする。
- [X] T002 `tools/cansim_runner/checks.cpp` の「004 FR-006: [Project] の既定値」の項目を、FrameDisplay=1 を期待するように直す。
- [X] T003 `CLAUDE.md` の「起動時は … フレーム表示 OFF」を ON（機能 021）に直す。
- [X] T004 4 構成のビルド（警告なし）・checks.exe 全 PASS・CSV の一致を確かめる。起動直後の画面写しで、チェックが入っていることを確かめる。
