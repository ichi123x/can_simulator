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
            return a.NodeIndex < b.NodeIndex;
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
