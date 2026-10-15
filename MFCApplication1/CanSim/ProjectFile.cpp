// ProjectFile.cpp
#include "pch.h"
#include "ProjectFile.h"

#include <cstdio>
#include <fstream>
#include <sstream>

namespace CanSim
{
    namespace
    {
        std::string Hex(unsigned value, int digits)
        {
            char buf[16];
            std::snprintf(buf, sizeof(buf), "0x%0*X", digits, value);
            return buf;
        }
    }

    bool SaveProject(const std::wstring& filePath, const ProjectSettings& settings,
        const Network& definitions)
    {
        std::ostringstream os;
        os << "; CAN bus simulator project file (.canprj)\r\n\r\n";
        os << "[Project]\r\n";
        os << "EcuCount=" << settings.EcuCount << "\r\n";
        os << "DurationSec=" << settings.DurationSec << "\r\n";
        os << "FrameDisplay=" << (settings.FrameDisplay ? 1 : 0) << "\r\n";
        os << "Speed=" << settings.Speed << "\r\n\r\n";

        os << "[Network]\r\n";
        os << "BitRateBps=" << definitions.BitRateBps << "\r\n";

        // A file without any [NodeN] is rejected on load (001 FR-001), so with no
        // definition at all write the nodes BuildRunNetwork would add instead
        // (ECU 1..EcuCount, or ECU 1 when EcuCount is 0). The run result after
        // reloading is the same.
        std::vector<Node> placeholders;
        if (definitions.Nodes.empty())
        {
            ProjectSettings atLeastOne = settings;
            if (atLeastOne.EcuCount < 1)
                atLeastOne.EcuCount = 1;
            placeholders = BuildRunNetwork(atLeastOne, definitions).Nodes;
        }
        const std::vector<Node>& nodes = definitions.Nodes.empty() ? placeholders : definitions.Nodes;

        for (const Node& node : nodes)
        {
            os << "\r\n[Node" << node.NodeId << "]\r\n";
            os << "NodeId=" << node.NodeId << "\r\n";
            os << "Name=" << node.Name << "\r\n";
            if (!node.Label.empty())
                os << "Label=" << node.Label << "\r\n"; // 022: only when set
            if (!node.RxFilterAll)
            {
                os << "RxFilter=";
                bool first = true;
                for (unsigned id : node.RxFilter)
                {
                    os << (first ? "" : ",") << Hex(id, 3);
                    first = false;
                }
                os << "\r\n";
            }
            os << "ReceiveDelayUs=" << node.ReceiveDelayUs << "\r\n";
            os << "PowerOnTimeUs=" << node.PowerOnTimeUs << "\r\n";
            os << "InitDurationUs=" << node.InitDurationUs << "\r\n";

            for (std::size_t i = 0; i < node.Frames.size(); ++i)
            {
                const FrameDefinition& fd = node.Frames[i];
                os << "\r\n[Node" << node.NodeId << ".Frame" << (i + 1) << "]\r\n";
                os << "Name=" << fd.Name << "\r\n";
                os << "CanId=" << Hex(fd.CanId, 3) << "\r\n";
                os << "Dlc=" << fd.Dlc << "\r\n";
                // No Data= line: the data is always Dlc bytes of 0xFF (feature 006 FR-014).
                os << "PeriodUs=" << fd.PeriodUs << "\r\n";
                os << "InitialOffsetUs=" << fd.InitialOffsetUs << "\r\n";
            }
        }

        std::ofstream file(filePath.c_str(), std::ios::binary | std::ios::trunc);
        if (!file)
            return false;
        const std::string text = os.str();
        file.write(text.data(), static_cast<std::streamsize>(text.size()));
        return static_cast<bool>(file);
    }

    Network BuildRunNetwork(const ProjectSettings& settings, const Network& definitions)
    {
        Network run;
        run.BitRateBps = definitions.BitRateBps;
        for (int n = 1; n <= settings.EcuCount; ++n)
        {
            const Node* defined = nullptr;
            for (const Node& node : definitions.Nodes)
            {
                if (node.NodeId == n)
                {
                    defined = &node;
                    break;
                }
            }
            if (defined != nullptr)
            {
                run.Nodes.push_back(*defined);
            }
            else
            {
                Node placeholder;
                placeholder.NodeId = n;
                placeholder.Name = "ECU" + std::to_string(n);
                run.Nodes.push_back(placeholder);
            }
        }
        return run;
    }

    std::wstring ComposeWindowTitle(const std::wstring& appTitle, const std::wstring& label,
        const std::wstring& projectPath)
    {
        return appTitle + L"    " + label + projectPath;
    }

    std::wstring FindStartupProjectArg(const std::vector<std::wstring>& args)
    {
        for (std::size_t i = 1; i < args.size(); ++i)
        {
            if (!args[i].empty())
                return args[i];
        }
        return std::wstring();
    }

    bool HasProjectExtension(const std::wstring& path)
    {
        const std::wstring ext = L".canprj";
        if (path.size() < ext.size())
            return false;
        const std::size_t start = path.size() - ext.size();
        for (std::size_t i = 0; i < ext.size(); ++i)
        {
            wchar_t c = path[start + i];
            if (c >= L'A' && c <= L'Z')
                c = static_cast<wchar_t>(c - L'A' + L'a');
            if (c != ext[i])
                return false;
        }
        return true;
    }
}
