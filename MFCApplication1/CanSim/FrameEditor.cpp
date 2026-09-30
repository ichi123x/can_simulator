// FrameEditor.cpp
#include "pch.h"
#include "FrameEditor.h"

#include <algorithm>
#include <cctype>

namespace CanSim
{
    namespace
    {
        std::string Trim(const std::string& s)
        {
            const std::size_t b = s.find_first_not_of(" \t\r\n");
            if (b == std::string::npos)
                return "";
            const std::size_t e = s.find_last_not_of(" \t\r\n");
            return s.substr(b, e - b + 1);
        }

        // Digits only (base 10 or 16), at most maxDigits, no sign.
        bool ParseUnsigned(const std::string& text, int base, int maxDigits, long long& value)
        {
            if (text.empty() || static_cast<int>(text.size()) > maxDigits)
                return false;
            long long v = 0;
            for (char ch : text)
            {
                const unsigned char c = static_cast<unsigned char>(ch);
                int digit = -1;
                if (std::isdigit(c))
                    digit = c - '0';
                else if (base == 16 && std::isxdigit(c))
                    digit = std::tolower(c) - 'a' + 10;
                if (digit < 0 || digit >= base)
                    return false;
                v = v * base + digit;
            }
            value = v;
            return true;
        }

        void Add(std::vector<FrameIssue>& issues, FrameField field, FrameProblem problem)
        {
            FrameIssue issue;
            issue.Field = field;
            issue.Problem = problem;
            issues.push_back(issue);
        }
    }

    std::vector<std::uint8_t> MakeFrameData(unsigned dlc)
    {
        return std::vector<std::uint8_t>(dlc, 0xFF);
    }

    const Node* FindNodeDefinition(const Network& definitions, int nodeId)
    {
        for (const Node& node : definitions.Nodes)
            if (node.NodeId == nodeId)
                return &node;
        return nullptr;
    }

    Node& EnsureNode(Network& definitions, int nodeId)
    {
        auto it = definitions.Nodes.begin();
        for (; it != definitions.Nodes.end(); ++it)
        {
            if (it->NodeId == nodeId)
                return *it;
            if (it->NodeId > nodeId)
                break;
        }
        Node node;
        node.NodeId = nodeId;
        node.Name = "ECU" + std::to_string(nodeId);
        return *definitions.Nodes.insert(it, node);
    }

    std::vector<FrameIssue> ValidateFrame(const Network& definitions, int nodeId, int editIndex,
        const FrameInput& in, FrameDefinition& out)
    {
        std::vector<FrameIssue> issues;
        const Node* node = FindNodeDefinition(definitions, nodeId);

        if (editIndex < 0 && node != nullptr && static_cast<int>(node->Frames.size()) >= kMaxFramesPerNode)
            Add(issues, FrameField::Name, FrameProblem::ListFull);

        // Name: optional, at most 32 characters
        if (in.NameChars > kMaxFrameNameChars)
            Add(issues, FrameField::Name, FrameProblem::TooLong);

        // ID: hex, "0x" optional, 0x000..0x7FF, unique in the whole project
        std::string idText = Trim(in.IdText);
        if (idText.size() >= 2 && idText[0] == '0' && (idText[1] == 'x' || idText[1] == 'X'))
            idText = idText.substr(2);
        long long id = -1;
        if (Trim(in.IdText).empty())
            Add(issues, FrameField::Id, FrameProblem::Empty);
        else if (!ParseUnsigned(idText, 16, 8, id))
            Add(issues, FrameField::Id, FrameProblem::NotANumber);
        else if (id > 0x7FF)
            Add(issues, FrameField::Id, FrameProblem::OutOfRange);
        else
        {
            for (const Node& other : definitions.Nodes)
            {
                for (std::size_t i = 0; i < other.Frames.size(); ++i)
                {
                    if (other.NodeId == nodeId && static_cast<int>(i) == editIndex)
                        continue; // the frame being edited
                    if (other.Frames[i].CanId == static_cast<unsigned>(id))
                    {
                        FrameIssue issue;
                        issue.Field = FrameField::Id;
                        issue.Problem = FrameProblem::Duplicate;
                        issue.DuplicateNodeId = other.NodeId;
                        issue.DuplicateIndex = static_cast<int>(i);
                        issues.push_back(issue);
                    }
                }
            }
        }

        // DLC: 0..8
        long long dlc = -1;
        const std::string dlcText = Trim(in.DlcText);
        if (dlcText.empty())
            Add(issues, FrameField::Dlc, FrameProblem::Empty);
        else if (!ParseUnsigned(dlcText, 10, 4, dlc))
            Add(issues, FrameField::Dlc, FrameProblem::NotANumber);
        else if (dlc > 8)
            Add(issues, FrameField::Dlc, FrameProblem::OutOfRange);

        // Period: 1..3,600,000 ms
        long long periodMs = -1;
        const std::string periodText = Trim(in.PeriodMsText);
        if (periodText.empty())
            Add(issues, FrameField::Period, FrameProblem::Empty);
        else if (!ParseUnsigned(periodText, 10, 9, periodMs))
            Add(issues, FrameField::Period, FrameProblem::NotANumber);
        else if (periodMs < 1 || periodMs > kMaxPeriodMs)
            Add(issues, FrameField::Period, FrameProblem::OutOfRange);

        // Initial offset: empty = 0, 0 <= offset < period
        long long offsetMs = 0;
        const std::string offsetText = Trim(in.OffsetMsText);
        if (!offsetText.empty())
        {
            if (!ParseUnsigned(offsetText, 10, 9, offsetMs))
                Add(issues, FrameField::Offset, FrameProblem::NotANumber);
            else if (periodMs >= 1 && periodMs <= kMaxPeriodMs && offsetMs >= periodMs)
                Add(issues, FrameField::Offset, FrameProblem::OffsetNotBelowPeriod);
        }

        if (issues.empty())
        {
            out = FrameDefinition();
            out.Name = in.Name;
            out.CanId = static_cast<unsigned>(id);
            out.Dlc = static_cast<unsigned>(dlc);
            out.Data = MakeFrameData(out.Dlc);
            out.PeriodUs = periodMs * 1000;
            out.InitialOffsetUs = offsetMs * 1000;
        }
        return issues;
    }

    void AddFrame(Network& definitions, int nodeId, const FrameDefinition& frame)
    {
        EnsureNode(definitions, nodeId).Frames.push_back(frame);
    }

    void ReplaceFrame(Network& definitions, int nodeId, int index, const FrameDefinition& frame)
    {
        Node& node = EnsureNode(definitions, nodeId);
        if (index >= 0 && index < static_cast<int>(node.Frames.size()))
            node.Frames[static_cast<std::size_t>(index)] = frame;
    }

    void DeleteFrame(Network& definitions, int nodeId, int index)
    {
        Node& node = EnsureNode(definitions, nodeId);
        if (index >= 0 && index < static_cast<int>(node.Frames.size()))
            node.Frames.erase(node.Frames.begin() + index);
    }
}
