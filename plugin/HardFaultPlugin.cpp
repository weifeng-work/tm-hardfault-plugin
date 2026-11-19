// HardFaultPlugin - TrafficMonitor 插件：显示 Windows 硬缺页（Hard Faults/sec）
// 数据源：PDH 计数器 \Memory\Pages Input/sec
// 报警：滑动均值超过阈值并持续 N 秒，调用主程序 ShowNotifyMessage 弹气泡 + 提示音
//
// Copyright (c) 2026 weifeng-work
// 本文件是 TrafficMonitor（Copyright (C) 2021 Zhong Yang）的衍生作品，
// 遵循 "Anti 996" License Version 1.0，详见仓库根目录 LICENSE 与 NOTICE。
#include <windows.h>
#include <pdh.h>
#include <pdhmsg.h>
#include <deque>
#include <string>
#include <cstdio>
#include "PluginInterface.h"

#pragma comment(lib, "pdh.lib")

namespace {

const wchar_t* const kIniSection = L"config";
const wchar_t* const kIniFile = L"hardfault.ini";

std::wstring Num2Str(double v)
{
    wchar_t buf[64]{};
    swprintf_s(buf, L"%.0f", v);
    return buf;
}

} // namespace

class CHardFaultItem : public IPluginItem
{
public:
    const wchar_t* GetItemName() const override { return L"\u786c\u7f3a\u9875"; }
    const wchar_t* GetItemId() const override { return L"hardfault"; }
    const wchar_t* GetItemLableText() const override { return L"\u786c\u7f3a\u9875: "; }
    const wchar_t* GetItemValueText() const override { return m_value.c_str(); }
    const wchar_t* GetItemValueSampleText() const override { return L"9999/s"; }

    void SetValue(double v)
    {
        m_value = Num2Str(v);
        m_value += L"/s";
    }

private:
    std::wstring m_value = L"-";
};

class CHardFaultPlugin : public ITMPlugin
{
public:
    static CHardFaultPlugin& Instance()
    {
        static CHardFaultPlugin inst;
        return inst;
    }

    // ---- ITMPlugin ----
    IPluginItem* GetItem(int index) override
    {
        return index == 0 ? static_cast<IPluginItem*>(&m_item) : nullptr;
    }

    void DataRequired() override;
    const wchar_t* GetInfo(PluginInfoIndex index) override;
    void OnInitialize(ITrafficMonitor* pApp) override;
    const wchar_t* GetTooltipInfo() override { return m_tip.c_str(); }

    // ---- 右键菜单命令（TrafficMonitor API >= 5）----
    int GetCommandCount() override { return 5; }
    const wchar_t* GetCommandName(int index) override;
    void OnPluginCommand(int index, void* hWnd, void* para) override;
    int IsCommandChecked(int index) override;

private:
    bool EnsureQuery();
    void LoadConfig();
    void SaveConfig();
    void Notify(const std::wstring& msg);

    CHardFaultItem m_item;
    ITrafficMonitor* m_app{};
    std::wstring m_tip;
    std::wstring m_cmd_name[5];

    PDH_HQUERY m_query{};
    PDH_HCOUNTER m_counter{};
    bool m_query_failed{};

    std::deque<double> m_samples;

    int m_threshold{400};
    int m_window{10};
    int m_sustain{8};
    int m_cooldown{600};
    int m_beep{1};
    int m_paused{0};

    int m_over{};
    ULONGLONG m_last_alert{};
    double m_peak{};
    double m_last_avg{};
    std::wstring m_ini_path;
};

bool CHardFaultPlugin::EnsureQuery()
{
    if (m_counter) return true;
    if (m_query_failed) return false;
    if (m_query)
    {
        PdhCloseQuery(m_query);
        m_query = nullptr;
    }
    if (PdhOpenQueryW(nullptr, 0, &m_query) != ERROR_SUCCESS)
    {
        m_query_failed = true;
        return false;
    }
    if (PdhAddEnglishCounterW(m_query, L"\\Memory\\Pages Input/sec", 0, &m_counter) != ERROR_SUCCESS)
    {
        PdhCloseQuery(m_query);
        m_query = nullptr;
        m_query_failed = true;
        return false;
    }
    PdhCollectQueryData(m_query); // 预热，第一次采集没有速率值
    return true;
}

