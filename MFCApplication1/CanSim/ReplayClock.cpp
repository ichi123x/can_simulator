// ReplayClock.cpp
#include "pch.h"
#include "ReplayClock.h"

#include <algorithm>
#include <climits>
#include <iterator>

namespace CanSim
{
    long long SimTimeAtUs(long long elapsedRealUs, int speed)
    {
        if (elapsedRealUs <= 0)
            return 0;
        if (speed >= 0)
        {
            if (speed >= 62 || elapsedRealUs > (LLONG_MAX >> speed))
                return LLONG_MAX;
            return elapsedRealUs << speed;
        }
        const int shift = -speed;
        return shift >= 63 ? 0 : (elapsedRealUs >> shift);
    }

    ActiveFrameCursor::ActiveFrameCursor(const std::vector<FrameEvent>& events)
        : m_events(events)
    {
    }

    ActiveFrames ActiveFrameCursor::Collect(long long fromUs, long long toUs)
    {
        ActiveFrames active;

        // Frames that ended at or before fromUs can no longer overlap any
        // later interval.
        while (m_first < m_events.size() && m_events[m_first].TxEndUs <= fromUs)
            ++m_first;

        for (std::size_t i = m_first; i < m_events.size(); ++i)
        {
            const FrameEvent& ev = m_events[i];
            if (ev.TxStartUs > toUs)
                break;
            if (ev.TxEndUs <= fromUs)
                continue;
            active.BusBusy = true;
            active.Senders.push_back(ev.TxNodeId);
            active.Receivers.insert(active.Receivers.end(), ev.RxNodeIds.begin(), ev.RxNodeIds.end());
        }

        std::sort(active.Senders.begin(), active.Senders.end());
        active.Senders.erase(std::unique(active.Senders.begin(), active.Senders.end()), active.Senders.end());
        std::sort(active.Receivers.begin(), active.Receivers.end());
        active.Receivers.erase(std::unique(active.Receivers.begin(), active.Receivers.end()), active.Receivers.end());
        // A sender is shown as a sender even if it also received something
        // in the same interval (005 spec US4 AS3).
        std::vector<int> receiversOnly;
        std::set_difference(active.Receivers.begin(), active.Receivers.end(),
            active.Senders.begin(), active.Senders.end(), std::back_inserter(receiversOnly));
        active.Receivers.swap(receiversOnly);
        return active;
    }
}
