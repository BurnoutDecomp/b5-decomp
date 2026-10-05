#define NOMINMAX
#include <Windows.h>
#include <atomic>
#include <cstdio>
#include <csignal>
#include <cstdarg>
#include <string>
#include <thread>

static HANDLE shEntered, shRelease;
static std::atomic<bool> sbBlock{true};
static DWORD suMainThread;
static BOOL WINAPI TestWriteFile(HANDLE, LPCVOID, DWORD, LPDWORD, LPOVERLAPPED);
static HANDLE WINAPI TestCreateFileA(LPCSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
static DWORD WINAPI TestGetModuleFileNameA(HMODULE, LPSTR, DWORD);
#define WriteFile TestWriteFile
#define CreateFileA TestCreateFileA
#define GetModuleFileNameA TestGetModuleFileNameA
#include "pc_log_emergency_source.inc"
#undef WriteFile
#undef CreateFileA
#undef GetModuleFileNameA

static DWORD WINAPI TestGetModuleFileNameA(HMODULE lhModule, LPSTR lpPath, DWORD luCapacity)
{
#if PC_LOG_LONG_PATH
    const std::string lPath = "C:\\" + std::string(248, 'a') + "\\a.exe";
    if (luCapacity <= lPath.size()) return luCapacity;
    std::strcpy(lpPath, lPath.c_str());
    return static_cast<DWORD>(lPath.size());
#else
    return GetModuleFileNameA(lhModule, lpPath, luCapacity);
#endif
}

static HANDLE WINAPI TestCreateFileA(LPCSTR lpPath, DWORD luAccess, DWORD luShare,
    LPSECURITY_ATTRIBUTES lpSecurity, DWORD luCreation, DWORD luAttributes, HANDLE lhTemplate)
{
    if (luCreation == CREATE_ALWAYS)
        CgsDev::Log::WriteToLogEmergency("startup fatal marker\n");
    // Keep the synthetic long-directory test on this fixture's private disk path.
    const char* lpcBase = std::strrchr(lpPath, '\\');
    if (lpcBase) lpPath = lpcBase + 1;
    return CreateFileA(lpPath, luAccess, luShare, lpSecurity, luCreation, luAttributes, lhTemplate);
}

static int siChecks, siFailures;
static void Check(bool lbValue, const char* lpcName)
{ ++siChecks; if (!lbValue) { ++siFailures; std::printf("FAIL %s\n", lpcName); } }
namespace CgsDev { namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char*, const char*, int) { ++siFailures; return 0; }
void* EndAssert() { return nullptr; }
} }
static BOOL WINAPI TestWriteFile(HANDLE lhFile, LPCVOID lpBytes, DWORD luBytes, LPDWORD lpuWritten, LPOVERLAPPED lpOverlapped)
{
    if (GetCurrentThreadId() != suMainThread && sbBlock.exchange(false))
    {
        SetEvent(shEntered);
        WaitForSingleObject(shRelease, 5000);
    }
    return WriteFile(lhFile, lpBytes, luBytes, lpuWritten, lpOverlapped);
}
static std::string ReadLog(const char* lpcName = "BrnGame.log")
{
    char lacPath[MAX_PATH]; GetModuleFileNameA(nullptr, lacPath, MAX_PATH);
    std::string lPath(lacPath); lPath.resize(lPath.find_last_of("\\/") + 1); lPath += lpcName;
    std::string lText;
    if (FILE* lpFile = std::fopen(lPath.c_str(), "rb"))
    {
        char lacBytes[4096]; size_t luCount;
        while ((luCount = std::fread(lacBytes, 1, sizeof(lacBytes), lpFile)) != 0) lText.append(lacBytes, luCount);
        std::fclose(lpFile);
    }
    return lText;
}

