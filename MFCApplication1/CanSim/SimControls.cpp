// SimControls.cpp
#include "pch.h"
#include "SimControls.h"
#include "ReplayClock.h"

#include <climits>

namespace CanSim
{
    SimCommandStates GetSimCommandStates(RunPhase phase, bool editingFrames)
    {
        SimCommandStates s;
        switch (phase)
        {
        case RunPhase::Idle:
            s.Start = !editingFrames;
            break;
        case RunPhase::Running:
            s.Pause = true;
            s.Stop = true;
            break;
        case RunPhase::Paused:
            s.Start = true; // resume
            s.Stop = true;
            break;
        }
        return s;
    }

    namespace
    {
        DiagramRect ScaledButtonPart(int index, int top, int bottom, int dpi)
        {
            const int left = kSimButtonLeft + kSimButtonPitch * index;
            DiagramRect r;
            r.Left = ScaleToDpi(left, dpi);
            r.Top = ScaleToDpi(top, dpi);
            r.Right = ScaleToDpi(left + kSimButtonWidth, dpi);
            r.Bottom = ScaleToDpi(bottom, dpi);
            return r;
        }
    }

    DiagramRect SimButtonRect(int index, int dpi)
    {
        return ScaledButtonPart(index, kSimButtonTop, kSimButtonBottom, dpi);
    }

    DiagramRect SimButtonBoxRect(int index, int dpi)
    {
        return ScaledButtonPart(index, kSimButtonTop, kSimButtonTop + kSimButtonBoxHeight, dpi);
    }

    DiagramRect SimButtonLabelRect(int index, int dpi)
    {
        return ScaledButtonPart(index, kSimButtonLabelTop, kSimButtonBottom, dpi);
    }

    int FindSimButtonAt(int x, int y, int dpi)
    {
        for (int i = 0; i < kSimButtonCount; ++i)
        {
            const DiagramRect r = SimButtonRect(i, dpi);
            if (x >= r.Left && x < r.Right && y >= r.Top && y < r.Bottom)
                return i;
        }
        return -1;
    }

    long long ResumedSimTimeUs(long long baseUs, long long elapsedRealUs, int speed)
    {
        if (baseUs < 0)
            baseUs = 0;
        const long long advance = SimTimeAtUs(elapsedRealUs, speed);
        if (advance > LLONG_MAX - baseUs)
            return LLONG_MAX;
        return baseUs + advance;
    }
}
