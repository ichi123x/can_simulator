// SimControls.h
// Simulation commands Start / Pause / Stop (feature 014): which of them can
// be used in each run phase, the geometry of the toolbar buttons under the
// menu bar, and the simulated time after a resume. No dependency on MFC / GDI;
// the dialog draws the rectangles computed here and uses the same states for
// the menu items and the toolbar buttons.

#pragma once

#include "BusDiagramLayout.h"
#include "CanTypes.h"

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
    const int kSimButtonPitch = 56;
    const int kSimButtonTop = 6;
    const int kSimButtonWidth = 48;
    const int kSimButtonBoxHeight = 36;
    const int kSimButtonCorner = 8;
    const int kSimButtonLabelTop = 42;  // name under the box
    const int kSimButtonBottom = 60;
    const int kSimButtonGlyph = 20;     // symbol drawn in the middle of the box

    // Rectangles of button index at the given DPI (scaled with ScaleToDpi).
    // SimButtonRect is the hit area: the box and the name together.
    DiagramRect SimButtonRect(int index, int dpi);
    DiagramRect SimButtonBoxRect(int index, int dpi);
    DiagramRect SimButtonLabelRect(int index, int dpi);

    // Button (0..2) whose hit area [Left, Right) x [Top, Bottom) contains the
    // client point, or -1.
    int FindSimButtonAt(int x, int y, int dpi);

    // Simulated time after a resume: baseUs (the time at which the run was
    // paused, 0 for a new run) plus SimTimeAtUs(elapsedRealUs, speed).
    // Saturates instead of overflowing.
    long long ResumedSimTimeUs(long long baseUs, long long elapsedRealUs, int speed);

    // ---- feature 015: speed slider right of the buttons (96 DPI) ----
    // Ticks for kMinSpeed..kMaxSpeed, evenly spaced; the thumb is a vertical
    // bar on the tick of the current speed, the values are drawn under it.
    const int kSpeedSliderLeft = 200;   // x of the -13 tick
    const int kSpeedTickPitch = 26;
    const int kSpeedTrackY = 20;
    const int kSpeedTrackThickness = 4;
    const int kSpeedThumbWidth = 10;
    const int kSpeedThumbTop = 7;
    const int kSpeedThumbBottom = 33;
    const int kSpeedTickTop = 28;
    const int kSpeedTickBottom = 33;
    const int kSpeedLabelTop = 38;
    const int kSpeedLabelBottom = 56;
    const int kSpeedHitTop = 4;
    const int kSpeedHitBottom = 60;

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

    // What a change of speed does to the current run (015 FR-007..FR-009).
    enum class SpeedChangeAction
    {
        None,       // only the value changes (Idle; Paused below kInstantSpeed)
        Rebase,     // Running: continue from the current simulated time
        FinishNow,  // Running or Paused, kInstantSpeed: complete at once
    };
    SpeedChangeAction GetSpeedChangeAction(RunPhase phase, int newSpeed);
}
