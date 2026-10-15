// spec_test_helpers.h（tests/support）
// 仕様のテスト（tests/unit/feature_<NNN>_spec_tests.cpp）で共通に使う小さな道具。
// 一時ファイル、構成（Network）の組み立て、シミュレーションの実行、ログの行のまとめ。
// テストはここには置かない。
#pragma once

// _wgetenv・_wfopen の非推奨の警告（C4996）を出さない
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <gtest/gtest.h>

#include "ConfigLoader.h"
#include "Logger.h"
#include "SimulationEngine.h"

#include <cstdio>
#include <cstdlib>
#include <set>
#include <string>
#include <vector>

namespace spectest
{
    // %TEMP%\cansim_spec_<name> のパス
    inline std::wstring TempPath(const std::wstring& name)
    {
        const wchar_t* temp = _wgetenv(L"TEMP");
        std::wstring dir = temp ? temp : L".";
        return dir + L"\\cansim_spec_" + name;
    }

    // サンプルプロジェクトのフォルダー（MFCApplication1/CanSim/SampleConfigs）。このヘッダーの場所（tests/support）から求める
    inline std::wstring SampleConfigPath(const std::wstring& fileName)
    {
        std::string here = __FILE__;
        const std::size_t cut = here.find_last_of("\\/");
        std::string root = here.substr(0, cut);            // tests/support
        root = root.substr(0, root.find_last_of("\\/"));   // tests
        root = root.substr(0, root.find_last_of("\\/"));   // リポジトリのルート
        return std::wstring(root.begin(), root.end()) + L"\\MFCApplication1\\CanSim\\SampleConfigs\\" + fileName;
    }

    // ASCII の文字列をワイド文字列にする（ファイル名用）
    inline std::wstring Widen(const std::string& s)
    {
        return std::wstring(s.begin(), s.end());
    }

    // テキストをそのまま（バイト列として）書く。書けなければ false
    inline bool WriteText(const std::wstring& path, const std::string& text)
    {
        FILE* f = _wfopen(path.c_str(), L"wb");
        if (!f)
            return false;
        fwrite(text.data(), 1, text.size(), f);
        fclose(f);
        return true;
    }

    // ファイルを丸ごと読む。無ければ空
    inline std::string ReadText(const std::wstring& path)
    {
        std::string text;
        FILE* f = _wfopen(path.c_str(), L"rb");
        if (!f)
            return text;
        char buf[4096];
        for (std::size_t n; (n = fread(buf, 1, sizeof(buf), f)) > 0;)
            text.append(buf, n);
        fclose(f);
        return text;
    }

    inline bool FileExists(const std::wstring& path)
    {
        FILE* f = _wfopen(path.c_str(), L"rb");
        if (!f)
            return false;
        fclose(f);
        return true;
    }

    // テキストを一時ファイルに書いて LoadConfig で読む
    inline CanSim::ConfigLoadResult LoadText(const std::wstring& name, const std::string& text)
    {
        const std::wstring path = TempPath(name);
        WriteText(path, text);
        return CanSim::LoadConfig(path);
    }

    // "\r\n" で区切った行
    inline std::vector<std::string> SplitLines(const std::string& text)
    {
        std::vector<std::string> lines;
        std::size_t start = 0;
        while (start < text.size())
        {
            std::size_t end = text.find("\r\n", start);
            if (end == std::string::npos)
                end = text.size();
            lines.push_back(text.substr(start, end - start));
            start = end + 2;
        }
        return lines;
    }

    // フレームの定義。データは DLC 分の 0xFF（機能 006 FR-014）
    inline CanSim::FrameDefinition MakeFrame(unsigned canId, unsigned dlc, long long periodUs, long long offsetUs = 0)
    {
        CanSim::FrameDefinition f;
        f.Name = "F";
        f.CanId = canId;
        f.Dlc = dlc;
        f.Data.assign(dlc, 0xFF);
        f.PeriodUs = periodUs;
        f.InitialOffsetUs = offsetUs;
        return f;
    }

    inline CanSim::Node MakeNode(int nodeId, const std::vector<CanSim::FrameDefinition>& frames = {})
    {
        CanSim::Node n;
        n.NodeId = nodeId;
        n.Name = "ECU" + std::to_string(nodeId);
        n.Frames = frames;
        return n;
    }

    // シミュレーションを最後まで実行してログの行を返す。engineOut を渡すと実行後のネットワークも返す
    inline std::vector<CanSim::LogRow> RunSim(const CanSim::Network& network, long long durationUs,
        CanSim::Network* networkOut = nullptr)
    {
        CanSim::SimulationEngine engine(network, durationUs);
        CanSim::Logger logger;
        engine.Run(logger);
        if (networkOut)
            *networkOut = engine.GetNetwork();
        return logger.Rows();
    }

    // CAN ID の送信 1 回ごとの代表の行（受信ノードの数だけ重複する行を送信開始でまとめる。OVERWRITE は除く）
    inline std::vector<CanSim::LogRow> Transmissions(const std::vector<CanSim::LogRow>& rows, unsigned canId)
    {
        std::vector<CanSim::LogRow> out;
        std::set<long long> seenStarts;
        for (const CanSim::LogRow& r : rows)
        {
            if (r.CanId != canId || r.Status == CanSim::kStatusOverwrite)
                continue;
            if (seenStarts.insert(r.TxStartUs).second)
                out.push_back(r);
        }
        return out;
    }

    // CAN ID の最初の行（無ければ nullptr）
    inline const CanSim::LogRow* FirstRowOf(const std::vector<CanSim::LogRow>& rows, unsigned canId)
    {
        for (const CanSim::LogRow& r : rows)
            if (r.CanId == canId)
                return &r;
        return nullptr;
    }

    // ログの行が全列一致するか
    inline bool SameRow(const CanSim::LogRow& a, const CanSim::LogRow& b)
    {
        return a.TimestampUs == b.TimestampUs && a.CanId == b.CanId && a.Dlc == b.Dlc &&
               a.TxNodeId == b.TxNodeId && a.RxNodeId == b.RxNodeId && a.TxRequestUs == b.TxRequestUs &&
               a.TxStartUs == b.TxStartUs && a.TxEndUs == b.TxEndUs && a.RxStartUs == b.RxStartUs &&
               a.RxEndUs == b.RxEndUs && a.RxProcessUs == b.RxProcessUs && a.BusLoadPct == b.BusLoadPct &&
               a.Status == b.Status && a.RetryCount == b.RetryCount && a.UpdateCount == b.UpdateCount;
    }
}
