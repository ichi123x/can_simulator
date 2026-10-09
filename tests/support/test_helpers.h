// test_helpers.h（tests/support）
// tools/cansim_runner/checks.cpp から移した補助（機能 030）。機能ごとのテストが共有するものを
// すべてここに置く。checks.cpp は廃止するまで自分のコピーを持ち続ける。
#pragma once

#include "check_support.h"
#include "ConfigLoader.h"
#include "SimulationEngine.h"
#include "Logger.h"
#include "MetricsAggregator.h"
#include "StartupControl.h"
#include "FrameTiming.h"
#include "BusDiagramLayout.h"
#include "ProjectFile.h"
#include "FrameEvents.h"
#include "ReplayClock.h"
#include "LogLines.h"
#include "FrameEditor.h"
#include "SimControls.h"
#include "FlowEffect.h"
#include "LogFileIndex.h"
#include "LogAnalysis.h"
#include <climits>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <direct.h>
#include <io.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace cansimtest
{
    using namespace CanSim;

    // 共通の入力の置き場所。サンプルのプロジェクトと一時出力のディレクトリは構成時に
    // 決まる（tests/CMakeLists.txt）。任意の基準 CSV は環境変数から取る
    // （機能 004 SC-009、tools/cansim_runner/README.md 参照）。
    inline std::string SampleDir()
    {
        return CANSIM_SAMPLE_DIR;
    }

    inline std::string TempDir()
    {
        static const std::string dir = []() {
            std::string d = CANSIM_TEST_TMP_DIR;
            _mkdir(d.c_str());
            return d;
        }();
        return dir;
    }

    inline std::string BaselineDir()
    {
        const char* v = std::getenv("CANSIM_BASELINE_DIR");
        return v ? std::string(v) : std::string();
    }

    inline std::wstring Widen(const std::string& s)
    {
        return std::wstring(s.begin(), s.end());
    }

    inline std::string Join(const std::string& dir, const std::string& name)
    {
        if (dir.empty())
            return name;
        const char last = dir[dir.size() - 1];
        if (last == '\\' || last == '/')
            return dir + name;
        return dir + "\\" + name;
    }

    struct RunResult
    {
        bool Loaded = false;
        std::string Error;
        Network Config;   // 読み込んだまま
        Network EndState; // 実行後（起動の状態など）
        std::vector<LogRow> Rows;
    };

    inline RunResult RunConfigFile(const std::string& path, long long durationUs)
    {
        RunResult r;
        ConfigLoadResult cfg = LoadConfig(Widen(path));
        r.Loaded = cfg.Success;
        r.Error = cfg.ErrorMessage;
        if (!cfg.Success)
            return r;

        r.Config = cfg.Config;
        SimulationEngine engine(cfg.Config, durationUs);
        Logger logger;
        engine.Run(logger);
        r.Rows = logger.Rows();
        r.EndState = engine.GetNetwork();
        return r;
    }

    inline bool WriteTextFile(const std::string& path, const std::string& content)
    {
        std::ofstream f(path.c_str(), std::ios::binary);
        if (!f)
            return false;
        f << content;
        return true;
    }

    inline const Node* FindNode(const Network& net, int nodeId)
    {
        for (const Node& n : net.Nodes)
            if (n.NodeId == nodeId)
                return &n;
        return nullptr;
    }

    inline bool RowsEqual(const std::vector<LogRow>& a, const std::vector<LogRow>& b)
    {
        if (a.size() != b.size())
            return false;
        for (std::size_t i = 0; i < a.size(); ++i)
        {
            if (a[i].TimestampUs != b[i].TimestampUs || a[i].CanId != b[i].CanId ||
                a[i].Dlc != b[i].Dlc || a[i].TxNodeId != b[i].TxNodeId ||
                a[i].RxNodeId != b[i].RxNodeId || a[i].TxRequestUs != b[i].TxRequestUs ||
                a[i].TxStartUs != b[i].TxStartUs || a[i].TxEndUs != b[i].TxEndUs ||
                a[i].RxProcessUs != b[i].RxProcessUs || a[i].Status != b[i].Status ||
                a[i].RetryCount != b[i].RetryCount || a[i].UpdateCount != b[i].UpdateCount)
                return false;
        }
        return true;
    }

    // ---- 機能 003: バス構成図のレイアウト ----
    inline bool InsideDiagram(const DiagramRect& r)
    {
        return r.Left >= 0 && r.Top >= 0 && r.Right <= kDiagramWidth && r.Bottom <= kDiagramHeight;
    }

    inline std::string ReadTextFile(const std::string& path)
    {
        std::ifstream f(path.c_str(), std::ios::binary);
        std::ostringstream buf;
        buf << f.rdbuf();
        return buf.str();
    }

    // このビルドで書いた CSV を、機能 008 より前に書いた基準と比べられる形にする:
    // ログの列 Overwrite_Count は Update_Count になり、集計には最後の 2 列
    // TxDiscard_Count と RxDiscard_Count が加わった。基準がすでに 008 以降なら
    // 何も変えない。
    inline std::string NormalizeToBaseline(const std::string& now, const std::string& before)
    {
        std::string out = now;
        if (before.find("Overwrite_Count") != std::string::npos)
        {
            const std::size_t pos = out.find("Update_Count");
            if (pos != std::string::npos)
                out.replace(pos, std::string("Update_Count").size(), "Overwrite_Count");
        }
        if (out.find("TxDiscard_Count") != std::string::npos && before.find("TxDiscard_Count") == std::string::npos)
        {
            std::istringstream in(out);
            std::string line, stripped;
            while (std::getline(in, line))
            {
                const bool cr = !line.empty() && line.back() == '\r';
                if (cr)
                    line.pop_back();
                for (int k = 0; k < 2; ++k)
                {
                    const std::size_t comma = line.rfind(',');
                    if (comma != std::string::npos)
                        line.erase(comma);
                }
                stripped += line + (cr ? "\r\n" : "\n");
            }
            out = stripped;
        }
        return out;
    }

    // ダイアログと同じ方法（シミュレーション > 開始）でプロジェクトを実行し、
    // 3 つの CSV を <接頭辞>.log.csv / .summary.csv / .startup.csv に書く。
    inline bool RunProjectToCsv(const std::string& projectPath, const std::string& prefix,
        long long durationUs, std::vector<LogRow>* rowsOut = nullptr)
    {
        ConfigLoadResult project = LoadConfig(Widen(projectPath));
        if (!project.Success)
            return false;
        const Network network = BuildRunNetwork(project.Project, project.Config);
        SimulationEngine engine(network, durationUs);
        Logger logger;
        engine.Run(logger);
        const std::wstring p = Widen(prefix);
        const bool ok = logger.WriteCsv(p + L".log.csv") &&
            WriteMetricsCsv(ComputeMetrics(logger.Rows(), network), p + L".summary.csv") &&
            WriteStartupCsv(BuildStartupRecords(engine.GetNetwork()), p + L".startup.csv");
        if (rowsOut)
            *rowsOut = logger.Rows();
        return ok;
    }

    // 起動記録の CSV から、"<nodeId>," で始まる行を除く。
    inline std::string DropStartupRow(const std::string& csv, int nodeId)
    {
        std::istringstream in(csv);
        std::string line, out;
        const std::string prefix = std::to_string(nodeId) + ",";
        while (std::getline(in, line))
        {
            if (line.compare(0, prefix.size(), prefix) == 0)
                continue;
            out += line + "\n";
        }
        return out;
    }

    inline ConfigLoadResult LoadText(const std::string& name, const std::string& text)
    {
        const std::string path = Join(TempDir(), name);
        WriteTextFile(path, text);
        return LoadConfig(Widen(path));
    }

    // ---- 機能 006: データフレーム編集 ----
    inline FrameInput MakeInput(const char* id, const char* dlc, const char* periodMs, const char* offsetMs,
        const char* name = "f")
    {
        FrameInput in;
        in.Name = name;
        in.NameChars = static_cast<int>(std::string(name).size());
        in.IdText = id;
        in.DlcText = dlc;
        in.PeriodMsText = periodMs;
        in.OffsetMsText = offsetMs;
        return in;
    }

    inline bool HasIssue(const std::vector<FrameIssue>& issues, FrameField field, FrameProblem problem)
    {
        for (const FrameIssue& i : issues)
            if (i.Field == field && i.Problem == problem)
                return true;
        return false;
    }

    // ログの索引の検証用に、文字列をそのまま（バイナリで）書く。
    inline void WriteRaw(const std::string& path, const std::string& text)
    {
        std::ofstream out(path.c_str(), std::ios::binary);
        out << text;
    }

}
