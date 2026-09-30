// Network.cpp
#include "pch.h"
#include "Network.h"

#include <limits>

namespace CanSim
{
    EnqueueResult EnqueueTxRequest(Node& node, std::size_t frameDefIndex, long long nowUs, bool transmitting)
    {
        for (PendingFrame& pf : node.PendingFrames)
        {
            if (pf.FrameDefIndex == frameDefIndex)
            {
                // Still queued from an earlier period: update the data in the
                // same buffer (FR-011, domain spec FR-092a).
                pf.CurrentData = node.Frames[frameDefIndex].Data;
                ++pf.UpdateCount;
                return EnqueueResult::Updated;
            }
        }

        // All transmit buffers in use: drop the new request (domain spec FR-092b).
        const std::size_t inUse = node.PendingFrames.size() + (transmitting ? 1 : 0);
        if (inUse >= static_cast<std::size_t>(kTxBufferCount))
            return EnqueueResult::Discarded;

        PendingFrame pf;
        pf.FrameDefIndex = frameDefIndex;
        pf.TxRequestUs = nowUs;
        pf.CurrentData = node.Frames[frameDefIndex].Data;
        pf.RetryCount = 0;
        pf.UpdateCount = 0;
        node.PendingFrames.push_back(pf);
        return EnqueueResult::Queued;
    }

    bool NodeAcceptsCanId(const Node& node, unsigned canId)
    {
        return node.RxFilterAll || node.RxFilter.count(canId) != 0;
    }

    namespace
    {
        // Index (within node.PendingFrames) of the pending frame with the
        // smallest CanId, i.e. the frame this node offers to this round of
        // arbitration (FR-014).
        bool FindNodeCandidate(const Node& node, std::size_t& outIndex)
        {
            bool found = false;
            unsigned bestCanId = std::numeric_limits<unsigned>::max();
            for (std::size_t i = 0; i < node.PendingFrames.size(); ++i)
            {
                unsigned canId = node.Frames[node.PendingFrames[i].FrameDefIndex].CanId;
                if (!found || canId < bestCanId)
                {
                    found = true;
                    bestCanId = canId;
                    outIndex = i;
                }
            }
            return found;
        }
    }

    ArbitrationResult RunArbitration(Network& network)
    {
        ArbitrationResult result;

        int winnerNodeIndex = -1;
        std::size_t winnerPendingIndex = 0;
        unsigned winnerCanId = std::numeric_limits<unsigned>::max();

        std::vector<std::pair<int, std::size_t>> candidates; // (nodeIndex, pendingIndex)

        for (std::size_t nodeIndex = 0; nodeIndex < network.Nodes.size(); ++nodeIndex)
        {
            Node& node = network.Nodes[nodeIndex];

            // 002 FR-008: a node that has not completed its startup sequence
            // does not take part in arbitration. Its pending frames stay
            // queued and are sent once it reaches Normal (002 FR-010).
            if (node.Startup != StartupState::Normal)
                continue;

            std::size_t candidateIndex = 0;
            if (!FindNodeCandidate(node, candidateIndex))
                continue;

            candidates.emplace_back(static_cast<int>(nodeIndex), candidateIndex);
            unsigned canId = node.Frames[node.PendingFrames[candidateIndex].FrameDefIndex].CanId;
            if (canId < winnerCanId)
            {
                winnerCanId = canId;
                winnerNodeIndex = static_cast<int>(nodeIndex);
                winnerPendingIndex = candidateIndex;
            }
        }

        if (winnerNodeIndex < 0)
            return result; // nothing pending anywhere

        // FR-013: every non-winning candidate loses this round and gets a retry.
        for (const auto& c : candidates)
        {
            if (c.first == winnerNodeIndex && c.second == winnerPendingIndex)
                continue;
            Network& net = network; // clarity
            net.Nodes[static_cast<std::size_t>(c.first)].PendingFrames[c.second].RetryCount += 1;
        }

        Node& winnerNode = network.Nodes[static_cast<std::size_t>(winnerNodeIndex)];
        result.HasWinner = true;
        result.WinnerNodeIndex = winnerNodeIndex;
        result.WinnerFrameDefIndex = winnerNode.PendingFrames[winnerPendingIndex].FrameDefIndex;
        result.WinnerPending = winnerNode.PendingFrames[winnerPendingIndex];
        winnerNode.PendingFrames.erase(winnerNode.PendingFrames.begin() + static_cast<long>(winnerPendingIndex));

        return result;
    }
}
