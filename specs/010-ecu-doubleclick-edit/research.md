# Research: 010 ECU ブロックのダブルクリック

## 決定1: 当たり判定を CanSim の配置計算に置く

- **Decision**: `CanSim/BusDiagramLayout` に `int FindEcuAt(const BusDiagramLayout& layout, int x, int y, int dpi)` を追加する。
  クライアント座標（構成図の左上が原点、構成図はクライアント左上に描かれる）と DPI を受け取り、各 ECU の箱を描画と同じ換算で
  拡大した矩形に点が含まれればその ECU 番号（1〜16）、どれにも含まれなければ 0 を返す。
- **Rationale**: 憲章 III（UI 非依存・検証可能）。描画と同じ配置データから求めるので、ECU 数を変えても判定がずれない。
- **Alternatives considered**: 描画時の矩形をダイアログで記憶（検証できない）。ECU ごとに子ウィンドウ（自前描画・強調と二重管理）。

## 決定2: DPI 換算を 1 つの関数に統一

- **Decision**: `CanSim::ScaleToDpi(int px, int dpi)` = `(px * dpi + 48) / 96`（非負の px で `MulDiv(px, dpi, 96)` と同じ四捨五入、中間値は
  `long long`）。ダイアログの `ScalePx` はこれを呼ぶ。
- **Rationale**: 描画と判定が同一の関数になり 125%・150% でもずれない（SC-005）。構成図の座標は非負なので見た目は変わらない。
- **Alternatives considered**: クリック座標を 96 dpi に逆換算（丸めで境界が 1 px ずれうる）。

## 決定3: 範囲は「左・上を含み、右・下を含まない」外接矩形

- **Decision**: 換算後の `[Left, Right) × [Top, Bottom)`。角の丸みの外側も含む（spec Assumptions）。
- **Rationale**: 箱は GDI の `RoundRect(Left, Top, Right, Bottom)` で描かれ、GDI は右・下端を含まない（`CRect::PtInRect` と同じ）。
- **Alternatives considered**: 角丸まで判定（`PtInRegion`）。得るものが小さい。

## 決定4: ダブルクリックの受け取り

- **Decision**: メインダイアログに `ON_WM_LBUTTONDBLCLK`。ダイアログクラス `#32770` は `CS_DBLCLKS` を持つので登録は不要（実装時に
  `GetClassLongPtr(GCL_STYLE)` で確認）。`WM_LBUTTONDOWN` は処理しない。
- **Rationale**: OS 標準のダブルクリック判定に従う。

## 決定5: 編集ウィンドウをモードレスにする（US3）

- **Decision**: `CFrameEditDlg` を `new` + `Create(IDD_FRAME_EDIT, mainDlg)` で生成し `ShowWindow(SW_SHOW)`。オーナーはメインダイアログ（常に
  メインの前面に出る）。「閉じる」（`IDCANCEL`）・Esc・✖ と Enter（`IDOK`）は `DestroyWindow()` にし、`PostNcDestroy` でオーナーへ
  「閉じた」ことを `SendMessage(WM_APP + n)` で知らせてから `delete this`。メインダイアログは `CFrameEditDlg* m_frameEditor` を持ち、通知で
  `nullptr` に戻す。
- **Rationale**: 開いたままメインウィンドウの構成図をダブルクリックできる必要がある（FR-009、FR-010）。`.rc` の `DS_MODALFRAME` はモードレスでも
  問題なく、見た目も変わらない。編集は機能 006 と同じくその場で `m_definitions` に反映されるので、どの経路で閉じても内容は失われない（FR-016）。
- **Alternatives considered**: モーダルのまま、メインウィンドウへの入力をフックして中継（複雑でモーダルの前提と矛盾）。2 つ目のウィンドウを
  並べて開く（spec の「同時に 1 つ」に反する）。

## 決定6: 切り替えは「閉じる → 500 ms のワンショットタイマー → 開く」

- **Decision**: ダブルクリックされた ECU が編集ウィンドウの表示中の ECU（`CurrentEcu()` = 選択欄の ECU）と違えば、`CloseFrameEditor()` で
  閉じ、`m_pendingEcu = ecu` にして `SetTimer(kSwitchTimerId, 500)`。タイマーで `KillTimer` → `OpenFrameEditor(m_pendingEcu)` →
  `m_pendingEcu = 0`。待ちの間のダブルクリックは `m_pendingEcu` を上書きするだけでタイマーは延長しない（閉じてから 0.5 秒で開く、FR-012）。
  同じ ECU なら閉じずに `SetForegroundWindow` で前面に出す（FR-011）。
