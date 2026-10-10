#include "GameSource/Game/BrnAutoTestManager.h"
#include "GameSource/Game/BrnGameModule.hpp"                                   // GetMainGameModule / GetGameTimer / GetGameDataModule
#include "GameSource/Resource/BrnGameDataModule.h"                             // GameDataModule::DebugReportPools
#include "GameShared/GameClasses/Core/CgsStringUtils.h"                        // CgsCore::StrCpy / StrCat
#include "GameShared/GameClasses/System/CgsHardwareInit.h"                     // HardwareInit::GetFOPENDirectory
#include "GameShared/GameClasses/System/Resource/CgsResourcePool.h"            // CgsResource::PoolStats
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"
#include "GameShared/GameClasses/Development/PerfMon/Gpu/CgsPerfMonGpu.h"

// BrnGame::AutoTestManager::AutoTestManager @ 0x827DB4C0 (BURNOUT_X360_ARTIST.XEX).
//
// The X360 constructor is nothing but member sub-object construction of the three log streams, in
// declaration order (the buffer/pointer members before them are left for Init()):
//
//   this+0x3C04  mAutoTestLogFile    -> StrStreamBase() (vtable off_82000D00, mePrintMode=0),
//                                       miFile = -1 (INVALID_HANDLE_VALUE), then LogFile vtable
//                                       off_820CDC9C.
//   this+0x3C10  mAutoTestLogChannel -> StrStreamBase(), miChannel = -1, then LogChannelOutput
//                                       vtable off_82014B8C.
//   this+0x3C1C  mAutoTestLog        -> StrStreamBase(), mapStreams[0..3] = 0, then LogCombined
//                                       vtable off_820AB7AC.
//
// Each member's own default constructor reproduces its vtable store + value-init, so the body is
// empty; the trailing zeroes the asm writes at 0x3C24..0x3C30 are LogCombined's four mapStreams
// slots. No other member is touched by the constructor (Hex-Rays' "BasePriorityQueue::Clear" calls
// are the misnamed StrStreamBase base ctor at each embedded log object).

namespace BrnGame
{
    AutoTestManager::AutoTestManager()
    {
    }
    namespace
    {
        // The GUI page the game is on while driving; the gamestate dump only runs there.
        const CgsID KN_IN_GAME_GUI_PAGE = 0xC80400007E1706CEull;

        // miFramesSinceLastGameStateSave wraps at this count.
        const s32 KI_GAMESTATE_SAVE_FRAME_WRAP = 600;

        const s32 KI_GAMESTATE_PATH_LENGTH = 1024;
    }

    bool AutoTestManager::IsInGame()
    {
        return BrnGame::GetMainGameModule()->GuiAcceptsControllerInput() && mnCurrentGuiPage == KN_IN_GAME_GUI_PAGE;
    }

    void AutoTestManager::PerfMonCpuReportCallback(const CgsDev::PerfMonCpuMonitorData& lrData, void* lpUserData)
    {
        CgsDev::StrStreamBase& lrStream = *static_cast<CgsDev::StrStreamBase*>(lpUserData);

        lrStream << "        <cpumonitor";
        lrStream << " name=\"";
        lrStream << ((lrData.mpcName != nullptr) ? lrData.mpcName : "<NULLSTRING>");
        lrStream << "\"";
        lrStream << " currentvalue=\"";
        lrStream << lrData.mfCurrentValue << "\"";
        lrStream << " averagevalue=\"";
        lrStream << lrData.mfAverageValue << "\"";
        lrStream << " maxvalue=\"";
        lrStream << lrData.mfMinMaxValue << "\"";
        lrStream << " cpubudget=\"";
        lrStream << lrData.mfCpuBudget << "\"";
        lrStream << " numcalls=\"";
        lrStream << lrData.miNumCalls << "\"";
        lrStream << " maxcalls=\"";
        lrStream << lrData.miMaxCalls << "\"";
        lrStream << " />\n";
    }

    void AutoTestManager::PerfMonGpuReportCallback(const CgsDev::PerfMonGpuMonitorData& lrData, void* lpUserData)
    {
        CgsDev::StrStreamBase& lrStream = *static_cast<CgsDev::StrStreamBase*>(lpUserData);

        lrStream << "        <gpumonitor";
        lrStream << " name=\"";
        lrStream << ((lrData.mpcName != nullptr) ? lrData.mpcName : "<NULLSTRING>");
        lrStream << "\"";
        lrStream << " currentvalue=\"";
        lrStream << lrData.mfCurrentValue << "\"";
        lrStream << " overbudget=\"";
        lrStream << lrData.mbOverBudget;
        lrStream << "\"";
        lrStream << " />\n";
    }

