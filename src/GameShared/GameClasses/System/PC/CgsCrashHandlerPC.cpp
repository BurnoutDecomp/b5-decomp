#include "GameShared/GameClasses/System/PC/CgsCrashHandlerPC.h"

#include "GameShared/GameClasses/Development/Log/CgsLog.h"                              // CgsDev::Log::WriteToLog
#include "GameShared/GameClasses/Development/StackUnpick/CgsStackUnpick.h"              // CgsDev::StackUnpick
#include "GameShared/GameClasses/Development/MapFile/Reader/CgsMapFileReaderMinimalMemory.h" // map-resolved names

#include <windows.h>
#include <objidl.h>     // IStream (gdiplus.h dependency)
#include <gdiplus.h>    // the PNG encoder for the crash screenshot (link gdiplus.lib)
#include <csignal>      // SIGABRT (CRT abort()/terminate() funnel)
#include <cstdio>
#include <cstring>
#include <cstdlib>      // _set_invalid_parameter_handler / _set_purecall_handler / atexit / abort
#include <exception>    // std::set_terminate
#include <intrin.h>     // __readgsqword (the x64 TIB stack bounds -- see PollProcessHealth)

// HOST-ONLY last-chance crash reporter; see the header. Everything here must be safe to
// run on a broken process: fixed buffers, no CRT heap on the log path, screenshot last
// (inside its own __try) so a failed capture cannot eat the log.

namespace CgsSystem
{
namespace CrashHandler
{
namespace
{
    // Own resolver instance: the assert manager's reader may be mid-parse on another
    // thread when the crash hits, so the crash path never shares it.
    CgsDev::MapFile::MinimalMemoryReader gCrashMapReader;

    // One report per process: a second fault (possibly raised by the reporter itself)
    // must not recurse.
    volatile LONG gCrashHandled = 0;

    void Emit(const char* lpcText) { CgsDev::Log::WriteToLog(lpcText); }

    void Emitf(const char* lpcFormat, ...)
    {
        char lacLine[512];
        va_list lArgs;
        va_start(lArgs, lpcFormat);
        std::vsnprintf(lacLine, sizeof(lacLine), lpcFormat, lArgs);
        va_end(lArgs);
        Emit(lacLine);
    }

    // Same derivation as the assert manager's GetDefaultMapFilePath (exe path with the
    // extension swapped) -- duplicated because that helper is file-static there.
    const char* MapFilePath()
    {
        static char sacPath[MAX_PATH] = { 0 };
        if (sacPath[0] == '\0')
        {
            GetModuleFileNameA(nullptr, sacPath, MAX_PATH);
            char* lpcExt = std::strrchr(sacPath, '.');
            if (!lpcExt)
                lpcExt = sacPath + std::strlen(sacPath);
            std::strcpy(lpcExt, ".cgsmap");
        }
        return sacPath;
    }

    const char* ExceptionCodeName(DWORD luCode)
    {
        switch (luCode)
        {
        case EXCEPTION_ACCESS_VIOLATION:         return "EXCEPTION_ACCESS_VIOLATION";
        case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:    return "EXCEPTION_ARRAY_BOUNDS_EXCEEDED";
        case EXCEPTION_DATATYPE_MISALIGNMENT:    return "EXCEPTION_DATATYPE_MISALIGNMENT";
        case EXCEPTION_FLT_DENORMAL_OPERAND:     return "EXCEPTION_FLT_DENORMAL_OPERAND";
        case EXCEPTION_FLT_DIVIDE_BY_ZERO:       return "EXCEPTION_FLT_DIVIDE_BY_ZERO";
        case EXCEPTION_FLT_INEXACT_RESULT:       return "EXCEPTION_FLT_INEXACT_RESULT";
        case EXCEPTION_FLT_INVALID_OPERATION:    return "EXCEPTION_FLT_INVALID_OPERATION";
        case EXCEPTION_FLT_OVERFLOW:             return "EXCEPTION_FLT_OVERFLOW";
        case EXCEPTION_FLT_STACK_CHECK:          return "EXCEPTION_FLT_STACK_CHECK";
        case EXCEPTION_FLT_UNDERFLOW:            return "EXCEPTION_FLT_UNDERFLOW";
        case EXCEPTION_ILLEGAL_INSTRUCTION:      return "EXCEPTION_ILLEGAL_INSTRUCTION";
        case EXCEPTION_IN_PAGE_ERROR:            return "EXCEPTION_IN_PAGE_ERROR";
        case EXCEPTION_INT_DIVIDE_BY_ZERO:       return "EXCEPTION_INT_DIVIDE_BY_ZERO";
        case EXCEPTION_INT_OVERFLOW:             return "EXCEPTION_INT_OVERFLOW";
        case EXCEPTION_INVALID_DISPOSITION:      return "EXCEPTION_INVALID_DISPOSITION";
        case EXCEPTION_NONCONTINUABLE_EXCEPTION: return "EXCEPTION_NONCONTINUABLE_EXCEPTION";
        case EXCEPTION_PRIV_INSTRUCTION:         return "EXCEPTION_PRIV_INSTRUCTION";
        case EXCEPTION_STACK_OVERFLOW:           return "EXCEPTION_STACK_OVERFLOW";
        // The two __fastfail carriers (see the header banner). They normally never reach a
        // handler at all, but they DO show up as the process exit code, and the vectored probe
        // sees them on the rare paths that raise rather than int-29 them.
        case 0xC0000409u:                        return "STATUS_STACK_BUFFER_OVERRUN (__fastfail)";
        case 0xC0000374u:                        return "STATUS_HEAP_CORRUPTION";
        default:                                 return "EXCEPTION";
        }
    }

