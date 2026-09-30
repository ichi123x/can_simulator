// EventQueue.cpp
#include "pch.h"
#include "EventQueue.h"
#include <algorithm>

namespace CanSim
{
    namespace
    {
        // Lower key sorts first == popped first.
        bool IsBefore(const Event& a, const Event& b)
        {
            if (a.TimeNs != b.TimeNs)
                return a.TimeNs < b.TimeNs;
            if (a.Kind != b.Kind)
                return static_cast<int>(a.Kind) < static_cast<int>(b.Kind);
            if (a.NodeIndex != b.NodeIndex)
                return a.NodeIndex < b.NodeIndex;
            // Feature 008: same-time transmit requests of one node are handled
            // in CAN ID order, so that with full transmit buffers the requests
            // with the largest IDs are the ones dropped (domain spec FR-092c).
            return a.CanId < b.CanId;
        }
    }

    void EventQueue::Push(const Event& e)
    {
        // Insert keeping the vector sorted (ascending) so PopNext is O(1).
        auto it = std::upper_bound(m_events.begin(), m_events.end(), e, IsBefore);
        m_events.insert(it, e);
    }

    bool EventQueue::Empty() const
    {
        return m_events.empty();
    }

    Event EventQueue::PopNext()
    {
        Event e = m_events.front();
        m_events.erase(m_events.begin());
        return e;
    }
}
