// ConfigLoader.cpp
#include "pch.h"
#include "ConfigLoader.h"
#include "FrameEditor.h"

#include <fstream>
#include <sstream>
#include <map>
#include <set>
#include <algorithm>
#include <cctype>

namespace CanSim
{
    namespace
    {
        std::string Trim(const std::string& s)
        {
            std::size_t begin = s.find_first_not_of(" \t\r\n");
            if (begin == std::string::npos)
                return "";
            std::size_t end = s.find_last_not_of(" \t\r\n");
            return s.substr(begin, end - begin + 1);
        }

        // Strips a ';' comment that is not inside the value (comments always
        // start at ';' per contracts/config-input-format.md examples).
        std::string StripComment(const std::string& s)
        {
            std::size_t pos = s.find(';');
            if (pos == std::string::npos)
                return s;
            return s.substr(0, pos);
        }

        std::vector<std::string> SplitCsv(const std::string& s)
        {
            std::vector<std::string> parts;
            std::stringstream ss(s);
            std::string item;
            while (std::getline(ss, item, ','))
            {
                item = Trim(item);
                if (!item.empty())
                    parts.push_back(item);
            }
            return parts;
        }

        bool TryParseULongBase(const std::string& s, int base, unsigned long& out)
        {
            if (s.empty())
                return false;
            // std::stoul accepts a leading '-' and wraps the value around, so a
            // negative number would slip through as a huge one. Every value read
            // here must be a non-negative integer (contracts/config-input-format.md).
            if (s[0] == '-')
                return false;
            try
            {
                std::size_t pos = 0;
                unsigned long v = std::stoul(s, &pos, base);
                if (pos != s.size())
                    return false;
                out = v;
                return true;
            }
            catch (...)
            {
                return false;
            }
        }

        // "Node3" -> nodeId=3. Returns false for anything else (e.g. "Network",
        // or a "NodeN.FrameM" sub-section, which is handled separately).
        bool TryParseNodeSection(const std::string& name, int& nodeId)
        {
            if (name.rfind("Node", 0) != 0)
                return false;
            std::string rest = name.substr(4);
            if (rest.empty() || rest.find('.') != std::string::npos)
                return false;
            for (char c : rest)
                if (!std::isdigit(static_cast<unsigned char>(c)))
                    return false;
            nodeId = std::stoi(rest);
            return true;
        }

        // "Node3.Frame2" -> nodeId=3, frameIndex=2.
        bool TryParseFrameSection(const std::string& name, int& nodeId, int& frameIndex)
        {
            std::size_t dot = name.find('.');
            if (dot == std::string::npos)
                return false;
            std::string nodePart = name.substr(0, dot);
            std::string framePart = name.substr(dot + 1);
            if (nodePart.rfind("Node", 0) != 0 || framePart.rfind("Frame", 0) != 0)
                return false;
            std::string nodeDigits = nodePart.substr(4);
            std::string frameDigits = framePart.substr(5);
            if (nodeDigits.empty() || frameDigits.empty())
                return false;
            for (char c : nodeDigits)
                if (!std::isdigit(static_cast<unsigned char>(c))) return false;
            for (char c : frameDigits)
                if (!std::isdigit(static_cast<unsigned char>(c))) return false;
            nodeId = std::stoi(nodeDigits);
            frameIndex = std::stoi(frameDigits);
            return true;
        }

        struct RawSections
        {
            std::map<std::string, std::string> project;                 // key -> value (feature 004)
            std::map<std::string, std::string> network;                 // key -> value
            std::map<int, std::map<std::string, std::string>> nodes;    // nodeId -> key->value
            std::map<int, std::map<int, std::map<std::string, std::string>>> frames; // nodeId -> frameIndex -> key->value
        };

        // Reads the raw bytes of the file as if they were UTF-8/ASCII text.
        // The config grammar only uses ASCII delimiters, so byte-wise parsing
        // is correct even when a value (e.g. Name=) contains UTF-8 text.
        bool ReadFileBytes(const std::wstring& path, std::string& outContent)
        {
            std::ifstream file(path.c_str(), std::ios::binary);
            if (!file)
                return false;
            std::ostringstream ss;
            ss << file.rdbuf();
            outContent = ss.str();
            return true;
        }
    }

