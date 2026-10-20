// FlowEffect.cpp
#include "pch.h"
#include "FlowEffect.h"

#include <cmath>
#include <set>

namespace CanSim
{
    double FlowPathLength(const EcuPlacement& s, const EcuPlacement& r)
    {
        return std::fabs(static_cast<double>(s.LineY1) - kBusY) +
            std::fabs(static_cast<double>(r.LineX) - s.LineX) +
            std::fabs(static_cast<double>(r.LineY1) - kBusY);
    }

    FlowPoint FlowPointAt(const EcuPlacement& s, const EcuPlacement& r, double f)
    {
        if (f < 0.0)
            f = 0.0;
        if (f > 1.0)
            f = 1.0;
        const double down = std::fabs(static_cast<double>(s.LineY1) - kBusY);
        const double along = std::fabs(static_cast<double>(r.LineX) - s.LineX);
        const double up = std::fabs(static_cast<double>(r.LineY1) - kBusY);
        double d = f * (down + along + up);

        FlowPoint p;
        if (d <= down)
        {
            p.X = s.LineX;
            p.Y = s.LineY1 + (kBusY > s.LineY1 ? d : -d);
            return p;
        }
        d -= down;
        if (d <= along)
        {
            p.X = s.LineX + (r.LineX > s.LineX ? d : -d);
            p.Y = kBusY;
            return p;
        }
        d -= along;
        p.X = r.LineX;
        p.Y = kBusY + (r.LineY1 > kBusY ? d : -d);
        return p;
    }

    double FlowOffset(long long elapsedUs)
    {
        if (elapsedUs <= 0)
            return 0.0;
        // Integer arithmetic first so that long runs keep their precision.
        const long long spacingUs = static_cast<long long>(kFlowSpacing) * 1000000LL;
        const long long r = (elapsedUs * kFlowSpeed) % spacingUs;
        return static_cast<double>(r) / 1000000.0;
    }

    namespace
    {
        const double kPi = 3.14159265358979323846;

        const EcuPlacement* FindEcu(const BusDiagramLayout& layout, int number)
        {
            for (const EcuPlacement& e : layout.Ecus)
                if (e.Number == number)
                    return &e;
            return nullptr;
        }

        double GlowStrength(long long elapsedUs, bool sender)
        {
            const double phase = 2.0 * kPi * static_cast<double>(elapsedUs % kFlowGlowPeriodUs) /
                static_cast<double>(kFlowGlowPeriodUs);
            const double s = 0.5 + 0.5 * std::sin(phase + (sender ? 0.0 : kPi));
            return s < 0.05 ? 0.05 : (s > 1.0 ? 1.0 : s);
        }
    }

    FlowEffect ComputeFlowEffect(const BusDiagramLayout& layout,
        const std::vector<std::pair<int, int>>& flows, long long elapsedUs)
    {
        FlowEffect effect;
        if (elapsedUs < 0)
            elapsedUs = 0;
        const double offset = FlowOffset(elapsedUs);
        std::map<int, bool> glowing; // ECU -> sender (a sender wins)
        std::set<std::pair<long long, long long>> seen; // particles already placed (1/8 px)
        for (const auto& flow : flows)
        {
            const EcuPlacement* s = FindEcu(layout, flow.first);
            const EcuPlacement* r = FindEcu(layout, flow.second);
            if (s == nullptr || r == nullptr || flow.first == flow.second)
                continue;
            const double length = FlowPathLength(*s, *r);
            if (length <= 0.0)
                continue;
            for (double d = offset; d <= length; d += kFlowSpacing)
            {
                // Paths from one sender share its stub line and the bus in the
                // same direction, so the same particle comes up many times:
                // draw it once (keeps 16 ECUs fast, SC-005).
                const FlowPoint q = FlowPointAt(*s, *r, d / length);
                const std::pair<long long, long long> key(std::llround(q.X * 8.0), std::llround(q.Y * 8.0));
                if (seen.insert(key).second)
                    effect.Particles.push_back(q);
            }
            glowing[flow.first] = true;
            if (glowing.find(flow.second) == glowing.end())
                glowing[flow.second] = false;
        }
        for (const auto& g : glowing)
            effect.Glows.push_back(FlowGlow{ g.first, g.second, GlowStrength(elapsedUs, g.second) });
        return effect;
    }

    void FlowPathHold::Update(const std::vector<std::pair<int, int>>& flows, long long nowUs)
    {
        for (const auto& flow : flows)
            m_lastSeen[flow] = nowUs;
    }

    std::vector<std::pair<int, int>> FlowPathHold::Current(long long nowUs)
    {
        std::vector<std::pair<int, int>> current;
        for (auto it = m_lastSeen.begin(); it != m_lastSeen.end();)
        {
            if (nowUs - it->second > kFlowHoldUs)
            {
                it = m_lastSeen.erase(it);
                continue;
            }
            current.push_back(it->first);
            ++it;
        }
        return current;
    }

    void FlowPathHold::Clear()
    {
        m_lastSeen.clear();
    }
}
