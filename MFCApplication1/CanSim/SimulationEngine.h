// SimulationEngine.h
// Ties EventQueue + Network + FrameTiming + Logger together into the DES
// main loop (FR-006, FR-021).

#pragma once

#include "CanTypes.h"
#include "EventQueue.h"
#include "Logger.h"

namespace CanSim
{
    class SimulationEngine
    {
    public:
        SimulationEngine(Network network, long long simDurationUs);

        // Runs the simulation to completion, writing one LogRow per
        // frame x receiving node into logger (FR-017).
        void Run(Logger& logger);

        // Post-run state, used to build the startup records (002 FR-011).
        const Network& GetNetwork() const { return m_network; }

    private:
        void HandleTxRequest(const Event& e);
        void HandleBusIdle(const Event& e);
        void HandleTxComplete(const Event& e, Logger& logger);
        void HandleIntermissionEnd(const Event& e);

        // --- startup control (feature 002) ---
        void HandlePowerOn(const Event& e);
        void HandleInitDone(const Event& e);
        void HandleSyncComplete(const Event& e);
        void ScheduleSyncComplete(std::size_t nodeIndex, long long detectStartNs);

        Network m_network;
        long long m_simDurationNs;
        EventQueue m_queue;

        bool m_busBusy = false;
        // A frame is on the wire (SOF..EOF). It keeps one transmit buffer of
        // its node until EOF (feature 008). The bus stays busy afterwards
        // during the intermission, but the buffer is free by then.
        bool m_txInProgress = false;
        Logger* m_logger = nullptr; // valid during Run()
        struct InFlight
        {
            int NodeIndex = -1;
            std::size_t FrameDefIndex = 0;
            PendingFrame Pending;
            long long TxStartUs = 0;
            long long TxEndUs = 0;
            long long IntermissionNs = 0;
        };
        InFlight m_currentTx;

        // Startup control (feature 002): bit time of the configured bus and the
        // start of the recessive run currently in progress (data-model.md).
        long long m_bitTimeNs = 0;
        long long m_recessiveRunStartNs = 0;

        // Rolling window for Bus_Load_Pct (FR-018), default 100ms (FR-071 equivalent).
        static constexpr long long kBusLoadWindowUs = 100000;
        std::vector<std::pair<long long, long long>> m_completedTxUs; // (start,end)
        double ComputeBusLoadPct(long long atUs);
    };
}
