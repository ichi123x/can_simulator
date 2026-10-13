# Research: 022 ECU 名（ECUラベル名）

## 決定1: `Name` ではなく新しい項目 `Label` に持つ

- **Decision**: `Node::Label`（UTF-8、0〜32 文字、既定は空）を足す。プロジェクトファイルの `[NodeN]` の `Label=` に、空でないときだけ書く。読込みでは、無ければ空にする。
- **Rationale**:
  - 既存の `Name` には、次の 2 つの事情がある。
    - 起動記録の CSV の `NodeName` 列に出力される（`StartupControl.cpp`）。
    - 名前の無い ECU に自動で「ECUn」「Noden」が入る。
  - そのため `Name` を使うと、既定を空白にできず（spec「デフォルトは空白」）、CSV も変わる（FR-007）。
  - `Label` を足せば、`Name`・CSV・サンプルの表示に影響しない。
- **Alternatives considered**:
  - `Name` を使い、自動の名前をやめる: 起動記録の CSV と基準 CSV が変わる。
  - `Name` を使い、自動の名前（「ECUn」）だけを表示しない: 規則が分かりにくく、サンプルの名前（英語の長い名前）が並ぶ。

## 決定2: 文字数の数え方・切り詰め・空白の除去（CanSim、`FrameEditor.h`）

- **Decision**:
  - `int CountUtf8Chars(const std::string&)`: UTF-8 の文字（コードポイント）の数。全角・半角とも 1。
  - `std::string EcuLabelForDiagram(const std::string& label)`: 10 文字以下ならそのまま。11 文字以上なら、先頭 10 文字 + `"..."`。
  - `std::string TrimEcuLabel(const std::string& text)`: 前後の半角空白・タブと、全角空白（U+3000）を除く。
  - 定数 `kMaxEcuLabelChars = 32`、`kEcuLabelShownChars = 10`。
- **Rationale**: spec の Edge Cases（1 文字 = 1、前後の空白、32 文字まで）と FR-003。純粋関数なので checks で確かめられる。

## 決定3: 構成図の箱と文字

- **Decision**:
  - `kEcuBoxHeight` を 34 → 48 にする。箱の接続線側の端は変えないので、上段は上へ、下段は下へ 14 px 広がる（上段 20〜68、下段 152〜200）。構成図の枠（4〜212）の内側に収まる。
  - ラベル名があるときの文字:
    - 「ECU nn」: 箱の上から 6〜26 px の範囲に、今の太字 15 px で描く。
    - ラベル名: 26〜44 px の範囲に、10 px・通常・同じ色で描く。どちらも中央寄せ。
  - ラベル名が無いとき: 「ECU nn」を箱の中央（今までどおり）に描く。
  - 当たり判定（`FindEcuAt`）・強調・グレー・流れのエフェクト（光の枠・接続線の端点 `LineY1`）は、`Box` と `LineY1` から求めているので、そのまま追従する。
- **Rationale**: FR-003〜FR-005。ラベル名を使わない構成では、箱が縦に大きくなるだけで、文字の位置は今と同じ中央になる。

## 決定4: 編集ウィンドウの入力欄と構成図の更新

- **Decision**:
  - `IDD_FRAME_EDIT` の「ノード:」のコンボの右に、次の 2 つを置く（DLU）。
    - `LTEXT "ECUラベル名:"`（x 130、y 12、幅 50）
    - `EDITTEXT IDC_EDIT_ECU_LABEL`（x 182、y 10、幅 120、高さ 12、`ES_AUTOHSCROLL`）
  - 入力欄の文字数は 32 に制限する（`SetLimitText`）。
  - ノードの選択を変えたとき・ウィンドウを開いたとき: 入力欄に、その ECU の `Label` を入れる。プログラムからの変更で `EN_CHANGE` が出ても、反映しないように印を立てる。
  - `EN_CHANGE`（ユーザーの入力）のとき:
    1. `EnsureNode(definitions, nodeId).Label = TrimEcuLabel(text)` にする。
    2. オーナーへ `WM_APP_ECU_LABEL_CHANGED`（`WM_APP + 3`）を送る。
  - メインダイアログは、`WM_APP_ECU_LABEL_CHANGED` を受けて構成図を無効化する（描き直し）。
- **Rationale**: FR-001・FR-002、SC-001（入力のたびに即時）。`EnsureNode` は、フレームの無い ECU にも定義を作る（006 と同じ）。`Name` には自動の「ECUn」が入るが、表示には使わない。
