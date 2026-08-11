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
                Emitf("    %s\n", lpcName);
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
}

    void Install()
    {
        SetUnhandledExceptionFilter(CrashFilter);
        std::signal(SIGABRT, AbortSignalHandler);
    }
}
}
