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
        // index is a ToolbarButton; the Start button sits one slot further
        // right, leaving a gap of one button after Save / Load (023 FR-001).
        DiagramRect ScaledButtonPart(int index, int top, int bottom, int dpi)
        {
            const int slot = index < kFileButtonCount ? index : index + 1;
            const int left = kSimButtonLeft + kSimButtonPitch * slot;
            DiagramRect r;
            r.Left = ScaleToDpi(left, dpi);
            r.Top = ScaleToDpi(top, dpi);
            r.Right = ScaleToDpi(left + kSimButtonWidth, dpi);
            r.Bottom = ScaleToDpi(bottom, dpi);
            return r;
        }
    }

    DiagramRect ToolbarButtonRect(int index, int dpi)
    {
        return ScaledButtonPart(index, kSimButtonTop, kSimButtonBottom, dpi);
    }

    DiagramRect ToolbarButtonBoxRect(int index, int dpi)
    {
        return ScaledButtonPart(index, kSimButtonTop, kSimButtonTop + kSimButtonBoxHeight, dpi);
    }

    DiagramRect ToolbarButtonLabelRect(int index, int dpi)
    {
        return ScaledButtonPart(index, kSimButtonLabelTop, kSimButtonBottom, dpi);
    }

    int FindToolbarButtonAt(int x, int y, int dpi)
    {
        for (int i = 0; i < kToolbarButtonCount; ++i)
        {
            const DiagramRect r = ToolbarButtonRect(i, dpi);
            if (x >= r.Left && x < r.Right && y >= r.Top && y < r.Bottom)
                return i;
        }
        return -1;
    }

    bool IsToolbarButtonEnabled(int index, RunPhase phase, bool editingFrames)
    {
        const SimCommandStates s = GetSimCommandStates(phase, editingFrames);
        switch (static_cast<ToolbarButton>(index))
        {
        case ToolbarButton::Save:
        case ToolbarButton::Load:
            return phase == RunPhase::Idle; // 023 FR-006: also while editing frames
        case ToolbarButton::Start:
            return s.Start;
        case ToolbarButton::Pause:
            return s.Pause;
        case ToolbarButton::Stop:
            return s.Stop;
        }
        return false;
    }

    DiagramRect SimButtonRect(int index, int dpi)
    {
        return ToolbarButtonRect(index + kFileButtonCount, dpi);
    }

    DiagramRect SimButtonBoxRect(int index, int dpi)
    {
        return ToolbarButtonBoxRect(index + kFileButtonCount, dpi);
    }

    DiagramRect SimButtonLabelRect(int index, int dpi)
    {
        return ToolbarButtonLabelRect(index + kFileButtonCount, dpi);
    }

    int FindSimButtonAt(int x, int y, int dpi)
    {
        const int i = FindToolbarButtonAt(x, y, dpi);
        return i >= kFileButtonCount ? i - kFileButtonCount : -1;
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

    namespace
    {
        int ClampSpeed(int speed)
        {
            return speed < kMinSpeed ? kMinSpeed : (speed > kMaxSpeed ? kMaxSpeed : speed);
        }
    }

    int SpeedTickX(int speed, int dpi)
    {
        return ScaleToDpi(kSpeedSliderLeft + (ClampSpeed(speed) - kMinSpeed) * kSpeedTickPitch, dpi);
    }

    int SpeedAtX(int x, int dpi)
    {
        int best = kMinSpeed;
        int bestDistance = -1;
        for (int s = kMinSpeed; s <= kMaxSpeed; ++s)
        {
            const int d = x > SpeedTickX(s, dpi) ? x - SpeedTickX(s, dpi) : SpeedTickX(s, dpi) - x;
            if (bestDistance < 0 || d < bestDistance) // ties keep the smaller value
            {
                best = s;
                bestDistance = d;
            }
        }
        return best;
    }

    bool IsOnSpeedSlider(int x, int y, int dpi)
    {
        const int half = kSpeedTickPitch / 2;
        const int left = ScaleToDpi(kSpeedSliderLeft - half, dpi);
        const int right = ScaleToDpi(kSpeedSliderLeft + (kMaxSpeed - kMinSpeed) * kSpeedTickPitch + half, dpi);
        return x >= left && x < right && y >= ScaleToDpi(kSpeedHitTop, dpi) && y < ScaleToDpi(kSpeedHitBottom, dpi);
    }

    DiagramRect SpeedTrackRect(int dpi)
    {
        DiagramRect r;
        r.Left = SpeedTickX(kMinSpeed, dpi);
        r.Right = SpeedTickX(kMaxSpeed, dpi);
        r.Top = ScaleToDpi(kSpeedTrackY - kSpeedTrackThickness / 2, dpi);
        r.Bottom = ScaleToDpi(kSpeedTrackY + kSpeedTrackThickness / 2, dpi);
        return r;
    }

    DiagramRect SpeedThumbRect(int speed, int dpi)
    {
        const int x = SpeedTickX(speed, dpi);
        const int half = ScaleToDpi(kSpeedThumbWidth, dpi) / 2;
        DiagramRect r;
        r.Left = x - half;
        r.Right = x + half;
        r.Top = ScaleToDpi(kSpeedThumbTop, dpi);
        r.Bottom = ScaleToDpi(kSpeedThumbBottom, dpi);
        return r;
    }

    DiagramRect SpeedLabelRect(int speed, int dpi)
    {
        const int x = SpeedTickX(speed, dpi);
        const int half = ScaleToDpi(kSpeedLabelWidth, dpi) / 2;
        DiagramRect r;
        r.Left = x - half;
        r.Right = x + half;
        r.Top = ScaleToDpi(kSpeedLabelTop, dpi);
        r.Bottom = ScaleToDpi(kSpeedLabelBottom, dpi);
        return r;
    }

    DiagramRect SpeedCaptionRect(int dpi)
    {
        DiagramRect r;
        r.Left = ScaleToDpi(kSpeedCaptionLeft, dpi);
        r.Top = ScaleToDpi(kSpeedCaptionTop, dpi);
        r.Right = ScaleToDpi(kSpeedCaptionRight, dpi);
        r.Bottom = ScaleToDpi(kSpeedCaptionBottom, dpi);
        return r;
    }

    bool IsSpeedLabelShown(int speed, int currentSpeed)
    {
        if (speed == currentSpeed)
            return true;
        const bool fifth = speed == kMinSpeed || speed % 5 == 0;
        const int distance = speed > currentSpeed ? speed - currentSpeed : currentSpeed - speed;
        return fifth && distance > 2;
    }

    SpeedChangeAction GetSpeedChangeAction(RunPhase phase, int newSpeed)
    {
        if (phase == RunPhase::Idle)
            return SpeedChangeAction::None;
        if (newSpeed >= kInstantSpeed)
            return SpeedChangeAction::FinishNow;
        return phase == RunPhase::Running ? SpeedChangeAction::Rebase : SpeedChangeAction::None;
    }

    namespace
    {
        DiagramRect ScaledRect(int left, int top, int right, int bottom, int dpi)
        {
            DiagramRect r;
            r.Left = ScaleToDpi(left, dpi);
            r.Top = ScaleToDpi(top, dpi);
            r.Right = ScaleToDpi(right, dpi);
            r.Bottom = ScaleToDpi(bottom, dpi);
            return r;
        }
    }

    DiagramRect FrameDisplayBoxRect(int dpi)
    {
        return ScaledRect(kFrameDisplayBoxLeft, kFrameDisplayBoxTop,
            kFrameDisplayBoxLeft + kFrameDisplayBoxSize, kFrameDisplayBoxTop + kFrameDisplayBoxSize, dpi);
    }

    DiagramRect FrameDisplayLabelRect(int dpi)
    {
        return ScaledRect(kFrameDisplayLabelLeft, kFrameDisplayLabelTop, kFrameDisplayLabelRight, kFrameDisplayLabelBottom, dpi);
    }

    bool IsOnFrameDisplayToggle(int x, int y, int dpi)
    {
        const DiagramRect r = ScaledRect(kFrameDisplayHitLeft, kFrameDisplayHitTop, kFrameDisplayHitRight, kFrameDisplayHitBottom, dpi);
        return x >= r.Left && x < r.Right && y >= r.Top && y < r.Bottom;
    }

    DiagramRect SimTimeCaptionRect(int dpi)
    {
        return ScaledRect(kSimTimeLeft, kSimTimeCaptionTop, kSimTimeRight, kSimTimeCaptionBottom, dpi);
    }

    DiagramRect SimTimeValueRect(int dpi)
    {
        return ScaledRect(kSimTimeLeft, kSimTimeValueTop, kSimTimeValueRight, kSimTimeValueBottom, dpi);
    }

    DiagramRect SimTimeUnitRect(int dpi)
    {
        return ScaledRect(kSimTimeUnitLeft, kSimTimeValueTop, kSimTimeRight, kSimTimeValueBottom, dpi);
    }

    DiagramRect SimTimeAreaRect(int dpi)
    {
        return ScaledRect(kSimTimeLeft, kSimTimeCaptionTop, kSimTimeRight, kSimTimeValueBottom, dpi);
    }

    std::string FormatSimTimeUs(long long us)
    {
        if (us < 0)
            us = 0;
        const std::string digits = std::to_string(us);
        std::string out;
        for (std::size_t i = 0; i < digits.size(); ++i)
        {
            if (i > 0 && (digits.size() - i) % 3 == 0)
                out += ',';
            out += digits[i];
        }
        return out;
    }
}