    // ---- the crash screenshot -----------------------------------------------------

    struct sFindWindowByPid
    {
        DWORD luPid;
        HWND  lhBest;
        long  llBestArea;
    };

    BOOL CALLBACK FindWindowByPidProc(HWND lhWnd, LPARAM lParam)
    {
        sFindWindowByPid* lpFind = reinterpret_cast<sFindWindowByPid*>(lParam);
        DWORD luWndPid = 0;
        GetWindowThreadProcessId(lhWnd, &luWndPid);
        if (luWndPid != lpFind->luPid || !IsWindowVisible(lhWnd))
            return TRUE;
        RECT lRect;
        if (!GetWindowRect(lhWnd, &lRect))
            return TRUE;
        const long llArea = (lRect.right - lRect.left) * (lRect.bottom - lRect.top);
        if (llArea > lpFind->llBestArea)
        {
            lpFind->llBestArea = llArea;
            lpFind->lhBest     = lhWnd;
        }
        return TRUE;
    }

    // {557CF406-1A04-11D3-9A73-0000F81EF32E} -- the stock GDI+ PNG encoder, by CLSID so
    // the crash path skips the GetImageEncoders enumeration (heap churn).
    const CLSID KPngEncoderClsid =
        { 0x557CF406, 0x1A04, 0x11D3, { 0x9A, 0x73, 0x00, 0x00, 0xF8, 0x1E, 0xF3, 0x2E } };

