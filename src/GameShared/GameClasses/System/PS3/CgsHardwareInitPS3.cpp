#include <cstdio>
#include <cstring>

// Build the X360 hardware-init surface of the shared header (this is the X360 TU
// despite the PS3 path; see the file note below). Must precede the header include.
#define CGS_PLATFORM_X360 1
#include "GameShared/GameClasses/System/CgsHardwareInit.h"

#include "GameShared/GameClasses/Core/CgsStringUtils.h"     // CgsCore::StrCpy / StrCat
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Memory/CgsHeapMalloc.h"   // CgsMemory::HeapMallocCoreAllocator
#include "GameShared/GameClasses/Development/Log/CgsLog.h"  // CgsDev::Log::gpDebugPrint, CgsDev::Message::gxMessageFilterFlags

#include "SDKs/EATech/eajobs/jobs.h"                    // EA::Jobs::SetAllocator
#include "SDKs/EATech/eajobs/job_scheduler.h"           // EA::Jobs::JobScheduler
#include "SDKs/EATech/eajobs/job_thread_parameters.h"   // EA::Jobs::JobThreadParameters

// GameShared/GameClasses/System/PS3/CgsHardwareInitPS3.cpp
//
// NOTE: despite the PS3 path this TU is the X360 hardware-init translation unit
// (the X360 .XEX: BURNOUT_X360_ARTIST.XEX). The reconstruction is driven by the
// X360 asm @ 0x828E05A0 (InitializeHardware) / 0x828D6128 (IsHardDiskAvailable):
// the assert text reads "...on xbox" and the platform calls are the X360 XDK
// XMountUtilityDrive / XFlushUtilityDrive. Reconstructed at the dest path the
// dossier specifies; the cross-platform accessors live (inline) in the shared
// CgsHardwareInit.h home and are NOT redefined here.

namespace
{
    // off_82F33FCC -- the default autotest script filename appended to the FOPEN
    // directory to look for the autotesting trigger file.
    const char* const KAC_AUTOTESTFILENAME = "autotest.txt";

    // aAutotest -- the command-line switch that selects an autotest script.
    const char* const KAC_AUTOTEST_CMD_PREFIX = "-autotest:";

    // The job-manager worker thread defaults the X360 asm stores into the
    // JobThreadParameters block before each AddThread (@ 0x828E0700..0x828E075C):
    //   priority  = 350   (0x15E)   -- stw r11(=0x15E), +0x8
    //   processor = 1 / 3 / 5       -- stw r11(=1,3,5),  +0xC
    // (environment, stack size, affinity and the thread name come straight from
    // the JobThreadParameters() ctor defaults.)
    const int   KI_JOBTHREAD_PRIORITY  = 350;
    const int   KI_JOBTHREAD_PROCESSORS[3] = { 1, 3, 5 };
    const int   KI_JOBSCHEDULER_THREADS = 128; // li r4,0x80 @ 0x828E06DC
    const int   KI_JOBSCHEDULER_JOBS    = 128; // li r5,0x80 @ 0x828E06D8

    // XMountUtilityDrive arguments (@ 0x828E091C..0x828E0928):
    //   fFormatClean      = 1        (li  r3,1)
    //   dwBytesPerCluster = 0x10000  (lis r4,1)
    //   dwFileCacheSize   = 0x40000  (lis r5,4)
    const int KI_XMOUNT_FORMAT_CLEAN       = 1;
    const int KI_XMOUNT_BYTES_PER_CLUSTER  = 0x10000;
    const int KI_XMOUNT_FILE_CACHE_SIZE    = 0x40000;

    // byte_830EA3EC -- 1 when the utility partition mounted cleanly (no flush
    // needed), 0 when XMountUtilityDrive reported an existing/dirty partition that
    // had to be flushed. IsHardDiskAvailable() returns this flag.
    bool gbHardDiskAvailable = false;

    // The process-wide job-manager scheduler (unk_830EA650) and its general-heap
    // allocator front-end (off_82F34160). On X360 these are file-scope statics the
    // job-manager SDK installs once at boot; the engine's general heap is exposed
    // to the EA middleware through the ICoreAllocator interface.
    CgsMemory::HeapMallocCoreAllocator gJobManagerAllocator;
    EA::Jobs::JobScheduler             gJobManager;
}

// ---------------------------------------------------------------------------
// X360 platform (XDK) APIs -- todo external callees; declaration suffices.
// Signatures recovered from the InitializeHardware call sites.
// ---------------------------------------------------------------------------
extern "C" int  XMountUtilityDrive(int fFormatClean, int dwBytesPerCluster, int dwFileCacheSize);
extern "C" void XFlushUtilityDrive();

// ---------------------------------------------------------------------------
// Static storage for the shared HardwareInit members (the X360 data symbols
// byte_82046464 / byte_83085F40 / byte_83085F80 / macTitleIdFromCmdLine etc.).
// The PC variant defines its own copy; only one platform .cpp links per build.
// ---------------------------------------------------------------------------
char CgsSystem::HardwareInit::macRootPath[knHardwarePathMaxLength];
char CgsSystem::HardwareInit::macFOPENPath[knHardwarePathMaxLength];
char CgsSystem::HardwareInit::macAutoTestScriptToRun[64];
char CgsSystem::HardwareInit::macTitleIdFromCmdLine[10];
char CgsSystem::HardwareInit::macJobManagerBuffer[400 * 1024];
volatile bool CgsSystem::HardwareInit::mbHardwareRequestsShutdown;
bool CgsSystem::HardwareInit::mbIsGuideOnScreen;
bool CgsSystem::HardwareInit::mbHasDetectedAutomaticTestingFile;

