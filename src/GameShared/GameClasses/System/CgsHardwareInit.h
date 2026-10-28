#pragma once

#include "types.hpp"
namespace EA { namespace Jobs { class JobScheduler; } }
namespace CgsMemory { class HeapMallocCoreAllocator; }

// CGS_PLATFORM_X360 selects the X360 hardware-init surface even when the PC
// toolchain (which defines WIN32) is building the X360 translation unit. The X360
// .cpp defines it before including this header; the PC build never does, so the
// WIN32 branch below is unchanged for PC.

namespace CgsSystem
{
    // CgsHardwareInit.h:135; ARTIST loader Construct passes the shared
    // HardwareInit scheduler at830EA650 to DecompressionJobInterface.
    EA::Jobs::JobScheduler* JobManager();
    static const s32 knHardwarePathMaxLength = 1024;

    class HardwareInit
    {
    public:
        // X360: argc/argv plus the (always-null on xbox) working-/fopen-dir overrides.
        static void InitializeHardware(s32 lnArgc, char** lapcArgv,
                                       const char* lpcWorkingDirOverride,
                                       const char* lpcFOpenDirOverride);

        // ARTIST 0x828D6138: unconditional unsupported assertion on X360; preserve it for PC callers.
        // This declaration does not enable or start job workers.
        static void EnableJobThread();

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
        friend EA::Jobs::JobScheduler* JobManager();
        static char macRootPath[knHardwarePathMaxLength];
        static char macFOPENPath[knHardwarePathMaxLength];

        static char macAutoTestScriptToRun[64];

        static char macTitleIdFromCmdLine[10];

        static EA::Jobs::JobScheduler mJobManager;

        static char macJobManagerBuffer[400 * 1024]; // 400 KB buffer for job manager

        static CgsMemory::HeapMallocCoreAllocator mJobManagerAllocator;

        static volatile bool mbHardwareRequestsShutdown;

        static bool mbIsGuideOnScreen;

        static bool mbHasDetectedAutomaticTestingFile;

        static bool DetermineIsPartyEditionVersion();

    };
}