    // Capture the game window into BrnCrash.png next to the exe. Returns false on any
    // failure; the caller logs the outcome either way.
    bool SaveCrashScreenshot(wchar_t* lpacPathOut, size_t lnPathChars)
    {
        sFindWindowByPid lFind = { GetCurrentProcessId(), nullptr, 0 };
        EnumWindows(FindWindowByPidProc, reinterpret_cast<LPARAM>(&lFind));
        if (!lFind.lhBest)
            return false;

        RECT lRect;
        if (!GetWindowRect(lFind.lhBest, &lRect))
            return false;
        const int liWidth  = lRect.right - lRect.left;
        const int liHeight = lRect.bottom - lRect.top;
        if (liWidth <= 0 || liHeight <= 0)
            return false;

        GetModuleFileNameW(nullptr, lpacPathOut, static_cast<DWORD>(lnPathChars));
        wchar_t* lpcSlash = wcsrchr(lpacPathOut, L'\\');
        if (lpcSlash)
            lpcSlash[1] = L'\0';
        wcsncat_s(lpacPathOut, lnPathChars, L"BrnCrash.png", _TRUNCATE);

        HDC lhWndDc  = GetWindowDC(lFind.lhBest);
        HDC lhMemDc  = CreateCompatibleDC(lhWndDc);
        HBITMAP lhBmp = CreateCompatibleBitmap(lhWndDc, liWidth, liHeight);
        HGDIOBJ lhOld = SelectObject(lhMemDc, lhBmp);

        // PW_RENDERFULLCONTENT (2) makes PrintWindow ask DWM for the composed surface, so
        // the D3D9 swap-chain content comes through; plain BitBlt of a D3D window returns
        // black. Screen-copy fallback for pre-DWM paths.
        const UINT KU_PW_RENDERFULLCONTENT = 2;
        if (!PrintWindow(lFind.lhBest, lhMemDc, KU_PW_RENDERFULLCONTENT))
        {
            HDC lhScreenDc = GetDC(nullptr);
            BitBlt(lhMemDc, 0, 0, liWidth, liHeight, lhScreenDc, lRect.left, lRect.top, SRCCOPY);
            ReleaseDC(nullptr, lhScreenDc);
        }
        SelectObject(lhMemDc, lhOld);

        // GDI+ started here (not at Install) so the cost exists only on the crash path.
        Gdiplus::GdiplusStartupInput lStartupInput;
        ULONG_PTR luGdiplusToken = 0;
        bool lbSaved = false;
        if (Gdiplus::GdiplusStartup(&luGdiplusToken, &lStartupInput, nullptr) == Gdiplus::Ok)
        {
            Gdiplus::Bitmap* lpBitmap = Gdiplus::Bitmap::FromHBITMAP(lhBmp, nullptr);
            if (lpBitmap)
            {
                lbSaved = (lpBitmap->Save(lpacPathOut, &KPngEncoderClsid, nullptr) == Gdiplus::Ok);
                delete lpBitmap;
            }
            Gdiplus::GdiplusShutdown(luGdiplusToken);
        }

        DeleteObject(lhBmp);
        DeleteDC(lhMemDc);
        ReleaseDC(lFind.lhBest, lhWndDc);
        return lbSaved;
    }

    // ---- the report ---------------------------------------------------------------

    void WriteReport(EXCEPTION_POINTERS* lpExceptionInfo, const char* lpcKind)
    {
        const uintptr_t luModuleBase = reinterpret_cast<uintptr_t>(GetModuleHandleA(nullptr));

        if (lpExceptionInfo && lpExceptionInfo->ExceptionRecord)
        {
            const EXCEPTION_RECORD* lpRecord = lpExceptionInfo->ExceptionRecord;
            const uintptr_t luAddress = reinterpret_cast<uintptr_t>(lpRecord->ExceptionAddress);
            Emitf("[EXCEPTION] %s (0x%08lX) at 0x%016llX (module+0x%llX)\n",
                  ExceptionCodeName(lpRecord->ExceptionCode),
                  static_cast<unsigned long>(lpRecord->ExceptionCode),
                  static_cast<unsigned long long>(luAddress),
                  static_cast<unsigned long long>(luAddress >= luModuleBase ? luAddress - luModuleBase : luAddress));

            // Access violations carry {0=read,1=write,8=execute} + the target address.
            if (lpRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && lpRecord->NumberParameters >= 2)
            {
                const ULONG_PTR luKind   = lpRecord->ExceptionInformation[0];
                const ULONG_PTR luTarget = lpRecord->ExceptionInformation[1];
                Emitf("  access violation %s 0x%016llX\n",
                      luKind == 0 ? "READING" : (luKind == 1 ? "WRITING" : "EXECUTING"),
                      static_cast<unsigned long long>(luTarget));
            }
        }
        else
        {
            Emitf("[EXCEPTION] %s\n", lpcKind);
        }

        if (lpExceptionInfo && lpExceptionInfo->ContextRecord)
        {
            const CONTEXT* lpCtx = lpExceptionInfo->ContextRecord;
            Emitf("  rip=%016llX rsp=%016llX rbp=%016llX rflags=%08lX\n",
                  lpCtx->Rip, lpCtx->Rsp, lpCtx->Rbp, static_cast<unsigned long>(lpCtx->EFlags));
            Emitf("  rax=%016llX rbx=%016llX rcx=%016llX rdx=%016llX\n",
                  lpCtx->Rax, lpCtx->Rbx, lpCtx->Rcx, lpCtx->Rdx);
            Emitf("  rsi=%016llX rdi=%016llX r8 =%016llX r9 =%016llX\n",
                  lpCtx->Rsi, lpCtx->Rdi, lpCtx->R8, lpCtx->R9);
            Emitf("  r10=%016llX r11=%016llX r12=%016llX r13=%016llX r14=%016llX r15=%016llX\n",
                  lpCtx->R10, lpCtx->R11, lpCtx->R12, lpCtx->R13, lpCtx->R14, lpCtx->R15);
        }

        // Same capture + map resolution + output shape as Assert::Manager::DoAssert, so
        // the log's crash block reads exactly like an assert block. The capture runs on
        // the faulting thread inside the filter: the top frames are the reporter + the OS
        // exception dispatcher (they print raw), then the faulting frames resolve.
        CgsDev::StackUnpick lStack;
        lStack.Prepare();
        gCrashMapReader.Prepare(MapFilePath(), &lStack);

        Emit("  Callstack:\n");
        for (s32 liIndex = 0; liIndex < lStack.GetNumStackAddresses(); ++liIndex)
        {
            const char* lpcName = gCrashMapReader.GetStackEntryName(liIndex);
            if (lpcName)
            {
                // ⭐ Print the OFFSET INTO the function too (2026-08-16). A bare name locates a
                // crash to a function that can be 7 KB of inlined code; "+ 0xNNN" locates it to
                // a statement once the matching build's .map is in hand. Also print the frame's
                // RVA, because a player's report is the only copy of it we ever get.
                Emitf("    %s + 0x%X    [rva 0x%llX]\n",
                      lpcName,
                      static_cast<unsigned>(gCrashMapReader.GetStackEntryOffset(liIndex)),
                      static_cast<unsigned long long>(lStack.GetStackAddress(liIndex)));
            }
            else
                Emitf("    0x%llX\n", static_cast<unsigned long long>(lStack.GetStackAddress(liIndex)));
        }
        Emit("  EndCallstack\n");
    }

