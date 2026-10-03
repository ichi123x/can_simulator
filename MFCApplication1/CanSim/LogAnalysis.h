// LogAnalysis.h
// Analysis of a frame log written by a run (feature 028): the time from the
// transmit request until each receiver has the frame, as a table per
// (sender, receiver) pair with a histogram, and the CAN bus load in 10 ms
// sections. Everything is computed from the .log.csv file, so a run can be
// analysed after a restart. No dependency on MFC; the dialog only draws the
// results.

#pragma once

#include <string>
#include <vector>

namespace CanSim
{
    enum class LogStatus { Success, RetrySuccess, Overwrite };

    // One row of the frame log (one frame x one receiver). Empty cells:
    // RxNode 0, times -1.
    struct LogRecord
    {
        unsigned CanId = 0;
        int TxNode = 0;
        int RxNode = 0;
        long long TxRequestUs = -1;
        long long TxStartUs = -1;
        long long TxEndUs = -1;
        long long RxEndUs = -1;
        LogStatus Status = LogStatus::Success;
    };

    // Reads the frame log. The columns are found by their names in the
    // header line. False (with "line N: ..." in error) if the file cannot be
    // opened, a column is missing or a value cannot be read.
    bool ReadLogRecords(const std::wstring& path, std::vector<LogRecord>& records, std::string& error);

    // ---- time from the transmit request until the receiver has the frame ----

    struct LatencyRow
    {
        int TxNode = 0;             // 0 / 0: every receive
        int RxNode = 0;
        long long Count = 0;
        long long MinUs = 0;
        long long MaxUs = 0;
        double MeanUs = 0.0;
        long long P95Us = 0;        // nearest rank: sorted[ceil(0.95 n) - 1]
        std::vector<long long> Values; // ascending
    };

    struct LatencyAnalysis
    {
        std::vector<LatencyRow> Rows;  // [0] = all, then (TxNode, RxNode) ascending
        long long OverwriteCount = 0;  // discarded rows, not in the table
    };

    // Rx_End - Tx_Request for every row with a receiver that was sent
    // (SUCCESS or ARBITRATION_LOST_RETRY).
    LatencyAnalysis AnalyzeLatency(const std::vector<LogRecord>& records);

    struct Histogram
    {
        long long StartUs = 0;          // left edge of bin 0
        long long WidthUs = 1;          // 1, 2 or 5 x 10^m
        std::vector<long long> Counts;  // bin i: [Start + i W, Start + (i + 1) W)
    };

    // About clamp(ceil(sqrt(n)), 10, 50) bins of a round width; the counts
    // add up to n. Empty for no values, one bin when all values are equal.
    Histogram MakeHistogram(const std::vector<long long>& sortedValues);

    // ---- bus load ----

    struct EcuBusUsage
    {
        int Node = 0;
        long long FrameCount = 0;
        long long BusyUs = 0;
        double UsagePct = 0.0;      // BusyUs / RangeUs
    };

    struct BusLoadAnalysis
    {
        long long SectionUs = 10000;
        long long RangeUs = 10000;  // 0 .. the section holding the last Tx_End
        std::vector<double> SectionPct;
        double AveragePct = 0.0;    // busy time / RangeUs (not the summary CSV's average)
        double MaxPct = 0.0;
        int MaxSection = 0;         // first section with MaxPct
        std::vector<EcuBusUsage> Ecus; // senders only, by node number
        long long TransmissionCount = 0;
    };

    // One transmission per (TxNode, CanId, Tx_Start) of the rows that were
    // sent; the bus is busy over [Tx_Start, Tx_End).
    BusLoadAnalysis AnalyzeBusLoad(const std::vector<LogRecord>& records);

    // ---- graph helpers ----

    // Ticks from lo to hi at a step of 1, 2 or 5 x 10^m giving about
    // approxCount intervals. {lo} when hi <= lo.
    std::vector<double> NiceTicks(double lo, double hi, int approxCount);

    // When there are more values than columns, the maximum of the values
    // falling into each column (so that no peak is lost); else the values.
    std::vector<double> MaxPerColumn(const std::vector<double>& values, int columns);
}
