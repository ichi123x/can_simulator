// LogAnalysis.cpp
#include "pch.h"
#include "LogAnalysis.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <tuple>

namespace CanSim
{
    namespace
    {
        std::vector<std::string> SplitCsv(const std::string& line)
        {
            std::vector<std::string> cells;
            std::string cell;
            std::istringstream in(line);
            while (std::getline(in, cell, ','))
                cells.push_back(cell);
            if (!line.empty() && line.back() == ',')
                cells.push_back(std::string());
            return cells;
        }

        bool ParseInt(const std::string& text, long long emptyValue, long long& value)
        {
            if (text.empty())
            {
                value = emptyValue;
                return true;
            }
            char* end = nullptr;
            value = std::strtoll(text.c_str(), &end, 10);
            return end != text.c_str() && *end == '\0';
        }

        bool ParseCanId(const std::string& text, unsigned& value)
        {
            if (text.size() < 3 || text[0] != '0' || (text[1] != 'x' && text[1] != 'X'))
                return false;
            char* end = nullptr;
            const unsigned long v = std::strtoul(text.c_str() + 2, &end, 16);
            if (end == text.c_str() + 2 || *end != '\0')
                return false;
            value = static_cast<unsigned>(v);
            return true;
        }

        bool ParseStatus(const std::string& text, LogStatus& status)
        {
            if (text == "SUCCESS")
                status = LogStatus::Success;
            else if (text == "ARBITRATION_LOST_RETRY")
                status = LogStatus::RetrySuccess;
            else if (text == "OVERWRITE")
                status = LogStatus::Overwrite;
            else
                return false;
            return true;
        }

        std::string LineError(long long line, const std::string& what)
        {
            std::ostringstream os;
            os << "line " << line << ": " << what;
            return os.str();
        }

        // 1, 2 or 5 x 10^m, the smallest such value not below x (x > 0).
        double NiceCeil(double x)
        {
            const double p = std::pow(10.0, std::floor(std::log10(x)));
            const double m = x / p;
            const double step = m <= 1.0 + 1e-9 ? 1.0 : (m <= 2.0 + 1e-9 ? 2.0 : (m <= 5.0 + 1e-9 ? 5.0 : 10.0));
            return step * p;
        }

        void FillStats(LatencyRow& row)
        {
            std::sort(row.Values.begin(), row.Values.end());
            row.Count = static_cast<long long>(row.Values.size());
            if (row.Count == 0)
                return;
            row.MinUs = row.Values.front();
            row.MaxUs = row.Values.back();
            double sum = 0.0;
            for (long long v : row.Values)
                sum += static_cast<double>(v);
            row.MeanUs = sum / static_cast<double>(row.Count);
            const long long rank = (row.Count * 95 + 99) / 100; // ceil(0.95 n), exact in integers
            row.P95Us = row.Values[static_cast<std::size_t>(rank - 1)];
        }
    }

    bool ReadLogRecords(const std::wstring& path, std::vector<LogRecord>& records, std::string& error)
    {
        records.clear();
        error.clear();
        std::ifstream in(path.c_str(), std::ios::binary);
        if (!in)
        {
            error = "cannot open the file";
            return false;
        }

        std::string line;
        if (!std::getline(in, line))
        {
            error = LineError(1, "no header");
            return false;
        }
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        const std::vector<std::string> header = SplitCsv(line);
        const char* names[] = { "CAN_ID", "Tx_Node", "Rx_Node", "Tx_Request", "Tx_Start", "Tx_End", "Rx_End", "Status" };
        const int kColumns = sizeof(names) / sizeof(names[0]);
        std::size_t col[kColumns];
        std::size_t needed = 0;
        for (int i = 0; i < kColumns; ++i)
        {
            const auto it = std::find(header.begin(), header.end(), names[i]);
            if (it == header.end())
            {
                error = LineError(1, std::string("no column ") + names[i]);
                return false;
            }
            col[i] = static_cast<std::size_t>(it - header.begin());
            needed = (std::max)(needed, col[i] + 1);
        }

        long long lineNo = 1;
        while (std::getline(in, line))
        {
            ++lineNo;
            if (!line.empty() && line.back() == '\r')
                line.pop_back();
            if (line.empty())
                continue;
            const std::vector<std::string> cells = SplitCsv(line);
            if (cells.size() < needed)
            {
                error = LineError(lineNo, "too few columns");
                return false;
            }
            LogRecord r;
            long long txNode = 0, rxNode = 0;
            if (!ParseCanId(cells[col[0]], r.CanId))
            {
                error = LineError(lineNo, "bad CAN_ID");
                return false;
            }
            if (!ParseInt(cells[col[1]], 0, txNode) || !ParseInt(cells[col[2]], 0, rxNode) ||
                !ParseInt(cells[col[3]], -1, r.TxRequestUs) || !ParseInt(cells[col[4]], -1, r.TxStartUs) ||
                !ParseInt(cells[col[5]], -1, r.TxEndUs) || !ParseInt(cells[col[6]], -1, r.RxEndUs))
            {
                error = LineError(lineNo, "bad number");
                return false;
            }
            if (!ParseStatus(cells[col[7]], r.Status))
            {
                error = LineError(lineNo, "bad Status");
                return false;
            }
            r.TxNode = static_cast<int>(txNode);
            r.RxNode = static_cast<int>(rxNode);
            records.push_back(r);
        }
        return true;
    }