    LONG WINAPI CrashFilter(EXCEPTION_POINTERS* lpExceptionInfo)
    {
        // First fault only; a fault inside the reporter falls straight through to the OS.
        if (InterlockedExchange(&gCrashHandled, 1) != 0)
            return EXCEPTION_CONTINUE_SEARCH;

        WriteReport(lpExceptionInfo, "unhandled exception");

        // Screenshot LAST and fenced off: the log block above must survive a capture
        // that faults on the broken process state.
        wchar_t lacShotPath[MAX_PATH] = { 0 };
        bool lbShot = false;
        __try
        {
            lbShot = SaveCrashScreenshot(lacShotPath, MAX_PATH);
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            lbShot = false;
        }
        if (lbShot)
            Emitf("  screenshot -> %ls\n", lacShotPath);
        else
            Emit("  screenshot -> FAILED\n");

        // Hand back to the default chain (the Windows error dialog / debugger) unchanged;
        // this reporter only observes.
        return EXCEPTION_CONTINUE_SEARCH;
    }

    // abort() (CRT asserts, std::terminate, unhandled C++ exceptions routed through the
    // CRT) never reaches the SEH filter, so it gets its own hook. No EXCEPTION_POINTERS
    // exist here; the report is the callstack + screenshot.
    void AbortSignalHandler(int)
    {
        if (InterlockedExchange(&gCrashHandled, 1) != 0)
            return;
        WriteReport(nullptr, "CRT abort() (std::terminate / unhandled C++ exception / CRT assert)");
        wchar_t lacShotPath[MAX_PATH] = { 0 };
        if (SaveCrashScreenshot(lacShotPath, MAX_PATH))
            Emitf("  screenshot -> %ls\n", lacShotPath);
    }

    // ================================================================================
    // ⭐ THE SILENT-EXIT NET (2026-08-20, gateui round 7 / defect B). See the header banner
    // for WHY the unhandled-exception filter alone is not one. Everything below is
    // FLAG PC-platform leaf: host process-death plumbing with no console counterpart (the
    // X360 title halts into the debug monitor and the kernel reclaims it).
    // ================================================================================

    // The stack high-water tripwire. On x64 the TIB lives at gs:[0]; NT_TIB::StackBase is at
    // +0x08 and NT_TIB::StackLimit at +0x10. StackLimit is the LOWEST COMMITTED address and it
    // only ever moves down (a touched stack page is never decommitted for the life of the
    // thread), so StackBase - StackLimit is this thread's exact peak stack use so far.
    const ULONG_PTR KU_TIB_STACK_BASE_OFFSET  = 0x08;
    const ULONG_PTR KU_TIB_STACK_LIMIT_OFFSET = 0x10;
    const size_t    KN_STACK_REPORT_STEP      = 32u * 1024u;   // report each new 32 KB step

