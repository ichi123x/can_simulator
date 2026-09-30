// SimulationEngine.cpp
#include "pch.h"
#include "SimulationEngine.h"
#include "Network.h"
#include "FrameTiming.h"
#include "StartupControl.h"

#include <algorithm>

namespace CanSim
{
    SimulationEngine::SimulationEngine(Network network, long long simDurationUs)
        : m_network(std::move(network))
        , m_simDurationNs(simDurationUs * 1000)
    {
    }

    void SimulationEngine::Run(Logger& logger)
    {
        m_queue = EventQueue();
        m_busBusy = false;
        m_txInProgress = false;
        m_logger = &logger;
        m_completedTxUs.clear();
        m_bitTimeNs = m_network.BitRateBps > 0 ? (1000000000LL / m_network.BitRateBps) : 0;
        m_recessiveRunStartNs = 0; // bus is idle from the start of the run

        // FR-021: schedule the periodic requests and the end-of-simulation marker.
        for (std::size_t i = 0; i < m_network.Nodes.size(); ++i)
        {
            Node& node = m_network.Nodes[i];

            // 002 FR-002/FR-003: every node starts powered off and is woken at
            // its configured power-on time.
            node.Startup = StartupState::PoweredOff;
            node.RxBuffer.clear();   // feature 008: buffers start empty
            node.TxDiscardCount = 0;
            node.RxDiscardCount = 0;
            node.InitDoneUs = -1;
            node.NormalEntryUs = -1;
            node.SyncTargetNs = -1;

            Event powerOn;
            powerOn.TimeNs = node.PowerOnTimeUs * 1000;
            powerOn.Kind = EventKind::PowerOn;
            powerOn.NodeIndex = static_cast<int>(i);
            m_queue.Push(powerOn);

            // The first transmit request of every frame is scheduled when the
            // node reaches Normal (feature 006 FR-015, HandleSyncComplete).
        }

        Event endEvent;
        endEvent.TimeNs = m_simDurationNs;
        endEvent.Kind = EventKind::SimulationEnd;
        endEvent.NodeIndex = -1;
        m_queue.Push(endEvent);

        while (!m_queue.Empty())
        {
            Event e = m_queue.PopNext();
            if (e.Kind == EventKind::SimulationEnd || e.TimeNs > m_simDurationNs)
                break;

            switch (e.Kind)
            {
            case EventKind::TxRequest:
                HandleTxRequest(e);
                break;
            case EventKind::BusIdle:
                HandleBusIdle(e);
                break;
            case EventKind::TxComplete:
                HandleTxComplete(e, logger);
                break;
            case EventKind::IntermissionEnd:
                HandleIntermissionEnd(e);
                break;
            case EventKind::PowerOn:
                HandlePowerOn(e);
                break;
            case EventKind::InitDone:
                HandleInitDone(e);
                break;
            case EventKind::SyncComplete:
                HandleSyncComplete(e);
                break;
            default:
                break; // RxProcess: value is computed inline in HandleTxComplete, no separate event needed yet.
            }
        }
    }

