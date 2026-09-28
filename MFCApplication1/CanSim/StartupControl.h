// StartupControl.h
// Node startup sequence support (feature 002): bus-synchronisation timing and
// the startup record written alongside the frame log.

#pragma once

#include "CanTypes.h"
#include <string>
#include <vector>

namespace CanSim
{
    // Number of consecutive recessive bit times required for bus
    // synchronisation (002 FR-005, domain spec FR-031a).
    const int kSyncRecessiveBits = 11;

    // Trailing recessive bits contained in a frame itself:
    // ACK delimiter(1) + EOF(7). The recessive run of a transmission therefore
    // starts at (TxEnd - kFrameTrailingRecessiveBits * bitTime).
    const int kFrameTrailingRecessiveBits = 8;

    // Time at which a node that starts (or restarts) bus-sync detection at
    // detectStartNs reaches Normal, given the start of the recessive run
    // currently in progress on the bus (002 research.md decision 1):
    //   windowStart  = max(detectStartNs, recessiveRunStartNs)
    //   syncComplete = windowStart + 11 * bitTime
    long long ComputeSyncCompleteNs(long long detectStartNs, long long recessiveRunStartNs,
        long long bitTimeNs);

    // Start of the recessive run produced by a transmission that ends at
    // txEndNs, i.e. txEndNs - 8 * bitTime.
    long long ComputeRecessiveRunStartNs(long long txEndNs, long long bitTimeNs);

    // data-model.md: StartupRecord (one per node).
    struct StartupRecord
    {
        int NodeId = 0;
        std::string NodeName;
        long long PowerOnUs = 0;
        long long InitDoneUs = -1;    // -1 == not reached
        long long NormalEntryUs = -1; // -1 == not reached
        bool ReachedNormal = false;
    };

    // Builds one record per node, ordered by ascending NodeId (002 FR-014).
    std::vector<StartupRecord> BuildStartupRecords(const Network& network);

    // Writes contracts/startup-log-format.md CSV (UTF-8, header row).
    bool WriteStartupCsv(const std::vector<StartupRecord>& records, const std::wstring& filePath);
}
