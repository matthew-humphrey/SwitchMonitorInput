#include <windows.h>
#include <highlevelmonitorconfigurationapi.h>
#include <physicalmonitorenumerationapi.h>
#include <lowlevelmonitorconfigurationapi.h>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <functional>
#include <stdexcept>
#include <codecvt>
#include <locale>
#include "SwitchMonitorInput.h"

// Link to necessary libraries
#pragma comment(lib, "Dxva2.lib")

class wruntime_error : public std::runtime_error {
public:
    explicit wruntime_error(const std::wstring& wide_message)
        : std::runtime_error(convert_wstring_to_string(wide_message)), wide_message_(wide_message) {}

    const std::wstring& wide_message() const noexcept {
        return wide_message_;
    }

private:
    std::wstring wide_message_;

    static std::string convert_wstring_to_string(const std::wstring& wide_message) {
        std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
        return converter.to_bytes(wide_message);
    }
};

class MonitorInputTypes
{
    std::map<BYTE, std::wstring> _inputCodeToName;
    std::map<std::wstring, BYTE> _inputNameToCode;

public:
    MonitorInputTypes()
    {
        _inputCodeToName.insert({ 0x01, std::wstring(L"RGB1") });
        _inputCodeToName.insert({ 0x02, std::wstring(L"RGB2") });
        _inputCodeToName.insert({ 0x03, std::wstring(L"DVI1") });
        _inputCodeToName.insert({ 0x04, std::wstring(L"DVI2") });
        _inputCodeToName.insert({ 0x05, std::wstring(L"COMPOSITE1") });
        _inputCodeToName.insert({ 0x06, std::wstring(L"COMPOSITE2") });
        _inputCodeToName.insert({ 0x07, std::wstring(L"SVIDEO1") });
        _inputCodeToName.insert({ 0x08, std::wstring(L"SVIDEO2") });
        _inputCodeToName.insert({ 0x09, std::wstring(L"TUNER1") });
        _inputCodeToName.insert({ 0x0A, std::wstring(L"TUNER2") });
        _inputCodeToName.insert({ 0x0B, std::wstring(L"TUNER3") });
        _inputCodeToName.insert({ 0x0C, std::wstring(L"COMPONENT1") });
        _inputCodeToName.insert({ 0x0D, std::wstring(L"COMPONENT2") });
        _inputCodeToName.insert({ 0x0E, std::wstring(L"COMPONENT3") });
        _inputCodeToName.insert({ 0x0F, std::wstring(L"DP1") });
        _inputCodeToName.insert({ 0x10, std::wstring(L"DP2") });
        _inputCodeToName.insert({ 0x11, std::wstring(L"HDMI1") });
        _inputCodeToName.insert({ 0x12, std::wstring(L"HDMI2") });
        _inputCodeToName.insert({ 0x1b, std::wstring(L"USB-C") });

        for (const auto& pair : _inputCodeToName)
        {
            _inputNameToCode[pair.second] = pair.first;
        }
    }

    std::wstring InputCodeToName(BYTE inputCode)
	{
		auto it = _inputCodeToName.find(inputCode);
		if (it == _inputCodeToName.end())
		{
			return std::wstring(L"UNKNOWN");
		}

		return it->second;
	}

    BYTE InputNameToCode(const std::wstring& inputName)
    {
        auto it = _inputNameToCode.find(inputName);
        if (it == _inputNameToCode.end())
        {
            throw wruntime_error(L"Invalid input name");
        }

        return it->second;
    }

    void VisitInputNames(const std::function<void(const std::wstring& inputName)>& visitor)
	{
		for (const auto& pair : _inputNameToCode)
		{
			visitor(pair.first);
		}
	}
};

class MonitorControl
{
private:
    struct DisplayInfo
    {
        HMONITOR _hMonitor;
        std::wstring _monitorDescription;
        std::vector<PHYSICAL_MONITOR> _physicalMonitors;
    };

    std::vector<DisplayInfo> _displays;
    MonitorInputTypes _monitorInputTypes;

    HANDLE GetPhysicalMonitorHandle(DWORD displayIndex, DWORD physicalMonitorIndex)
    {
        if (displayIndex >= _displays.size())
        {
            throw wruntime_error(L"Invalid display index");
        }

        auto& display = _displays[displayIndex];

        if (physicalMonitorIndex >= display._physicalMonitors.size())
        {
            throw wruntime_error(L"Invalid physical monitor index");
        }

        return display._physicalMonitors[physicalMonitorIndex].hPhysicalMonitor;
    }

