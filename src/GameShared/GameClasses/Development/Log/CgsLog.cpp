#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "GameShared/GameClasses/Development/Log/CgsLogFileQueuePC.h"
#include "GameShared/GameClasses/Development/MessageSystem/CgsMessage.h"   // KX_FILTER_GSMEMORY

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <windows.h>

namespace CgsDev
{
namespace Log
{
    static volatile LONG sLogFileReady = 0;
    static HANDLE shLogFile = INVALID_HANDLE_VALUE;
    static LogFileQueuePC<> sFileQueue;
    static bool sbFileInitialized = false, sbAsynchronousFile = false, sbFileStopped = false;

    static HANDLE OpenLogFile(bool lbFresh, const char* lpcName = "BrnGame.log")
    {
        char lacPath[MAX_PATH];
        const DWORD luLen = GetModuleFileNameA(NULL, lacPath, MAX_PATH);
        if (luLen == 0 || luLen >= MAX_PATH) std::strcpy(lacPath, lpcName);
        else
        {
            DWORD luDir = luLen;
            while (luDir > 0 && lacPath[luDir - 1] != '\\' && lacPath[luDir - 1] != '/') --luDir;
            // Never shorten the basename: a long executable directory could
            // otherwise alias the ordinary and emergency files to one path.
            if (luDir + std::strlen(lpcName) >= MAX_PATH) std::strcpy(lacPath, lpcName);
            else std::strcpy(lacPath + luDir, lpcName);
        }
        if (lbFresh)
        {
            const HANDLE lhFresh = CreateFileA(lacPath, GENERIC_WRITE,
                FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (lhFresh == INVALID_HANDLE_VALUE) return lhFresh;
            CloseHandle(lhFresh);
        }
        // Append-only access prevents a fatal report's independent handle from
        // being overwritten by a writer whose earlier operation was delayed.
        return CreateFileA(lacPath, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE,
                           nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    }

    static bool AppendTextPC(HANDLE lhFile, const char* lpcText, size_t luBytes)
    {
        if (lhFile == INVALID_HANDLE_VALUE) return false;
        char lacBytes[4096];
        DWORD luUsed = 0;
        const auto FlushBytes = [&]() {
            DWORD luOffset = 0;
            while (luOffset < luUsed)
            {
                DWORD luWritten = 0;
                if (!WriteFile(lhFile, lacBytes + luOffset, luUsed - luOffset, &luWritten, nullptr)
                    || !luWritten) return false;
                luOffset += luWritten;
            }
            luUsed = 0;
            return true;
        };
        for (size_t lu = 0; lu < luBytes; ++lu)
        {
            // Preserve the previous CRT text-mode LF-to-CRLF translation.
            const DWORD luNeeded = lpcText[lu] == '\n' ? 2u : 1u;
            if (luUsed + luNeeded > sizeof(lacBytes) && !FlushBytes()) return false;
            if (lpcText[lu] == '\n') lacBytes[luUsed++] = '\r';
            lacBytes[luUsed++] = lpcText[lu];
        }
        return FlushBytes();
    }

    // ---- the DebugPrint LINE buffer -----------------------------------------------------
    // `*gpDebugPrint << "[tag] " << value << " more " << value << "\n"` arrives here one PIECE at
    // a time (StrStreamBase renders each scalar and calls the char* sink), and every piece used to
    // be a full WriteToLog: fputs + fflush (an NTFS write) + OutputDebugStringA. MEASURED
    // 2026-08-15 in the driving state: with a handful of per-frame diagnostics of ~10-30 pieces
    // each, that was 5.8% of the main thread (~1 ms a frame) -- fflush alone 3.7%.
    // So DebugPrint now accumulates a line and hands it to WriteToLog when the piece carries a
    // '\n' (or the buffer is full). Completed chunks are now queued for a native file
    // worker. Explicit assertion flushes and the independent fatal-report path keep
    // diagnostics available without putting routine disk waits inside a game frame.
    // FLAG PC-platform leaf: each native thread assembles its own stream line;
    // only completed writes share the file/debugger sink.
    static thread_local char   s_lacLine[2048];
    static thread_local size_t s_luLineLen = 0;
    static thread_local unsigned suCriticalDepth = 0;
    static SRWLOCK sOutputLock = SRWLOCK_INIT;

    // FLAG PC-platform leaf: opt-in timing of the native log sink. Records stay
    // in fixed storage during play; only orderly shutdown writes the report.
    // The independent timing lock never covers IO and is also used by the file worker.
    struct LogTimingRecordPC
    {
        LONGLONG miBegin, miLocked, miFileEnd, miDebugEnd;
        DWORD muThreadId;
        bool mbFileWorker;
        char macLabel[96];
    };
    struct LogTimingPC
    {
        static constexpr unsigned KU_CAPACITY = 1024;
        LogTimingRecordPC maRecords[KU_CAPACITY]{};
        unsigned muCount = 0;
        unsigned long long muCalls = 0, muSlowCalls = 0;
        unsigned long long maTicks[3]{};
        LONGLONG miFrequency = 0;
        bool mbStopped = false;
    };
    static LogTimingPC sLogTiming;
    static SRWLOCK sTimingLock = SRWLOCK_INIT;
    struct TimingLockPC
    {
        TimingLockPC() { AcquireSRWLockExclusive(&sTimingLock); }
        ~TimingLockPC() { ReleaseSRWLockExclusive(&sTimingLock); }
    };

    static void WriteLogTimingPC()
    {
        TimingLockPC lLock;
        sLogTiming.mbStopped = true;
        char lacPath[MAX_PATH];
        const DWORD luLength = GetModuleFileNameA(nullptr, lacPath, MAX_PATH);
        if (luLength && luLength + 20 < MAX_PATH && sLogTiming.miFrequency > 0)
        {
            const double lfMs = 1000.0 / static_cast<double>(sLogTiming.miFrequency);
            std::snprintf(lacPath + luLength, MAX_PATH - luLength, ".logtiming.csv");
            if (FILE* lpFile = std::fopen(lacPath, "w"))
            {
                std::fputs("thread_id,begin_qpc,end_qpc,lock_wait_ms,file_ms,debugger_ms,total_ms,source,label\n", lpFile);
                for (unsigned lu = 0; lu < sLogTiming.muCount; ++lu)
                {
                    const LogTimingRecordPC& lr = sLogTiming.maRecords[lu];
                    std::fprintf(lpFile, "%lu,%lld,%lld,%.6f,%.6f,%.6f,%.6f,%s,\"",
                        static_cast<unsigned long>(lr.muThreadId), lr.miBegin, lr.miDebugEnd,
                        (lr.miLocked - lr.miBegin) * lfMs, (lr.miFileEnd - lr.miLocked) * lfMs,
                        (lr.miDebugEnd - lr.miFileEnd) * lfMs, (lr.miDebugEnd - lr.miBegin) * lfMs,
                        lr.mbFileWorker ? "file_worker" : "producer");
                    for (const char* lpc = lr.macLabel; *lpc; ++lpc)
                    {
                        if (*lpc == '"') std::fputc('"', lpFile);
                        std::fputc(*lpc == '\n' || *lpc == '\r' ? ' ' : *lpc, lpFile);
                    }
                    std::fputs("\"\n", lpFile);
                }
                std::fclose(lpFile);
            }
            std::snprintf(lacPath + luLength, MAX_PATH - luLength, ".logtiming.json");
            if (FILE* lpFile = std::fopen(lacPath, "w"))
            {
                const auto lStats = sFileQueue.GetStatistics();
                std::fprintf(lpFile,
                    "{\"calls\":%llu,\"slow_calls\":%llu,\"records\":%u,\"dropped_records\":%llu,"
                    "\"threshold_ms\":5,\"counter_frequency\":%lld,\"lock_wait_ms\":%.6f,"
                    "\"file_ms\":%.6f,\"debugger_ms\":%.6f,\"async_file\":%s,"
                    "\"queue_dropped_writes\":%llu,\"queue_dropped_bytes\":%llu,\"file_write_errors\":%llu}\n",
                    sLogTiming.muCalls, sLogTiming.muSlowCalls, sLogTiming.muCount,
                    sLogTiming.muSlowCalls - sLogTiming.muCount, sLogTiming.miFrequency,
                    sLogTiming.maTicks[0] * lfMs, sLogTiming.maTicks[1] * lfMs,
                    sLogTiming.maTicks[2] * lfMs, sbAsynchronousFile ? "true" : "false",
                    lStats.muDroppedWrites, lStats.muDroppedBytes, lStats.muWriteErrors);
                std::fclose(lpFile);
            }
        }
    }

    static bool LogTimingEnabledPC()
    {
        static const bool sbEnabled = [] {
            const char* lpcEnable = std::getenv("BRN_LOG_PROFILE");
            LARGE_INTEGER lFrequency{};
            if (!lpcEnable || !lpcEnable[0] || lpcEnable[0] == '0'
                || !QueryPerformanceFrequency(&lFrequency) || lFrequency.QuadPart <= 0)
                return false;
            sLogTiming.miFrequency = lFrequency.QuadPart;
            return std::atexit(WriteLogTimingPC) == 0;
        }();
        return sbEnabled;
    }

    static void RecordLogTimingPC(const char* lpcText, LONGLONG liBegin,
                                  LONGLONG liLocked, LONGLONG liFileEnd, LONGLONG liDebugEnd,
                                  bool lbFileWorker = false)
    {
        TimingLockPC lLock;
        if (sLogTiming.mbStopped) return;
        ++sLogTiming.muCalls;
        sLogTiming.maTicks[0] += liLocked - liBegin;
        sLogTiming.maTicks[1] += liFileEnd - liLocked;
        sLogTiming.maTicks[2] += liDebugEnd - liFileEnd;
        if (static_cast<double>(liDebugEnd - liBegin) * 1000.0
            < static_cast<double>(sLogTiming.miFrequency) * 5.0) return;
        ++sLogTiming.muSlowCalls;
        if (sLogTiming.muCount == LogTimingPC::KU_CAPACITY) return;
        LogTimingRecordPC& lr = sLogTiming.maRecords[sLogTiming.muCount++];
        lr.miBegin = liBegin; lr.miLocked = liLocked;
        lr.miFileEnd = liFileEnd; lr.miDebugEnd = liDebugEnd;
        lr.muThreadId = GetCurrentThreadId();
        lr.mbFileWorker = lbFileWorker;
        std::strncpy(lr.macLabel, lpcText, sizeof(lr.macLabel) - 1);
        lr.macLabel[sizeof(lr.macLabel) - 1] = '\0';
    }

    static bool WriteFileBatchPC(const char* lpcText, size_t luBytes)
    {
        const bool lbProfile = LogTimingEnabledPC();
        LARGE_INTEGER lBegin{}, lEnd{};
        if (lbProfile) QueryPerformanceCounter(&lBegin);
        const bool lbWritten = AppendTextPC(shLogFile, lpcText, luBytes);
        if (lbProfile)
        {
            QueryPerformanceCounter(&lEnd);
            char lacLabel[80];
            std::snprintf(lacLabel, sizeof(lacLabel), "[log-file-worker] batch bytes=%zu", luBytes);
            RecordLogTimingPC(lacLabel, lBegin.QuadPart, lBegin.QuadPart,
                              lEnd.QuadPart, lEnd.QuadPart, true);
        }
        return lbWritten;
    }

    static void StopFileLoggingPC()
    {
        AcquireSRWLockExclusive(&sOutputLock);
        sFileQueue.Stop();
        sbFileStopped = true;
        // The append handle remains valid for late CRT-exit diagnostics. The OS
        // closes it at process exit, after any remaining exit callbacks.
        ReleaseSRWLockExclusive(&sOutputLock);
    }

    static void InitializeFileLoggingPC()
    {
        if (sbFileInitialized) return;
        sbFileInitialized = true;
        shLogFile = OpenLogFile(true);
        InterlockedExchange(&sLogFileReady, 1);
        const char* lpcAsync = std::getenv("BRN_LOG_ASYNC");
        if (shLogFile != INVALID_HANDLE_VALUE && (!lpcAsync || lpcAsync[0] != '0'))
        {
            sbAsynchronousFile = sFileQueue.Start(WriteFileBatchPC);
            if (sbAsynchronousFile && std::atexit(StopFileLoggingPC) != 0)
            {
                sFileQueue.Stop();
                sbAsynchronousFile = false;
            }
        }
    }

    static void WriteRaw(const char* lpcText)
    {
        if (suCriticalDepth) { WriteToLogEmergency(lpcText); return; }
        const bool lbProfile = LogTimingEnabledPC();
        LARGE_INTEGER lBegin{}, lLocked{}, lFileEnd{}, lDebugEnd{};
        if (lbProfile) QueryPerformanceCounter(&lBegin);
        AcquireSRWLockExclusive(&sOutputLock);
        if (lbProfile) QueryPerformanceCounter(&lLocked);
        InitializeFileLoggingPC();
        const size_t luBytes = std::strlen(lpcText);
        if (sbAsynchronousFile && !sbFileStopped) sFileQueue.TryWrite(lpcText, luBytes);
        else AppendTextPC(shLogFile, lpcText, luBytes);
        if (lbProfile) QueryPerformanceCounter(&lFileEnd);
        OutputDebugStringA(lpcText);
        if (lbProfile)
        {
            QueryPerformanceCounter(&lDebugEnd);
            RecordLogTimingPC(lpcText, lBegin.QuadPart, lLocked.QuadPart,
                              lFileEnd.QuadPart, lDebugEnd.QuadPart);
        }
        ReleaseSRWLockExclusive(&sOutputLock);
    }

    static void FlushPendingLine()
    {
        if (s_luLineLen == 0)
            return;
        s_lacLine[s_luLineLen] = '\0';
        s_luLineLen = 0;
        WriteRaw(s_lacLine);
    }

    CriticalLogScopePC::CriticalLogScopePC() { ++suCriticalDepth; }
    CriticalLogScopePC::~CriticalLogScopePC()
    {
        FlushPendingLine();
        --suCriticalDepth;
    }

    // Ordinary output is queued. Normal exit drains it; fatal reports use the
    // separate append handle below rather than relying on a healthy worker.
    void WriteToLog(const char* lpcText)
    {
        if (!lpcText)
            return;
        FlushPendingLine();
        WriteRaw(lpcText);
    }

    void FlushLog()
    {
        FlushPendingLine();
        // Critical text was already written synchronously by the emergency sink.
        // Waiting for routine output here could deadlock the assertion reporter.
        if (suCriticalDepth) return;
        AcquireSRWLockExclusive(&sOutputLock);
        if (sbAsynchronousFile && !sbFileStopped) sFileQueue.Flush();
        ReleaseSRWLockExclusive(&sOutputLock);
    }

    void WriteToLogEmergency(const char* lpcText)
    {
        if (!lpcText) return;
        // This append-only backup is never truncated by normal initialization.
        // A fatal report may arrive while another thread is creating BrnGame.log.
        const size_t luBytes = std::strlen(lpcText);
        const HANDLE lhFile = OpenLogFile(false, "BrnGame.emergency.log");
        if (lhFile != INVALID_HANDLE_VALUE)
        {
            AppendTextPC(lhFile, lpcText, luBytes);
            CloseHandle(lhFile);
        }
        if (InterlockedCompareExchange(&sLogFileReady, 0, 0) != 0)
        {
            const HANDLE lhMain = OpenLogFile(false);
            if (lhMain != INVALID_HANDLE_VALUE)
            {
                AppendTextPC(lhMain, lpcText, luBytes);
                CloseHandle(lhMain);
            }
        }
        OutputDebugStringA(lpcText);
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
    // [PC] Every category of the low word except the memory category, whose only user is the
    // per-frame resource memory-check print (re-enable it from the debug menu's message filter).
    u64 gxMessageFilterFlags = 0xFFFFFFFFull & ~KX_FILTER_GSMEMORY;
}
}
