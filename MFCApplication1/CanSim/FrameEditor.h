// FrameEditor.h
// Editing of the per-node transmit frame lists (feature 006, domain spec
// 3.1a): input validation and add / replace / delete. No MFC dependency; the
// dialogs only pass the text the user typed and show the returned issues.

#pragma once

#include "CanTypes.h"
#include <cstdint>
#include <string>
#include <vector>

namespace CanSim
{
    // Raw text of the entry dialog. NameChars is the number of characters
    // counted by the UI (Name itself is UTF-8).
    struct FrameInput
    {
        std::string Name;
        int NameChars = 0;
        std::string IdText;       // hex, "0x" optional
        std::string DlcText;
        std::string PeriodMsText;
        std::string OffsetMsText; // empty = 0
    };

    enum class FrameField { Name, Id, Dlc, Period, Offset };

    enum class FrameProblem
    {
        Empty,
        NotANumber,
        OutOfRange,
        Duplicate,            // Id already used; see DuplicateNodeId / DuplicateIndex
        TooLong,
        OffsetNotBelowPeriod,
        ListFull
    };

    struct FrameIssue
    {
        FrameField Field = FrameField::Id;
        FrameProblem Problem = FrameProblem::Empty;
        int DuplicateNodeId = 0;  // for Duplicate
        int DuplicateIndex = -1;  // 0-based position in that node's list
    };

    // Validates an entry for node nodeId. editIndex is the position of the
    // frame being edited, or -1 when adding. On success (empty result) out
    // holds the frame (period/offset in us, Data = Dlc x 0xFF).
    std::vector<FrameIssue> ValidateFrame(const Network& definitions, int nodeId, int editIndex,
        const FrameInput& in, FrameDefinition& out);

    // Dlc bytes of 0xFF (domain spec FR-004).
    std::vector<std::uint8_t> MakeFrameData(unsigned dlc);

    // Returns the node definition with the given NodeId, creating a default
    // one named "ECU<n>" (kept in NodeId order) if it does not exist yet.
    Node& EnsureNode(Network& definitions, int nodeId);

    // nullptr when the node has no definition.
    const Node* FindNodeDefinition(const Network& definitions, int nodeId);

    void AddFrame(Network& definitions, int nodeId, const FrameDefinition& frame);
    void ReplaceFrame(Network& definitions, int nodeId, int index, const FrameDefinition& frame);
    void DeleteFrame(Network& definitions, int nodeId, int index);
}