void CgsSystem::HardwareInit::InitializeHardware(s32 lnArgc, char** lapcArgv,
                                                 const char* lpcWorkingDirOverride,
                                                 const char* lpcFOpenDirOverride)
{
    // The X360 has no separate working-/fopen-directory roots to override, so any
    // caller-supplied override is a programming error.
    CGS_ASSERT(lpcWorkingDirOverride == nullptr, "Can not override working dir on xbox\n");
    CGS_ASSERT(lpcFOpenDirOverride == nullptr, "Can not override fopen dir on xbox\n");

    // --- Bring up the job manager ---------------------------------------------
    // Build the engine's general heap over the static buffer and expose it to the
    // EA middleware through the ICoreAllocator interface.
    gJobManagerAllocator.Construct(macJobManagerBuffer, sizeof(macJobManagerBuffer));
    EA::Jobs::SetAllocator(&gJobManagerAllocator);

    gJobManager.SetProfiling(true);
    gJobManager.Initialize(KI_JOBSCHEDULER_THREADS, KI_JOBSCHEDULER_JOBS);

    // Three worker threads, on processors 1, 3 and 5 (hardware threads 1/3/5 of the
    // X360's three cores). All share the same priority/affinity/name defaults.
    for (s32 lnThread = 0; lnThread < 3; ++lnThread)
    {
        EA::Jobs::JobThreadParameters jtp;
        jtp.priority  = KI_JOBTHREAD_PRIORITY;
        jtp.processor = KI_JOBTHREAD_PROCESSORS[lnThread];
        gJobManager.AddThread(jtp);
    }

    // --- Autotesting -----------------------------------------------------------
    // Look for "<fopen-dir>/autotest.txt"; the first line of that file (if present)
    // names the script to run, otherwise an "-autotest:<script>" command-line
    // switch can supply it.
    char lacFileName[knHardwarePathMaxLength];
    CgsCore::StrCpy(lacFileName, sizeof(lacFileName), GetFOPENDirectory());
    CgsCore::StrCat(lacFileName, sizeof(lacFileName), KAC_AUTOTESTFILENAME);

    std::FILE* lpFile = fopen(lacFileName, "r");
    if (lpFile)
    {
        fgets(macAutoTestScriptToRun, sizeof(macAutoTestScriptToRun), lpFile);
        CGS_ASSERT(macAutoTestScriptToRun[0] != '\0',
                   "Failed to select a script for autotesting");
        mbHasDetectedAutomaticTestingFile = true;
        fclose(lpFile);
    }
    else
    {
        mbHasDetectedAutomaticTestingFile = false;
        for (s32 lnArg = 0; lnArg < lnArgc; ++lnArg)
        {
            if (strncmp(KAC_AUTOTEST_CMD_PREFIX, lapcArgv[lnArg], 10) == 0)
            {
                strncpy(macAutoTestScriptToRun,
                        lapcArgv[lnArg] + 10,
                        sizeof(macAutoTestScriptToRun));
                mbHasDetectedAutomaticTestingFile = true;
                break;
            }
        }
    }

    // --- Mount the utility (hard-disk) partition -------------------------------
    const int liMountResult = XMountUtilityDrive(KI_XMOUNT_FORMAT_CLEAN,
                                                 KI_XMOUNT_BYTES_PER_CLUSTER,
                                                 KI_XMOUNT_FILE_CACHE_SIZE);

    if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
    {
        *CgsDev::Log::gpDebugPrint << "XMountUtilityDrive() : "
                                   << static_cast<u32>(liMountResult)
                                   << "\n";
    }

    if (liMountResult != 0)
    {
        // XMountUtilityDrive reported an error: the utility partition is not usable.
        gbHardDiskAvailable = false;
    }
    else
    {
        // Mounted successfully: flush it, then mark the hard disk available.
        XFlushUtilityDrive();
        gbHardDiskAvailable = true;
    }
}

bool CgsSystem::HardwareInit::IsHardDiskAvailable()
{
    return gbHardDiskAvailable;
}

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsSystem::HardwareInit::EnableJobThread  @ 0x828D6138
//
// X360: the job-manager worker threads are brought up during
// InitializeHardware() (fixed processors 1/3/5). There is no runtime
// enable-a-job-thread path on xbox, so this entry point is a hard stop.
// (De-inlined BeginAssert/FireAssert/EndAssert chain @ 0x828D6138, no branch
// guard -> unconditional assert. File/line args dropped; rodata message kept
// verbatim including its trailing \n.) Called by BrnReplays::ReplayModule::StopPlaying.
void CgsSystem::HardwareInit::EnableJobThread()
{
    CGS_ASSERT(false, "Not supported on xbox\n");
}
