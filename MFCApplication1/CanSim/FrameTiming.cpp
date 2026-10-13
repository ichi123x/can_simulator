// FrameTiming.cpp
#include "pch.h"
#include "FrameTiming.h"

namespace CanSim
{
    namespace
    {
        void AppendBitsMsbFirst(std::vector<int>& bits, unsigned value, int width)
        {
            for (int i = width - 1; i >= 0; --i)
                bits.push_back((value >> i) & 1);
        }
    }

    std::uint16_t ComputeCanCrc15(const std::vector<int>& bitsMsbFirst)
    {
        // Standard bit-by-bit CAN CRC-15 LFSR, polynomial 0x4599.
        std::uint16_t crc = 0;
        for (int bit : bitsMsbFirst)
        {
            int crcNextBit = ((crc >> 14) & 1) ^ (bit & 1);
            crc = static_cast<std::uint16_t>((crc << 1) & 0x7FFF);
            if (crcNextBit)
                crc ^= 0x4599;
        }
        return crc & 0x7FFF;
    }

    FrameTimingResult ComputeFrameTiming(unsigned canId, unsigned dlc,
        const std::vector<std::uint8_t>& data, long long bitRateBps)
    {
        // Build the SOF..data bit sequence (before CRC), MSB first.
        std::vector<int> preCrcBits;
        preCrcBits.push_back(0);                 // SOF (dominant)
        AppendBitsMsbFirst(preCrcBits, canId & 0x7FF, 11); // ID[10:0]
        preCrcBits.push_back(0);                 // RTR (data frame -> dominant)
        preCrcBits.push_back(0);                 // IDE (standard frame -> dominant)
        preCrcBits.push_back(0);                 // r0
        AppendBitsMsbFirst(preCrcBits, dlc & 0xF, 4);
        for (std::uint8_t byte : data)
            AppendBitsMsbFirst(preCrcBits, byte, 8);

        const std::uint16_t crc = ComputeCanCrc15(preCrcBits);

        // Stuffed region: SOF .. CRC sequence (FR-021: "SOF through the CRC sequence").
        std::vector<int> stuffedRegionBits = preCrcBits;
        AppendBitsMsbFirst(stuffedRegionBits, crc, 15);

        // Apply bit stuffing (Bosch CAN 2.0A section 5, domain spec FR-021):
        // copy the bits into a stuffed buffer and, whenever the last 5 bits of
        // that buffer have the same level, append one bit of the opposite level.
        // The stuff bit is part of the buffer, so it counts as the first bit of
        // the next run (feature 031). A run of 5 at the end of the CRC sequence
        // also gets a stuff bit, before the CRC delimiter.
        std::vector<int> stuffedBits;
        stuffedBits.reserve(stuffedRegionBits.size() + stuffedRegionBits.size() / 4 + 1);
        int runLength = 0;
        for (int bit : stuffedRegionBits)
        {
            runLength = (!stuffedBits.empty() && stuffedBits.back() == bit) ? runLength + 1 : 1;
            stuffedBits.push_back(bit);
            if (runLength == 5)
            {
                stuffedBits.push_back(1 - bit); // stuff bit
                runLength = 1;                  // first bit of the next run
            }
        }
        const int stuffBitCount = static_cast<int>(stuffedBits.size() - stuffedRegionBits.size());

        FrameTimingResult result;
        result.StuffBitCount = stuffBitCount;
        // Fixed-form fields after the CRC sequence, never stuffed:
        // CRC delimiter(1) + ACK slot(1) + ACK delimiter(1) + EOF(7) = 10 bits.
        result.TotalBits = static_cast<int>(stuffedBits.size()) + 10;
        result.BitTimeNs = bitRateBps > 0 ? (1000000000LL / bitRateBps) : 0;
        result.TxDurationNs = static_cast<long long>(result.TotalBits) * result.BitTimeNs;
        result.IntermissionNs = 3 * result.BitTimeNs;
        return result;
    }
}
