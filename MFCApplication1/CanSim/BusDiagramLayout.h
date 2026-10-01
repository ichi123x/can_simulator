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
    const int kEcuBoxHeight = 48;   // 022: room for the ECU label under "ECU nn" (was 34)
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

    // Border drawn around the diagram, this far inside the diagram area
    // (feature 011). The diagram sits at the client origin, so there is no
    // room for it outside.
    const int kDiagramFrameInset = 4;

    struct BusDiagramLayout
    {
        DiagramRect BusLine;             // thickness included
        std::vector<EcuPlacement> Ecus;  // ordered by Number
        DiagramRect Frame;               // border line, same for any ECU count
    };

    // ecuCount is clamped to 0..16. The position of ECU n does not depend on
    // ecuCount (003 FR-009).
    BusDiagramLayout ComputeBusDiagramLayout(int ecuCount);

    // Converts a 96 DPI pixel value to the given DPI: (px * dpi + 48) / 96,
    // the same rounding as MulDiv(px, dpi, 96) for px >= 0. Used both to draw
    // the diagram and to hit-test it, so that the two always agree (feature 010).
    int ScaleToDpi(int px, int dpi);

    // ECU number (1..16) whose box contains the client point (x, y) at the
    // given DPI, or 0 when the point is on no box. A box covers
    // [Left, Right) x [Top, Bottom) after scaling, like GDI's RoundRect
    // (feature 010, research.md decisions 1 and 3).
    int FindEcuAt(const BusDiagramLayout& layout, int x, int y, int dpi);

    // Log view under the diagram (feature 009): grey area plus scroll bar, in
    // 96 DPI client pixels, and its border this far outside (feature 011).
    const int kLogViewLeft = 150;
    const int kLogViewTop = 260;
    const int kLogViewWidth = 850;
    const int kLogViewHeight = 260;
    const int kLogFrameGap = 3;

    // The log view and its border in client pixels at the given DPI. The
    // right / bottom edges are left / top plus the scaled size, as the dialog
    // has always placed them.
    DiagramRect ScaleLogViewRect(int dpi);
    DiagramRect ScaleLogFrameRect(int dpi);

    // Feature 020: label "Event & log view" just above the log view border,
    // at its left edge, 96 DPI content coordinates.
    const int kLogLabelTop = 236;
    const int kLogLabelRight = 300;
    const int kLogLabelBottom = 254;

    // At the given DPI, left edge taken from ScaleLogFrameRect so that the
    // label and the border always line up.
    DiagramRect ScaleLogLabelRect(int dpi);
}