namespace CrashEntryFixture
{
    static unsigned suFatalGuard, suReports;
    static void Emit(const char* lpcText) { CgsDev::Log::WriteToLog(lpcText); }
    static void Emitf(const char* lpcFormat, ...)
    {
        char lacText[512]; va_list lArgs; va_start(lArgs, lpcFormat);
        std::vsnprintf(lacText, sizeof(lacText), lpcFormat, lArgs); va_end(lArgs); Emit(lacText);
    }
    static void AbortSignalHandler(int)
    {
        Check(suFatalGuard++ == 0, "fatal prefix does not consume the report guard");
        ++suReports;
        throw 1; // Isolate process termination after exercising the real entry point.
    }
    struct StackUnpickFixture
    {
        void Prepare() {}
        s32 GetNumStackAddresses() { return 0; }
        u64 GetStackAddress(s32) { return 0; }
    };
    static struct MapReaderFixture
    {
        void Prepare(const char*, StackUnpickFixture*)
        {
            Check(ReadLog().find("HEAP CHECK FAILED") != std::string::npos,
                  "heap-corruption witness reaches disk before map-reader work can fail");
            throw 2;
        }
        const char* GetStackEntryName(s32) { return nullptr; }
        unsigned GetStackEntryOffset(s32) { return 0; }
    } gCrashMapReader;
    static const char* MapFilePath() { return "unused"; }
#include "pc_log_crash_entries.inc"
}
int main()
{
    using namespace CgsDev::Log;
    suMainThread = GetCurrentThreadId();
    shEntered = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    shRelease = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    WriteToLog("ordinary before\n");
    Check(ReadLog("BrnGame.emergency.log").find("startup fatal marker\r\n") != std::string::npos,
          "emergency data survives a normal initialization that truncates afterwards");
    Check(WaitForSingleObject(shEntered, 2000) == WAIT_OBJECT_0, "default file output runs on the native worker");
    WriteToLog("ordinary after\n");
    HANDLE lhDone = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    AcquireSRWLockExclusive(&sOutputLock);
    std::thread lEmergency([&] {
        WriteToLogEmergency("FATAL marker\n");
        { CriticalLogScopePC lCritical; *gpDebugPrint << "assert marker " << u32(7) << "\n"; FlushLog(); }
        for (auto lpProbe : {CrashEntryFixture::PurecallProbe, CrashEntryFixture::TerminateProbe})
        {
            CrashEntryFixture::suFatalGuard = 0;
            try { lpProbe(); } catch (int) {}
        }
        try { CrashEntryFixture::HeapFailureProbe(); } catch (int) {}
        SetEvent(lhDone);
    });
    const bool lbImmediate = WaitForSingleObject(lhDone, 200) == WAIT_OBJECT_0;
    Check(lbImmediate, "fatal output bypasses both a blocked worker and the held front-end lock");
    Check(lbImmediate && ReadLog().find("FATAL marker\r\nassert marker 7\r\n") != std::string::npos,
          "fatal and scoped assertion reports reach the file before the normal writer resumes");
    Check(lbImmediate && CrashEntryFixture::suReports == 2
          && ReadLog().find("PURE VIRTUAL CALL\r\n") != std::string::npos
          && ReadLog().find("std::terminate\r\n") != std::string::npos,
          "real fatal entry points report while ordinary logging is blocked");
    ReleaseSRWLockExclusive(&sOutputLock);
    SetEvent(shRelease);
    lEmergency.join();
    FlushLog();
    std::string lText = ReadLog();
    Check(lText.find("ordinary before\r\nordinary after\r\n") != std::string::npos,
          "routine output preserves byte order and text-mode line endings");
    Check(lText.find("FATAL marker") == lText.rfind("FATAL marker"), "later appends do not duplicate or overwrite the fatal report");
    StopFileLoggingPC();
    WriteToLog("late exit\n");
    Check(ReadLog().find("late exit\r\n") != std::string::npos, "late exit diagnostics remain available after worker shutdown");
    Check(sFileQueue.GetStatistics().muDroppedWrites == 0, "ordinary capture loses no queued records");
    CloseHandle(lhDone); CloseHandle(shEntered); CloseHandle(shRelease);
    std::printf("PCLogEmergency: %d checks, %d failures\n", siChecks, siFailures);
    return siFailures ? 1 : 0;
}
