// ProjectFile.h
// .canprj project support (feature 004): saving a project and building the
// network that is actually simulated from the project settings.
// Loading is done by ConfigLoader (the .canprj format extends the .cfg one).

#pragma once

#include "CanTypes.h"
#include <string>

namespace CanSim
{
    // Writes settings + every node/frame definition in the .canprj format
    // (contracts/project-file-format.md) as UTF-8. Comments of the original
    // file are not preserved. Returns false if the file cannot be written.
    bool SaveProject(const std::wstring& filePath, const ProjectSettings& settings,
        const Network& definitions);

    // Network used for a run (004 FR-011, research.md decision 2): ECU n is the
    // node with NodeId n, for n = 1..settings.EcuCount. A definition with that
    // NodeId is used as is; otherwise an ECU without frames named "ECU<n>" is
    // added. Definitions with a NodeId above EcuCount are not simulated.
    Network BuildRunNetwork(const ProjectSettings& settings, const Network& definitions);
}
