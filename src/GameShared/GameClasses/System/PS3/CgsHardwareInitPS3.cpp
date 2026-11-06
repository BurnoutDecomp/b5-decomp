#include <xtl.h>
#include <cstdio>
#include <cstring>

#include "GameShared/GameClasses/System/CgsHardwareInit.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Memory/CgsHeapMalloc.h"
#include "GameShared/GameClasses/Development/CgsStrStream.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "SDKs/EATech/eajobs/jobs.h"
#include "SDKs/EATech/eajobs/job_scheduler.h"
#include "SDKs/EATech/eajobs/job_thread_parameters.h"

// Native X360 hardware initialization; the historical PS3 path is attribution.
// ARTIST828E05A0 owns startup;828D60F8 owns the DVD-root getter.
namespace
{
    const char* KAC_AUTOTESTFILENAME = "autotest.txt"; // ARTIST82F33FCC
    const char* const KAC_AUTOTEST_CMD_PREFIX = "-autotest:";
    bool gbHardDiskAvailable; // ARTIST830EA3EC, native BSS
}

// CRT initializers82C6F908 then82C6F940 construct these actual objects in this
// order and register the real destructors82C757A0/82C757B0. JobManager() returns
// this same scheduler; no second scheduler or lazy initialization is involved.
EA::Jobs::JobScheduler CgsSystem::HardwareInit::mJobManager; // ARTIST830EA650
CgsMemory::HeapMallocCoreAllocator CgsSystem::HardwareInit::mJobManagerAllocator; //82F34160
char CgsSystem::HardwareInit::macJobManagerBuffer[400 * 1024]; // ARTIST83086388
char CgsSystem::HardwareInit::macRootPath[knHardwarePathMaxLength];
char CgsSystem::HardwareInit::macAutoTestScriptToRun[64]; // ARTIST83085F40
char CgsSystem::HardwareInit::macTitleIdFromCmdLine[10];
volatile bool CgsSystem::HardwareInit::mbHardwareRequestsShutdown;
bool CgsSystem::HardwareInit::mbIsGuideOnScreen;
bool CgsSystem::HardwareInit::mbHasDetectedAutomaticTestingFile; // ARTIST83085F80

EA::Jobs::JobScheduler* CgsSystem::JobManager()
{
    // CgsHardwareInit.h135 (DecFIGS); inlined address830EA650 in ARTIST callers.
    return &HardwareInit::mJobManager;
}

char* CgsSystem::HardwareInit::GetFOPENDirectory()
{
    return "d:\\";
}

void CgsSystem::HardwareInit::InitializeHardware(s32 lnArgc, char** lapcArgv,
                                                 const char* lpcWorkingDirOverride,
                                                 const char* lpcFOpenDirOverride)
{
    if (lpcWorkingDirOverride != 0)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::StrStream lMessage(CgsDev::Assert::gpcMessageBuffer,
                                   CgsDev::Assert::giMessageBufferSize);
        lMessage << "Can not override working dir on xbox\n";
        CgsDev::Assert::FireAssert(CgsDev::Assert::gpcMessageBuffer,
            "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\system\\X360/CgsHardwareInitX360.cpp", 64);
        CgsDev::Assert::EndAssert();
    }
    if (lpcFOpenDirOverride != 0)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::StrStream lMessage(CgsDev::Assert::gpcMessageBuffer,
                                   CgsDev::Assert::giMessageBufferSize);
        lMessage << "Can not override fopen dir on xbox\n";
        CgsDev::Assert::FireAssert(CgsDev::Assert::gpcMessageBuffer,
            "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\system\\X360/CgsHardwareInitX360.cpp", 65);
        CgsDev::Assert::EndAssert();
    }

    mJobManagerAllocator.Construct(macJobManagerBuffer, sizeof(macJobManagerBuffer));
    EA::Jobs::SetAllocator(&mJobManagerAllocator);
    mJobManager.SetProfiling(true);
    mJobManager.Initialize(128, 128);

    // ARTIST828E06E8..075C constructs the parameter block once, changing only
    // the processor between the three genuine asynchronous AddThread calls.
    EA::Jobs::JobThreadParameters jtp;
    jtp.priority = 350;
    jtp.processor = 1;
    mJobManager.AddThread(jtp);
    jtp.processor = 3;
    mJobManager.AddThread(jtp);
    jtp.processor = 5;
    mJobManager.AddThread(jtp);

    char lacFileName[knHardwarePathMaxLength];
    std::strcpy(lacFileName, GetFOPENDirectory());
    // Original inlined CgsCore::StrCat guard and copy,828E0784..0830.
    if (std::strlen(KAC_AUTOTESTFILENAME) + std::strlen(lacFileName) >= sizeof(lacFileName) - 1)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("(strlen(lpcSource)+strlen(lpcDest))<luBytes - 1",
            "..\\..\\..\\GameShared\\GameClasses\\Core/CgsStringUtils.h", 75);
        CgsDev::Assert::EndAssert();
    }
    std::strcat(lacFileName, KAC_AUTOTESTFILENAME);

    std::FILE* lpFile = std::fopen(lacFileName, "r");
    if (lpFile)
    {
        std::fgets(macAutoTestScriptToRun, sizeof(macAutoTestScriptToRun), lpFile);
        if (macAutoTestScriptToRun[0] == '\0')
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("Failed to select a script for autotesting",
                "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\system\\X360/CgsHardwareInitX360.cpp", 109);
            CgsDev::Assert::EndAssert();
        }
        mbHasDetectedAutomaticTestingFile = true;
        std::fclose(lpFile);
    }
    else
    {
        mbHasDetectedAutomaticTestingFile = false;
        for (s32 lnArg = 0; lnArg < lnArgc; ++lnArg)
        {
            if (std::strncmp(KAC_AUTOTEST_CMD_PREFIX, lapcArgv[lnArg], 10) == 0)
            {
                std::strncpy(macAutoTestScriptToRun, lapcArgv[lnArg] + 10,
                             sizeof(macAutoTestScriptToRun));
                mbHasDetectedAutomaticTestingFile = true;
                break;
            }
        }
    }

    const DWORD luMountResult = XMountUtilityDrive(1, 0x10000, 0x40000);
    if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
        *CgsDev::Log::gpDebugPrint << "XMountUtilityDrive() : "
                                 << static_cast<u32>(luMountResult) << "\n";
    if (luMountResult == 0)
    {
        XFlushUtilityDrive();
        gbHardDiskAvailable = true;
    }
    else
    {
        gbHardDiskAvailable = false;
    }
}

bool CgsSystem::HardwareInit::IsHardDiskAvailable()
{
    return gbHardDiskAvailable;
}

// ARTIST828D6138: enabling another worker dynamically is unsupported.
void CgsSystem::HardwareInit::EnableJobThread()
{
    CgsDev::Assert::BeginAssert();
    CgsDev::Assert::FireAssert("Not supported on xbox\n",
        "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\system\\X360/CgsHardwareInitX360.cpp", 247);
    CgsDev::Assert::EndAssert();
}
