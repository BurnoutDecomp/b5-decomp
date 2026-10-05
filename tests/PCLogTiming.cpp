#define NOMINMAX
#include <Windows.h>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>

static std::atomic<int> siDelay{0};
static std::atomic<unsigned> suClockCalls{0};
static HANDLE shDebuggerEntered, shDebuggerRelease;
static void WINAPI TestDebugOutput(LPCSTR);
static BOOL WINAPI TestCounter(LARGE_INTEGER*);
static BOOL WINAPI TestWriteFile(HANDLE, LPCVOID, DWORD, LPDWORD, LPOVERLAPPED);
#define OutputDebugStringA TestDebugOutput
#define QueryPerformanceCounter TestCounter
#define WriteFile TestWriteFile
#include "pc_log_timing_source.inc"
#undef WriteFile
#undef QueryPerformanceCounter
#undef OutputDebugStringA

static int siChecks, siFailures;
static void Check(bool lbCondition, const char* lpcName)
{
    ++siChecks;
    if (!lbCondition) { ++siFailures; std::printf("FAIL %s\n", lpcName); }
}
namespace CgsDev { namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char*, const char*, int) { ++siFailures; return 0; }
void* EndAssert() { return nullptr; }
} }
static BOOL WINAPI TestCounter(LARGE_INTEGER* lpValue)
{
    ++suClockCalls;
    return QueryPerformanceCounter(lpValue);
}
static BOOL WINAPI TestWriteFile(HANDLE lhFile, LPCVOID lpBytes, DWORD luBytes, LPDWORD lpuWritten, LPOVERLAPPED lpOverlapped)
{
    if (siDelay == 2) Sleep(25);
    return WriteFile(lhFile, lpBytes, luBytes, lpuWritten, lpOverlapped);
}
static void WINAPI TestDebugOutput(LPCSTR lpcText)
{
    if (std::strcmp(lpcText, "debug \"quoted\"\n") == 0)
    {
        SetEvent(shDebuggerEntered);
        WaitForSingleObject(shDebuggerRelease, 3000);
    }
    OutputDebugStringA(lpcText);
}
static std::string ReadFile(const std::string& lrPath)
{
    std::string lResult;
    if (FILE* lpFile = std::fopen(lrPath.c_str(), "rb"))
    {
        char lacBuffer[4096]; size_t luCount;
        while ((luCount = std::fread(lacBuffer, 1, sizeof(lacBuffer), lpFile)) != 0)
            lResult.append(lacBuffer, luCount);
        std::fclose(lpFile);
    }
    return lResult;
}
int main()
{
    using namespace CgsDev::Log;
    char lacExe[MAX_PATH]; GetModuleFileNameA(nullptr, lacExe, MAX_PATH);
    const std::string lExe(lacExe), lCsv = lExe + ".logtiming.csv";
    const std::string lMeta = lExe + ".logtiming.json";
    const bool lbEnabled = LogTimingEnabledPC();
    shDebuggerEntered = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    shDebuggerRelease = CreateEventW(nullptr, TRUE, FALSE, nullptr);

    WriteToLog("ordinary line\n");
    siDelay = 2;
    WriteToLog("file delay\n");
    siDelay = 0;
    std::thread lDebugger([] { WriteToLog("debug \"quoted\"\n"); });
    Check(WaitForSingleObject(shDebuggerEntered, 3000) == WAIT_OBJECT_0,
          "debugger boundary entered while the output lock is held");
    std::atomic<bool> lbContenderStarted{false};
    std::thread lContender([&] {
        lbContenderStarted = true;
        WriteToLog("contender\n");
    });
    while (!lbContenderStarted.load()) std::this_thread::yield();
    Sleep(30);
    SetEvent(shDebuggerRelease);
    lDebugger.join(); lContender.join();
    CloseHandle(shDebuggerEntered); CloseHandle(shDebuggerRelease);

    const std::string lLog = ReadFile(lExe.substr(0, lExe.find_last_of("\\/") + 1) + "BrnGame.log");
    Check(lLog.find("ordinary line") != std::string::npos && lLog.find("file delay") != std::string::npos
          && lLog.find("debug \"quoted\"") != std::string::npos && lLog.find("contender") != std::string::npos,
          "timing leaves file and debugger output intact");
    Check(GetFileAttributesA(lCsv.c_str()) == INVALID_FILE_ATTRIBUTES
          && GetFileAttributesA(lMeta.c_str()) == INVALID_FILE_ATTRIBUTES,
          "capture does no report-file IO during play");
    if (!lbEnabled)
    {
        Check(suClockCalls == 0 && sLogTiming.muCalls == 0,
              "disabled timing performs no clock reads or profile updates");
    }
    else
    {
        Check(sLogTiming.muCalls == 4, "concurrent calls counted once under the output lock");
        bool lbFile = false, lbDebugger = false, lbLock = false;
        const double lfMs = 1000.0 / sLogTiming.miFrequency;
        for (unsigned lu = 0; lu < sLogTiming.muCount; ++lu)
        {
            const auto& lr = sLogTiming.maRecords[lu];
            if (std::strcmp(lr.macLabel, "file delay\n") == 0)
                lbFile = (lr.miFileEnd - lr.miLocked) * lfMs >= 20;
            if (std::strcmp(lr.macLabel, "debug \"quoted\"\n") == 0)
                lbDebugger = (lr.miDebugEnd - lr.miFileEnd) * lfMs >= 25;
            if (std::strcmp(lr.macLabel, "contender\n") == 0)
                lbLock = (lr.miLocked - lr.miBegin) * lfMs >= 20;
        }
        Check(lbFile, "slow file flush is attributed to file IO");
        Check(lbDebugger, "slow debugger callback is attributed separately");
        Check(lbLock, "a second thread records contention rather than attributing it to its own sink");
        const auto luSlowBefore = sLogTiming.muSlowCalls;
        const LONGLONG liSlow = sLogTiming.miFrequency / 100; // synthetic 10 ms sample
        const std::string lLongLabel(500, 'x');
        AcquireSRWLockExclusive(&sOutputLock);
        for (unsigned lu = 0; lu < LogTimingPC::KU_CAPACITY + 3; ++lu)
            RecordLogTimingPC(lLongLabel.c_str(), 1, 1, 1, 1 + liSlow);
        ReleaseSRWLockExclusive(&sOutputLock);
        Check(sLogTiming.muCount == LogTimingPC::KU_CAPACITY
              && sLogTiming.muSlowCalls == luSlowBefore + LogTimingPC::KU_CAPACITY + 3,
              "fixed record storage reports overflow without losing the total count");
        Check(std::strlen(sLogTiming.maRecords[LogTimingPC::KU_CAPACITY - 1].macLabel) == 95,
              "long messages retain a bounded terminated label");
        WriteLogTimingPC();
        const std::string lReport = ReadFile(lCsv), lMetadata = ReadFile(lMeta);
        Check(lReport.find("debug \"\"quoted\"\" ") != std::string::npos,
              "report escapes quotes and embedded line endings");
        Check(lMetadata.find("\"records\":1024") != std::string::npos
              && lMetadata.find("\"counter_frequency\":") != std::string::npos,
              "report includes its record count and QPC frequency for frame correlation");
        const auto luStoppedCalls = sLogTiming.muCalls;
        WriteToLog("after reporting\n");
        Check(sLogTiming.muCalls == luStoppedCalls, "shutdown reporting stops capture without disabling logging");
    }
    std::printf("PCLogTiming: %d checks, %d failures\n", siChecks, siFailures);
    return siFailures ? 1 : 0;
}
