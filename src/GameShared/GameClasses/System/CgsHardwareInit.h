#pragma once

#include "types.hpp"

// CGS_PLATFORM_X360 selects the X360 hardware-init surface even when the PC
// toolchain (which defines WIN32) is building the X360 translation unit. The X360
// .cpp defines it before including this header; the PC build never does, so the
// WIN32 branch below is unchanged for PC.
#if defined(WIN32) && !defined(CGS_PLATFORM_X360)
#include <Windows.h>
#endif

namespace CgsSystem
{
#if defined(WIN32) && !defined(CGS_PLATFORM_X360)
    static const s32 knHardwarePathMaxLength = MAX_PATH + 7;
#else
    static const s32 knHardwarePathMaxLength = 1024;
#endif

    class HardwareInit
    {
    public:
#if defined(WIN32) && !defined(CGS_PLATFORM_X360)
        static void InitializeHardware(const char* lpCmdLine);
#elif defined(CGS_PLATFORM_X360)
        // X360: argc/argv plus the (always-null on xbox) working-/fopen-dir overrides.
        static void InitializeHardware(s32 lnArgc, char** lapcArgv,
                                       const char* lpcWorkingDirOverride,
                                       const char* lpcFOpenDirOverride);
#else
        static void InitializeHardware(s32 lnArgc, char* lapcArgv[]);
#endif

#if defined(CGS_PLATFORM_X360)
        static void EnableJobThread();   // @0x828D6138 -- X360 stub, always asserts "Not supported on xbox\n"
#endif

        static char* GetWorkingDirectory() { return macRootPath; }
        static char* GetFOPENDirectory() { return macFOPENPath; }

        static void ReleaseHardware();

        static bool IsAlreadyRunning();

        static bool IsHardwareWantingToShutdown() { return mbHardwareRequestsShutdown; }

        static bool IsGuideOnScreen() { return mbIsGuideOnScreen; }

        static bool HasDetectedAutomaticTestingFile() { return mbHasDetectedAutomaticTestingFile; }

        static char* GetAutoScriptToRun() { return macAutoTestScriptToRun; }

        static char* GetTitleIDFromCommandLine() { return macTitleIdFromCmdLine; }

        static bool IsHardDiskAvailable();

    private:
        static char macRootPath[knHardwarePathMaxLength];
        static char macFOPENPath[knHardwarePathMaxLength];

        static char macAutoTestScriptToRun[64];

        static char macTitleIdFromCmdLine[10];

        //static JobScheduler mJobManager; // TODO: Implement HardwareInit

        static char macJobManagerBuffer[400 * 1024]; // 400 KB buffer for job manager

        //static CgsMemory::HeapMallocCoreAllocator mJobManagerAllocator; // TODO: Implement HardwareInit

        static volatile bool mbHardwareRequestsShutdown;

        static bool mbIsGuideOnScreen;

        static bool mbHasDetectedAutomaticTestingFile;

        static bool DetermineIsPartyEditionVersion();

    };
}
