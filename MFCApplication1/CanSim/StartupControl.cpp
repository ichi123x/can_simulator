// StartupControl.cpp
#include "pch.h"
#include "StartupControl.h"

#include <algorithm>
#include <fstream>

namespace CanSim
{
    long long ComputeSyncCompleteNs(long long detectStartNs, long long recessiveRunStartNs,
        long long bitTimeNs)
    {
        const long long windowStart = (std::max)(detectStartNs, recessiveRunStartNs);
        return windowStart + static_cast<long long>(kSyncRecessiveBits) * bitTimeNs;
    }

    long long ComputeRecessiveRunStartNs(long long txEndNs, long long bitTimeNs)
    {
        return txEndNs - static_cast<long long>(kFrameTrailingRecessiveBits) * bitTimeNs;
    }

    std::vector<StartupRecord> BuildStartupRecords(const Network& network)
    {
        // Network::Nodes is already ordered by ascending NodeId (ConfigLoader),
        // so iterating it preserves the required output order.
        std::vector<StartupRecord> records;
        records.reserve(network.Nodes.size());
        for (const Node& node : network.Nodes)
        {
            StartupRecord r;
            r.NodeId = node.NodeId;
            r.NodeName = node.Name;
            r.PowerOnUs = node.PowerOnTimeUs;
            r.InitDoneUs = node.InitDoneUs;
            r.NormalEntryUs = node.NormalEntryUs;
            r.ReachedNormal = (node.Startup == StartupState::Normal);
            records.push_back(r);
        }
        return records;
    }

    namespace
    {
        std::string QuoteIfNeeded(const std::string& value)
        {
            if (value.find(',') == std::string::npos && value.find('"') == std::string::npos)
                return value;
            std::string quoted = "\"";
            for (char c : value)
            {
                if (c == '"')
                    quoted += '"'; // CSV escaping: double the quote
                quoted += c;
            }
            quoted += '"';
            return quoted;
        }

        std::string OptionalTime(long long value)
        {
            if (value < 0)
                return "";
            return std::to_string(value);
        }
    }

    bool WriteStartupCsv(const std::vector<StartupRecord>& records, const std::wstring& filePath)
    {
        std::ofstream file(filePath.c_str(), std::ios::binary);
        if (!file)
            return false;

        file << "Node_Id,Node_Name,Power_On_us,Init_Done_us,Normal_Entry_us,Reached_Normal\r\n";
        for (const StartupRecord& r : records)
        {
            file << r.NodeId << ','
                 << QuoteIfNeeded(r.NodeName) << ','
                 << r.PowerOnUs << ','
                 << OptionalTime(r.InitDoneUs) << ','
                 << OptionalTime(r.NormalEntryUs) << ','
                 << (r.ReachedNormal ? "TRUE" : "FALSE") << "\r\n";
        }
        return true;
    }
}
