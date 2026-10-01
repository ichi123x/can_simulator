// FlowEffect.h
// Flow animation drawn over the frame display (feature 018): light particles
// moving from each highlighted sender, down (or up) its stub line, along the
// bus and up (or down) the stub line of each highlighted receiver, plus a
// glow around the boxes when a particle leaves or arrives. A fixed pattern
// repeating every kFlowPeriodUs of real time, independent of the speed.
// 96 DPI content coordinates as in BusDiagramLayout; no dependency on MFC.

#pragma once

#include "BusDiagramLayout.h"
#include "ReplayClock.h"

#include <vector>

namespace CanSim
{
    const long long kFlowPeriodUs = 600000; // one round, real time
    const int kFlowParticles = 3;           // per path
    const int kFlowParticleRadius = 4;
    const int kFlowGlowInset = 3;           // glow frame outside the box
    const double kFlowGlowWindow = 0.15;    // part of the round counted as "just left" / "about to arrive"

    // (elapsedUs mod kFlowPeriodUs) / kFlowPeriodUs, 0 for elapsedUs <= 0.
    double FlowPhase(long long elapsedUs);

    struct FlowPoint
    {
        double X = 0;
        double Y = 0;
    };

    struct FlowGlow
    {
        int Ecu = 0;
        bool Sender = false;
        double Strength = 0; // (0, 1]
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

    // Particles for every (sender, receiver) of the frames in the interval
    // (ActiveFrames::Flows) and the glows. Empty when the bus is idle or there
    // is no such pair.
    FlowEffect ComputeFlowEffect(const BusDiagramLayout& layout, const ActiveFrames& active, double phase);
}
