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

    // ---- the DebugPrint LINE buffer -----------------------------------------------------
    // `*gpDebugPrint << "[tag] " << value << " more " << value << "\n"` arrives here one PIECE at
    // a time (StrStreamBase renders each scalar and calls the char* sink), and every piece used to
    // be a full WriteToLog: fputs + fflush (an NTFS write) + OutputDebugStringA. MEASURED
    // 2026-08-15 in the driving state: with a handful of per-frame diagnostics of ~10-30 pieces
    // each, that was 5.8% of the main thread (~1 ms a frame) -- fflush alone 3.7%.
    // So DebugPrint now accumulates a line and hands it to WriteToLog when the piece carries a
    // '\n' (or the buffer is full). The log is still flushed PER LINE, so a crash still leaves
    // every completed line on disk; only the intra-line syscalls are gone. Anything that writes
    // through WriteToLog directly (asserts, the crash reporter) first drains a pending partial
    // line, so ordering in the file is unchanged.
    static char   s_lacLine[2048];
    static size_t s_luLineLen = 0;

    static void WriteRaw(const char* lpcText)
    {
        static FILE* s_lpLogFile = OpenLogFile();
        if (s_lpLogFile)
        {
            std::fputs(lpcText, s_lpLogFile);
            std::fflush(s_lpLogFile);
        }
        OutputDebugStringA(lpcText);
    }

    static void FlushPendingLine()
    {
        if (s_luLineLen == 0)
            return;
        s_lacLine[s_luLineLen] = '\0';
        s_luLineLen = 0;
        WriteRaw(s_lacLine);
    }

    // One game log file for everything (asserts + debug prints + channel output). Opened fresh
    // each run; flushed per line so a crash still leaves the log intact.
    void WriteToLog(const char* lpcText)
    {
        if (!lpcText)
            return;
        FlushPendingLine();
        WriteRaw(lpcText);
    }

    StrStreamBase& DebugPrint::operator<<(const char* lpcText)
    {
        if (!lpcText)
            return *this;
        for (const char* lpc = lpcText; *lpc != '\0'; ++lpc)
        {
            if (s_luLineLen >= sizeof(s_lacLine) - 1)
                FlushPendingLine();
            s_lacLine[s_luLineLen++] = *lpc;
            if (*lpc == '\n')
                FlushPendingLine();
        }
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
