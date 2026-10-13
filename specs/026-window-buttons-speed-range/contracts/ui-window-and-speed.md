# Contract: タイトルバーのボタンとスピードの範囲

## タイトルバー（メインウィンドウ）

```text
[アイコン] CAN Bus Simulator    プロジェクト名：…        [ _ ][ □ ][ ✖ ]
```

- Windows 標準のボタン（テーマどおりの見た目・間隔）。最大化中は □ が「元に戻す」の形になる。
- システムメニュー（アイコンのクリック・Alt+Space）にも「元のサイズに戻す・最小化・最大化・閉じる」が出る（末尾に「バージョン情報」）。
- 最大化しても中身は左上の固定位置。余りは白、ツールバーの帯は幅いっぱい。

## スピードスライダー

```text
スピード：  |----|----|----|----|----|----|----|----|----|----|----|----|----|----|----|----|----|----|----|----|
           -10            -5                   0                   +5                  +10
```

- 目盛り: -10〜+10 の 21 個。-10 は 96 dpi で x 604、間隔 13（+10 は x 864）。
- 数字: 5 刻み（-10・-5・0・+5・+10）と今の値（太字・黄色）。今の値から 2 目盛り以内の 5 刻みは隠す（機能 019）。
- 押した・ドラッグした位置の一番近い目盛りの値（範囲外は -10 / +10）。

## CanSim の変更（ASCII のみ）

```cpp
// CanTypes.h
const int kMinSpeed = -10;        // feature 026 (was -13)
const int kLegacyMinSpeed = -13;  // read from older project files as kMinSpeed
const int kMaxSpeed = 10;
const int kInstantSpeed = 10;
```

`ConfigLoader`: `Speed=` が `kLegacyMinSpeed`〜`kMaxSpeed` の整数なら受け入れ、`kMinSpeed` 未満は `kMinSpeed` にする。それ以外はエラー`"[Project] Speed: '<text>' must be an integer between -10 and 10."`。
