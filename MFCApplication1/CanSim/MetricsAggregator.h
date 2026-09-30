// MetricsAggregator.h
// Computes performance metrics (FR-018..FR-020) from a completed run's
// LogRow set: total latency, queueing delay, frame transmission time, bus
// load and (per CAN ID) jitter, aggregated by ID, by node and overall.

#pragma once

#include "CanTypes.h"
#include <string>
#include <vector>

namespace CanSim
{
    struct StatSummary
    {
        int Count = 0;
        double MinUs = 0.0;
        double MaxUs = 0.0;
        double AvgUs = 0.0;
    };

    struct GroupMetrics
    {
        std::string GroupType; // "ID" | "Node" | "ALL"
        std::string GroupKey;  // e.g. "0x080", "3", or "" for ALL

        StatSummary TotalLatency;   // Rx_Process - Tx_Request (per receiving row)
        StatSummary QueueingDelay;  // Tx_Start - Tx_Request (per unique transmission)
        StatSummary TxDuration;     // Tx_End - Tx_Start (per unique transmission)
        double AvgBusLoadPct = 0.0;

        // Only populated for GroupType == "ID" (FR-019).
        bool HasJitter = false;
        double JitterMaxUs = 0.0;
        double JitterMinUs = 0.0;
        double JitterStdDevUs = 0.0;

        // Only populated for GroupType == "Node" (feature 008): frames dropped
        // because the node's transmit / receive buffers were full.
        bool HasDiscards = false;
        int TxDiscardCount = 0;
        int RxDiscardCount = 0;
    };

    // network is used to look up each frame's configured PeriodUs for jitter.
    // OVERWRITE rows (feature 008) are left out of every statistic and only
    // counted as per-node discards.
    std::vector<GroupMetrics> ComputeMetrics(const std::vector<LogRow>& rows, const Network& network);

    // Writes the aggregated metrics as CSV (FR-020, contracts/log-output-format.md).
    bool WriteMetricsCsv(const std::vector<GroupMetrics>& metrics, const std::wstring& filePath);
}
