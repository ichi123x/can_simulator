// FlowEffect.cpp
#include "pch.h"
#include "FlowEffect.h"

#include <cmath>
#include <map>

namespace CanSim
{
    double FlowPhase(long long elapsedUs)
    {
        if (elapsedUs <= 0)
            return 0.0;
        return static_cast<double>(elapsedUs % kFlowPeriodUs) / static_cast<double>(kFlowPeriodUs);
    }

    double FlowPathLength(const EcuPlacement& s, const EcuPlacement& r)
    {
        return std::fabs(static_cast<double>(s.LineY1 - kBusY)) +
            std::fabs(static_cast<double>(r.LineX - s.LineX)) +
            std::fabs(static_cast<double>(r.LineY1 - kBusY));
    }

    FlowPoint FlowPointAt(const EcuPlacement& s, const EcuPlacement& r, double f)
    {
        if (f < 0.0)
            f = 0.0;
        if (f > 1.0)
            f = 1.0;
        const double down = std::fabs(static_cast<double>(s.LineY1 - kBusY));
        const double along = std::fabs(static_cast<double>(r.LineX - s.LineX));
        const double up = std::fabs(static_cast<double>(r.LineY1 - kBusY));
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

    namespace
    {
        const EcuPlacement* FindEcu(const BusDiagramLayout& layout, int number)
        {
            for (const EcuPlacement& e : layout.Ecus)
                if (e.Number == number)
                    return &e;
            return nullptr;
        }

        void KeepMax(std::map<int, double>& strengths, int ecu, double value)
        {
            if (value <= 0.0)
                return;
            double& s = strengths[ecu];
            if (value > s)
                s = value > 1.0 ? 1.0 : value;
        }
    }

    FlowEffect ComputeFlowEffect(const BusDiagramLayout& layout, const ActiveFrames& active, double phase)
    {
        FlowEffect effect;
        if (!active.BusBusy || active.Flows.empty())
            return effect;

        std::map<int, double> senderGlow;
        std::map<int, double> receiverGlow;
        for (const auto& flow : active.Flows)
        {
            const int sender = flow.first;
            const int receiver = flow.second;
            const EcuPlacement* s = FindEcu(layout, sender);
            const EcuPlacement* r = FindEcu(layout, receiver);
            if (s == nullptr || r == nullptr || sender == receiver)
                continue;
            for (int k = 0; k < kFlowParticles; ++k)
            {
                double f = phase + static_cast<double>(k) / kFlowParticles;
                f -= std::floor(f);
                effect.Particles.push_back(FlowPointAt(*s, *r, f));
                if (f < kFlowGlowWindow)
                    KeepMax(senderGlow, sender, 1.0 - f / kFlowGlowWindow);
                if (f > 1.0 - kFlowGlowWindow)
                    KeepMax(receiverGlow, receiver, (f - (1.0 - kFlowGlowWindow)) / kFlowGlowWindow);
            }
        }
        for (const auto& g : senderGlow)
            effect.Glows.push_back(FlowGlow{ g.first, true, g.second });
        for (const auto& g : receiverGlow)
            effect.Glows.push_back(FlowGlow{ g.first, false, g.second });
        return effect;
    }
}
