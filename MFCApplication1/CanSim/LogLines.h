// LogLines.h
// Rows shown in the log view of the main dialog (feature 009): node state
// transitions and the frame log, merged into one list in time order. The
// rows carry no text; the dialog formats only the rows that are visible.

#pragma once

#include "CanTypes.h"
#include <vector>

namespace CanSim
{
    enum class LogLineKind
    {
        State = 0, // a node changed its startup state
        Frame = 1  // one row of the frame log (frame x receiving node)
    };

    struct LogLine
    {
        long long TimeUs = 0;                  // us since the start of the run
        LogLineKind Kind = LogLineKind::State;
        int NodeId = 0;                        // State: the node; Frame: the sender
        int Index = 0;                         // State: new StartupState (as int); Frame: index into the LogRow list
    };

    // States that appear in the log view: Initializing (power on),
    // BusSyncDetecting (init done) and Normal. Transitions later than
    // durationUs or never reached (-1) are left out.
    //
    // Order: time ascending; at the same time state transitions come before
    // frames, then by node id, then in the original order (stable).
    std::vector<LogLine> BuildLogLines(const std::vector<LogRow>& rows, const Network& endState,
        long long durationUs);
}
