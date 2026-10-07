# Contract: テスト名の規則

## 形

> 実装: 1 機能 1 テスト。名前は `Feature<NNN>.<ids>`（例: `Feature001.SC001to005_FR004`、`SC001to005` は SC-001〜SC-005）。ids が無い機能は `Checks`。以下の `<内容>` つきの形は、SC/FR ごとに分けた場合の規則。

```text
Feature<NNN>.<SC|FR><id>_<内容>        例: Feature006.SC003_ArbitrationLowestIdWins
Feature<NNN>.Rev<n>_<内容>             例: Feature029.Rev1_ReadLogRecordsCancel
Feature<NNN>Slow.<...>                 時間のかかるもの（research R-04 に該当したときのみ）
```

- `NNN`: 対応する機能の 3 桁の番号（`specs/NNN-...`）。
- `id`: その機能の `spec.md` の成功基準（SC-nnn）または機能要件（FR-nnn）の番号。その機能以外の仕様を参照する検証（例: 機能 029 が 001 の FR-019 と照合）は、テスト名は検証する側の機能番号にし、参照先は `CHECK` のラベルに書く。
- `内容`: 英数字と `_` のみ（ASCII）。GoogleTest の名前に `_` 以外の記号は使えない。
- 1 つのテストに複数の SC/FR がまとまる場合は、主なものを名前に入れ、残りは `CHECK` のラベルに残す。

## ラベル（`CHECK` の第 2 引数）

`checks.cpp` の既存のラベル（例: `"006 SC-003: ..."`）をそのまま使う。失敗時の出力で、元の検証項目と 1 対 1 に対応する。

## 検証

- すべてのテスト名が `Feature\d{3}(Slow)?\.` で始まること（`ctest -N` の出力で確認）。
- 機能ごとのスイートの `CHECK` の数が、`migration-ledger.md` の「移した項目数」と一致すること。
