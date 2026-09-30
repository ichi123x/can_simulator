# Data Model: CAN バス構成図の表示

永続データなし。表示用の値型のみ（`CanSim/BusDiagramLayout.h`）。

## DiagramRect
| 項目 | 型 | 内容 |
|---|---|---|
| Left, Top, Right, Bottom | int | 図領域内ピクセル座標 |

## EcuPlacement
| 項目 | 型 | 内容 |
|---|---|---|
| Number | int | 1〜16 |
| OnTop | bool | 奇数=true（バス線の上）、偶数=false（下） |
| Box | DiagramRect | ECU 箱 |
| LineX | int | 接続線の x（箱の中心） |
| LineY1, LineY2 | int | 接続線の両端 y（箱側／バス線側） |

## BusDiagramLayout（関数）
- `ComputeBusDiagramLayout(int ecuCount)` → `{ BusLine: DiagramRect, Ecus: vector<EcuPlacement> }`
- ecuCount は 0〜16 にクランプする（範囲外は 0 未満→0、17 以上→16）。
- 不変条件: 全 ecuCount で ECU n の座標は同一（FR-009）。奇数は箱が bus より上、偶数は下。
  Ecus.size() == clamp(ecuCount)。全 Box が図領域（1140×216）内。

## ダイアログの状態
- `m_ecuCount`（int, 初期 2）: 表示中の ECU 数。設定元は機能 004。
