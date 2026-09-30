// MetricsAggregator.cpp
#include "pch.h"
#include "MetricsAggregator.h"

#include <map>
#include <set>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>
#include <iomanip>

namespace CanSim
{
    namespace
    {
        struct Transmission
        {
            unsigned CanId = 0;
            int TxNodeId = 0;
            long long TxRequestUs = 0;
            long long TxStartUs = 0;
            long long TxEndUs = 0;
            double BusLoadPct = 0.0;
        };

        StatSummary Summarize(const std::vector<double>& values)
        {
            StatSummary s;
            s.Count = static_cast<int>(values.size());
            if (values.empty())
                return s;
            double sum = 0.0;
            s.MinUs = values.front();
            s.MaxUs = values.front();
            for (double v : values)
            {
                sum += v;
                s.MinUs = std::min(s.MinUs, v);
                s.MaxUs = std::max(s.MaxUs, v);
            }
            s.AvgUs = sum / static_cast<double>(values.size());
            return s;
        }

        double Average(const std::vector<double>& values)
        {
            if (values.empty())
                return 0.0;
            double sum = 0.0;
            for (double v : values) sum += v;
            return sum / static_cast<double>(values.size());
        }

        long long FindPeriodUsForCanId(const Network& network, unsigned canId)
        {
            for (const Node& node : network.Nodes)
                for (const FrameDefinition& fd : node.Frames)
                    if (fd.CanId == canId)
                        return fd.PeriodUs;
            return 0;
        }

        std::string FormatCanIdKey(unsigned canId)
        {
            std::ostringstream os;
            os << "0x" << std::hex << std::nouppercase << std::setw(3) << std::setfill('0') << canId;
            return os.str();
        }

        GroupMetrics BuildGroup(const std::string& groupType, const std::string& groupKey,
            const std::vector<Transmission>& txs, const std::vector<double>& totalLatencies)
        {
            GroupMetrics g;
            g.GroupType = groupType;
            g.GroupKey = groupKey;

            std::vector<double> queueing, duration, busLoad;
            queueing.reserve(txs.size());
            duration.reserve(txs.size());
            busLoad.reserve(txs.size());
            for (const Transmission& t : txs)
            {
                queueing.push_back(static_cast<double>(t.TxStartUs - t.TxRequestUs));
                duration.push_back(static_cast<double>(t.TxEndUs - t.TxStartUs));
                busLoad.push_back(t.BusLoadPct);
            }

            g.QueueingDelay = Summarize(queueing);
            g.TxDuration = Summarize(duration);
            g.AvgBusLoadPct = Average(busLoad);
            g.TotalLatency = Summarize(totalLatencies);
            return g;
        }
    }

