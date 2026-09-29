// FrameEvents.h
// Turns the frame log into one event per transmitted frame, used by the
// dialog to replay the bus traffic (feature 004 "frame display").

#pragma once

#include "CanTypes.h"
#include <vector>

namespace CanSim
{
    struct FrameEvent
    {
        long long TxStartUs = 0;
        long long TxEndUs = 0;
        int TxNodeId = 0;
        std::vector<int> RxNodeIds; // nodes that received the frame
        unsigned CanId = 0;
    };

    // LogRow is one row per frame x receiving node. Rows sharing
    // (TxNodeId, TxStartUs) become one FrameEvent; the result is sorted by
    // TxStartUs (then TxNodeId).
    std::vector<FrameEvent> BuildFrameEvents(const std::vector<LogRow>& rows);
}