    void SimulationEngine::HandleTxRequest(const Event& e)
    {
        Node& node = m_network.Nodes[static_cast<std::size_t>(e.NodeIndex)];
        const long long nowUs = e.TimeNs / 1000;
        const bool transmitting = m_txInProgress && m_currentTx.NodeIndex == e.NodeIndex;
        const EnqueueResult enqueued = EnqueueTxRequest(node, e.FrameDefIndex, nowUs, transmitting);
        if (enqueued == EnqueueResult::Discarded)
        {
            // Domain spec FR-092b: all 16 transmit buffers are in use; the
            // request is dropped and recorded as one OVERWRITE row.
            ++node.TxDiscardCount;
            const FrameDefinition& fd = node.Frames[e.FrameDefIndex];
            LogRow row;
            row.TimestampUs = nowUs;
            row.CanId = fd.CanId;
            row.Dlc = fd.Dlc;
            row.TxNodeId = node.NodeId;
            row.RxNodeId = -1;
            row.TxRequestUs = nowUs;
            row.TxStartUs = -1;
            row.TxEndUs = -1;
            row.RxStartUs = -1;
            row.RxEndUs = -1;
            row.RxProcessUs = -1;
            row.BusLoadPct = ComputeBusLoadPct(nowUs);
            row.Status = kStatusOverwrite;
            row.RetryCount = 0;
            row.UpdateCount = 0;
            m_logger->AddRow(row);
        }

        // Schedule the next period (FR-006), if it still falls within the run.
        const long long nextNs = e.TimeNs + node.Frames[e.FrameDefIndex].PeriodUs * 1000;
        if (nextNs <= m_simDurationNs)
        {
            Event next;
            next.TimeNs = nextNs;
            next.Kind = EventKind::TxRequest;
            next.CanId = node.Frames[e.FrameDefIndex].CanId;
            next.NodeIndex = e.NodeIndex;
            next.FrameDefIndex = e.FrameDefIndex;
            m_queue.Push(next);
        }

        if (!m_busBusy && enqueued != EnqueueResult::Discarded)
        {
            Event busIdle;
            busIdle.TimeNs = e.TimeNs;
            busIdle.Kind = EventKind::BusIdle;
            busIdle.NodeIndex = -1;
            m_queue.Push(busIdle);
        }
    }

    void SimulationEngine::HandleBusIdle(const Event& e)
    {
        if (m_busBusy)
            return;

        ArbitrationResult r = RunArbitration(m_network);
        if (!r.HasWinner)
            return; // FR-012/014: nothing pending, bus stays idle.

        const FrameDefinition& fd =
            m_network.Nodes[static_cast<std::size_t>(r.WinnerNodeIndex)].Frames[r.WinnerFrameDefIndex];
        const FrameTimingResult timing =
            ComputeFrameTiming(fd.CanId, fd.Dlc, r.WinnerPending.CurrentData, m_network.BitRateBps);

        const long long txStartNs = e.TimeNs;
        const long long txEndNs = txStartNs + timing.TxDurationNs;

        m_busBusy = true;
        m_txInProgress = true;
        m_currentTx.NodeIndex = r.WinnerNodeIndex;
        m_currentTx.FrameDefIndex = r.WinnerFrameDefIndex;
        m_currentTx.Pending = r.WinnerPending;
        m_currentTx.TxStartUs = txStartNs / 1000;
        m_currentTx.TxEndUs = txEndNs / 1000;
        m_currentTx.IntermissionNs = timing.IntermissionNs;

        // 002: this transmission ends the current recessive run; the next one
        // starts at its ACK delimiter (TxEnd - 8 bit times).
        m_recessiveRunStartNs = ComputeRecessiveRunStartNs(txEndNs, timing.BitTimeNs);

        // 002 FR-007: any node still counting recessive bits has just seen a
        // dominant bit, so its count restarts. Reschedule it against the new
        // recessive run; the event already queued becomes stale and is dropped
        // by the SyncTargetNs check (research.md decision 2 - no queue
        // cancellation mechanism).
        for (std::size_t i = 0; i < m_network.Nodes.size(); ++i)
        {
            if (m_network.Nodes[i].Startup == StartupState::BusSyncDetecting)
                ScheduleSyncComplete(i, e.TimeNs);
        }

        Event complete;
        complete.TimeNs = txEndNs;
        complete.Kind = EventKind::TxComplete;
        complete.NodeIndex = r.WinnerNodeIndex;
        m_queue.Push(complete);
    }

