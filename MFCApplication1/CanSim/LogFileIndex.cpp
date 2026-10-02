// LogFileIndex.cpp
#include "pch.h"
#include "LogFileIndex.h"

#include <fstream>

namespace CanSim
{
    bool LogFileIndex::Build(const std::wstring& path)
    {
        m_path = path;
        m_checkpoints.clear();
        m_lineCount = 0;
        m_maxLineLength = 0;

        std::ifstream in(path.c_str(), std::ios::binary);
        if (!in)
            return false;

        const std::size_t kChunk = 1 << 20;
        std::vector<char> buffer(kChunk);
        std::vector<long long> checkpoints;
        checkpoints.push_back(0);
        long long lines = 0;      // completed lines
        long long offset = 0;     // file offset of buffer[0]
        long long lineLength = 0; // characters of the current line so far (CR included)
        bool pendingCR = false;   // the current line so far ends with '\r'
        int maxLength = 0;

        for (;;)
        {
            in.read(buffer.data(), static_cast<std::streamsize>(kChunk));
            const std::streamsize got = in.gcount();
            if (got <= 0)
                break;
            for (std::streamsize i = 0; i < got; ++i)
            {
                const char c = buffer[static_cast<std::size_t>(i)];
                if (c == '\n')
                {
                    const long long length = pendingCR ? lineLength - 1 : lineLength;
                    if (length > maxLength)
                        maxLength = length > 0x7FFFFFFF ? 0x7FFFFFFF : static_cast<int>(length);
                    ++lines;
                    lineLength = 0;
                    pendingCR = false;
                    if (lines % kIndexStride == 0)
                        checkpoints.push_back(offset + i + 1);
                }
                else
                {
                    ++lineLength;
                    pendingCR = c == '\r';
                }
            }
            offset += got;
            if (got < static_cast<std::streamsize>(kChunk))
                break;
        }
        if (in.bad())
            return false;

        if (lineLength > 0)
        {
            const long long length = pendingCR ? lineLength - 1 : lineLength;
            if (length > maxLength)
                maxLength = length > 0x7FFFFFFF ? 0x7FFFFFFF : static_cast<int>(length);
            ++lines;
        }
        // A checkpoint just after the last '\n' points at no line; drop it.
        while (!checkpoints.empty() &&
            static_cast<long long>(checkpoints.size() - 1) * kIndexStride >= lines && checkpoints.size() > 1)
            checkpoints.pop_back();

        m_checkpoints.swap(checkpoints);
        m_lineCount = lines;
        m_maxLineLength = maxLength;
        return true;
    }

    std::vector<std::string> LogFileIndex::ReadLines(long long first, int count) const
    {
        std::vector<std::string> out;
        if (first < 0)
        {
            count = static_cast<int>(first + count < 0 ? 0 : first + count);
            first = 0;
        }
        if (count <= 0 || first >= m_lineCount || m_checkpoints.empty())
            return out;
        long long last = first + count; // exclusive
        if (last > m_lineCount)
            last = m_lineCount;

        std::ifstream in(m_path.c_str(), std::ios::binary);
        if (!in)
            return out;
        const std::size_t checkpoint = static_cast<std::size_t>(first / kIndexStride);
        if (checkpoint >= m_checkpoints.size())
            return out;
        in.seekg(static_cast<std::streamoff>(m_checkpoints[checkpoint]), std::ios::beg);
        if (!in)
            return out;

        std::string line;
        long long index = static_cast<long long>(checkpoint) * kIndexStride;
        out.reserve(static_cast<std::size_t>(last - first));
        while (index < last && std::getline(in, line))
        {
            if (index >= first)
            {
                if (!line.empty() && line.back() == '\r')
                    line.pop_back();
                out.push_back(line);
            }
            ++index;
        }
        return out;
    }

    std::wstring LogPathFor(const std::wstring& projectPath)
    {
        return projectPath + L".log.csv";
    }
}