    LatencyAnalysis AnalyzeLatency(const std::vector<LogRecord>& records)
    {
        LatencyAnalysis result;
        LatencyRow all;
        std::map<std::pair<int, int>, LatencyRow> pairs;
        for (const LogRecord& r : records)
        {
            if (r.Status == LogStatus::Overwrite)
            {
                ++result.OverwriteCount;
                continue;
            }
            if (r.RxNode <= 0 || r.RxEndUs < 0 || r.TxRequestUs < 0)
                continue;
            const long long t = r.RxEndUs - r.TxRequestUs;
            all.Values.push_back(t);
            LatencyRow& row = pairs[std::make_pair(r.TxNode, r.RxNode)];
            row.TxNode = r.TxNode;
            row.RxNode = r.RxNode;
            row.Values.push_back(t);
        }
        FillStats(all);
        result.Rows.push_back(all);
        for (auto& entry : pairs)
        {
            FillStats(entry.second);
            result.Rows.push_back(entry.second);
        }
        return result;
    }

    Histogram MakeHistogram(const std::vector<long long>& sortedValues)
    {
        Histogram h;
        if (sortedValues.empty())
            return h;
        const long long lo = sortedValues.front();
        const long long hi = sortedValues.back();
        if (hi == lo)
        {
            h.StartUs = lo;
            h.WidthUs = 1;
            h.Counts.assign(1, static_cast<long long>(sortedValues.size()));
            return h;
        }
        const double n = static_cast<double>(sortedValues.size());
        const int target = (std::max)(10, (std::min)(50, static_cast<int>(std::ceil(std::sqrt(n)))));
        h.WidthUs = (std::max)(1LL, static_cast<long long>(std::llround(NiceCeil(static_cast<double>(hi - lo) / target))));
        // floor division, also for negative values
        h.StartUs = lo >= 0 ? (lo / h.WidthUs) * h.WidthUs : -((-lo + h.WidthUs - 1) / h.WidthUs) * h.WidthUs;
        const long long bins = (hi - h.StartUs) / h.WidthUs + 1;
        h.Counts.assign(static_cast<std::size_t>(bins), 0);
        for (long long v : sortedValues)
            ++h.Counts[static_cast<std::size_t>((v - h.StartUs) / h.WidthUs)];
        return h;
    }

    BusLoadAnalysis AnalyzeBusLoad(const std::vector<LogRecord>& records)
    {
        BusLoadAnalysis result;
        std::set<std::tuple<int, unsigned, long long>> seen;
        std::vector<const LogRecord*> transmissions;
        long long lastEnd = 0;
        for (const LogRecord& r : records)
        {
            if (r.Status == LogStatus::Overwrite || r.TxStartUs < 0 || r.TxEndUs <= r.TxStartUs)
                continue;
            if (!seen.insert(std::make_tuple(r.TxNode, r.CanId, r.TxStartUs)).second)
                continue; // the same transmission seen by another receiver
            transmissions.push_back(&r);
            lastEnd = (std::max)(lastEnd, r.TxEndUs);
        }
        result.TransmissionCount = static_cast<long long>(transmissions.size());

        const long long section = result.SectionUs;
        result.RangeUs = lastEnd <= 0 ? section : ((lastEnd + section - 1) / section) * section;
        const std::size_t sections = static_cast<std::size_t>(result.RangeUs / section);
        std::vector<long long> busy(sections, 0);
        std::map<int, EcuBusUsage> ecus;
        long long total = 0;
        for (const LogRecord* r : transmissions)
        {
            for (long long t = r->TxStartUs; t < r->TxEndUs;)
            {
                const long long index = t / section;
                const long long end = (std::min)(r->TxEndUs, (index + 1) * section);
                busy[static_cast<std::size_t>(index)] += end - t;
                t = end;
            }
            const long long duration = r->TxEndUs - r->TxStartUs;
            total += duration;
            EcuBusUsage& e = ecus[r->TxNode];
            e.Node = r->TxNode;
            ++e.FrameCount;
            e.BusyUs += duration;
        }

        result.SectionPct.resize(sections);
        for (std::size_t i = 0; i < sections; ++i)
        {
            result.SectionPct[i] = 100.0 * static_cast<double>(busy[i]) / static_cast<double>(section);
            if (result.SectionPct[i] > result.MaxPct)
            {
                result.MaxPct = result.SectionPct[i];
                result.MaxSection = static_cast<int>(i);
            }
        }
        const double range = static_cast<double>(result.RangeUs);
        result.AveragePct = 100.0 * static_cast<double>(total) / range;
        for (auto& entry : ecus)
        {
            entry.second.UsagePct = 100.0 * static_cast<double>(entry.second.BusyUs) / range;
            result.Ecus.push_back(entry.second);
        }
        return result;
    }

    std::vector<double> NiceTicks(double lo, double hi, int approxCount)
    {
        if (!(hi > lo) || approxCount <= 0)
            return { lo };
        const double step = NiceCeil((hi - lo) / approxCount);
        std::vector<double> ticks;
        for (double t = std::ceil(lo / step - 1e-9) * step; t <= hi + step * 1e-9; t += step)
            ticks.push_back(std::fabs(t) < step * 1e-9 ? 0.0 : t);
        return ticks;
    }

    std::vector<double> MaxPerColumn(const std::vector<double>& values, int columns)
    {
        const std::size_t n = values.size();
        if (columns <= 0 || n <= static_cast<std::size_t>(columns))
            return values;
        std::vector<double> out(static_cast<std::size_t>(columns));
        for (std::size_t c = 0; c < out.size(); ++c)
        {
            const std::size_t begin = c * n / out.size();
            const std::size_t end = (c + 1) * n / out.size();
            double m = values[begin];
            for (std::size_t i = begin + 1; i < end; ++i)
                m = (std::max)(m, values[i]);
            out[c] = m;
        }
        return out;
    }
}
