# Quickstart: スタートアップ制御の検証

`spec.md` の User Story 1〜3 と SC-001〜SC-006 を確認するための手順。機能 001 と同様、専用の自動テスト基盤は導入しないため、実装後はここに記載した手順で確認する。

## 前提

- `MFCApplication1.sln` を Debug / x64（または Win32(x86)）でビルド済みであること。
- 本機能で追加する2つのサンプル構成が存在すること（いずれも 500 kbps ＝ 1ビット時間 2 µs、11ビット時間 = 22 µs）。
  - `CanSim/SampleConfigs/sample_startup.cfg`: 6ノードの `PowerOnTimeUs` を0 / 10000 / 20000 / 30000 / 40000 / 50000 とし、各ノードに周期 100 ms・初回オフセット 0 のフレームを1つ持たせた構成（バスが空いている時間が長く、同期成立時刻を手計算しやすい）。
  - `CanSim/SampleConfigs/sample_startup_latejoin.cfg`: ノードAが短周期（例: 300 µs）で送信し続け、ノードBの `PowerOnTimeUs` をノードAのフレーム送信中（例: 100 µs）に設定した構成。

## 手順

1. **US1 / SC-001 / SC-003: 起動シーケンスと通常動作開始時刻**
   - `sample_startup.cfg` を読み込み、1秒間実行する。
   - 出力された `sample_startup.cfg.startup.csv` を開く（列定義は [contracts/startup-log-format.md](./contracts/startup-log-format.md)）。
   - **期待結果**: 6ノードすべてが `Reached_Normal=TRUE` で、`Normal_Entry_us` が`Power_On_us + Init_Duration + 22` と一致する（22 / 10022 / 20022 / 30022 / 40022 / 50022 µs）。

2. **US1 AS#3 / FR-013: 通常動作に到達しないノード**
   - `sample_startup.cfg` の任意の1ノードの `PowerOnTimeUs` をシミュレーション時間より後（例: 2000000）に変更して、1秒間実行する。
   - **期待結果**: そのノードの行は `Reached_Normal=FALSE`、`Init_Done_us` と `Normal_Entry_us` は空欄。フレームログにそのノードを送信元とする行が1件も存在しない。

3. **US2 / SC-002: 起動前の送信要求の保留**
   - 手順1のフレームログ `sample_startup.cfg.log.csv` を開く。
   - **期待結果**: 各行の `Tx_Start` が、その `Tx_Node` の `Normal_Entry_us` 以上になっている（通常動作前の送信が0件）。電源投入を遅らせたノードの最初の行は `Tx_Request=0`（最初に要求が発生した時刻）のまま残り、`Tx_Start − Tx_Request` に起動待ち時間が含まれる。

4. **US3 / SC-004: 送信中のバスへの後発参加**
   - `sample_startup_latejoin.cfg` を読み込み、10 ms 程度実行する。
   - `startup.csv` のノードBの `Normal_Entry_us` と、`log.csv` でノードBの同期検出開始時刻（= `Power_On_us + Init_Duration`）を含んでいたノードAのフレームの `Tx_End` を比較する。
   - **期待結果**: ノードBの `Normal_Entry_us` = そのフレームの `Tx_End + 6 µs`（＝フレーム間休止期間3ビット分の終了時刻）。例: ノードAが 22〜274 µs で送信していれば、ノードBの通常動作開始は 280 µs。

5. **SC-005: 再現性**
   - 手順1と同じ構成・同じ時間で再実行する。
   - **期待結果**: `startup.csv` と `log.csv` の両方が1バイト単位で完全に一致する。

6. **SC-006: 機能 001 との後方互換**
   - 機能 001 のサンプル `sample_appendix_a.cfg`（電源投入・初期化時間はすべて既定の 0）を10 秒間実行する。
   - 本機能の実装前に同じ構成で得られていたログと比較する。
   - **期待結果**:
     - 行数・行の順序・`CAN_ID`・`DLC`・`Tx_Node` / `Rx_Node`・`Status`・`Retry_Count`・`Overwrite_Count` は**まったく変化しない**。
     - `Tx_Request` も変化しない（送信要求は起動状態と無関係に周期どおり発生するため）。
     - その他の時刻列（`Timestamp_us` `Tx_Start` `Tx_End` `Rx_Start` `Rx_End` `Rx_Process`）の差分は **0 または +22 µs のみ**。起動直後にバスが連続して埋まっている区間のフレームは+22 µs ずれ、バスが一度空いて以降のフレームは送信要求時刻どおりに戻るため差分 0 になる。参考値（実測）: 6ノード・10秒の実行では 16750 行中、先頭の 40 行だけが +22 µs、残り 16710 行は差分 0。`0x080` の初回 `Tx_Start` は 140 µs → 162 µs。

## 補足: MFC を介さない検証（推奨）

`CanSim/` は MFC に依存しないプレーン C++ のままなので、GUI を操作せずに手順1〜6 に相当する検証を一括実行できる。そのためのツールが `tools/cansim_runner/` にある。

```bat
set OUT=%TEMP%\cansim_tools
mkdir "%OUT%" 2>nul
cl /nologo /std:c++14 /EHsc /I tools\cansim_runner /I MFCApplication1\CanSim ^
   /Fo"%OUT%\\" /Fe"%OUT%\checks.exe" ^
   tools\cansim_runner\checks.cpp MFCApplication1\CanSim\*.cpp
"%OUT%\checks.exe" MFCApplication1\CanSim\SampleConfigs "%OUT%"
```

`checks.exe` は本機能の SC-001〜SC-005 と FR-007 / FR-008 / FR-009 / FR-010 / FR-011 / FR-013、および機能 001 の成功基準をアサーションとして検証し、全パスで終了コード 0 を返す。単発の実行結果（CSV）を目視したい場合は同ディレクトリの `runner.cpp` を使う。詳細は[tools/cansim_runner/README.md](../../tools/cansim_runner/README.md) を参照。

## 参考

- 起動記録の列定義: [contracts/startup-log-format.md](./contracts/startup-log-format.md)
- 設定ファイルの追加キー: [contracts/config-input-format-delta.md](./contracts/config-input-format-delta.md)
- エンティティ・状態遷移: [data-model.md](./data-model.md)
- 同期成立時刻の算出根拠: [research.md](./research.md)
