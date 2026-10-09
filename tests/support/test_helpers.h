// テストで共通に使う小さな道具（一時ファイル・構成の組み立て・実行・CSV の分解）
// 機能 002 以降の仕様テストが使う。
#pragma once

#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

#include "CanTypes.h"
#include "ConfigLoader.h"
#include "Logger.h"
#include "SimulationEngine.h"

// 一時ファイルの作成・削除に使う（CanSim のファイル API が std::wstring のパスを取るため Windows 専用）
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace TestHelpers
{
    inline std::wstring Widen(const std::string& s)
    {
        return std::wstring(s.begin(), s.end());
    }

    // %TEMP% の下のパス（name は ASCII）
    inline std::wstring TempPath(const std::string& name)
    {
        wchar_t dir[MAX_PATH];
        GetTempPathW(MAX_PATH, dir);
        return std::wstring(dir) + Widen(name);
    }

    inline std::string ReadAll(const std::wstring& path)
    {
        std::ifstream in(path.c_str(), std::ios::binary);
        return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    }

    inline void WriteAll(const std::wstring& path, const std::string& text)
    {
        std::ofstream out(path.c_str(), std::ios::binary);
        out << text;
    }

    // 設定ファイルを一時ファイルに書いて LoadConfig し、ファイルは消す
    inline CanSim::ConfigLoadResult LoadFromText(const std::string& name, const std::string& text)
    {
        const std::wstring path = TempPath(name);
        WriteAll(path, text);
        CanSim::ConfigLoadResult result = CanSim::LoadConfig(path);
        DeleteFileW(path.c_str());
        return result;
    }

    struct FrameSpec
    {
        unsigned CanId;
        unsigned Dlc;
        long long PeriodUs;
        long long OffsetUs;
    };

    inline CanSim::Node MakeNode(int nodeId, const std::vector<FrameSpec>& frames)
    {
        CanSim::Node node;
        node.NodeId = nodeId;
        node.Name = "ECU" + std::to_string(nodeId);
        for (const FrameSpec& f : frames)
        {
            CanSim::FrameDefinition fd;
            fd.CanId = f.CanId;
            fd.Dlc = f.Dlc;
            fd.Data.assign(f.Dlc, 0xFF);
            fd.PeriodUs = f.PeriodUs;
            fd.InitialOffsetUs = f.OffsetUs;
            node.Frames.push_back(fd);
        }
        return node;
    }

    struct RunResult
    {
        std::vector<CanSim::LogRow> Rows;
        CanSim::Network After; // 実行後のネットワーク（起動の状態など）
    };

    inline RunResult RunSim(const CanSim::Network& net, long long durationUs)
    {
        CanSim::SimulationEngine engine(net, durationUs);
        CanSim::Logger logger;
        engine.Run(logger);
        RunResult r;
        r.Rows = logger.Rows();
        r.After = engine.GetNetwork();
        return r;
    }

    // 受信ノードの数だけ重複する行を、送信 1 回（送信ノード, CAN ID, 送信開始）にまとめる。OVERWRITE 行は除く
    inline std::vector<CanSim::LogRow> UniqueTransmissions(const std::vector<CanSim::LogRow>& rows)
    {
        std::set<std::tuple<int, unsigned, long long>> seen;
        std::vector<CanSim::LogRow> out;
        for (const CanSim::LogRow& r : rows)
        {
            if (r.Status == CanSim::kStatusOverwrite) continue;
            if (seen.insert(std::make_tuple(r.TxNodeId, r.CanId, r.TxStartUs)).second) out.push_back(r);
        }
        return out;
    }

    inline std::vector<std::string> SplitLines(const std::string& text)
    {
        std::vector<std::string> lines;
        std::string line;
        std::istringstream in(text);
        while (std::getline(in, line))
        {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            lines.push_back(line);
        }
        return lines;
    }

    // 単純なカンマ区切り（引用符は扱わない）。末尾が空の欄も 1 つと数える
    inline std::vector<std::string> SplitFields(const std::string& line)
    {
        std::vector<std::string> fields;
        std::string field;
        std::istringstream in(line);
        while (std::getline(in, field, ',')) fields.push_back(field);
        if (!line.empty() && line.back() == ',') fields.push_back("");
        return fields;
    }
}