void CHardFaultPlugin::LoadConfig()
{
    if (m_ini_path.empty())
    {
        if (m_app) m_ini_path = m_app->GetPluginConfigDir();
        if (m_ini_path.empty())
        {
            wchar_t buf[MAX_PATH]{};
            GetModuleFileNameW(nullptr, buf, MAX_PATH);
            std::wstring dir(buf);
            size_t p = dir.find_last_of(L"\\/");
            if (p != std::wstring::npos) dir = dir.substr(0, p);
            m_ini_path = dir;
        }
        if (!m_ini_path.empty() && m_ini_path.back() != L'\\') m_ini_path += L'\\';
        m_ini_path += kIniFile;
    }

    m_threshold = GetPrivateProfileIntW(kIniSection, L"threshold", 400, m_ini_path.c_str());
    m_window    = GetPrivateProfileIntW(kIniSection, L"window", 10, m_ini_path.c_str());
    m_sustain   = GetPrivateProfileIntW(kIniSection, L"sustain", 8, m_ini_path.c_str());
    m_cooldown  = GetPrivateProfileIntW(kIniSection, L"cooldown", 600, m_ini_path.c_str());
    m_beep      = GetPrivateProfileIntW(kIniSection, L"beep", 1, m_ini_path.c_str());
    m_paused    = GetPrivateProfileIntW(kIniSection, L"paused", 0, m_ini_path.c_str());

    if (m_threshold <= 0) m_threshold = 400;
    if (m_window < 2) m_window = 10;
    if (m_sustain < 1) m_sustain = 8;
    if (m_cooldown < 0) m_cooldown = 600;
}

void CHardFaultPlugin::SaveConfig()
{
    if (m_ini_path.empty()) return;
    const wchar_t* p = m_ini_path.c_str();
    WritePrivateProfileStringW(kIniSection, L"threshold", std::to_wstring(m_threshold).c_str(), p);
    WritePrivateProfileStringW(kIniSection, L"window",    std::to_wstring(m_window).c_str(),    p);
    WritePrivateProfileStringW(kIniSection, L"sustain",   std::to_wstring(m_sustain).c_str(),   p);
    WritePrivateProfileStringW(kIniSection, L"cooldown",  std::to_wstring(m_cooldown).c_str(),  p);
    WritePrivateProfileStringW(kIniSection, L"beep",      std::to_wstring(m_beep).c_str(),      p);
    WritePrivateProfileStringW(kIniSection, L"paused",    std::to_wstring(m_paused).c_str(),    p);
}

void CHardFaultPlugin::OnInitialize(ITrafficMonitor* pApp)
{
    m_app = pApp;
    LoadConfig();
    SaveConfig();   // 首次运行时生成配置文件，方便用户改阈值
}

void CHardFaultPlugin::DataRequired()
{
    double value = 0.0;

    if (EnsureQuery())
    {
        if (PdhCollectQueryData(m_query) == ERROR_SUCCESS)
        {
            PDH_FMT_COUNTERVALUE cv{};
            if (PdhGetFormattedCounterValue(m_counter, PDH_FMT_DOUBLE, nullptr, &cv) == ERROR_SUCCESS)
            {
                if (cv.CStatus == PDH_CSTATUS_VALID_DATA || cv.CStatus == PDH_CSTATUS_NEW_DATA)
                    value = cv.doubleValue;
            }
        }
    }

    m_item.SetValue(value);
    if (value > m_peak) m_peak = value;

    m_samples.push_back(value);
    while (static_cast<int>(m_samples.size()) > m_window) m_samples.pop_front();

    double sum = 0.0;
    for (double v : m_samples) sum += v;
    double avg = m_samples.empty() ? 0.0 : sum / static_cast<double>(m_samples.size());
    m_last_avg = avg;

    // 提示文本
    {
        std::wstring tip = L"\u786c\u7f3a\u9875/\u79d2: ";
        tip += Num2Str(value);
        tip += L"\n\u5747\u503c: ";
        tip += Num2Str(avg);
        tip += L"\n\u5cf0\u503c: ";
        tip += Num2Str(m_peak);
        tip += L"\n\u9608\u503c: ";
        tip += std::to_wstring(m_threshold);
        m_tip = tip;
    }

    // 报警
    if (m_paused)
    {
        m_over = 0;
    }
    else if (avg >= static_cast<double>(m_threshold))
    {
        ++m_over;
    }
    else
    {
        m_over = 0;
    }

    if (m_over >= m_sustain)
    {
        ULONGLONG now = GetTickCount64();
        if (m_last_alert == 0 || now - m_last_alert >= static_cast<ULONGLONG>(m_cooldown) * 1000ULL)
        {
            m_last_alert = now;
            m_over = 0;

            std::wstring msg = L"\u5185\u5b58\u6362\u9875\u538b\u529b\u544a\u8b66\uff1a\u786c\u7f3a\u9875\u5747\u503c ";
            msg += Num2Str(avg);
            msg += L"/\u79d2\uff08\u9608\u503c ";
            msg += std::to_wstring(m_threshold);
            msg += L"\uff09";
            if (m_app) m_app->ShowNotifyMessage(msg.c_str());
            if (m_beep) MessageBeep(MB_ICONWARNING);
        }
    }
}

