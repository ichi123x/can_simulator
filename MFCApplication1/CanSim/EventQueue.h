// EventQueue.h
// Deterministic DES event queue (FR-006, FR-007).
// Events are always popped in (TimeNs, Kind priority, NodeId) order so that
// the same configuration always yields the same simulation result.

#pragma once

#include "CanTypes.h"
#include <vector>

namespace CanSim
{
    class EventQueue
    {
    public:
        void Push(const Event& e);

        bool Empty() const;

        // Removes and returns the next event in deterministic order.
        // Precondition: !Empty()
        Event PopNext();

    private:
        std::vector<Event> m_events;
    };
}
