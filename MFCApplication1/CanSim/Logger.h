// Logger.h
// Collects LogRow records and writes them as CSV per
// contracts/log-output-format.md (FR-017).

#pragma once

#include "CanTypes.h"
#include <string>
#include <vector>

namespace CanSim
{
    class Logger
    {
    public:
        void AddRow(const LogRow& row);
        const std::vector<LogRow>& Rows() const { return m_rows; }

        // Writes UTF-8 CSV with a header row. Returns false if the file
        // could not be created or written.
        bool WriteCsv(const std::wstring& filePath) const;

    private:
        std::vector<LogRow> m_rows;
    };

    // Same CSV as Logger::WriteCsv for rows kept elsewhere (no copy into a Logger).
    bool WriteLogCsv(const std::vector<LogRow>& rows, const std::wstring& filePath);
}
