// Network.h
// Arbitration, automatic retry, overwrite-on-reissue and reception delivery
// for the CAN bus simulation (FR-011..FR-016).

#pragma once

#include "CanTypes.h"

namespace CanSim
{
    // Adds/refreshes a pending frame for (node, frameDefIndex) at time nowUs.
    // If a PendingFrame for this FrameDefinition already exists (still queued
    // from a previous period), its data is refreshed and UpdateCount is
    // incremented; TxRequestUs (the *first* request time) is left unchanged
    // (FR-011). Otherwise a new PendingFrame is appended.
    //
    // Feature 008 (domain spec FR-092..FR-092b): a new request needs one of
    // the node's kTxBufferCount transmit buffers. The frames waiting in
    // PendingFrames and the frame being transmitted (transmitting == true)
    // each hold one. When all are in use the request is dropped (Discarded)
    // and nothing changes.
    enum class EnqueueResult { Queued, Updated, Discarded };
    EnqueueResult EnqueueTxRequest(Node& node, std::size_t frameDefIndex, long long nowUs, bool transmitting);

    // True if canId passes the node's receive filter (FR-005).
    bool NodeAcceptsCanId(const Node& node, unsigned canId);

    struct ArbitrationResult
    {
        bool HasWinner = false;
        int WinnerNodeIndex = -1;      // index into Network::Nodes
        std::size_t WinnerFrameDefIndex = 0;
        PendingFrame WinnerPending;    // removed from the node's queue already
    };

    // Runs one round of bus arbitration (FR-012, FR-014): each node with
    // pending frames offers its lowest-CanId frame as its candidate; the
    // candidate with the globally lowest CanId wins and is removed from its
    // node's queue. Every other node's candidate stays queued with its
    // RetryCount incremented by 1 (FR-013). No-op (HasWinner=false) when no
    // node currently has a pending frame.
    ArbitrationResult RunArbitration(Network& network);
}
