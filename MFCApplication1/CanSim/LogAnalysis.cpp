// LogAnalysis.cpp
#include "pch.h"
#include "LogAnalysis.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
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

        // Sorts the values and gives count, min, max, mean and the nearest-rank
        // P95 (all 0 for no values). Shared by 028 and 029.
        void ComputeStats(std::vector<long long>& values, long long& count, long long& minUs, long long& maxUs,
            double& meanUs, long long& p95Us)
        {
            std::sort(values.begin(), values.end());
            count = static_cast<long long>(values.size());
            minUs = maxUs = p95Us = 0;
            meanUs = 0.0;
            if (count == 0)
                return;
            minUs = values.front();
            maxUs = values.back();
            double sum = 0.0;
            for (long long v : values)
                sum += static_cast<double>(v);
            meanUs = sum / static_cast<double>(count);
            const long long rank = (count * 95 + 99) / 100; // ceil(0.95 n), exact in integers
            p95Us = values[static_cast<std::size_t>(rank - 1)];
        }

        void FillStats(LatencyRow& row)
        {
            ComputeStats(row.Values, row.Count, row.MinUs, row.MaxUs, row.MeanUs, row.P95Us);
        }

        void FillStats(TxWaitRow& row)
        {
            ComputeStats(row.Values, row.Count, row.MinUs, row.MaxUs, row.MeanUs, row.P95Us);
        }
    }

    const char* const kLogReadCancelled = "cancelled";

    namespace
    {
        // 029 revision 1: the cells of a data line are parsed in place,
        // without making a string per cell (millions of lines).
        bool ParseIntRange(const char* b, const char* e, long long emptyValue, long long& value)
        {
            if (b == e)
            {
                value = emptyValue;
                return true;
            }
            bool negative = false;
            if (*b == '-' || *b == '+')
            {
                negative = *b == '-';
                ++b;
            }
            if (b == e)
                return false;
            long long v = 0;
            for (; b < e; ++b)
            {
                if (*b < '0' || *b > '9')
                    return false;
                v = v * 10 + (*b - '0');
            }
            value = negative ? -v : v;
            return true;
        }

        bool ParseCanIdRange(const char* b, const char* e, unsigned& value)
        {
            if (e - b < 3 || b[0] != '0' || (b[1] != 'x' && b[1] != 'X'))
                return false;
            unsigned long v = 0;
            for (b += 2; b < e; ++b)
            {
                const char c = *b;
                int d;
                if (c >= '0' && c <= '9')
                    d = c - '0';
                else if (c >= 'a' && c <= 'f')
                    d = c - 'a' + 10;
                else if (c >= 'A' && c <= 'F')
                    d = c - 'A' + 10;
                else
                    return false;
                v = v * 16 + static_cast<unsigned long>(d);
            }
            value = static_cast<unsigned>(v);
            return true;
        }

        bool Equals(const char* b, const char* e, const char* text)
        {
            const std::size_t n = std::strlen(text);
            return static_cast<std::size_t>(e - b) == n && std::memcmp(b, text, n) == 0;
        }

        bool ParseStatusRange(const char* b, const char* e, LogStatus& status)
        {
            if (Equals(b, e, "SUCCESS"))
                status = LogStatus::Success;
            else if (Equals(b, e, "ARBITRATION_LOST_RETRY"))
                status = LogStatus::RetrySuccess;
            else if (Equals(b, e, "OVERWRITE"))
                status = LogStatus::Overwrite;
            else
                return false;
            return true;
        }
    }

    bool ReadLogRecords(const std::wstring& path, std::vector<LogRecord>& records, std::string& error,
        const std::function<bool(long long bytesRead, long long totalBytes)>& progress)
    {
        records.clear();
        error.clear();
        std::ifstream in(path.c_str(), std::ios::binary);
        if (!in)
        {
            error = "cannot open the file";
            return false;
        }
        in.seekg(0, std::ios::end);
        const long long totalBytes = static_cast<long long>(in.tellg());
        in.seekg(0, std::ios::beg);
        if (totalBytes > 0)
            records.reserve(static_cast<std::size_t>(totalBytes / 90)); // a log row is about 100 bytes

        const char* names[] = { "CAN_ID", "Tx_Node", "Rx_Node", "Tx_Request", "Tx_Start", "Tx_End", "Rx_End", "Status" };
        const int kColumns = sizeof(names) / sizeof(names[0]);
        std::size_t col[kColumns] = {};
        std::size_t needed = 0;
        bool haveHeader = false;
        long long lineNo = 0;
        std::vector<const char*> starts; // start of each cell of the current line
        std::vector<const char*> ends;

        // Handles one line [b, e) without the line break; false on an error.
        auto handleLine = [&](const char* b, const char* e) -> bool {
            ++lineNo;
            if (e > b && e[-1] == '\r')
                --e;
            if (!haveHeader)
            {
                const std::vector<std::string> header = SplitCsv(std::string(b, e));
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
                haveHeader = true;
                return true;
            }
            if (b == e)
                return true; // an empty line is skipped
            starts.clear();
            ends.clear();
            const char* cell = b;
            for (const char* p = b; ; ++p)
            {
                if (p == e || *p == ',')
                {
                    starts.push_back(cell);
                    ends.push_back(p);
                    if (p == e)
                        break;
                    cell = p + 1;
                }
            }
            if (starts.size() < needed)
            {
                error = LineError(lineNo, "too few columns");
                return false;
            }
            LogRecord r;
            long long txNode = 0, rxNode = 0;
            if (!ParseCanIdRange(starts[col[0]], ends[col[0]], r.CanId))
            {
                error = LineError(lineNo, "bad CAN_ID");
                return false;
            }
            if (!ParseIntRange(starts[col[1]], ends[col[1]], 0, txNode) ||
                !ParseIntRange(starts[col[2]], ends[col[2]], 0, rxNode) ||
                !ParseIntRange(starts[col[3]], ends[col[3]], -1, r.TxRequestUs) ||
                !ParseIntRange(starts[col[4]], ends[col[4]], -1, r.TxStartUs) ||
                !ParseIntRange(starts[col[5]], ends[col[5]], -1, r.TxEndUs) ||
                !ParseIntRange(starts[col[6]], ends[col[6]], -1, r.RxEndUs))
            {
                error = LineError(lineNo, "bad number");
                return false;
            }
            if (!ParseStatusRange(starts[col[7]], ends[col[7]], r.Status))
            {
                error = LineError(lineNo, "bad Status");
                return false;
            }
            r.TxNode = static_cast<int>(txNode);
            r.RxNode = static_cast<int>(rxNode);
            records.push_back(r);
            return true;
        };

        const std::size_t kChunk = 4 * 1024 * 1024;
        std::vector<char> buffer;
        std::size_t kept = 0; // bytes of an unfinished line at the front of buffer
        long long bytesRead = 0;
        for (;;)
        {
            buffer.resize(kept + kChunk);
            in.read(buffer.data() + kept, static_cast<std::streamsize>(kChunk));
            const std::size_t got = static_cast<std::size_t>(in.gcount());
            if (in.bad())
            {
                error = "cannot read the file";
                return false;
            }
            bytesRead += static_cast<long long>(got);
            const std::size_t size = kept + got;
            const bool atEnd = got < kChunk;
            const char* p = buffer.data();
            const char* end = buffer.data() + size;
            for (;;)
            {
                const char* nl = static_cast<const char*>(std::memchr(p, '\n', static_cast<std::size_t>(end - p)));
                if (nl == nullptr)
                    break;
                if (!handleLine(p, nl))
                    return false;
                p = nl + 1;
            }
            if (atEnd)
            {
                if (p < end && !handleLine(p, end)) // a last line without a line break
                    return false;
                break;
            }
            kept = static_cast<std::size_t>(end - p);
            std::memmove(buffer.data(), p, kept);
            if (progress && !progress(bytesRead, totalBytes))
            {
                records.clear();
                error = kLogReadCancelled;
                return false;
            }
        }
        if (progress && !progress(bytesRead, totalBytes))
        {
            records.clear();
            error = kLogReadCancelled;
            return false;
        }
        if (!haveHeader)
        {
            error = LineError(1, "no header");
            return false;
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

    // ---- feature 029 ----

    std::vector<Transmission> CollectTransmissions(const std::vector<LogRecord>& records, long long& overwriteCount)
    {
        overwriteCount = 0;
        // 029 revision 1: one bucket per 11-bit CAN ID (the last one also takes
        // any larger ID). The log is in time order and repeats a transmission on
        // consecutive rows (one per receiver), so a bucket normally needs no
        // sorting, only the neighbours removed; this keeps Debug builds fast.
        const std::size_t kBuckets = 0x800;
        std::vector<std::vector<Transmission>> buckets(kBuckets);
        for (const LogRecord& r : records)
        {
            if (r.Status == LogStatus::Overwrite)
            {
                ++overwriteCount;
                continue;
            }
            if (r.TxRequestUs < 0 || r.TxStartUs < 0)
                continue;
            std::vector<Transmission>& bucket = buckets[(std::min)(static_cast<std::size_t>(r.CanId), kBuckets - 1)];
            if (!bucket.empty() && bucket.back().CanId == r.CanId && bucket.back().TxStartUs == r.TxStartUs &&
                bucket.back().TxNode == r.TxNode)
                continue; // the same transmission seen by another receiver
            Transmission t;
            t.CanId = r.CanId;
            t.TxNode = r.TxNode;
            t.TxRequestUs = r.TxRequestUs;
            t.TxStartUs = r.TxStartUs;
            bucket.push_back(t);
        }

        auto less = [](const Transmission& a, const Transmission& b) {
            return std::tie(a.CanId, a.TxStartUs, a.TxNode) < std::tie(b.CanId, b.TxStartUs, b.TxNode);
        };
        auto same = [](const Transmission& a, const Transmission& b) {
            return a.CanId == b.CanId && a.TxStartUs == b.TxStartUs && a.TxNode == b.TxNode;
        };
        std::size_t total = 0;
        for (const std::vector<Transmission>& bucket : buckets)
            total += bucket.size();
        std::vector<Transmission> all;
        all.reserve(total);
        for (std::vector<Transmission>& bucket : buckets)
        {
            if (bucket.empty())
                continue;
            if (!std::is_sorted(bucket.begin(), bucket.end(), less))
                std::sort(bucket.begin(), bucket.end(), less);
            bucket.erase(std::unique(bucket.begin(), bucket.end(), same), bucket.end());
            all.insert(all.end(), bucket.begin(), bucket.end());
        }
        return all;
    }

    TxWaitAnalysis AnalyzeTxWait(const std::vector<LogRecord>& records)
    {
        TxWaitAnalysis result;
        const std::vector<Transmission> transmissions = CollectTransmissions(records, result.OverwriteCount);
        TxWaitRow all;
        all.All = true;
        std::vector<TxWaitRow> rows;
        for (const Transmission& t : transmissions)
        {
            const long long wait = t.TxStartUs - t.TxRequestUs;
            all.Values.push_back(wait);
            if (rows.empty() || rows.back().CanId != t.CanId) // sorted by CAN ID
            {
                TxWaitRow row;
                row.CanId = t.CanId;
                row.TxNode = t.TxNode;
                rows.push_back(row);
            }
            rows.back().Values.push_back(wait);
        }
        FillStats(all);
        result.Rows.push_back(all);
        for (TxWaitRow& row : rows)
        {
            FillStats(row);
            result.Rows.push_back(row);
        }
        return result;
    }

    JitterAnalysis AnalyzeJitter(const std::vector<LogRecord>& records)
    {
        JitterAnalysis result;
        long long overwrites = 0;
        const std::vector<Transmission> transmissions = CollectTransmissions(records, overwrites);
        std::size_t begin = 0;
        while (begin < transmissions.size())
        {
            std::size_t end = begin;
            while (end < transmissions.size() && transmissions[end].CanId == transmissions[begin].CanId)
                ++end;

            JitterRow row;
            row.CanId = transmissions[begin].CanId;
            row.TxNode = transmissions[begin].TxNode;
            std::map<long long, long long> intervals; // request interval -> how often
            for (std::size_t k = begin + 1; k < end; ++k)
            {
                const Transmission& prev = transmissions[k - 1];
                const Transmission& cur = transmissions[k];
                const long long requestInterval = cur.TxRequestUs - prev.TxRequestUs;
                JitterPoint p;
                p.TimeUs = cur.TxStartUs;
                p.DeviationUs = (cur.TxStartUs - prev.TxStartUs) - requestInterval;
                row.Points.push_back(p);
                ++intervals[requestInterval];
            }
            row.Count = static_cast<long long>(row.Points.size());
            if (row.Count > 0)
            {
                long long best = 0;
                for (const auto& entry : intervals) // ascending, so a tie keeps the smaller interval
                {
                    if (entry.second > best)
                    {
                        best = entry.second;
                        row.PeriodUs = entry.first;
                    }
                }
                row.MinUs = row.MaxUs = row.Points.front().DeviationUs;
                double sum = 0.0;
                for (const JitterPoint& p : row.Points)
                {
                    row.MinUs = (std::min)(row.MinUs, p.DeviationUs);
                    row.MaxUs = (std::max)(row.MaxUs, p.DeviationUs);
                    sum += static_cast<double>(p.DeviationUs);
                }
                const double mean = sum / static_cast<double>(row.Count);
                double squares = 0.0;
                for (const JitterPoint& p : row.Points)
                {
                    const double d = static_cast<double>(p.DeviationUs) - mean;
                    squares += d * d;
                }
                row.StdDevUs = std::sqrt(squares / static_cast<double>(row.Count)); // population, as the summary CSV
            }
            result.Rows.push_back(row);
            begin = end;
        }
        return result;
    }

    std::vector<ColumnRange> MinMaxPerColumn(const std::vector<JitterPoint>& points, long long t0, long long t1, int columns)
    {
        std::vector<ColumnRange> out;
        if (columns <= 0)
            return out;
        out.resize(static_cast<std::size_t>(columns));
        const long long span = (std::max)(1LL, t1 - t0 + 1);
        for (const JitterPoint& p : points)
        {
            long long c = (p.TimeUs - t0) * columns / span;
            c = (std::max)(0LL, (std::min)(c, static_cast<long long>(columns) - 1));
            ColumnRange& r = out[static_cast<std::size_t>(c)];
            if (!r.Has)
            {
                r.Has = true;
                r.MinUs = r.MaxUs = p.DeviationUs;
            }
            else
            {
                r.MinUs = (std::min)(r.MinUs, p.DeviationUs);
                r.MaxUs = (std::max)(r.MaxUs, p.DeviationUs);
            }
        }
        return out;
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