    BOOL ReadVcpByteFeature(HANDLE hPhysicalMonitor, BYTE bVCPCode, BYTE& currentValue)
    {
        BOOL rc;
        DWORD cv, mv;

        rc = GetVCPFeatureAndVCPFeatureReply(
            hPhysicalMonitor,
            bVCPCode,
            NULL,
            &cv,
            &mv);

        if (rc)
        {
            currentValue = cv & 0xFF;
        }

        return rc;
    }

    static BOOL CALLBACK MonitorEnumProc(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData)
    {
        MONITORINFOEX monitorInfo = {};
        monitorInfo.cbSize = sizeof(MONITORINFOEX);

        if (!GetMonitorInfo(hMonitor, &monitorInfo))
        {
            throw wruntime_error(L"Error getting monitor info");
        }

        MonitorControl* pThis = reinterpret_cast<MonitorControl*>(dwData);

        pThis->_displays.emplace_back(DisplayInfo{ hMonitor, monitorInfo.szDevice });

        // Get physical monitors
        DWORD numPhysicalMonitors;
        if (!GetNumberOfPhysicalMonitorsFromHMONITOR(hMonitor, &numPhysicalMonitors))
        {
            throw wruntime_error(L"Error getting the number of physical monitors");
        }

        auto& display = pThis->_displays.back();
        display._physicalMonitors.resize(numPhysicalMonitors);

        if (!GetPhysicalMonitorsFromHMONITOR(hMonitor, numPhysicalMonitors, display._physicalMonitors.data()))
        {
            throw wruntime_error(L"Error getting the physical monitors");
        }

        return TRUE;
    }

public:

    explicit MonitorControl()
    {
        EnumDisplayMonitors(NULL, NULL, MonitorEnumProc, reinterpret_cast<LPARAM>(this));
    }

    MonitorControl(const MonitorControl&) = delete;
    MonitorControl& operator=(const MonitorControl&) = delete;

    virtual ~MonitorControl()
    {
        for (auto& display : _displays)
        {
            if (display._physicalMonitors.size() > 0)
            {
                DestroyPhysicalMonitors(static_cast<DWORD>(display._physicalMonitors.size()), display._physicalMonitors.data());
                display._physicalMonitors.clear();
            }

            // We do not need to call CloseHandle on the HMONITOR
        }
    }

    void VisitMonitors(const std::function<void(DWORD displayIndex, HMONITOR hDisplay, const std::wstring& displayDescription, DWORD physicalMonitorIndex, HANDLE hPhysicalMonitor, const std::wstring& physicalMonitorDescription)>& visitor)
    {
        DWORD displayIndex = 0;

        for (auto& display : _displays)
        {
            DWORD physicalMonitorIndex = 0;
            for (auto& physicalMonitor : display._physicalMonitors)
            {
                visitor(displayIndex, display._hMonitor, display._monitorDescription, physicalMonitorIndex, physicalMonitor.hPhysicalMonitor, physicalMonitor.szPhysicalMonitorDescription);
                physicalMonitorIndex++;
            }

            displayIndex++;
        }
    }

    BYTE GetMonitorInputCode(DWORD displayIndex, DWORD physicalMonitorIndex)
    {
        BYTE inputCode;

        if (!ReadVcpByteFeature(GetPhysicalMonitorHandle(displayIndex, physicalMonitorIndex), 0x60, inputCode))
        {
            throw wruntime_error(L"Error reading input code");
        }

        return inputCode;
    }

    std::wstring GetMonitorInputName(DWORD displayIndex, DWORD physicalMonitorIndex)
    {
        BYTE inputCode = GetMonitorInputCode(displayIndex, physicalMonitorIndex);

        return _monitorInputTypes.InputCodeToName(inputCode);
    }

    void SetMonitorInputByCode(DWORD displayIndex, DWORD physicalMonitorIndex, BYTE inputCode)
    {
        if (!::SetVCPFeature(GetPhysicalMonitorHandle(displayIndex, physicalMonitorIndex), 0x60, inputCode))
        {
            throw wruntime_error(L"Error setting VCP feature for monitor input");
        }
    }