    void SimulationEngine::HandleTxComplete(const Event& e, Logger& logger)
    {
        const InFlight tx = m_currentTx;
        const long long txEndUs = e.TimeNs / 1000;
        m_txInProgress = false; // EOF: the sender's transmit buffer is free (feature 008)

        m_completedTxUs.emplace_back(tx.TxStartUs, txEndUs);
        const double busLoadPct = ComputeBusLoadPct(txEndUs);

        const Node& txNode = m_network.Nodes[static_cast<std::size_t>(tx.NodeIndex)];
        const FrameDefinition& fd = txNode.Frames[tx.FrameDefIndex];
        const std::string status = (tx.Pending.RetryCount > 0) ? "ARBITRATION_LOST_RETRY" : "SUCCESS";

        bool anyReceiver = false;
        for (std::size_t i = 0; i < m_network.Nodes.size(); ++i)
        {
            if (static_cast<int>(i) == tx.NodeIndex)
                continue;
            Node& rxNode = m_network.Nodes[i];
            // 002 FR-009: only nodes that reached Normal receive frames.
            if (rxNode.Startup != StartupState::Normal)
                continue;
            if (!NodeAcceptsCanId(rxNode, fd.CanId))
                continue;

            anyReceiver = true;
            const long long rxProcessUs = txEndUs + rxNode.ReceiveDelayUs;

            // Receive FIFO of 8 buffers (domain spec FR-093/FR-094). Frames
            // whose Rx_Process has come by now have been taken out; this is
            // done first so that a take-out at the same time frees a buffer.
            while (!rxNode.RxBuffer.empty() && rxNode.RxBuffer.front() <= txEndUs)
                rxNode.RxBuffer.pop_front();
            const bool rxDropped = rxNode.RxBuffer.size() >= static_cast<std::size_t>(kRxBufferCount);
            if (rxDropped)
                ++rxNode.RxDiscardCount;
            else
                rxNode.RxBuffer.push_back(rxProcessUs);

            LogRow row;
            row.TimestampUs = rxProcessUs; // FR-017: representative time = Rx process time
            row.CanId = fd.CanId;
            row.Dlc = fd.Dlc;
            row.TxNodeId = txNode.NodeId;
            row.RxNodeId = rxNode.NodeId;
            row.TxRequestUs = tx.Pending.TxRequestUs;
            row.TxStartUs = tx.TxStartUs;
            row.TxEndUs = txEndUs;
            row.RxStartUs = tx.TxStartUs;  // FR-041 equivalent: Rx start == Tx start
            row.RxEndUs = txEndUs;         // Rx end == Tx end
            row.RxProcessUs = rxProcessUs;
            row.BusLoadPct = busLoadPct;
            row.Status = status;
            row.RetryCount = tx.Pending.RetryCount;
            row.UpdateCount = tx.Pending.UpdateCount;
            if (rxDropped)
            {
                // Domain spec FR-094: the new frame is dropped; no Rx_Process.
                row.TimestampUs = txEndUs;
                row.RxProcessUs = -1;
                row.Status = kStatusOverwrite;
            }
            logger.AddRow(row);
        }

        if (!anyReceiver)
        {
            LogRow row;
            row.TimestampUs = txEndUs; // FR-017: no receiver -> Tx end time
            row.CanId = fd.CanId;
            row.Dlc = fd.Dlc;
            row.TxNodeId = txNode.NodeId;
            row.RxNodeId = -1;
            row.TxRequestUs = tx.Pending.TxRequestUs;
            row.TxStartUs = tx.TxStartUs;
            row.TxEndUs = txEndUs;
            row.RxStartUs = 0;
            row.RxEndUs = 0;
            row.RxProcessUs = 0;
            row.BusLoadPct = busLoadPct;
            row.Status = status;
            row.RetryCount = tx.Pending.RetryCount;
            row.UpdateCount = tx.Pending.UpdateCount;
            logger.AddRow(row);
        }

        Event intermissionEnd;
        intermissionEnd.TimeNs = e.TimeNs + tx.IntermissionNs;
        intermissionEnd.Kind = EventKind::IntermissionEnd;
        intermissionEnd.NodeIndex = tx.NodeIndex;
        m_queue.Push(intermissionEnd);
    }

    void SimulationEngine::HandleIntermissionEnd(const Event& e)
    {
        m_busBusy = false;

        Event busIdle;
        busIdle.TimeNs = e.TimeNs;
        busIdle.Kind = EventKind::BusIdle;
        busIdle.NodeIndex = -1;
        m_queue.Push(busIdle);
    }

