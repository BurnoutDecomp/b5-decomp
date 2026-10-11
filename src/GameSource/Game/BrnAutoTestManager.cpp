#include "GameSource/Game/BrnAutoTestManager.h"
#include "GameSource/Game/BrnGameModule.hpp"                                   // GetMainGameModule / GetGameTimer / GetGameDataModule
#include "GameSource/Resource/BrnGameDataModule.h"                             // GameDataModule::DebugReportPools
#include "GameShared/GameClasses/Core/CgsStringUtils.h"                        // CgsCore::StrCpy / StrCat
#include "GameShared/GameClasses/System/CgsHardwareInit.h"                     // HardwareInit::GetFOPENDirectory
#include "GameShared/GameClasses/System/Resource/CgsResourcePool.h"            // CgsResource::PoolStats
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"
#include "GameShared/GameClasses/Development/PerfMon/Gpu/CgsPerfMonGpu.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

extern "C"
{
#include "lua.h"
#include "lualib.h"
}

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
    // ARTIST 0x823A9150; the library table at 0x82035274 contains these six
    // entries in this order, followed by a null pair. Base opens separately.
    void OpenLUALibs(lua_State* lpLuaHandle)
    {
        struct Library
        {
            const char* mpcName;
            lua_CFunction mpfnOpen;
        };
        static const Library KAC_LIBRARIES[] =
        {
            { "package", luaopen_package },
            { "table", luaopen_table },
            { "io", luaopen_io },
            { "string", luaopen_string },
            { "math", luaopen_math },
            { "debug", luaopen_debug },
            { 0, 0 },
        };

        luaopen_base(lpLuaHandle);
        for (const Library* lpLibrary = KAC_LIBRARIES; lpLibrary->mpfnOpen; ++lpLibrary)
        {
            lua_pushcclosure(lpLuaHandle, lpLibrary->mpfnOpen, 0);
            lua_pushstring(lpLuaHandle, lpLibrary->mpcName);
            lua_call(lpLuaHandle, 1, 0);
        }
    }

    // ARTIST 0x823A91D0. Lua's allocator callback has no implicit this.
    void* AutoTestManager::LuaAllocator(void* /*lpUserData*/, void* lpPtr,
                                        size_t /*luOldSize*/, size_t luNewSize)
    {
        if (luNewSize)
            return std::realloc(lpPtr, luNewSize);
        std::free(lpPtr);
        return 0;
    }

    // ARTIST 0x823A9218; DecFIGS BrnAutoTestManager.cpp:163 supplies the
    // void(char*, int32_t, const char*) shape. The root's final character is
    // the separator used between TESTINGSCRIPTS and the supplied script name.
    void GetScriptName(char* lpcBuffer, s32 liBufferSize, const char* lpcScriptName)
    {
        if (CgsSystem::HardwareInit::HasDetectedAutomaticTestingFile())
        {
            const char* lpcFOPENDirectory = CgsSystem::HardwareInit::GetFOPENDirectory();
            const char lcSeparator = lpcFOPENDirectory[std::strlen(lpcFOPENDirectory) - 1];
            CgsCore::SPrintf(lpcBuffer, liBufferSize, "%sTESTINGSCRIPTS%c%s",
                             lpcFOPENDirectory, lcSeparator, lpcScriptName);
            const size_t luLength = std::strlen(lpcBuffer);
            for (size_t luIndex = 0; luIndex < luLength; ++luIndex)
            {
                if (lpcBuffer[luIndex] == '\r' || lpcBuffer[luIndex] == '\n')
                    lpcBuffer[luIndex] = '\0';
            }
        }
    }

    // ARTIST 0x823C0438. The current GUI page and load-completion flag are
    // left for the script state machine; neither is written by Init.
    void AutoTestManager::Init(BrnGameModule* lpGameModule)
    {
        mpGameModule = lpGameModule;
        miFramesSinceLastGameStateSave = 0;
        miNumScriptsLoaded = 0;
        mpLuaHandle = 0;
        meScriptState = eScriptLoading;
        mbControlPad = true;
        mAutoTestLogChannel.SetChannel(static_cast<CgsDev::Log::LogChannelOutput::EUserChannel>(0));
        mAutoTestLog.AddStream(&mAutoTestLogFile);
        mAutoTestLog.AddStream(&mAutoTestLogChannel);
        for (s32 liAction = 0; liAction < 61; ++liAction)
            maActions[liAction] = 0.0f;
        LoadScripts();
    }

    // ARTIST 0x823A9300. The seven callback bindings are the complete table
    // at 0x820352B0. Their bodies belong to the independent script callbacks.
    void AutoTestManager::LoadScripts()
    {
        if (!CgsSystem::HardwareInit::HasDetectedAutomaticTestingFile())
            return;

        char lacScriptName[1024];
        GetScriptName(lacScriptName, sizeof(lacScriptName),
                      CgsSystem::HardwareInit::GetAutoScriptToRun());
        FILE* lpFile = std::fopen(lacScriptName, "r");
        if (lpFile)
        {
            std::memset(macScript, 0, sizeof(macScript));
            char lacLine[1024];
            while (!std::feof(lpFile))
            {
                std::fgets(lacLine, sizeof(lacLine), lpFile);
                if (!std::feof(lpFile))
                {
                    // ARTIST inlines the original asserted append. The current
                    // shared StrCat truncates, so retain the binary's behavior.
                    CGS_ASSERT(std::strlen(lacLine) + std::strlen(macScript) < sizeof(macScript) - 1,
                        "(strlen(lpcSource)+strlen(lpcDest))<luBytes - 1");
                    std::strcat(macScript, lacLine);
                }
            }
            std::fclose(lpFile);
            ++miNumScriptsLoaded;
            mpLuaHandle = lua_newstate(&AutoTestManager::LuaAllocator, 0);
            OpenLUALibs(mpLuaHandle);
            CGS_ASSERT(mpLuaHandle, "mpLuaHandle");
        }

        struct Binding
        {
            const char* mpcName;
            lua_CFunction mpfnCallback;
        };
        static const Binding KAC_CALLBACKS[] =
        {
            { "CallDebugFunction", &AutoTestManager::LUACBCallDebugFunction },
            { "RecordLog", &AutoTestManager::LUACBRecordLog },
            { "PressButton", &AutoTestManager::LUACBPressButton },
            { "AcquirePad", &AutoTestManager::LUACBAcquirePad },
            { "RelinquishPad", &AutoTestManager::LUACBRelinquishPad },
            { "ActivateDebugComponent", &AutoTestManager::LUACBActivateDebugComponent },
            { "DeactivateDebugComponent", &AutoTestManager::LUACBDeactivateDebugComponent },
            { 0, 0 },
        };
        for (const Binding* lpBinding = KAC_CALLBACKS; lpBinding->mpcName; ++lpBinding)
        {
            CGS_ASSERT(mpLuaHandle, "mpLuaHandle");
            lua_pushcclosure(mpLuaHandle, lpBinding->mpfnCallback, 0);
            lua_setfield(mpLuaHandle, LUA_GLOBALSINDEX, lpBinding->mpcName);
            lua_settop(mpLuaHandle, 0);
            if (CgsDev::Message::gxMessageFilterFlags & 1)
                *CgsDev::Log::gpDebugPrint << "Registered function " << lpBinding->mpcName << "\n";
        }
    }

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
