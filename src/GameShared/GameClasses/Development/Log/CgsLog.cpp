#include "GameShared/GameClasses/Development/Log/CgsLog.h"

#include <cstdio>
#include <cstring>
#include <windows.h>

namespace CgsDev
{
namespace Log
{
    // Open BrnGame.log next to the executable (not the CWD) so it's findable however the game
    // is launched. Opened fresh each run ("w"); falls back to the CWD if the exe path is
    // unavailable.
    static FILE* OpenLogFile()
    {
        char lacPath[MAX_PATH];
        const DWORD luLen = GetModuleFileNameA(NULL, lacPath, MAX_PATH);
        if (luLen == 0 || luLen >= MAX_PATH)
            return std::fopen("BrnGame.log", "w");
        DWORD luDir = luLen;
        while (luDir > 0 && lacPath[luDir - 1] != '\\' && lacPath[luDir - 1] != '/')
            --luDir;
        lacPath[luDir] = '\0';
        std::strncat(lacPath, "BrnGame.log", MAX_PATH - luDir - 1);
        return std::fopen(lacPath, "w");
    }

    // One game log file for everything (asserts + debug prints + channel output). Opened fresh
    // each run; flushed per line so a crash still leaves the log intact.
    void WriteToLog(const char* lpcText)
    {
        if (!lpcText)
            return;
        static FILE* s_lpLogFile = OpenLogFile();
        if (s_lpLogFile)
        {
            std::fputs(lpcText, s_lpLogFile);
            std::fflush(s_lpLogFile);
        }
        OutputDebugStringA(lpcText);
    }

    StrStreamBase& DebugPrint::operator<<(const char* lpcText)
    {
        WriteToLog(lpcText);
        return *this;
    }

    static DebugPrint sDebugPrint;
    DebugPrint* gpDebugPrint = &sDebugPrint;
}

namespace Message
{
    u64 gxMessageFilterFlags = 0xFFFFFFFF;
}
}
