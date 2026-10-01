// FlowEffect.h
// Flow animation drawn over the frame display (feature 018): light particles
// moving from the sender of each frame, down (or up) its stub line, along the
// bus and up (or down) the stub line of the receiver, plus a slow glow around
// the boxes. A fixed pattern in real time, independent of the speed: the
// particles keep a constant spacing and a constant speed along every path
// (revision 1). 96 DPI content coordinates as in BusDiagramLayout; no
// dependency on MFC.

#pragma once

#include "BusDiagramLayout.h"

#include <map>
#include <utility>
#include <vector>

namespace CanSim
{
    const int kFlowSpeed = 150;                 // px per second of real time
    const int kFlowSpacing = 16;                // px between two particles
    const int kFlowParticleRadius = 4;
    const int kFlowGlowInset = 3;               // glow frame outside the box
    const long long kFlowGlowPeriodUs = 800000; // slow pulse of the glow
    const long long kFlowHoldUs = 300000;       // a path stays this long after it was last seen

    struct FlowPoint
    {
        double X = 0;
        double Y = 0;
    };

    struct FlowGlow
    {
        int Ecu = 0;
        bool Sender = false;
        double Strength = 0; // [0.05, 1]
    };

    struct FlowEffect
    {
        std::vector<FlowPoint> Particles;
        std::vector<FlowGlow> Glows;
    };

    // Path from the box edge of s, its stub line, the bus and the stub line
    // of r to the box edge of r.
    double FlowPathLength(const EcuPlacement& s, const EcuPlacement& r);
    // Point at f (clamped to 0..1) of the path length.
    FlowPoint FlowPointAt(const EcuPlacement& s, const EcuPlacement& r, double f);

    // Distance of the first particle from the start of a path, in
    // [0, kFlowSpacing): elapsedUs * kFlowSpeed / 1e6 modulo the spacing.
    double FlowOffset(long long elapsedUs);

    // Particles at offset + k * kFlowSpacing along the path of every
    // (sender, receiver) pair (a particle shared by several paths of the same
    // sender is listed once), and one glow per sender / receiver ECU
    // (0.5 + 0.5 sin, the receivers half a period later; an ECU that is both
    // glows as a sender). Empty when there is no pair.
    FlowEffect ComputeFlowEffect(const BusDiagramLayout& layout,
        const std::vector<std::pair<int, int>>& flows, long long elapsedUs);

    // Keeps a (sender, receiver) path for kFlowHoldUs after it was last seen,
    // so that the particles do not blink when the frames of one refresh
    // differ from the next (revision 1).
    class FlowPathHold
    {
    public:
        void Update(const std::vector<std::pair<int, int>>& flows, long long nowUs);
        std::vector<std::pair<int, int>> Current(long long nowUs);
        void Clear();

    private:
        std::map<std::pair<int, int>, long long> m_lastSeen;
    };
}