    void SetMonitorInputByName(DWORD displayIndex, DWORD physicalMonitorIndex, std::wstring inputName)
    {
        std::transform(inputName.begin(), inputName.end(), inputName.begin(), ::towupper);

        BYTE inputCode = _monitorInputTypes.InputNameToCode(inputName);

        SetMonitorInputByCode(displayIndex, physicalMonitorIndex, inputCode);
    }
};

void PrintUsage(const wchar_t* progName)
{
    std::wcout << L"Usage: " << progName << " [options]" << std::endl;
    std::wcout << L"Options:" << std::endl;
    std::wcout << L"  -l: List all physical monitors" << std::endl;
    std::wcout << L"  -i input_name: Set monitor input (see list of valid inputs below)" << std::endl;
    std::wcout << L"  -d display_index: Index of the display (default: 1)" << std::endl;
    std::wcout << L"  -m monitor_index: Physical monitor index for the specified display (default: 1)" << std::endl;
    std::wcout << L"  -s : Show list of valid input names" << std::endl;
    std::wcout << std::endl;
    std::wcout << L"Input names:" << std::endl;
    MonitorInputTypes mit;
    mit.VisitInputNames([](const std::wstring& inputName) { std::wcout << L"  " << inputName << std::endl; });
}

struct CmdLineArgs
{
    bool _listMonitors{};
    bool _setInput{};
    std::wstring _inputName;
    DWORD _displayIndex{};
    DWORD _physicalMonitorIndex{};

    CmdLineArgs() {}
};

void ParseArgs(int argc, wchar_t* argv[], CmdLineArgs& args)
{
    args = {};

    for (int i = 1; i < argc; ++i)
    {
        std::wstring arg = argv[i];

        if (arg == L"-l")
        {
            args._listMonitors = true;
        }
        else if (arg == L"-i")
        {
            if (i + 1 < argc)
            {
                args._inputName = argv[++i];
                args._setInput = true;
            }
            else
            {
                throw wruntime_error(L"Missing argument to -i");
            }
        }
        else if (arg == L"-d")
        {
            if (i + 1 < argc)
            {
                int displayIndex = std::stoi(argv[++i]) - 1;
                if (displayIndex < 0)
                {
                    throw wruntime_error(L"Invalid display index");
                }
                args._displayIndex = displayIndex;
            }
            else
            {
                throw wruntime_error(L"Missing argument to -d");
            }
        }
        else if (arg == L"-m")
        {
            if (i + 1 < argc)
            {
                int physicalMonitorIndex = std::stoi(argv[++i]) - 1;
                if (physicalMonitorIndex < 0)
                {
                    throw wruntime_error(L"Invalid monitor index");
                }
                args._physicalMonitorIndex = physicalMonitorIndex;
            }
            else
            {
                throw wruntime_error(L"Missing argument to -m");
            }
        }
        else
        {
            throw wruntime_error(L"Unknown argument: " + arg);
        }
    }

}

int wmain(int argc, wchar_t* argv[])
{
    if (argc < 2)
    {
        PrintUsage(argv[0]);
        return 0;
    }

    try
    {
        CmdLineArgs args;
        ParseArgs(argc, argv, args);

        MonitorControl mc;

        if (args._listMonitors)
        {
            std::wcout << "Monitors: " << std::endl;
            auto visitor = [&mc](DWORD displayIndex, HMONITOR hDisplay, const std::wstring& displayDescription, DWORD physicalMonitorIndex, HANDLE hPhysicalMonitor, const std::wstring& physicalMonitorDescription)
                {                    
                    std::wcout << L"  " << displayDescription << L"\\MONITOR" << (physicalMonitorIndex + 1) << L": \"" << physicalMonitorDescription << L"\" on input: " << mc.GetMonitorInputName(displayIndex, physicalMonitorIndex) << std::endl;
                };

            mc.VisitMonitors(visitor);
        }
        
        if (args._setInput)
        {
            mc.SetMonitorInputByName(args._displayIndex, args._physicalMonitorIndex, args._inputName);
        }
    }
    catch (wruntime_error e)
    {
        std::wcout << e.wide_message() << std::endl;
        return 1;
    }

    return 0;
}