    size_t CurrentStackHighWater()
    {
        const ULONG_PTR luBase  = static_cast<ULONG_PTR>(__readgsqword(KU_TIB_STACK_BASE_OFFSET));
        const ULONG_PTR luLimit = static_cast<ULONG_PTR>(__readgsqword(KU_TIB_STACK_LIMIT_OFFSET));
        return (luBase > luLimit) ? static_cast<size_t>(luBase - luLimit) : 0u;
    }

    // Per-thread, so a worker thread's own growth is reported against its own reserve.
    __declspec(thread) size_t gtnStackReported = 0;

    // ---- the footprint line + the opt-in heap sweep (legs 2 and 3 of PollProcessHealth) ----
    const ULONGLONG KU_MEMORY_REPORT_PERIOD_MS = 10000ull;
    ULONGLONG gsuNextMemoryReportMs = 0;
    ULONGLONG gsuNextHeapCheckMs    = 0;

    // PROCESS_MEMORY_COUNTERS_EX, declared locally so this TU needs no psapi.h and the link no
    // psapi.lib: the function is resolved out of kernel32 (K32GetProcessMemoryInfo, present on
    // every Windows this build runs on).
    struct sPROCESS_MEMORY_COUNTERS_EX
    {
        DWORD  cb;
        DWORD  PageFaultCount;
        SIZE_T PeakWorkingSetSize;
        SIZE_T WorkingSetSize;
        SIZE_T QuotaPeakPagedPoolUsage;
        SIZE_T QuotaPagedPoolUsage;
        SIZE_T QuotaPeakNonPagedPoolUsage;
        SIZE_T QuotaNonPagedPoolUsage;
        SIZE_T PagefileUsage;
        SIZE_T PeakPagefileUsage;
        SIZE_T PrivateUsage;
    };

    typedef BOOL (WINAPI* PFN_GetProcessMemoryInfo)(HANDLE, sPROCESS_MEMORY_COUNTERS_EX*, DWORD);

    bool GetProcessMemoryInfoDynamic(sPROCESS_MEMORY_COUNTERS_EX* lpCounters)
    {
        static PFN_GetProcessMemoryInfo sfn = NULL;
        static bool sbResolved = false;
        if (!sbResolved)
        {
            sbResolved = true;
            HMODULE lhKernel = GetModuleHandleA("kernel32.dll");
            if (lhKernel != NULL)
            {
                sfn = reinterpret_cast<PFN_GetProcessMemoryInfo>(
                          reinterpret_cast<void*>(GetProcAddress(lhKernel, "K32GetProcessMemoryInfo")));
            }
        }
        return sfn != NULL &&
               sfn(GetCurrentProcess(), lpCounters, static_cast<DWORD>(sizeof(*lpCounters))) != FALSE;
    }

    // BRN_HEAP_CHECK=<seconds>. 0 / unset / unparseable = off, which is the default: HeapValidate
    // walks every block in the process heap and is far too slow to run unasked.
    ULONGLONG HeapCheckPeriodMs()
    {
        static ULONGLONG suPeriodMs = 0;
        static bool      sbRead     = false;
        if (!sbRead)
        {
            sbRead = true;
            const char* lpcValue = std::getenv("BRN_HEAP_CHECK");
            if (lpcValue != NULL)
            {
                ULONGLONG luSeconds = 0;
                for (const char* lpc = lpcValue; *lpc >= '0' && *lpc <= '9'; ++lpc)
                    luSeconds = luSeconds * 10ull + static_cast<ULONGLONG>(*lpc - '0');
                suPeriodMs = luSeconds * 1000ull;
            }
        }
        return suPeriodMs;
    }

    // First-chance observer. Runs BEFORE any frame-based handler, so it sees a fault even when
    // something further out swallows it, and it sees EXCEPTION_STACK_OVERFLOW at the instant the
    // guard page is hit -- while there is still a stack to log from. Bounded to KI_MAX_VECTORED
    // lines so a driver's routine internal first-chance traffic can never flood the log.
    const LONG KI_MAX_VECTORED = 8;
    volatile LONG gVectoredLogged = 0;

