// BusDiagramLayout.cpp
#include "pch.h"
#include "BusDiagramLayout.h"

namespace CanSim
{
    BusDiagramLayout ComputeBusDiagramLayout(int ecuCount)
    {
        if (ecuCount < 0)
            ecuCount = 0;
        if (ecuCount > kMaxEcuCount)
            ecuCount = kMaxEcuCount;

        BusDiagramLayout layout;
        layout.BusLine.Left = kBusLeft;
        layout.BusLine.Right = kBusRight;
        layout.BusLine.Top = kBusY - kBusThickness / 2;
        layout.BusLine.Bottom = kBusY + kBusThickness / 2;

        layout.Frame.Left = kDiagramFrameInset;
        layout.Frame.Top = kDiagramFrameInset;
        layout.Frame.Right = kDiagramWidth - kDiagramFrameInset;
        layout.Frame.Bottom = kDiagramHeight - kDiagramFrameInset;

        layout.Ecus.reserve(static_cast<std::size_t>(ecuCount));
        for (int n = 1; n <= ecuCount; ++n)
        {
            EcuPlacement ecu;
            ecu.Number = n;
            ecu.OnTop = (n % 2) == 1;

            const int centerX = kFirstEcuCenterX + (n - 1) * kEcuPitch;
            ecu.Box.Left = centerX - kEcuBoxWidth / 2;
            ecu.Box.Right = centerX + kEcuBoxWidth / 2;
            ecu.LineX = centerX;

            if (ecu.OnTop)
            {
                ecu.LineY2 = layout.BusLine.Top;
                ecu.LineY1 = ecu.LineY2 - kEcuStubLength;
                ecu.Box.Bottom = ecu.LineY1;
                ecu.Box.Top = ecu.Box.Bottom - kEcuBoxHeight;
            }
            else
            {
                ecu.LineY2 = layout.BusLine.Bottom;
                ecu.LineY1 = ecu.LineY2 + kEcuStubLength;
                ecu.Box.Top = ecu.LineY1;
                ecu.Box.Bottom = ecu.Box.Top + kEcuBoxHeight;
            }
            layout.Ecus.push_back(ecu);
        }
        return layout;
    }

    int ScaleToDpi(int px, int dpi)
    {
        return static_cast<int>((static_cast<long long>(px) * dpi + 48) / 96);
    }

    int FindEcuAt(const BusDiagramLayout& layout, int x, int y, int dpi)
    {
        for (const EcuPlacement& ecu : layout.Ecus)
        {
            if (x >= ScaleToDpi(ecu.Box.Left, dpi) && x < ScaleToDpi(ecu.Box.Right, dpi) &&
                y >= ScaleToDpi(ecu.Box.Top, dpi) && y < ScaleToDpi(ecu.Box.Bottom, dpi))
                return ecu.Number;
        }
        return 0;
    }

    DiagramRect ScaleLogViewRect(int dpi)
    {
        DiagramRect r;
        r.Left = ScaleToDpi(kLogViewLeft, dpi);
        r.Top = ScaleToDpi(kLogViewTop, dpi);
        r.Right = r.Left + ScaleToDpi(kLogViewWidth, dpi);
        r.Bottom = r.Top + ScaleToDpi(kLogViewHeight, dpi);
        return r;
    }

    DiagramRect ScaleLogFrameRect(int dpi)
    {
        DiagramRect r = ScaleLogViewRect(dpi);
        const int gap = ScaleToDpi(kLogFrameGap, dpi);
        r.Left -= gap;
        r.Top -= gap;
        r.Right += gap;
        r.Bottom += gap;
        return r;
    }

    DiagramRect ScaleLogLabelRect(int dpi)
    {
        const DiagramRect frame = ScaleLogFrameRect(dpi);
        DiagramRect r;
        r.Left = frame.Left;
        r.Top = ScaleToDpi(kLogLabelTop, dpi);
        r.Right = ScaleToDpi(kLogLabelRight, dpi);
        r.Bottom = ScaleToDpi(kLogLabelBottom, dpi);
        if (r.Bottom > frame.Top)
            r.Bottom = frame.Top;
        return r;
    }
}