    void SimulationEngine::HandlePowerOn(const Event& e)
    {
        Node& node = m_network.Nodes[static_cast<std::size_t>(e.NodeIndex)];
        if (node.Startup != StartupState::PoweredOff)
            return;

        node.Startup = StartupState::Initializing; // 002 FR-003

        Event initDone;
        initDone.TimeNs = e.TimeNs + node.InitDurationUs * 1000;
        initDone.Kind = EventKind::InitDone;
        initDone.NodeIndex = e.NodeIndex;
        m_queue.Push(initDone);
    }

    void SimulationEngine::HandleInitDone(const Event& e)
    {
        Node& node = m_network.Nodes[static_cast<std::size_t>(e.NodeIndex)];
        if (node.Startup != StartupState::Initializing)
            return;

        node.Startup = StartupState::BusSyncDetecting; // 002 FR-004
        node.InitDoneUs = e.TimeNs / 1000;

        ScheduleSyncComplete(static_cast<std::size_t>(e.NodeIndex), e.TimeNs);
    }

    void SimulationEngine::ScheduleSyncComplete(std::size_t nodeIndex, long long detectStartNs)
    {
        Node& node = m_network.Nodes[nodeIndex];
        const long long target =
            ComputeSyncCompleteNs(detectStartNs, m_recessiveRunStartNs, m_bitTimeNs);
        node.SyncTargetNs = target;

        Event sync;
        sync.TimeNs = target;
        sync.Kind = EventKind::SyncComplete;
        sync.NodeIndex = static_cast<int>(nodeIndex);
        m_queue.Push(sync);
    }

    void SimulationEngine::HandleSyncComplete(const Event& e)
    {
        Node& node = m_network.Nodes[static_cast<std::size_t>(e.NodeIndex)];

        // 002 research.md decision 2: lazy invalidation. A stale event (the
        // recessive run was interrupted and the node was rescheduled) is
        // simply dropped.
        if (node.Startup != StartupState::BusSyncDetecting || e.TimeNs != node.SyncTargetNs)
            return;

        node.Startup = StartupState::Normal; // 002 FR-005/FR-006
        node.NormalEntryUs = e.TimeNs / 1000;
        node.SyncTargetNs = -1;

        // Feature 006 FR-015 (domain spec FR-020a): the Normal entry is the
        // origin of the periodic requests. Each frame first requests at
        // NormalEntry + InitialOffset, then every PeriodUs (HandleTxRequest).
        // Since no request exists before Normal any more, the 002 FR-010
        // "hold requests raised during startup" case no longer occurs.
        // SyncComplete sorts before TxRequest at the same time (EventKind
        // order), so an offset of 0 is handled within this same instant.
        for (std::size_t j = 0; j < node.Frames.size(); ++j)
        {
            const long long firstNs = e.TimeNs + node.Frames[j].InitialOffsetUs * 1000;
            if (firstNs > m_simDurationNs)
                continue;
            Event request;
            request.TimeNs = firstNs;
            request.Kind = EventKind::TxRequest;
            request.CanId = node.Frames[j].CanId;
            request.NodeIndex = e.NodeIndex;
            request.FrameDefIndex = j;
            m_queue.Push(request);
        }
    }

    double SimulationEngine::ComputeBusLoadPct(long long atUs)
    {
        const long long windowStart = atUs - kBusLoadWindowUs;

        while (!m_completedTxUs.empty() && m_completedTxUs.front().second <= windowStart)
            m_completedTxUs.erase(m_completedTxUs.begin());

        long long sum = 0;
        for (const auto& p : m_completedTxUs)
        {
            const long long start = std::max(p.first, windowStart);
            const long long end = std::min(p.second, atUs);
            if (end > start)
                sum += (end - start);
        }
        return (static_cast<double>(sum) / static_cast<double>(kBusLoadWindowUs)) * 100.0;
    }
}
