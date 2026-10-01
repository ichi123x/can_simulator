// SimControls.h
// Simulation commands Start / Pause / Stop (feature 014): which of them can
// be used in each run phase, the geometry of the toolbar buttons under the
// menu bar, and the simulated time after a resume. No dependency on MFC / GDI;
// the dialog draws the rectangles computed here and uses the same states for
// the menu items and the toolbar buttons.

#pragma once

#include "BusDiagramLayout.h"
#include "CanTypes.h"

#include <string>

namespace CanSim
{
    // Idle: no run. Running: the replay clock advances. Paused: a run is in
    // progress but its clock is stopped.
    enum class RunPhase { Idle, Running, Paused };

    struct SimCommandStates
    {
        bool Start = false; // starts a new run (Idle) or resumes (Paused)
        bool Pause = false;
        bool Stop = false;
    };

    // 014 FR-004. editingFrames (the frame editor is open or about to reopen,
    // feature 010) only matters while Idle.
    SimCommandStates GetSimCommandStates(RunPhase phase, bool editingFrames);

    // Toolbar under the menu bar, 96 DPI pixels in client coordinates. The
    // diagram and the log view are moved down by kToolbarHeight.
    const int kToolbarHeight = 64;
    const int kSimButtonCount = 3;      // 0 = Start, 1 = Pause, 2 = Stop
    const int kSimButtonLeft = 8;
    const int kSimButtonPitch = 28;
    const int kSimButtonTop = 14;
    const int kSimButtonWidth = 24;
    const int kSimButtonBoxHeight = 18;
    const int kSimButtonCorner = 4;
    const int kSimButtonLabelTop = 33;  // name under the box
    const int kSimButtonBottom = 49;
    const int kSimButtonGlyph = 10;     // symbol drawn in the middle of the box

    // ---- feature 023: Save / Load buttons left of Start ----
    // All toolbar buttons numbered left to right. Button i is at slot i for
    // Save / Load and slot i + 1 for Start..Stop (one free slot between), its
    // left edge at kSimButtonLeft + kSimButtonPitch * slot; all have the same size.
    enum class ToolbarButton { Save, Load, Start, Pause, Stop };
    const int kToolbarButtonCount = 5;
    const int kFileButtonCount = 2;     // Save and Load

    // Rectangles of toolbar button index (ToolbarButton) at the given DPI
    // (scaled with ScaleToDpi). ToolbarButtonRect is the hit area: the box
    // and the name together.
    DiagramRect ToolbarButtonRect(int index, int dpi);
    DiagramRect ToolbarButtonBoxRect(int index, int dpi);
    DiagramRect ToolbarButtonLabelRect(int index, int dpi);

    // Toolbar button (0..4) whose hit area [Left, Right) x [Top, Bottom)
    // contains the client point, or -1 (also on the free slot).
    int FindToolbarButtonAt(int x, int y, int dpi);

    // Save / Load only while Idle (also while editing frames, 023 FR-006);
    // Start / Pause / Stop as GetSimCommandStates.
    bool IsToolbarButtonEnabled(int index, RunPhase phase, bool editingFrames);

    // Start / Pause / Stop as index 0..2 (feature 014): the same rectangles
    // as ToolbarButton index + kFileButtonCount.
    DiagramRect SimButtonRect(int index, int dpi);
    DiagramRect SimButtonBoxRect(int index, int dpi);
    DiagramRect SimButtonLabelRect(int index, int dpi);

    // Start / Pause / Stop (0..2) under the client point, or -1.
    int FindSimButtonAt(int x, int y, int dpi);

    // Simulated time after a resume: baseUs (the time at which the run was
    // paused, 0 for a new run) plus SimTimeAtUs(elapsedRealUs, speed).
    // Saturates instead of overflowing.
    long long ResumedSimTimeUs(long long baseUs, long long elapsedRealUs, int speed);

