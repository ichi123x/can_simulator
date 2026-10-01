# Data Model: 022 ECU 名（ECUラベル名）

## Node（`CanSim/CanTypes.h`）

| 項目 | 内容 |
|---|---|
| `std::string Label`（新規） | ECUラベル名。UTF-8、0〜32 文字（`kMaxEcuLabelChars`）、前後の空白なし。既定は空 |
| `std::string Name`（既存、変更なし） | ノードの名前。起動記録の CSV に出力される。自動の「ECUn」などを含む |

## プロジェクトファイル（`.canprj`）

```text
[Node1]
NodeId=1
Name=ECU1
Label=エンジン      ← 新規・省略可。Label が空なら書かない
```

- 読込み: `Label=` が無ければ空。値は前後の空白を除き、33 文字目以降は捨てる（32 文字まで）。
- 保存: `Label` が空でなければ、`Name=` の次の行に書く。

## 関数（`CanSim/FrameEditor.h`）

| 関数 | 内容 |
|---|---|
| `int CountUtf8Chars(const std::string&)` | 文字数（コードポイント） |
| `std::string EcuLabelForDiagram(const std::string&)` | 10 文字以下はそのまま、11 文字以上は先頭 10 文字 + `...` |
| `std::string TrimEcuLabel(const std::string&)` | 前後の半角空白・タブ・全角空白を除く |
| `std::string LimitEcuLabel(const std::string&)` | 先頭 32 文字に切る（読込み用） |

## 構成図（`CanSim/BusDiagramLayout.h`）

- `kEcuBoxHeight` 34 → 48。上段の箱 y 20〜68、下段 152〜200（96 dpi）。
