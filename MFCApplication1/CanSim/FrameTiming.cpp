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

        // Apply bit stuffing: insert one opposite bit after every 5
        // consecutive identical bits in the original (unstuffed) stream.
        int stuffBitCount = 0;
        int runLength = 1;
        for (std::size_t i = 1; i < stuffedRegionBits.size(); ++i)
        {
            if (stuffedRegionBits[i] == stuffedRegionBits[i - 1])
            {
                ++runLength;
                if (runLength == 5)
                {
                    ++stuffBitCount;
                    runLength = 1; // the inserted opposite bit starts a new run of 1
                }
            }
            else
            {
                runLength = 1;
            }
        }

        FrameTimingResult result;
        result.StuffBitCount = stuffBitCount;
        // Fixed-form fields after the CRC sequence, never stuffed:
        // CRC delimiter(1) + ACK slot(1) + ACK delimiter(1) + EOF(7) = 10 bits.
        result.TotalBits = static_cast<int>(stuffedRegionBits.size()) + stuffBitCount + 10;
        result.BitTimeNs = bitRateBps > 0 ? (1000000000LL / bitRateBps) : 0;
        result.TxDurationNs = static_cast<long long>(result.TotalBits) * result.BitTimeNs;
        result.IntermissionNs = 3 * result.BitTimeNs;
        return result;
    }
}