    bool IsFatalClassException(DWORD luCode)
    {
        switch (luCode)
        {
        case EXCEPTION_ACCESS_VIOLATION:
        case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
        case EXCEPTION_DATATYPE_MISALIGNMENT:
        case EXCEPTION_ILLEGAL_INSTRUCTION:
        case EXCEPTION_IN_PAGE_ERROR:
        case EXCEPTION_INT_DIVIDE_BY_ZERO:
        case EXCEPTION_PRIV_INSTRUCTION:
        case EXCEPTION_STACK_OVERFLOW:
        case EXCEPTION_NONCONTINUABLE_EXCEPTION:
        case EXCEPTION_INVALID_DISPOSITION:
        case 0xC0000409u:   // STATUS_STACK_BUFFER_OVERRUN (also the __fastfail carrier)
        case 0xC0000374u:   // STATUS_HEAP_CORRUPTION
            return true;
        default:
            return false;   // C++ EH (0xE06D7363), thread-name (0x406D1388), DBG_* -- ignored
        }
    }

    LONG CALLBACK VectoredProbe(EXCEPTION_POINTERS* lpExceptionInfo)
    {
        if (lpExceptionInfo == nullptr || lpExceptionInfo->ExceptionRecord == nullptr)
            return EXCEPTION_CONTINUE_SEARCH;

        const EXCEPTION_RECORD* lpRecord = lpExceptionInfo->ExceptionRecord;
        if (!IsFatalClassException(lpRecord->ExceptionCode))
            return EXCEPTION_CONTINUE_SEARCH;
        if (InterlockedIncrement(&gVectoredLogged) > KI_MAX_VECTORED)
            return EXCEPTION_CONTINUE_SEARCH;

        const uintptr_t luModuleBase = reinterpret_cast<uintptr_t>(GetModuleHandleA(nullptr));
        const uintptr_t luAddress    = reinterpret_cast<uintptr_t>(lpRecord->ExceptionAddress);
        Emitf("[exit-diag] FIRST-CHANCE %s (0x%08lX) at 0x%016llX (module+0x%llX) thread %lu"
              " stack-used %llu KB\n",
              ExceptionCodeName(lpRecord->ExceptionCode),
              static_cast<unsigned long>(lpRecord->ExceptionCode),
              static_cast<unsigned long long>(luAddress),
              static_cast<unsigned long long>(luAddress >= luModuleBase ? luAddress - luModuleBase : luAddress),
              static_cast<unsigned long>(GetCurrentThreadId()),
              static_cast<unsigned long long>(CurrentStackHighWater() / 1024u));
        return EXCEPTION_CONTINUE_SEARCH;
    }

    // The CRT's invalid-parameter leg. WITHOUT a handler installed here the UCRT runs
    // _invoke_watson, which ends in __fastfail(FAST_FAIL_INVALID_ARG): the process dies with no
    // exception delivered to anyone, no log line and no screenshot -- one of the exact silhouettes
    // defect B presents. With a handler installed the CRT routine simply returns its error code
    // instead, so the run SURVIVES and the bad call is named. Deliberately does NOT abort: the
    // point is to convert a silent kill into a visible, continuable diagnostic.
    // ⚠️ ROUND-8 QUALIFIERS (this is a real, permanent, UNGATED behaviour change -- know what it
    // buys and what it costs before treating it as free):
    //   * "the run SURVIVES" holds for THIS exe only. Burnout_PC.map links _invalid_parameter,
    //     _invalid_parameter_noinfo and _invoke_watson but NOT _invalid_parameter_noinfo_noreturn;
    //     the moment a *_noreturn secure-CRT path links in, this handler is called and the process
    //     __fastfail's anyway.
    //   * it converts a fatal bad argument into "log a line and return an error code", so a
    //     reconstructed caller that ignores that error code then continues on garbage. No call has
    //     ever tripped it (0 hits across every post-round-7 run), so the suppression is currently
    //     buying nothing and costing that risk in the shipping build.
    //   * "safe from any thread" is true of the handler's control flow but NOT of its logging:
    //     CgsLog's line buffer (static char[2048] + a static length, no lock) interleaves under
    //     concurrent writers. Pre-existing, but this probe is what adds arbitrary-thread callers.
    // Open follow-up (deliberately NOT done here -- it is a behaviour change, not a comment fix):
    // env-gate the install behind BRN_EXIT_DIAG rather than shipping it on by default.
    volatile LONG gInvalidParamLogged = 0;