    std::vector<GroupMetrics> ComputeMetrics(const std::vector<LogRow>& allRows, const Network& network)
    {
        // Feature 008: OVERWRITE rows are not part of any statistic. A dropped
        // transmit request (no Tx_Start) counts for its Tx_Node, a dropped
        // received frame for its Rx_Node.
        std::vector<LogRow> rows;
        std::map<int, std::pair<int, int>> discards; // node -> (tx, rx)
        rows.reserve(allRows.size());
        for (const LogRow& row : allRows)
        {
            if (row.Status != kStatusOverwrite)
                rows.push_back(row);
            else if (row.TxStartUs < 0)
                ++discards[row.TxNodeId].first;
            else
                ++discards[row.RxNodeId].second;
        }

        // 1) Rebuild the unique per-transmission records (rows are per receiver).
        std::map<std::pair<unsigned, long long>, Transmission> uniqueTx;
        for (const LogRow& row : rows)
        {
            auto key = std::make_pair(row.CanId, row.TxStartUs);
            if (uniqueTx.count(key) == 0)
            {
                Transmission t;
                t.CanId = row.CanId;
                t.TxNodeId = row.TxNodeId;
                t.TxRequestUs = row.TxRequestUs;
                t.TxStartUs = row.TxStartUs;
                t.TxEndUs = row.TxEndUs;
                t.BusLoadPct = row.BusLoadPct;
                uniqueTx[key] = t;
            }
        }

        std::vector<GroupMetrics> result;

        // 2) Group by CAN ID.
        std::map<unsigned, std::vector<Transmission>> byId;
        std::map<unsigned, std::vector<double>> latencyById;
        for (const auto& kv : uniqueTx)
            byId[kv.first.first].push_back(kv.second);
        for (const LogRow& row : rows)
            if (row.RxNodeId >= 0)
                latencyById[row.CanId].push_back(static_cast<double>(row.RxProcessUs - row.TxRequestUs));

        for (const auto& kv : byId)
        {
            unsigned canId = kv.first;
            GroupMetrics g = BuildGroup("ID", FormatCanIdKey(canId), kv.second,
                latencyById.count(canId) ? latencyById[canId] : std::vector<double>());

            // FR-019: jitter = consecutive Tx_Start interval - configured period.
            std::vector<long long> starts;
            for (const Transmission& t : kv.second) starts.push_back(t.TxStartUs);
            std::sort(starts.begin(), starts.end());
            const long long periodUs = FindPeriodUsForCanId(network, canId);
            if (starts.size() >= 2 && periodUs > 0)
            {
                std::vector<double> jitters;
                for (std::size_t i = 1; i < starts.size(); ++i)
                    jitters.push_back(static_cast<double>((starts[i] - starts[i - 1]) - periodUs));

                double sum = 0.0, sumSq = 0.0;
                double mn = jitters.front(), mx = jitters.front();
                for (double j : jitters)
                {
                    sum += j;
                    sumSq += j * j;
                    mn = std::min(mn, j);
                    mx = std::max(mx, j);
                }
                const double mean = sum / static_cast<double>(jitters.size());
                const double variance = (sumSq / static_cast<double>(jitters.size())) - (mean * mean);
                g.HasJitter = true;
                g.JitterMinUs = mn;
                g.JitterMaxUs = mx;
                g.JitterStdDevUs = variance > 0.0 ? std::sqrt(variance) : 0.0;
            }

            result.push_back(g);
        }

        // 3) Group by transmitting node.
        std::map<int, std::vector<Transmission>> byNode;
        std::map<int, std::vector<double>> latencyByNode;
        for (const auto& kv : uniqueTx)
            byNode[kv.second.TxNodeId].push_back(kv.second);
        for (const LogRow& row : rows)
            if (row.RxNodeId >= 0)
                latencyByNode[row.TxNodeId].push_back(static_cast<double>(row.RxProcessUs - row.TxRequestUs));

        std::set<int> nodeIds;
        for (const auto& kv : byNode)
            nodeIds.insert(kv.first);
        for (const auto& kv : discards)
            nodeIds.insert(kv.first); // a node that only dropped frames still gets a row
        for (int nodeId : nodeIds)
        {
            GroupMetrics g;
            auto it = byNode.find(nodeId);
            if (it != byNode.end())
                g = BuildGroup("Node", std::to_string(nodeId), it->second,
                    latencyByNode.count(nodeId) ? latencyByNode[nodeId] : std::vector<double>());
            else
            {
                g.GroupType = "Node";
                g.GroupKey = std::to_string(nodeId);
            }
            g.HasDiscards = true;
            auto d = discards.find(nodeId);
            if (d != discards.end())
            {
                g.TxDiscardCount = d->second.first;
                g.RxDiscardCount = d->second.second;
            }
            result.push_back(g);
        }

        // 4) Overall.
        {
            std::vector<Transmission> all;
            all.reserve(uniqueTx.size());
            for (const auto& kv : uniqueTx) all.push_back(kv.second);
            std::vector<double> allLatency;
            for (const LogRow& row : rows)
                if (row.RxNodeId >= 0)
                    allLatency.push_back(static_cast<double>(row.RxProcessUs - row.TxRequestUs));
            result.push_back(BuildGroup("ALL", "", all, allLatency));
        }

        return result;
    }

    bool WriteMetricsCsv(const std::vector<GroupMetrics>& metrics, const std::wstring& filePath)
    {
        std::ofstream file(filePath.c_str(), std::ios::binary);
        if (!file)
            return false;

        file << "GroupType,GroupKey,Count,"
                "TotalLatency_Min_us,TotalLatency_Max_us,TotalLatency_Avg_us,"
                "QueueingDelay_Min_us,QueueingDelay_Max_us,QueueingDelay_Avg_us,"
                "TxDuration_Min_us,TxDuration_Max_us,TxDuration_Avg_us,"
                "BusLoad_Avg_Pct,Jitter_Max_us,Jitter_Min_us,Jitter_StdDev_us,"
                "TxDiscard_Count,RxDiscard_Count\r\n";

        file << std::fixed << std::setprecision(2);
        for (const GroupMetrics& g : metrics)
        {
            file << g.GroupType << ',' << g.GroupKey << ',' << g.TotalLatency.Count << ','
                 << g.TotalLatency.MinUs << ',' << g.TotalLatency.MaxUs << ',' << g.TotalLatency.AvgUs << ','
                 << g.QueueingDelay.MinUs << ',' << g.QueueingDelay.MaxUs << ',' << g.QueueingDelay.AvgUs << ','
                 << g.TxDuration.MinUs << ',' << g.TxDuration.MaxUs << ',' << g.TxDuration.AvgUs << ','
                 << g.AvgBusLoadPct << ',';
            if (g.HasJitter)
                file << g.JitterMaxUs << ',' << g.JitterMinUs << ',' << g.JitterStdDevUs;
            else
                file << ",,";
            file << ',';
            if (g.HasDiscards)
                file << g.TxDiscardCount << ',' << g.RxDiscardCount;
            else
                file << ',';
            file << "\r\n";
        }
        return true;
    }
}
