// Logger.cpp
#include "pch.h"
#include "Logger.h"

#include <cstdio>
#include <fstream>
#include <string>

namespace CanSim
{
    void Logger::AddRow(const LogRow& row)
    {
        m_rows.push_back(row);
    }

    namespace
    {
        // The rows are formatted into one buffer that is written in large
        // blocks: a 600 s run has about two million rows, and a stream
        // insertion per cell (with a temporary string each) took seconds in
        // Release and more than a minute in Debug. The text is the same as
        // before, byte for byte.
        const std::size_t kFlushBytes = 4 * 1024 * 1024;

        void AppendInt(std::string& out, long long value)
        {
            char digits[24];
            int n = 0;
            unsigned long long v = value < 0 ? 0ULL - static_cast<unsigned long long>(value)
                                             : static_cast<unsigned long long>(value);
            do
            {
                digits[n++] = static_cast<char>('0' + v % 10);
                v /= 10;
            } while (v != 0);
            if (value < 0)
                out.push_back('-');
            while (n > 0)
                out.push_back(digits[--n]);
        }

        // "0x" and at least three lower-case hex digits (as setw(3), setfill('0')).
        void AppendCanId(std::string& out, unsigned canId)
        {
            static const char kHex[] = "0123456789abcdef";
            char digits[8];
            int n = 0;
            do
            {
                digits[n++] = kHex[canId & 0xF];
                canId >>= 4;
            } while (canId != 0);
            while (n < 3)
                digits[n++] = '0';
            out.append("0x");
            while (n > 0)
                out.push_back(digits[--n]);
        }

        // A negative time is the "blank" marker of OVERWRITE rows (feature 008).
        void AppendOptional(std::string& out, long long value, bool present)
        {
            if (present && value >= 0)
                AppendInt(out, value);
        }

        // Same text as std::fixed with precision 1.
        void AppendPct(std::string& out, double value)
        {
            char text[64];
            const int n = std::snprintf(text, sizeof(text), "%.1f", value);
            if (n > 0)
                out.append(text, static_cast<std::size_t>(n) < sizeof(text) ? static_cast<std::size_t>(n) : sizeof(text) - 1);
        }
    }

    bool Logger::WriteCsv(const std::wstring& filePath) const
    {
        return WriteLogCsv(m_rows, filePath);
    }

    bool WriteLogCsv(const std::vector<LogRow>& rows, const std::wstring& filePath)
    {
        std::ofstream file(filePath.c_str(), std::ios::binary);
        if (!file)
            return false;

        std::string out;
        out.reserve(kFlushBytes + 4096);
        out.append("Timestamp_us,CAN_ID,DLC,Tx_Node,Rx_Node,Tx_Request,Tx_Start,Tx_End,"
                   "Rx_Start,Rx_End,Rx_Process,Bus_Load_Pct,Status,Retry_Count,Update_Count\r\n");

        for (const LogRow& row : rows)
        {
            const bool hasRx = row.RxNodeId >= 0;
            AppendInt(out, row.TimestampUs);
            out.push_back(',');
            AppendCanId(out, row.CanId);
            out.push_back(',');
            AppendInt(out, row.Dlc);
            out.push_back(',');
            AppendInt(out, row.TxNodeId);
            out.push_back(',');
            if (row.RxNodeId >= 0)
                AppendInt(out, row.RxNodeId);
            out.push_back(',');
            AppendInt(out, row.TxRequestUs);
            out.push_back(',');
            AppendOptional(out, row.TxStartUs, true);
            out.push_back(',');
            AppendOptional(out, row.TxEndUs, true);
            out.push_back(',');
            AppendOptional(out, row.RxStartUs, hasRx);
            out.push_back(',');
            AppendOptional(out, row.RxEndUs, hasRx);
            out.push_back(',');
            AppendOptional(out, row.RxProcessUs, hasRx);
            out.push_back(',');
            AppendPct(out, row.BusLoadPct);
            out.push_back(',');
            out.append(row.Status);
            out.push_back(',');
            AppendInt(out, row.RetryCount);
            out.push_back(',');
            AppendInt(out, row.UpdateCount);
            out.append("\r\n");

            if (out.size() >= kFlushBytes)
            {
                file.write(out.data(), static_cast<std::streamsize>(out.size()));
                out.clear();
            }
        }
        file.write(out.data(), static_cast<std::streamsize>(out.size()));
        file.flush();
        return static_cast<bool>(file);
    }
}