- **Rationale**: `Sleep` で待つと UI が固まる（FR-010「画面が固まらない」）。タイマー ID は再生用（1）と別の 2 を使う。
  待ち時間は定数 1 つ（spec Assumptions: 固定値）。
- **Alternatives considered**: 待ちの間のダブルクリックでタイマーを張り直す（連打すると開かないまま延びる）。

## 決定7: 編集中の「開始」無効化と、読込み・セッティング前の自動クローズ

- **Decision**: 「編集中」= `m_frameEditor != nullptr || m_pendingEcu != 0`。`OnInitMenuPopup` で編集中は「開始」を `MF_GRAYED` にし、
  `OnSimStart` の先頭でも編集中なら何もしない（FR-013）。`OnFileLoadProject`・`OnEditSettings` の先頭で `CloseFrameEditor()`（ウィンドウを
  破棄し、待ちタイマーも止めて `m_pendingEcu = 0`）を呼んでから従来の処理（FR-014）。「データフレーム編集」メニューは、開いていれば前面に
  出すだけ、待ちの間は何もしない（まもなく開くため）、どちらでもなければ ECU 01 で開く（FR-008）。
- **Rationale**: ユーザー回答（編集中はシミュレーション不可、読込み・セッティングは閉じてから）。「開始」は機能 007 と同じくメニュー項目の
  有効／無効で示す。
- **Alternatives considered**: 「開始」で自動的に閉じる（ユーザーが明示的に不可を選んだ）。

## 決定8: 入力画面・削除確認の間はメインウィンドウを無効化

- **Decision**: `CFrameEditDlg` の `OnAdd`・`OnEdit`・`OnDelete` で、入れ子のモーダル（`CFrameEntryDlg::DoModal`、`AfxMessageBox`）の前に
  オーナー（メインダイアログ）を `EnableWindow(FALSE)`、戻った後に `EnableWindow(TRUE)` する（RAII の小さな補助で必ず戻す）。
- **Rationale**: FR-015（その間のダブルクリックは無視）を、無効化されたウィンドウはマウス入力を受けないことで自然に満たす。同時に、入れ子の
  モーダルループ中に「読込み」「セッティング」「✖」で編集ウィンドウが破棄されてクラッシュする経路を塞ぐ。
- **Alternatives considered**: 編集ウィンドウに「処理中」フラグを持たせメインが参照（メニュー・✖ をすべて個別に止める必要があり漏れやすい）。

## 決定10: 編集ウィンドウを構成図の下に開く（FR-017）

- **Decision**: `OpenFrameEditor` で `Create` の後（MFC の中央寄せの後）に、編集ウィンドウの左上を、メインのクライアント座標
  `(ScaleToDpi(150), ScaleToDpi(kDiagramHeight + 8))`（ログ表示エリアの左端、構成図の下端 216 px の 8 px 下）を `ClientToScreen` した位置へ
  `SetWindowPos(SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE)` で動かす。`MonitorFromWindow` の作業領域の下端・右端からはみ出す場合は、収まるよう
  上・左へずらす（ただし作業領域の上端・左端より上・左には出さない）。
- **Rationale**: MFC は位置指定のないダイアログを親の中央に置くため、編集ウィンドウ（430×230 DLU）が構成図の ECU 06〜16 付近とバス線を覆い、
  US3 のダブルクリックができなくなる。構成図の最下部（下段 ECU の箱の下端 186 px）より下に置けば ECU は常に見える。ログ表示エリアは編集中に
  更新されない（編集中は開始できない）ので、覆っても支障がない。
- **Alternatives considered**: メインウィンドウの右側の外（画面幅が狭いと画面外に出る）。最後にユーザーが動かした位置を覚える（最初の 1 回は
  覆ってしまい、覚えた位置が構成図に重なることもある）。

## 決定9: メインウィンドウの終了時の後始末

- **Decision**: メインダイアログの `OnDestroy`（`ON_WM_DESTROY`）で切替タイマーを止め、`m_pendingEcu = 0`、編集ウィンドウが開いていれば
  `DestroyWindow()` する（通知で `m_frameEditor` が `nullptr` になる）。
- **Rationale**: 待ちの間に ✖ で閉じたとき新しいウィンドウを開かない（spec Edge Cases）。オーナーの破棄に伴う自動破棄に頼らず、順序を明示する。
