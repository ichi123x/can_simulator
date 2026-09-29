// BusDiagramLayout.h
// Pure geometry for the CAN bus diagram drawn on the main dialog
// (feature 003). No dependency on MFC / GDI: the dialog only draws the
// rectangles and lines computed here.
//
// All coordinates are pixels at 96 DPI inside the diagram area
// (kDiagramWidth x kDiagramHeight, origin at the top-left). The caller scales
// them by the current DPI.

#pragma once

#include <vector>

namespace CanSim
{
    const int kDiagramWidth = 1140;
    const int kDiagramHeight = 216;
    const int kMaxEcuCount = 16;

    // Bus line (drawn as a thick horizontal line) and its caption.
    const int kBusLeft = 10;
    const int kBusRight = 1130;
    const int kBusY = 110;
    const int kBusThickness = 4;

    // Caption "CAN BUS" is drawn above the left end of the bus. Its right edge
    // is used to keep the first ECU's stub line clear of the text (FR-011).
    const int kCaptionLeft = 14;
    const int kCaptionRight = 118;

    // ECU boxes.
    const int kEcuBoxWidth = 96;
    const int kEcuBoxHeight = 34;
    const int kEcuStubLength = 40;      // gap between box edge and bus line
    const int kFirstEcuCenterX = 150;   // 140 px margin from the bus left end
    const int kEcuPitch = 56;           // center-to-center distance of neighbours

    struct DiagramRect
    {
        int Left = 0;
        int Top = 0;
        int Right = 0;
        int Bottom = 0;
    };

    struct EcuPlacement
    {
        int Number = 0;     // 1..16
        bool OnTop = true;  // odd numbers above the bus, even numbers below
        DiagramRect Box;
        int LineX = 0;      // x of the stub line (box center)
        int LineY1 = 0;     // box side end of the stub line
        int LineY2 = 0;     // bus side end of the stub line
    };

    struct BusDiagramLayout
    {
        DiagramRect BusLine;             // thickness included
        std::vector<EcuPlacement> Ecus;  // ordered by Number
    };

    // ecuCount is clamped to 0..16. The position of ECU n does not depend on
    // ecuCount (003 FR-009).
    BusDiagramLayout ComputeBusDiagramLayout(int ecuCount);
}
