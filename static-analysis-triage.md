# can_simulator 静的解析 警告分類表

対象: MFCApplication1.sln / Debug|x64

## 判定の種類

- 本物: 実際に問題がある。修正する
- 本物（影響小）: 問題はあるが実害はほぼない。簡単なら修正する
- 誤検知: 問題はない。直すか、理由を書いて抑制する
- 誤検知だが要注意: 今は問題ないが、前提が変わると本物になる。前提をコードで守る

## 1回目: Microsoft ネイティブ推奨規則（14件、2026-10-19）

| コード | 場所 | 内容 | 判定 | 対応 |
|---|---|---|---|---|
| C6031 ×12 | AnalysisDlg 27 / FrameEditDlg 16 / FrameEntryDlg 16 / LogViewDlg 23 / MFCApplication1Dlg 654, 680, 691, 692, 715, 961, 2127 / SettingsDlg 77 | LoadString の戻り値を見ていない | 本物（影響小） | VERIFY(...) で囲んだ |
| C6385 | CanSim/Logger.cpp 39 | digits[--n] が範囲外を読むかも | 誤検知 | do-while で n>=1 が保証されるため。理由コメント＋#pragma warning(suppress: 6385) |
| C6385 | CanSim/Logger.cpp 57 | digits[--n] が範囲外を読むかも | 誤検知だが要注意 | canId が32ビットである前提。static_assert(sizeof(canId) <= 4) ＋抑制 |

## 2回目: can_simulator.ruleset（推奨＋Core Check 6ルール、18件、2026-10-20）

| コード | 場所 | 内容 | 判定 | 対応 |
|---|---|---|---|---|
| C26451 ×6 | CanSim/FlowEffect.cpp 12-14, 23-25 | int で引き算してから double に広げている | 誤検知 | 画素座標なので溢れない。先に double へ変換してから引き算 |
| C26451 | CanSim/LogAnalysis.cpp 105 | *b - '0' を int で計算して long long に足す | 誤検知 | 0〜9 しかない。先に long long へ変換 |
| C26451 | LogViewDlg.cpp 178 | b - a を int で計算して size_t に広げる | 誤検知 | b > a 確認済み。計算を別の行で int のまま受ける |
| C26451 | MFCApplication1Dlg.cpp 2240 | ホイール段数×行数を int で計算 | 誤検知 | 段数は小さい。段数を long long に変換してから掛ける |
| C26451 | AnalysisDlg.cpp 595 | MaxSection + 1 を int で計算 | 誤検知 | 約21億にはならない。先に long long へ変換 |
| C26409 | MFCApplication1.cpp 65 | new CShellManager を手で delete | 本物（影響小） | std::unique_ptr ＋ make_unique、delete を reset() に |
| C26409 | MFCApplication1Dlg.cpp 1758 | m_cursor.reset(new ...) | 本物（書き方） | m_cursor = std::make_unique<...>(...) |
| C26409 ×6 | MFCApplication1Dlg.cpp 1142, 1327, 1348, 1383, 1404, 1530 | モードレスダイアログを new | 誤検知 | PostNcDestroy() で delete this する MFC の設計。理由コメント＋#pragma warning(suppress: 26409) |

## 3回目: IntelliSense のメッセージ（青い ⓘ、2026-10-20）

ビルドの警告ではなく、エディタの提案。警告0件の条件には入らないが、1件ずつ判定した。

| コード | 場所 | 内容 | 判定 | 対応 |
|---|---|---|---|---|
| Int-uninit | MFCApplication1.cpp 52 | InitCtrls が初期化されていない | 誤検知 | 2つのメンバーとも使う前に代入済み。InitCtrls{} で0初期化して解消 |
| Int-arith | CanSim/LogAnalysis.cpp 251 | 4 * 1024 * 1024 を int で計算 | 誤検知 | 定数で溢れない。std::size_t{4} * 1024 * 1024 に |
| Int-arith | LogViewDlg.cpp 311 | point.y の計算を int で行って long long に入れる | 誤検知 | 画素位置で溢れない。const long long y = point.y; にし、m_lineHeight - y - 1 の順で計算（引き算は左から評価されるため） |
| Int-make | LogViewDlg.cpp 431 OnLButtonUp | const にできる | 誤検知・対応なし | MFC のメッセージハンドラは決まった形なので const にできない |
| Int-make | MFCApplication1Dlg.cpp 934 OnQueryDragIcon | const にできる | 誤検知・対応なし | 同上（ウィザード生成のハンドラ） |
| Int-make | MFCApplication1Dlg.cpp 466 DrawFlowEffect | const にできる | 本物（書き方・任意） | 自作の描画関数でメンバーを読むだけ。.cpp の定義と .h の宣言の両方に const を付けた |
| VCR102 | MFCApplication1Dlg.cpp 469〜 など | マクロに関連する操作を参照できない可能性 | 対応なし | IntelliSense が RGB などのマクロを読み切れないときのヒント。ビルドとは無関係 |

## 集計

- 合計 32件 → 現在 0件
- 本物・本物（影響小）: 14件（すべて修正）
- 誤検知: 17件（10件は書き方を変えて解消、7件は理由付きで抑制）
- 誤検知だが要注意: 1件（static_assert で前提を守っている）
- IntelliSense の ⓘ: 6件を判定（4件は書き方を変えて解消、2件は MFC ハンドラのため対応なし）。VCR102 は対応なし

## 4回目：clang-tidy（VS2022 付属・既定のチェック）

実行コマンド（開発者コマンド プロンプトで）:

```
msbuild MFCApplication1.sln /t:Rebuild /p:Configuration=Debug /p:Platform=x64 /m /p:EnableClangTidyCodeAnalysis=true
```

準備: CanSim\*.cpp で「pch.h が見つからない」エラーが18件出た。プロジェクトの「追加のインクルード ディレクトリ」に `$(ProjectDir)` を足して解消（MSVC のビルドには影響なし）。

| コード | 場所 | 内容 | 判定 | 対応 |
|---|---|---|---|---|
| inconsistent-missing-override | MFCApplication1Dlg.cpp 46 | DoDataExchange に override が無い | 本物（軽微） | override を追加 |
| FloatLoopCounter | CanSim/FlowEffect.cpp 97 | double をループの数え役に使っている（誤差で回数がずれる） | 要注意 | int で数えて、double はかけ算で計算 |
| FloatLoopCounter | CanSim/LogAnalysis.cpp 604 | 同上（NiceTicks の目盛り） | 要注意 | 同上 |
| C26451 | CanSim/FlowEffect.cpp 97, 102 | 上の修正で新たに発生（i * kFlowSpacing を4バイトで計算） | 本物 | static_cast<double>(i) を先にかける |

結果: MSVC 0件、clang-tidy 0件。

学び: MSVC で0件でも clang-tidy だけが見つける指摘がある。片方を直すともう片方が反応することもあるので、両方をまとめて走らせる。
