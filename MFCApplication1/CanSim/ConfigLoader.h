// ConfigLoader.h
// Reads and validates a network configuration file in the format defined by
// contracts/config-input-format.md (FR-001..FR-005).

#pragma once

#include "CanTypes.h"
#include <string>

namespace CanSim
{
    struct ConfigLoadResult
    {
        bool Success = false;
        Network Config;
        std::string ErrorMessage; // set only when Success == false
    };

    // filePath: a Windows path, may contain non-ASCII characters.
    ConfigLoadResult LoadConfig(const std::wstring& filePath);
}