    void InvalidParameterProbe(const wchar_t* lpwcExpression, const wchar_t* lpwcFunction,
                               const wchar_t* lpwcFile, unsigned int luLine, uintptr_t)
    {
        if (InterlockedIncrement(&gInvalidParamLogged) > KI_MAX_VECTORED)
            return;
        Emitf("[exit-diag] CRT INVALID PARAMETER: %ls in %ls (%ls:%u) -- would have been a silent "
              "__fastfail; the call returns an error instead\n",
              lpwcExpression ? lpwcExpression : L"<no expression>",
              lpwcFunction   ? lpwcFunction   : L"<no function>",
              lpwcFile       ? lpwcFile       : L"<no file>",
              luLine);
        CgsDev::StackUnpick lStack;
        lStack.Prepare();
        gCrashMapReader.Prepare(MapFilePath(), &lStack);
        Emit("  Callstack:\n");
        for (s32 liIndex = 0; liIndex < lStack.GetNumStackAddresses(); ++liIndex)
        {
            const char* lpcName = gCrashMapReader.GetStackEntryName(liIndex);
            if (lpcName)
                Emitf("    %s + 0x%X    [rva 0x%llX]\n", lpcName,
                      static_cast<unsigned>(gCrashMapReader.GetStackEntryOffset(liIndex)),
                      static_cast<unsigned long long>(lStack.GetStackAddress(liIndex)));
            else
                Emitf("    0x%llX\n", static_cast<unsigned long long>(lStack.GetStackAddress(liIndex)));
        }
        Emit("  EndCallstack\n");
    }

    // A pure-virtual call (a vtable used during construction/destruction, or through a freed
    // object). The CRT's default handler prints to stderr -- which this process does not have --
    // and terminates. Report it the same way an assert reports, then fall through to abort() so
    // the SIGABRT hook's screenshot still lands.
    void PurecallProbe()
    {
        Emit("[exit-diag] PURE VIRTUAL CALL\n");
        AbortSignalHandler(SIGABRT);
        std::abort();
    }

    void TerminateProbe()
    {
        Emit("[exit-diag] std::terminate\n");
        AbortSignalHandler(SIGABRT);
        std::abort();
    }

    // Any route through the CRT's exit(): a bring-up early-out, a vendor panic path, or WinMain
    // returning normally. Prints the reason the shutdown path already logged, or names itself as
    // the only line if nothing else did -- which is what distinguishes "the game asked to quit"
    // from "the process was killed".
    void ExitProbe()
    {
        Emitf("[exit-diag] CRT exit reached (stack high-water %llu KB) -- the process is leaving "
              "through exit()/WinMain return, NOT through a fault\n",
              static_cast<unsigned long long>(CurrentStackHighWater() / 1024u));
    }
}

    void Install()
    {
        SetUnhandledExceptionFilter(CrashFilter);
        std::signal(SIGABRT, AbortSignalHandler);

        // ⭐ Reserve a slice of this thread's stack for the exception filter (see the header
        // banner, case (b)). Without it EXCEPTION_STACK_OVERFLOW is reported from the single page
        // left below the guard page, WriteReport + GDI+ overrun that immediately, and the second
        // fault inside the filter takes the process out with no report at all -- a stack overflow
        // and a clean quit then look IDENTICAL in the log, which is exactly the ambiguity defect B
        // has been sitting in. 128 KB is comfortably more than the reporter's frame.
        ULONG luStackGuarantee = 128u * 1024u;
        SetThreadStackGuarantee(&luStackGuarantee);

        AddVectoredExceptionHandler(1u, VectoredProbe);
        _set_invalid_parameter_handler(InvalidParameterProbe);
        _set_purecall_handler(PurecallProbe);
        std::set_terminate(TerminateProbe);
        std::atexit(ExitProbe);
    }

