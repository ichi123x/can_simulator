# Research: 016 受信中の ECU を青で強調

## 決定1: 受信 ECU の色

- **Decision**: 次の色にする。
  - 塗り: `RGB(160,195,245)`（明るい青）
  - 枠: `kActiveBusColor` = `RGB(30,90,220)`（バスと同じ青）。太さ 2 px（96 dpi）。
  - ECU 名の文字は黒のまま。
- **Rationale**:
  - FR-001。バスと同じ青の枠で「バスにつながって受信している」ことを表す。
  - 塗りは、黒い文字が読める明るさ（輝度が十分高い）にした。受信していない ECU の薄紫・薄オレンジ、送信の赤のどれとも色相が違う。
  - 枠を 2 px にしたのは、送信（赤、3 px）より控えめにしつつ、受信していない ECU（1 px）と区別するためである。
- **Alternatives considered**:
  - 塗りもバスと同じ濃い青: 黒い文字が読みにくい。
  - 白い文字にする: ECU の文字の色の規則が増える。

## 決定2: 受信 ECU の接続線

- **Decision**: 受信 ECU の接続線を、送信 ECU と同じ `activeLinePen`（`kActiveBusColor`、太さ 3 px）で描く。ペンの選択の条件を `isSender || isReceiver` にする。
- **Rationale**: FR-002（ユーザー: 送信側の青い線が OK、受信側も同じに）。

## 決定3: 強調の対象と期間は変えない

- **Decision**:
  - 受信 ECU の集合は、`ActiveFrameCursor::Collect` の `Receivers` のまま変えない。
    - 書き換え 1 回分の時間に、バス上にあったフレームの受信 ECU が入る。
    - 送信 ECU と重なる ECU は除かれている。
  - 強調を描く条件（`InRun() && m_settings.FrameDisplay`）も変えない。
  - グレー表示（機能 011）が最優先であることも変えない。
- **Rationale**: FR-003〜FR-005。「受信中の間だけ」は、今の受信 ECU の強調と同じ範囲（spec Assumptions）。
