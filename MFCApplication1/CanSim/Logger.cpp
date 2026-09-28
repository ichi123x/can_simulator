// Logger.cpp
#include "pch.h"
#include "Logger.h"

#include <fstream>
#include <sstream>
#include <iomanip>

namespace CanSim
{
    void Logger::AddRow(const LogRow& row)
    {
        m_rows.push_back(row);
    }

    namespace
    {
        std::string FormatCanId(unsigned canId)
        {
            std::ostringstream os;
            os << "0x" << std::hex << std::nouppercase << std::setw(3) << std::setfill('0') << canId;
            return os.str();
        }

        std::string FormatOptional(long long value, bool present)
        {
            if (!present)
                return "";
            return std::to_string(value);
        }

        std::string FormatOptionalNode(int nodeId)
        {
            if (nodeId < 0)
                return "";
            return std::to_string(nodeId);
        }
    }

    bool Logger::WriteCsv(const std::wstring& filePath) const
    {
        std::ofstream file(filePath.c_str(), std::ios::binary);
        if (!file)
            return false;

        file << "Timestamp_us,CAN_ID,DLC,Tx_Node,Rx_Node,Tx_Request,Tx_Start,Tx_End,"
                "Rx_Start,Rx_End,Rx_Process,Bus_Load_Pct,Status,Retry_Count,Overwrite_Count\r\n";

        for (const LogRow& row : m_rows)
        {
            const bool hasRx = row.RxNodeId >= 0;
            file << row.TimestampUs << ','
                 << FormatCanId(row.CanId) << ','
                 << row.Dlc << ','
                 << row.TxNodeId << ','
                 << FormatOptionalNode(row.RxNodeId) << ','
                 << row.TxRequestUs << ','
                 << row.TxStartUs << ','
                 << row.TxEndUs << ','
                 << FormatOptional(row.RxStartUs, hasRx) << ','
                 << FormatOptional(row.RxEndUs, hasRx) << ','
                 << FormatOptional(row.RxProcessUs, hasRx) << ','
                 << std::fixed << std::setprecision(1) << row.BusLoadPct << ','
                 << row.Status << ','
                 << row.RetryCount << ','
                 << row.OverwriteCount << "\r\n";
        }
        return true;
    }
}