    // ---- feature 015: speed slider right of the buttons (96 DPI) ----
    // Ticks for kMinSpeed..kMaxSpeed, evenly spaced; the thumb is a vertical
    // bar on the tick of the current speed, the values are drawn under it.
    const int kSpeedSliderLeft = 604;   // x of the -13 tick (019: right end, after the simulated time; 023: +84)
    const int kSpeedTickPitch = 13;
    const int kSpeedTrackY = 20;
    const int kSpeedTrackThickness = 2;
    const int kSpeedThumbWidth = 5;
    const int kSpeedThumbTop = 13;
    const int kSpeedThumbBottom = 27;
    const int kSpeedTickTop = 24;
    const int kSpeedTickBottom = 27;
    const int kSpeedLabelTop = 32;
    const int kSpeedLabelBottom = 48;
    const int kSpeedHitTop = 4;
    const int kSpeedHitBottom = 60;
    // 019: value labels are this wide; only every 5th value and the current
    // one are shown, so they never overlap although the ticks are 13 apart.
    const int kSpeedLabelWidth = 26;
    // 019: "Speed:" caption left of the slider.
    const int kSpeedCaptionLeft = 514;
    const int kSpeedCaptionRight = 584;
    const int kSpeedCaptionTop = 10;
    const int kSpeedCaptionBottom = 30;

    // x of the tick of speed (clamped to kMinSpeed..kMaxSpeed) at the DPI.
    int SpeedTickX(int speed, int dpi);

    // Speed whose tick is nearest to x (the smaller one on a tie); outside
    // the slider the end value.
    int SpeedAtX(int x, int dpi);

    // Area where a press starts moving the slider: half a pitch beyond the
    // end ticks, from kSpeedHitTop to kSpeedHitBottom.
    bool IsOnSpeedSlider(int x, int y, int dpi);

    DiagramRect SpeedTrackRect(int dpi);
    DiagramRect SpeedThumbRect(int speed, int dpi);
    DiagramRect SpeedLabelRect(int speed, int dpi);
    DiagramRect SpeedCaptionRect(int dpi);

    // 019 FR-004: value label shown for speed when the current speed is
    // currentSpeed: the current value, and -13 / -10 / -5 / 0 / +5 / +10 unless
    // they are within 2 ticks of the current value.
    bool IsSpeedLabelShown(int speed, int currentSpeed);

    // What a change of speed does to the current run (015 FR-007..FR-009).
    enum class SpeedChangeAction
    {
        None,       // only the value changes (Idle; Paused below kInstantSpeed)
        Rebase,     // Running: continue from the current simulated time
        FinishNow,  // Running or Paused, kInstantSpeed: complete at once
    };
    SpeedChangeAction GetSpeedChangeAction(RunPhase phase, int newSpeed);

    // ---- feature 017: frame display check box and the simulated time (96 DPI) ----
    // Check box between the Stop button and the speed slider.
    const int kFrameDisplayBoxLeft = 188;
    const int kFrameDisplayBoxTop = 25;
    const int kFrameDisplayBoxSize = 14;
    const int kFrameDisplayLabelLeft = 208;
    const int kFrameDisplayLabelRight = 292;
    const int kFrameDisplayLabelTop = 22;
    const int kFrameDisplayLabelBottom = 42;
    const int kFrameDisplayHitLeft = 184;
    const int kFrameDisplayHitRight = 296;
    const int kFrameDisplayHitTop = 4;
    const int kFrameDisplayHitBottom = 60;

    // Simulated time right of the check box: caption above, value and unit below.
    const int kSimTimeLeft = 314;
    const int kSimTimeRight = 494;
    const int kSimTimeCaptionTop = 8;
    const int kSimTimeCaptionBottom = 28;
    const int kSimTimeValueTop = 30;
    const int kSimTimeValueBottom = 54;
    const int kSimTimeValueRight = 464;
    const int kSimTimeUnitLeft = 468;

    DiagramRect FrameDisplayBoxRect(int dpi);
    DiagramRect FrameDisplayLabelRect(int dpi);

    // Area where a press toggles the frame display: box and label together.
    bool IsOnFrameDisplayToggle(int x, int y, int dpi);

    DiagramRect SimTimeCaptionRect(int dpi);
    DiagramRect SimTimeValueRect(int dpi);
    DiagramRect SimTimeUnitRect(int dpi);
    DiagramRect SimTimeAreaRect(int dpi);   // caption, value and unit together

    // "600,000,000" style: digits grouped by three; a negative value is "0".
    std::string FormatSimTimeUs(long long us);
}
