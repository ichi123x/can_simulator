// CanTypes.h
// Plain C++ value types for the CAN bus simulation core.
// No dependency on MFC / Win32 GUI types (see plan.md Structure Decision).

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <set>
#include <deque>

namespace CanSim
{
    // FR-012 arbitration / FR-013 retry / FR-011 overwrite state per node.
    enum class TxState
    {
        Idle,
        ArbitratingTransmitting,
        Won_Complete,
        Lost_Waiting,
        RetryPending
    };

    // Startup state machine of a node (feature 002 FR-002).
    // Transitions only ever move forward through this order.
    enum class StartupState
    {
        PoweredOff,
        Initializing,
        BusSyncDetecting,
        Normal
    };

    // FR-006 DES event kinds. Order below also defines same-timestamp
    // processing priority (see EventQueue.h) per FR-007. The startup events
    // sit between RxProcess and TxRequest so that a node becoming Normal at
    // time T can still join the arbitration triggered at T (002 research.md).
    enum class EventKind
    {
        TxComplete = 0,
        IntermissionEnd = 1,
        RxProcess = 2,
        PowerOn = 3,
        InitDone = 4,
        SyncComplete = 5,
        TxRequest = 6,
        BusIdle = 7,
        SimulationEnd = 8
    };

    // data-model.md: FrameDefinition
    struct FrameDefinition
    {
        std::string Name;            // user given frame name, UTF-8, 0..32 characters (feature 006)
        unsigned CanId = 0;          // 0x000..0x7FF (11-bit standard ID)
        unsigned Dlc = 0;            // 0..8
        std::vector<std::uint8_t> Data; // always Dlc bytes of 0xFF (feature 006 FR-014)
        long long PeriodUs = 0;      // > 0
        long long InitialOffsetUs = 0; // >= 0 and < PeriodUs; counted from the node's Normal entry
    };

    // Feature 006: transmit frame list limits (domain spec FR-091, FR-081).
    const int kMaxFramesPerNode = 100;
    const int kMaxFrameNameChars = 32;
    const int kMaxEcuLabelChars = 32;   // 022: ECU label length
    const int kEcuLabelShownChars = 10; // 022: characters shown in the diagram, then "..."
    const long long kMaxPeriodMs = 3600000; // 1 hour; only enforced by the editor

    // data-model.md: PendingFrame
    // FrameDefIndex indexes into the owning Node's Frames vector.
    struct PendingFrame
    {
        std::size_t FrameDefIndex = 0;
        long long TxRequestUs = 0;               // first request time, FR-011
        std::vector<std::uint8_t> CurrentData;    // latest data, FR-011
        int RetryCount = 0;                       // FR-013
        int UpdateCount = 0;                      // data updates while waiting (domain spec FR-024)
    };

    // Feature 008 / domain spec FR-090: frame buffers of every ECU.
    const int kTxBufferCount = 16; // frames that may wait for (or be in) transmission at once
    const int kRxBufferCount = 8;  // received frames waiting for the application (FIFO)

    // data-model.md: Node
    struct Node
    {
        int NodeId = 0;                 // 1..16, unique within Network
        std::string Name;
        std::string Label;              // 022: ECU label shown in the diagram, UTF-8, 0..32 characters, empty by default
        std::vector<FrameDefinition> Frames;
        bool RxFilterAll = true;        // FR-005: no filter configured -> receive all IDs
        std::set<unsigned> RxFilter;    // used only when RxFilterAll == false
        long long ReceiveDelayUs = 0;   // FR-016: added to Rx end time to get Rx process time

        // --- frame buffers (feature 008, domain spec 3.7a) ---
        std::deque<long long> RxBuffer; // Rx_Process times of the frames held in the receive FIFO (ascending, at most 8)
        int TxDiscardCount = 0;         // transmit requests dropped because the 16 transmit buffers were in use
        int RxDiscardCount = 0;         // received frames dropped because the 8 receive buffers were in use

        // --- startup control (feature 002) ---
        long long PowerOnTimeUs = 0;     // 002 FR-001, >= 0, default 0
        long long InitDurationUs = 0;    // 002 FR-001, >= 0, default 0
        StartupState Startup = StartupState::PoweredOff;
        long long InitDoneUs = -1;       // -1 == not reached yet
        long long NormalEntryUs = -1;    // -1 == not reached yet
        long long SyncTargetNs = -1;     // currently valid sync-completion time; only
                                         // meaningful while Startup == BusSyncDetecting

        TxState State = TxState::Idle;
        std::vector<PendingFrame> PendingFrames; // queued/pending frames, at most one per FrameDefinition
    };

    // data-model.md: Network
    struct Network
    {
        long long BitRateBps = 500000;  // FR-002: 125000/250000/500000/1000000
        std::vector<Node> Nodes;        // FR-001: 1..16
    };

    // Project level settings stored in the [Project] section of a .canprj
    // file (feature 004 data-model.md). The bit rate lives in
    // Network::BitRateBps so that there is a single source for it.
    struct ProjectSettings
    {
        int EcuCount = 2;           // 0..16
        int DurationSec = 10;       // 1..600
        bool FrameDisplay = true;   // feature 021: on by default (startup and a file without FrameDisplay=)
        int Speed = 0;              // -10..10 (feature 026; was -13..10); 0 = real time
    };

    const int kMinDurationSec = 1;
    const int kMaxDurationSec = 600;

    // Feature 005: the simulation time runs at 2^Speed times the real time.
    // kInstantSpeed does not wait at all.
    const int kMinSpeed = -10;        // feature 026 (was -13)
    const int kLegacyMinSpeed = -13;  // feature 026: older project files may say -13..-11; read as kMinSpeed
    const int kMaxSpeed = 10;
    const int kInstantSpeed = 10;

    // DES event. NodeIndex == -1 marks a bus-wide event (BusIdle, SimulationEnd).
    // NodeIndex is the *position* of the node within Network::Nodes (which is
    // built sorted by ascending configured Node::NodeId), not Node::NodeId
    // itself -- using the index for the FR-007 same-timestamp tie-break rule
    // (lowest configured NodeId first) is equivalent since the vector is
    // already sorted, and it avoids an
    // extra NodeId -> index lookup on every event.
    struct Event
    {
        long long TimeNs = 0;
        EventKind Kind = EventKind::TxRequest;
        int NodeIndex = -1;
        std::size_t FrameDefIndex = 0; // meaningful for per-node events only
        unsigned CanId = 0;            // TxRequest only: same-time tie-break (feature 008)
    };

    // data-model.md: LogRow
    struct LogRow
    {
        long long TimestampUs = 0;
        unsigned CanId = 0;
        unsigned Dlc = 0;
        int TxNodeId = 0;
        int RxNodeId = -1;      // -1 == no receiving node for this row
        // Times in us. -1 means "blank" in the CSV (OVERWRITE rows, feature 008).
        long long TxRequestUs = 0;
        long long TxStartUs = 0;
        long long TxEndUs = 0;
        long long RxStartUs = 0;
        long long RxEndUs = 0;
        long long RxProcessUs = 0;
        double BusLoadPct = 0.0;
        std::string Status;     // "SUCCESS", "ARBITRATION_LOST_RETRY" or "OVERWRITE" (feature 008)
        int RetryCount = 0;
        int UpdateCount = 0;    // data updates while waiting (log column Update_Count)
    };

    const char* const kStatusOverwrite = "OVERWRITE";
}
