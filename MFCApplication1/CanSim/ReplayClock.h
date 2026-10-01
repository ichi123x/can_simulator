// ReplayClock.h
// Time base of a running simulation shown on screen (feature 005): the
// simulation time advances with the real elapsed time scaled by 2^speed, and
// every screen refresh highlights the frames that were on the bus since the
// previous refresh. No dependency on MFC / Win32.

#pragma once

#include "FrameEvents.h"
#include <utility>
#include <vector>

namespace CanSim
{
    // elapsedRealUs * 2^speed for speed in kMinSpeed..kInstantSpeed-1.
    // Saturates instead of overflowing. kInstantSpeed means "no waiting" and
    // is handled by the caller (the run completes at once).
    long long SimTimeAtUs(long long elapsedRealUs, int speed);

    // Frames to highlight for one screen refresh.
    struct ActiveFrames
    {
        std::vector<int> Senders;   // ECU numbers, ascending, unique
        std::vector<int> Receivers; // ascending, unique, never also in Senders
        bool BusBusy = false;
        // Feature 018: (sender, receiver) of every frame in the interval,
        // ascending, unique, sender != receiver. Unlike Receivers it keeps an
        // ECU that also sent in the same interval, so that the flow animation
        // has a path even when every ECU sends within one refresh.
        std::vector<std::pair<int, int>> Flows;
    };

    // Walks the replay events (sorted by TxStartUs; frames never overlap on
    // the bus, so TxEndUs is sorted too) and collects the frames whose
    // transmission [TxStartUs, TxEndUs] overlaps (fromUs, toUs].
    // toUs must not decrease between calls.
    class ActiveFrameCursor
    {
    public:
        explicit ActiveFrameCursor(const std::vector<FrameEvent>& events);
        ActiveFrames Collect(long long fromUs, long long toUs);

    private:
        const std::vector<FrameEvent>& m_events;
        std::size_t m_first = 0; // events before this one ended at or before a previous fromUs
    };
}
