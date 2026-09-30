# Contract: `.canprj` の `Speed` キー

機能 004 の `contracts/project-file-format.md` の `[Project]` セクションに 1 キーを追加する。

```ini
[Project]
EcuCount=6
DurationSec=10
FrameDisplay=0
Speed=0            ; -13..10、省略時 0。+10 = 待ちなし、0 = 実時間、-13 = 実時間の 1/8192
```

| キー | 必須 | 型 | 検証 |
|---|---|---|---|
| `Speed` | 任意（既定 0） | 符号付き整数（`+3` / `-3` / `3`） | -13〜10。範囲外・整数でない値は読み込まず理由を示す |

- 保存時は常に `Speed=<値>` を書く（`+` は付けない）。
- `Speed` を持たない 004 のファイルは 0 として読み込む。

対応する要件: FR-001, FR-002。