    void PollProcessHealth()
    {
        // ---- leg 1: the stack high-water tripwire (one gs-relative read on the common path) --
        const size_t lnUsed = CurrentStackHighWater();
        if (lnUsed >= gtnStackReported + KN_STACK_REPORT_STEP)
        {
            gtnStackReported = lnUsed - (lnUsed % KN_STACK_REPORT_STEP);

            // The reserve is the whole VirtualAlloc reservation the stack was carved from:
            // querying any address inside it (a local will do -- we are standing on it) reports
            // the AllocationBase, and StackBase minus that is the reserved size, guard included.
            MEMORY_BASIC_INFORMATION lInfo;
            SIZE_T lnReserve = 0;
            char lcOnStack = 0;
            if (VirtualQuery(&lcOnStack, &lInfo, sizeof(lInfo)) != 0)
            {
                const ULONG_PTR luBase = static_cast<ULONG_PTR>(__readgsqword(KU_TIB_STACK_BASE_OFFSET));
                lnReserve = static_cast<SIZE_T>(luBase - reinterpret_cast<ULONG_PTR>(lInfo.AllocationBase));
            }
            Emitf("[exit-diag] stack high-water %llu KB of %llu KB reserved (thread %lu)\n",
                  static_cast<unsigned long long>(lnUsed / 1024u),
                  static_cast<unsigned long long>(lnReserve / 1024u),
                  static_cast<unsigned long>(GetCurrentThreadId()));
        }

        // ---- leg 2: the footprint line, once every 10 s --------------------------------------
        // The brief's leak suspicion rests on the on-screen allocator counter; this puts the same
        // question on a number in the log, sampled on a fixed cadence so a per-frame or per-event
        // leak shows as a slope rather than as an impression. K32GetProcessMemoryInfo is resolved
        // through kernel32 so the link gains no psapi dependency.
        const ULONGLONG luNow = GetTickCount64();
        if (luNow >= gsuNextMemoryReportMs)
        {
            gsuNextMemoryReportMs = luNow + KU_MEMORY_REPORT_PERIOD_MS;

            sPROCESS_MEMORY_COUNTERS_EX lCounters;
            ZeroMemory(&lCounters, sizeof(lCounters));
            lCounters.cb = sizeof(lCounters);
            if (GetProcessMemoryInfoDynamic(&lCounters))
            {
                Emitf("[exit-diag] mem workingSet=%llu MB peakWorkingSet=%llu MB private=%llu MB "
                      "pageFaults=%lu\n",
                      static_cast<unsigned long long>(lCounters.WorkingSetSize / (1024u * 1024u)),
                      static_cast<unsigned long long>(lCounters.PeakWorkingSetSize / (1024u * 1024u)),
                      static_cast<unsigned long long>(lCounters.PrivateUsage / (1024u * 1024u)),
                      static_cast<unsigned long>(lCounters.PageFaultCount));
            }
        }

        // ---- leg 3: the OPT-IN heap sweep, BRN_HEAP_CHECK=<seconds> --------------------------
        if (HeapCheckPeriodMs() != 0 && luNow >= gsuNextHeapCheckMs)
        {
            gsuNextHeapCheckMs = luNow + HeapCheckPeriodMs();
            const HANDLE lhHeap = GetProcessHeap();
            if (lhHeap != NULL && HeapValidate(lhHeap, 0, NULL) == FALSE)
            {
                // One report: once the heap is damaged every later sweep fails too, and each
                // report re-reads the 1.1 MB map file.
                gsuNextHeapCheckMs = ~0ull;
                Emit("[exit-diag] HEAP CHECK FAILED -- the process heap is corrupt. The next\n"
                     "            HeapAlloc/HeapFree that touches the damaged block will\n"
                     "            __fastfail (exit code 0xC0000374) with no handler and no log.\n");
                CgsDev::StackUnpick lStack;
                lStack.Prepare();
                gCrashMapReader.Prepare(MapFilePath(), &lStack);
                Emit("  Callstack:\n");
                for (s32 liIndex = 0; liIndex < lStack.GetNumStackAddresses(); ++liIndex)
                {
                    const char* lpcName = gCrashMapReader.GetStackEntryName(liIndex);
                    if (lpcName)
                        Emitf("    %s + 0x%X\n", lpcName,
                              static_cast<unsigned>(gCrashMapReader.GetStackEntryOffset(liIndex)));
                    else
                        Emitf("    0x%llX\n",
                              static_cast<unsigned long long>(lStack.GetStackAddress(liIndex)));
                }
                Emit("  EndCallstack\n");
            }
        }
    }
}
}
