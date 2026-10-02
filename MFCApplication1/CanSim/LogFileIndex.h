// LogFileIndex.h
// Line index of a simulation log file (feature 025). The log viewer shows
// every line of logs with millions of lines without loading them: Build reads
// the file once and remembers where every kIndexStride-th line starts, then
// ReadLines seeks to the nearest remembered line and reads only what is asked.
// The file is opened for each call and never kept open or written.

#pragma once

#include <string>
#include <vector>

namespace CanSim
{
    class LogFileIndex
    {
    public:
        static const int kIndexStride = 256;

        // Reads the whole file once. False if it cannot be opened or read; the
        // index is then empty. An empty file is 0 lines (true).
        bool Build(const std::wstring& path);

        // Lines are separated by '\n'; a last line without '\n' also counts.
        long long LineCount() const { return m_lineCount; }

        // Longest line in characters, without CR / LF.
        int MaxLineLength() const { return m_maxLineLength; }

        // Lines [first, first + count) clipped to [0, LineCount()), without
        // CR / LF. Empty when the file cannot be read any more.
        std::vector<std::string> ReadLines(long long first, int count) const;

    private:
        std::wstring m_path;
        std::vector<long long> m_checkpoints; // byte offset of line k * kIndexStride
        long long m_lineCount = 0;
        int m_maxLineLength = 0;
    };

    // Frame log written next to a project file (feature 004): "<project>.log.csv".
    std::wstring LogPathFor(const std::wstring& projectPath);
}
