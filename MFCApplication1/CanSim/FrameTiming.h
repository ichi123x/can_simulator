// FrameTiming.h
// Computes the actual on-wire transmission time of a CAN 2.0A standard data
// frame, including real (data-dependent) bit stuffing (FR-008, FR-009,
// FR-010). See contracts/ and the domain spec doc FR-020/021/023.
//
// Bit stuffing (feature 031, Bosch CAN 2.0A section 5): over SOF .. the end of
// the CRC sequence, a bit of the opposite level is inserted after every 5
// consecutive bits of the same level. The inserted stuff bit is counted as the
// first bit of the next run. The CRC delimiter, ACK slot, ACK delimiter and EOF
// (10 bits) are not stuffed. TotalBits = 34 + 8 * dlc + StuffBitCount + 10.

#pragma once

#include "CanTypes.h"
#include <cstdint>
#include <vector>

namespace CanSim
{
    struct FrameTimingResult
    {
        int TotalBits = 0;         // SOF..EOF, after stuffing
        int StuffBitCount = 0;     // number of stuff bits inserted
        long long BitTimeNs = 0;   // 1e9 / bit rate
        long long TxDurationNs = 0;    // TotalBits * BitTimeNs
        long long IntermissionNs = 0;  // 3 * BitTimeNs
    };

    // canId: 0..0x7FF, dlc: 0..8, data.size() must equal dlc.
    FrameTimingResult ComputeFrameTiming(unsigned canId, unsigned dlc,
        const std::vector<std::uint8_t>& data, long long bitRateBps);

    // Exposed for reuse/testing: CAN CRC-15 (x^15+x^14+x^10+x^8+x^7+x^4+x^3+1)
    // over a bit sequence given MSB-first as 0/1 values.
    std::uint16_t ComputeCanCrc15(const std::vector<int>& bitsMsbFirst);
}