const wchar_t* CHardFaultPlugin::GetInfo(PluginInfoIndex index)
{
    switch (index)
    {
    case TMI_NAME:        return L"HardFault";
    case TMI_DESCRIPTION: return L"\u786c\u7f3a\u9875\uff08Hard Faults/sec\uff09\u76d1\u63a7\u4e0e\u544a\u8b66";
    case TMI_AUTHOR:      return L"buddy";
    case TMI_COPYRIGHT:   return L"";
    case TMI_VERSION:     return L"1.1.0";
    case TMI_URL:         return L"";
    default:              return L"";
    }
}

void CHardFaultPlugin::Notify(const std::wstring& msg)
{
    if (m_app) m_app->ShowNotifyMessage(msg.c_str());
}

const wchar_t* CHardFaultPlugin::GetCommandName(int index)
{
    if (index < 0 || index >= 5) return nullptr;
    switch (index)
    {
    case 0:
        m_cmd_name[0] = L"阈值 +50（当前 " + std::to_wstring(m_threshold) + L"/s）";
        break;
    case 1:
        m_cmd_name[1] = L"阈值 -50";
        break;
    case 2:
        m_cmd_name[2] = L"阈值 恢复默认 400";
        break;
    case 3:
        m_cmd_name[3] = std::wstring(L"报警提示音") + (m_beep ? L"（开）" : L"（关）");
        break;
    case 4:
        m_cmd_name[4] = std::wstring(L"暂停报警") + (m_paused ? L"（已暂停）" : L"");
        break;
    default:
        return nullptr;
    }
    return m_cmd_name[index].c_str();
}

int CHardFaultPlugin::IsCommandChecked(int index)
{
    if (index == 3) return m_beep ? 1 : 0;
    if (index == 4) return m_paused ? 1 : 0;
    return 0;
}

void CHardFaultPlugin::OnPluginCommand(int index, void* hWnd, void* para)
{
    (void)hWnd; (void)para;
    switch (index)
    {
    case 0: m_threshold += 50; break;
    case 1: m_threshold -= 50; break;
    case 2: m_threshold = 400; break;
    case 3: m_beep = m_beep ? 0 : 1; break;
    case 4: m_paused = m_paused ? 0 : 1; m_over = 0; break;
    default: return;
    }

    if (m_threshold < 10) m_threshold = 10;
    if (m_threshold > 100000) m_threshold = 100000;
    SaveConfig();

    std::wstring msg;
    if (index == 3)
        msg = std::wstring(L"报警提示音已") + (m_beep ? L"开启" : L"关闭");
    else if (index == 4)
        msg = std::wstring(L"报警已") + (m_paused ? L"暂停" : L"恢复");
    else
        msg = L"硬缺页报警阈值已设为 " + std::to_wstring(m_threshold) + L"/s";

    Notify(msg);
    if (m_beep) MessageBeep(MB_OK);
}

extern "C" __declspec(dllexport) ITMPlugin* TMPluginGetInstance()
{
    return &CHardFaultPlugin::Instance();
}
