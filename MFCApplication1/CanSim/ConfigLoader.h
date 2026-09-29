// ConfigLoader.h
// Reads and validates a network configuration file in the format defined by
// contracts/config-input-format.md (FR-001..FR-005), extended by feature 004
// into the .canprj project file (optional [Project] section, zero nodes
// allowed; specs/004-menu-project-settings/contracts/project-file-format.md).

#pragma once

#include "CanTypes.h"
#include <string>

namespace CanSim
{
    struct ConfigLoadResult
    {
        bool Success = false;
        Network Config;           // every node definition found in the file
        ProjectSettings Project;  // [Project] section, defaults when absent
        std::string ErrorMessage; // set only when Success == false
    };

    // filePath: a Windows path, may contain non-ASCII characters.
    ConfigLoadResult LoadConfig(const std::wstring& filePath);
}
