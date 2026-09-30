// LogLines.cpp
#include "pch.h"
#include "LogLines.h"

#include <algorithm>

namespace CanSim
{
    namespace
    {
        void AddState(std::vector<LogLine>& lines, long long timeUs, long long durationUs, int nodeId, StartupState state)
        {
            if (timeUs < 0 || timeUs > durationUs)
                return; // never reached, or after the end of the run
            LogLine line;
            line.TimeUs = timeUs;
            line.Kind = LogLineKind::State;
            line.NodeId = nodeId;
            line.Index = static_cast<int>(state);
            lines.push_back(line);
        }
    }

    std::vector<LogLine> BuildLogLines(const std::vector<LogRow>& rows, const Network& endState,
        long long durationUs)
    {
        std::vector<LogLine> lines;
        lines.reserve(rows.size() + endState.Nodes.size() * 3);

        for (const Node& node : endState.Nodes)
        {
            AddState(lines, node.PowerOnTimeUs, durationUs, node.NodeId, StartupState::Initializing);
            AddState(lines, node.InitDoneUs, durationUs, node.NodeId, StartupState::BusSyncDetecting);
            AddState(lines, node.NormalEntryUs, durationUs, node.NodeId, StartupState::Normal);
        }

        for (std::size_t i = 0; i < rows.size(); ++i)
        {
            LogLine line;
            line.TimeUs = rows[i].TimestampUs;
            line.Kind = LogLineKind::Frame;
            line.NodeId = rows[i].TxNodeId;
            line.Index = static_cast<int>(i);
            lines.push_back(line);
        }

        std::stable_sort(lines.begin(), lines.end(), [](const LogLine& a, const LogLine& b)
        {
            if (a.TimeUs != b.TimeUs)
                return a.TimeUs < b.TimeUs;
            if (a.Kind != b.Kind)
                return static_cast<int>(a.Kind) < static_cast<int>(b.Kind);
            return a.NodeId < b.NodeId;
        });
        return lines;
    }
}
