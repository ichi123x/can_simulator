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

    // ---- feature 027: Log > Clear log ----
    // The three result files a run writes next to the project file, in this
    // order: frame log, summary, startup record.
    std::vector<std::wstring> ResultFilePaths(const std::wstring& projectPath);

    // Those of ResultFilePaths that exist as regular files (a directory of the
    // same name does not count), in the same order. Empty for an empty path.
    std::vector<std::wstring> ExistingResultFiles(const std::wstring& projectPath);

    struct LogClearResult
    {
        std::vector<std::wstring> Deleted; // in the order given
        std::vector<std::wstring> Failed;  // read-only, open in another program, ...
    };

    // Deletes each path. File attributes are left alone, so a read-only file
    // ends up in Failed.
    LogClearResult DeleteResultFiles(const std::vector<std::wstring>& paths);
}
