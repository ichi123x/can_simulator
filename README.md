# CAN Bus Simulator

A CAN 2.0A compliant bus simulator for automotive embedded systems development, built with MFC/C++ using Spec-Driven Development (SDD).

---

## Overview

This simulator faithfully reproduces CAN bus behavior including arbitration, automatic retransmission, and frame buffering — the same mechanisms found in real automotive ECUs.

**Key capabilities:**
- Up to 16 ECUs with real automotive labels (Engine, Brake/ESC, Gateway, etc.)
- CAN 2.0A arbitration (ID-priority based, bit-by-bit)
- Automatic retransmission on arbitration loss
- Tx/Rx buffer management with OVERWRITE detection
- Node startup control (power-on → init → bus sync → normal operation)
- Adjustable playback speed (×1/1024 to ×1024)
- Analysis tools: reception latency, bus load, Tx wait time, jitter

## Features

| # | Feature |
|---|---------|
| 001 | CAN simulation core (DES engine, arbitration, retransmission) |
| 002 | Node startup control (bus sync detection) |
| 003 | CAN bus diagram view (up to 16 ECUs) |
| 004 | Menu-driven project settings (save/load .canprj) |
| 005 | Replay clock with adjustable speed |
| 006 | Frame definition editor (per ECU, up to 100 frames) |
| 007–016 | UI polish, ECU highlight, frame display, flow animation |
| 017–024 | Frame toggle, toolbar layout, area labels, ECU labels, startup args |
| 025–027 | Log viewer, startup CSV, log clear |
| 028 | Analysis: reception latency & bus load charts |
| 029 | Analysis: Tx wait time & jitter charts |

## Performance

Tested with 16 ECUs, 500 kbps, 600 seconds simulation:

| Metric | Value |
|--------|-------|
| Log rows generated | 1,930,000+ |
| Log write time (Release) | 0.9 sec |
| Log analysis time (Release) | 0.7 sec |
| Bus utilization (vehicle demo) | ~3–19% |
| Retransmissions (16 ECU, 5 sec) | 990 frames |

## Development Approach

This project was developed using **Spec-Driven Development (SDD)** with **Claude Code + Spec Kit**:

```
/speckit.specify   → Write spec.md from natural language requirements
/speckit.plan      → Generate implementation plan
/speckit.tasks     → Generate task list with verification steps (ST/DT)
/speckit.implement → Execute implementation
/speckit.converge  → Verify all spec requirements are met
```

All 29 features follow this workflow. Each `tasks.md` records verification results inline:

```
T007 … Result: PASS (8 files, 0 warnings, 0 errors)
T013 … Result: PASS (confirmed via screenshot of folder selection dialog)
```

**Constitution v1.3.0** governs the project: CAN 2.0A compliance, max 16 nodes,
frame-unit DES, UI/logic separation (CanSim/ is plain C++ with no MFC dependency).

## Build

Visual Studio 2022, MFC (dynamic link), Unicode.

```powershell
# Debug x64
msbuild MFCApplication1\MFCApplication1.sln /p:Configuration=Debug /p:Platform=x64

# Release x64
msbuild MFCApplication1\MFCApplication1.sln /p:Configuration=Release /p:Platform=x64

# Win32 (note: solution-level platform name is "x86", not "Win32")
msbuild MFCApplication1\MFCApplication1.sln /p:Configuration=Debug /p:Platform=x86
```

All 4 configurations (Debug/Release x Win32/x64) must build clean.

## Verification Tool

```bat
REM Build
cl /nologo /std:c++14 /EHsc /I tools\cansim_runner /I MFCApplication1\CanSim ^
   /Fo"%TEMP%\cansim\\" /Fe"%TEMP%\cansim\checks.exe" ^
   tools\cansim_runner\checks.cpp MFCApplication1\CanSim\*.cpp

REM Run all success criteria (exit code 0 = all pass)
"%TEMP%\cansim\checks.exe" MFCApplication1\CanSim\SampleConfigs "%TEMP%\cansim"
```

Covers features 001-011, 014-015, 017-020, 022-029 (non-GUI criteria).

## Architecture

```
MFCApplication1/
├── CanSim/              # Core logic -- plain C++, no MFC dependency
│   ├── SimulationEngine # DES main loop
│   ├── Network          # Arbitration, retransmission, Rx filter
│   ├── EventQueue       # Priority queue (time + node order)
│   ├── FrameTiming      # CAN bit stuffing, CRC-15, frame duration
│   ├── LogAnalysis      # Post-run analysis (latency, load, jitter)
│   ├── ReplayClock      # Playback speed control (QPC-based)
│   └── SampleConfigs/   # Sample .canprj files (2/4/8/16 ECU vehicle)
├── MFCApplication1Dlg   # Main dialog (thin glue code only)
├── FrameEditDlg         # Frame definition editor (modeless)
├── AnalysisDlg          # Analysis windows (4 types)
└── LogViewDlg           # Simulation log viewer
specs/
├── 001-can-simulation-core/
├── 002-node-startup/
│   ...
└── 029-tx-wait-jitter/
tools/
└── cansim_runner/       # CLI runner + batch verification tool
```

## Tech Stack

- **Language**: C++14
- **UI Framework**: MFC (Microsoft Foundation Classes)
- **Build**: MSBuild / Visual Studio 2022
- **Platform**: Windows (x86/x64)
- **Development method**: Spec-Driven Development (SDD) with Claude Code + Spec Kit

## Author

Embedded systems engineer with automotive CAN experience (SH, V850, R3000 architectures).  
Developed as a portfolio project demonstrating SDD methodology and CAN domain expertise.