    // One <pool> element with a <memtype> child per memory type.
    void AutoTestManager::PoolReportCallback(const CgsResource::PoolStats& lrStats, void* lpUserData)
    {
        CgsDev::StrStreamBase& lrStream = *static_cast<CgsDev::StrStreamBase*>(lpUserData);

        lrStream << "        <pool";
        lrStream << " name=\"";
        lrStream << ((lrStats.mpcPoolName != nullptr) ? lrStats.mpcPoolName : "<NULLSTRING>");
        lrStream << "\"";
        lrStream << " maxresources=\"";
        lrStream << lrStats.miPoolMaxResources << "\"";
        lrStream << " usedresources=\"";
        lrStream << lrStats.miPoolUsedResources << "\"";
        lrStream << " freeresources=\"";
        lrStream << lrStats.miPoolFreeResources << "\"";
        lrStream << " >\n";

        for (s32 liMemType = 0; liMemType < 3; ++liMemType)
        {
            lrStream << "            <memtype";
            lrStream << " id=\"";
            lrStream << liMemType;
            lrStream << "\"";
            lrStream << " size=\"";
            lrStream << lrStats.maiHeapSize[liMemType];
            lrStream << "\"";
            lrStream << " used=\"";
            lrStream << lrStats.maiHeapUsed[liMemType];
            lrStream << "\"";
            lrStream << " free=\"";
            lrStream << lrStats.maiHeapFree[liMemType];
            lrStream << "\"";
            lrStream << " maxresources=\"";
            lrStream << lrStats.maiHeapMaxResources[liMemType];
            lrStream << "\"";
            lrStream << " usedresources=\"";
            lrStream << lrStats.maiHeapUsedResources[liMemType];
            lrStream << "\"";
            lrStream << " freeresources=\"";
            lrStream << lrStats.maiHeapFreeResources[liMemType];
            lrStream << "\"";
            lrStream << " largestfreeblock=\"";
            lrStream << lrStats.maiHeapLargestBlock[liMemType];
            lrStream << "\"";
            lrStream << " />\n";
        }

        lrStream << "        </pool>\n";
    }

    // The capture time is the game timer's whole ticks plus its accumulator. The build date and
    // time are the compiler's.
    void AutoTestManager::DumpGameState(const char* lpcName)
    {
        if (!IsInGame())
        {
            return;
        }

        const char* lpcFileName = (lpcName != nullptr) ? lpcName : "gamestate.xml";

        ++miFramesSinceLastGameStateSave;
        if (miFramesSinceLastGameStateSave >= KI_GAMESTATE_SAVE_FRAME_WRAP)
        {
            miFramesSinceLastGameStateSave = 0;
        }

        mAutoTestLogChannel << "\n------------BEGGINING LOG-------------\n";

        char lacPath[KI_GAMESTATE_PATH_LENGTH];
        CgsCore::StrCpy(lacPath, sizeof(lacPath), CgsSystem::HardwareInit::GetFOPENDirectory());
        CgsCore::StrCat(lacPath, sizeof(lacPath), lpcFileName);
        mAutoTestLogFile.Open(lacPath, true);

        mAutoTestLog << "<capture>\n";
        mAutoTestLog << "    <buildconfiguration>artist</buildconfiguration>\n";
        mAutoTestLog << "    <platform>X360</platform>\n";
        mAutoTestLog << "    <builddate>";
        mAutoTestLog << __DATE__;
        mAutoTestLog << "</builddate>\n";
        mAutoTestLog << "    <buildtime>";
        mAutoTestLog << __TIME__;
        mAutoTestLog << "</buildtime>\n";

        const CgsSystem::Timer& lrGameTimer = mpGameModule->GetGameTimer();
        const f32 lfCaptureTime = static_cast<f32>(lrGameTimer.GetAccumTicks()) + lrGameTimer.GetAccumulator();
        CgsDev::StrStreamBase& lrLog = mAutoTestLog;
        mAutoTestLog << "    <capturetime>";
        lrLog << lfCaptureTime << "</capturetime>\n";

        mAutoTestLog << "    <cpumonitors>\n";
        CgsDev::PerfMonCpu::ReportMonitors(&AutoTestManager::PerfMonCpuReportCallback, &mAutoTestLog, CgsDev::E_PMP_MAX);
        mAutoTestLog << "    </cpumonitors>\n";

        mAutoTestLog << "    <gpumonitors>\n";
        CgsDev::PerfMonGpu::ReportMonitors(&AutoTestManager::PerfMonGpuReportCallback, &mAutoTestLog, true);
        mAutoTestLog << "    </gpumonitors>\n";

        mAutoTestLog << "    <pools>\n";
        mpGameModule->GetGameDataModule().DebugReportPools(&AutoTestManager::PoolReportCallback, &mAutoTestLog);
        mAutoTestLog << "    </pools>\n";

        mAutoTestLog << "</capture>\n";
        mAutoTestLogFile.Close();

        mAutoTestLogChannel << "\n------------LOG COMPLETE-------------\n";
    }
}
