// FrameEvents.cpp
#include "pch.h"
#include "FrameEvents.h"

#include <map>
#include <utility>

namespace CanSim
{
    std::vector<FrameEvent> BuildFrameEvents(const std::vector<LogRow>& rows)
    {
        // Keyed by (TxStartUs, TxNodeId) so that the map order is the replay order.
        std::map<std::pair<long long, int>, FrameEvent> byFrame;
        for (const LogRow& row : rows)
        {
            // A transmit request dropped for full buffers never reached the
            // bus (feature 008): nothing to replay.
            if (row.TxStartUs < 0)
                continue;
            const std::pair<long long, int> key(row.TxStartUs, row.TxNodeId);
            auto it = byFrame.find(key);
            if (it == byFrame.end())
            {
                FrameEvent ev;
                ev.TxStartUs = row.TxStartUs;
                ev.TxEndUs = row.TxEndUs;
                ev.TxNodeId = row.TxNodeId;
                ev.CanId = row.CanId;
                it = byFrame.insert(std::make_pair(key, ev)).first;
            }
            if (row.RxNodeId != -1)
                it->second.RxNodeIds.push_back(row.RxNodeId);
        }

        std::vector<FrameEvent> events;
        events.reserve(byFrame.size());
        for (auto& entry : byFrame)
            events.push_back(entry.second);
        return events;
    }
}