    ConfigLoadResult LoadConfig(const std::wstring& filePath)
    {
        ConfigLoadResult result;

        std::string content;
        if (!ReadFileBytes(filePath, content))
        {
            result.ErrorMessage = "Failed to open configuration file.";
            return result;
        }

        RawSections raw;
        std::string currentSection;
        std::istringstream stream(content);
        std::string line;
        int lineNo = 0;
        while (std::getline(stream, line))
        {
            ++lineNo;
            std::string trimmed = Trim(StripComment(line));
            if (trimmed.empty())
                continue;

            if (trimmed.front() == '[' && trimmed.back() == ']')
            {
                currentSection = Trim(trimmed.substr(1, trimmed.size() - 2));
                continue;
            }

            std::size_t eq = trimmed.find('=');
            if (eq == std::string::npos)
            {
                result.ErrorMessage = "Line " + std::to_string(lineNo) +
                    ": expected 'key=value' or a '[Section]' header.";
                return result;
            }
            std::string key = Trim(trimmed.substr(0, eq));
            std::string value = Trim(trimmed.substr(eq + 1));

            if (currentSection == "Network")
            {
                raw.network[key] = value;
            }
            else if (currentSection == "Project")
            {
                raw.project[key] = value;
            }
            else
            {
                int nodeId = 0, frameIndex = 0;
                if (TryParseFrameSection(currentSection, nodeId, frameIndex))
                {
                    raw.frames[nodeId][frameIndex][key] = value;
                }
                else if (TryParseNodeSection(currentSection, nodeId))
                {
                    raw.nodes[nodeId][key] = value;
                }
                else
                {
                    result.ErrorMessage = "Line " + std::to_string(lineNo) +
                        ": '[" + currentSection + "]' is not a recognized section "
                        "(expected [Project], [Network], [NodeN] or [NodeN.FrameM]).";
                    return result;
                }
            }
        }

        // ---- Network.BitRateBps (FR-002) ----
        Network network;
        network.BitRateBps = 500000;
        auto brIt = raw.network.find("BitRateBps");
        if (brIt != raw.network.end())
        {
            unsigned long v = 0;
            if (!TryParseULongBase(brIt->second, 10, v))
            {
                result.ErrorMessage = "[Network] BitRateBps: '" + brIt->second + "' is not a valid number.";
                return result;
            }
            network.BitRateBps = static_cast<long long>(v);
        }
        static const long long kAllowedBitRates[] = { 125000, 250000, 500000, 1000000 };
        if (std::find(std::begin(kAllowedBitRates), std::end(kAllowedBitRates), network.BitRateBps)
            == std::end(kAllowedBitRates))
        {
            result.ErrorMessage = "[Network] BitRateBps must be one of 125000/250000/500000/1000000.";
            return result;
        }

        // ---- [Project] (feature 004 FR-005/FR-006), every key optional ----
        ProjectSettings project;
        struct IntKey { const char* Name; int Min; int Max; };
        const IntKey projectKeys[] = {
            { "EcuCount", 0, 16 },
            { "DurationSec", kMinDurationSec, kMaxDurationSec },
            { "FrameDisplay", 0, 1 },
        };
        for (int k = 0; k < 3; ++k)
        {
            const IntKey& key = projectKeys[k];
            auto it = raw.project.find(key.Name);
            if (it == raw.project.end())
                continue;
            unsigned long v = 0;
            if (!TryParseULongBase(it->second, 10, v) || v < static_cast<unsigned long>(key.Min) ||
                v > static_cast<unsigned long>(key.Max))
            {
                result.ErrorMessage = std::string("[Project] ") + key.Name + ": '" + it->second +
                    "' must be an integer between " + std::to_string(key.Min) + " and " +
                    std::to_string(key.Max) + ".";
                return result;
            }
            const int value = static_cast<int>(v);
            if (k == 0)
                project.EcuCount = value;
            else if (k == 1)
                project.DurationSec = value;
            else
                project.FrameDisplay = value != 0;
        }

        // Speed is signed (feature 005 FR-002): an optional '+' or '-' sign
        // followed by digits, kMinSpeed..kMaxSpeed. Feature 026: files saved
        // before the range became -10..10 may hold -13..-11; they read as
        // kMinSpeed. The error still names the current range.
        auto speedIt = raw.project.find("Speed");
        if (speedIt != raw.project.end())
        {
            const std::string& text = speedIt->second;
            const bool negative = !text.empty() && text[0] == '-';
            const std::string digits = (!text.empty() && (text[0] == '-' || text[0] == '+')) ? text.substr(1) : text;
            unsigned long magnitude = 0;
            const bool isNumber = !digits.empty() && digits.find_first_not_of("0123456789") == std::string::npos &&
                                  TryParseULongBase(digits, 10, magnitude) && magnitude <= 100;
            const long value = negative ? -static_cast<long>(magnitude) : static_cast<long>(magnitude);
            if (!isNumber || value < kLegacyMinSpeed || value > kMaxSpeed)
            {
                result.ErrorMessage = "[Project] Speed: '" + text + "' must be an integer between " +
                    std::to_string(kMinSpeed) + " and " + std::to_string(kMaxSpeed) + ".";
                return result;
            }
            project.Speed = value < kMinSpeed ? kMinSpeed : static_cast<int>(value);
        }

        // ---- Nodes (FR-001: 1 to 16 nodes) ----
        if (raw.nodes.empty())
        {
            result.ErrorMessage = "No node is defined. At least 1 node is required.";
            return result;
        }
        if (raw.nodes.size() > 16)
        {
            result.ErrorMessage = "Too many nodes (" + std::to_string(raw.nodes.size()) +
                "). At most 16 nodes are allowed.";
            return result;
        }

        std::map<int, Node> nodesById;
        std::set<unsigned> seenCanIds;

        for (const auto& nodeEntry : raw.nodes)
        {
            const int sectionNodeId = nodeEntry.first;
            const auto& kv = nodeEntry.second;

            Node node;
            auto nodeIdIt = kv.find("NodeId");
            if (nodeIdIt == kv.end())
            {
                result.ErrorMessage = "[Node" + std::to_string(sectionNodeId) + "] is missing required key 'NodeId'.";
                return result;
            }
            unsigned long parsedNodeId = 0;
            if (!TryParseULongBase(nodeIdIt->second, 10, parsedNodeId) || parsedNodeId < 1 || parsedNodeId > 16)
            {
                result.ErrorMessage = "[Node" + std::to_string(sectionNodeId) +
                    "] NodeId must be an integer between 1 and 16.";
                return result;
            }
            node.NodeId = static_cast<int>(parsedNodeId);

            if (nodesById.count(node.NodeId) != 0)
            {
                result.ErrorMessage = "Duplicate NodeId " + std::to_string(node.NodeId) + ".";
                return result;
            }

            auto nameIt = kv.find("Name");
            node.Name = (nameIt != kv.end()) ? nameIt->second : ("Node" + std::to_string(node.NodeId));
            // 022: optional ECU label shown in the diagram
            auto labelIt = kv.find("Label");
            if (labelIt != kv.end())
                node.Label = LimitEcuLabel(TrimEcuLabel(labelIt->second));

            node.ReceiveDelayUs = 0;
            auto delayIt = kv.find("ReceiveDelayUs");
            if (delayIt != kv.end())
            {
                unsigned long delay = 0;
                if (!TryParseULongBase(delayIt->second, 10, delay))
                {
                    result.ErrorMessage = "[Node" + std::to_string(sectionNodeId) +
                        "] ReceiveDelayUs must be a non-negative integer.";
                    return result;
                }
                node.ReceiveDelayUs = static_cast<long long>(delay);
            }

            // --- startup settings (feature 002 FR-001), optional, default 0 ---
            node.PowerOnTimeUs = 0;
            auto powerOnIt = kv.find("PowerOnTimeUs");
            if (powerOnIt != kv.end())
            {
                unsigned long powerOn = 0;
                if (!TryParseULongBase(powerOnIt->second, 10, powerOn))
                {
                    result.ErrorMessage = "[Node" + std::to_string(sectionNodeId) +
                        "] PowerOnTimeUs must be a non-negative integer.";
                    return result;
                }
                node.PowerOnTimeUs = static_cast<long long>(powerOn);
            }

            node.InitDurationUs = 0;
            auto initIt = kv.find("InitDurationUs");
            if (initIt != kv.end())
            {
                unsigned long initDuration = 0;
                if (!TryParseULongBase(initIt->second, 10, initDuration))
                {
                    result.ErrorMessage = "[Node" + std::to_string(sectionNodeId) +
                        "] InitDurationUs must be a non-negative integer.";
                    return result;
                }
                node.InitDurationUs = static_cast<long long>(initDuration);
            }

            auto rxIt = kv.find("RxFilter");
            if (rxIt == kv.end() || Trim(rxIt->second).empty())
            {
                node.RxFilterAll = true; // FR-005 default
            }
            else
            {
                node.RxFilterAll = false;
                for (const std::string& tok : SplitCsv(rxIt->second))
                {
                    unsigned long id = 0;
                    if (!TryParseULongBase(tok, 16, id) || id > 0x7FF)
                    {
                        result.ErrorMessage = "[Node" + std::to_string(node.NodeId) +
                            "] RxFilter entry '" + tok + "' is not a valid 11-bit CAN ID.";
                        return result;
                    }
                    node.RxFilter.insert(static_cast<unsigned>(id));
                }
            }

            // ---- Frames for this node (FR-003, FR-004) ----
            auto framesIt = raw.frames.find(sectionNodeId);
            if (framesIt != raw.frames.end())
            {
                // Feature 006 / domain spec FR-091: at most 100 frames per node.
                if (framesIt->second.size() > static_cast<std::size_t>(kMaxFramesPerNode))
                {
                    result.ErrorMessage = "[Node" + std::to_string(node.NodeId) + "] has more than " +
                        std::to_string(kMaxFramesPerNode) + " frames.";
                    return result;
                }
                for (const auto& frameEntry : framesIt->second)
                {
                    const auto& fkv = frameEntry.second;
                    FrameDefinition fd;

                    auto get = [&](const char* key) -> const std::string*
                    {
                        auto it = fkv.find(key);
                        return it != fkv.end() ? &it->second : nullptr;
                    };

                    const std::string* canIdStr = get("CanId");
                    const std::string* dlcStr = get("Dlc");
                    const std::string* periodStr = get("PeriodUs");
                    if (!canIdStr || !dlcStr || !periodStr)
                    {
                        result.ErrorMessage = "[Node" + std::to_string(node.NodeId) + ".Frame" +
                            std::to_string(frameEntry.first) + "] requires CanId, Dlc and PeriodUs.";
                        return result;
                    }

                    unsigned long canId = 0;
                    if (!TryParseULongBase(*canIdStr, 16, canId) || canId > 0x7FF)
                    {
                        result.ErrorMessage = "[Node" + std::to_string(node.NodeId) + ".Frame" +
                            std::to_string(frameEntry.first) + "] CanId '" + *canIdStr +
                            "' is not a valid 11-bit CAN ID.";
                        return result;
                    }
                    fd.CanId = static_cast<unsigned>(canId);

                    unsigned long dlc = 0;
                    if (!TryParseULongBase(*dlcStr, 10, dlc) || dlc > 8)
                    {
                        result.ErrorMessage = "[Node" + std::to_string(node.NodeId) + ".Frame" +
                            std::to_string(frameEntry.first) + "] Dlc must be an integer between 0 and 8.";
                        return result;
                    }
                    fd.Dlc = static_cast<unsigned>(dlc);

                    // Feature 006 FR-014: the data field is always Dlc bytes of 0xFF.
                    // A Data= key (written by older versions) is accepted and ignored.
                    fd.Data.assign(fd.Dlc, 0xFF);

                    if (const std::string* nameStr = get("Name"))
                    {
                        if (CountUtf8Chars(*nameStr) > kMaxFrameNameChars) // shared with 022 (FrameEditor.h)
                        {
                            result.ErrorMessage = "[Node" + std::to_string(node.NodeId) + ".Frame" +
                                std::to_string(frameEntry.first) + "] Name is longer than " +
                                std::to_string(kMaxFrameNameChars) + " characters.";
                            return result;
                        }
                        fd.Name = *nameStr;
                    }

                    unsigned long period = 0;
                    if (!TryParseULongBase(*periodStr, 10, period) || period == 0)
                    {
                        result.ErrorMessage = "[Node" + std::to_string(node.NodeId) + ".Frame" +
                            std::to_string(frameEntry.first) + "] PeriodUs must be a positive integer.";
                        return result;
                    }
                    fd.PeriodUs = static_cast<long long>(period);

                    fd.InitialOffsetUs = 0;
                    if (const std::string* offsetStr = get("InitialOffsetUs"))
                    {
                        unsigned long offset = 0;
                        if (!TryParseULongBase(*offsetStr, 10, offset))
                        {
                            result.ErrorMessage = "[Node" + std::to_string(node.NodeId) + ".Frame" +
                                std::to_string(frameEntry.first) + "] InitialOffsetUs must be a non-negative integer.";
                            return result;
                        }
                        fd.InitialOffsetUs = static_cast<long long>(offset);
                    }
                    if (fd.InitialOffsetUs >= fd.PeriodUs)
                    {
                        result.ErrorMessage = "[Node" + std::to_string(node.NodeId) + ".Frame" +
                            std::to_string(frameEntry.first) + "] InitialOffsetUs must be smaller than PeriodUs.";
                        return result;
                    }

                    if (seenCanIds.count(fd.CanId) != 0)
                    {
                        result.ErrorMessage = "CAN ID 0x" + [&] {
                            std::ostringstream os; os << std::hex << fd.CanId; return os.str();
                        }() + " is transmitted by more than one node (FR-004).";
                        return result;
                    }
                    seenCanIds.insert(fd.CanId);

                    node.Frames.push_back(fd);
                }
            }

            nodesById[node.NodeId] = node;
        }

        for (const auto& entry : nodesById)
            network.Nodes.push_back(entry.second);

        result.Success = true;
        result.Config = network;
        result.Project = project;
        return result;
    }
}
