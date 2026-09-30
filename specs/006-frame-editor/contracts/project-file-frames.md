# Contract: `.canprj` のフレーム定義（006 での変更）

機能 004 の `contracts/project-file-format.md`（005 で `Speed` 追加）の `[NodeN.FrameM]` を次のように変える。

```ini
[Node1.Frame1]
Name=エンジン回転数    ; 任意、0〜32 文字（UTF-8）。省略時は空
CanId=0x100
Dlc=8
PeriodUs=10000
InitialOffsetUs=0      ; 0 以上かつ PeriodUs 未満。省略時 0
```

| キー | 変更 |
|---|---|
| `Name` | 追加（任意）。32 文字を超える場合は拒否 |
| `Data` | 読み込まない（書かれていても誤りにしない）。データは常に `Dlc` 個の 0xFF。保存時は書かない |
| `InitialOffsetUs` | 検証を追加: `PeriodUs` 未満であること |
| フレーム数 | 1 ノードあたり 100 まで。101 以上のノードを含むファイルは拒否 |

`InitialOffsetUs` の起点は「送信ノードが通常動作に入った時刻」（Doc FR-020a）。

対応する要件: spec FR-012〜FR-015。
