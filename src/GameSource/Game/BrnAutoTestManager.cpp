#include "GameSource/Game/BrnAutoTestManager.h"
#include "GameSource/Game/BrnGameModule.hpp"                                   // gpBurnoutGame / GetGameTimer / GetGameDataModule
#include "GameSource/Resource/BrnGameDataModule.h"                             // GameDataModule::DebugReportPools
#include "GameShared/GameClasses/Core/CgsStringUtils.h"                        // CgsCore::StrCpy / StrCat
#include "GameShared/GameClasses/System/CgsHardwareInit.h"                     // HardwareInit::GetFOPENDirectory
#include "GameShared/GameClasses/System/Input/CgsInputModuleIO.h"              // InputIO::ActionInfo
#include "GameShared/GameClasses/System/Resource/CgsResourcePool.h"            // CgsResource::PoolStats
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"
#include "GameShared/GameClasses/Development/PerfMon/Gpu/CgsPerfMonGpu.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern "C" {
    typedef int (*AutoTestLuaFunction)(lua_State*);
    lua_State* lua_newstate(void* (*)(void*, void*, size_t, size_t), void*);
    void lua_pushcclosure(lua_State*, AutoTestLuaFunction, int);
    void lua_pushstring(lua_State*, const char*);
    void lua_call(lua_State*, int, int);
    void lua_setfield(lua_State*, int, const char*);
    void lua_settop(lua_State*, int);
    int luaopen_base(lua_State*);
    int luaopen_package(lua_State*);
    int luaopen_table(lua_State*);
    int luaopen_io(lua_State*);
    int luaopen_string(lua_State*);
    int luaopen_math(lua_State*);
    int luaopen_debug(lua_State*);
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
    AutoTestManager::AutoTestManager()
    {
    }
    // ARTIST 823C0438. The log sinks are already constructed by the constructor.
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
        for (s32 i = 0; i < 61; ++i)
            maActions[i] = 0.0f;
        LoadScripts();
    }

    // ARTIST @0x823A95B8. While the test harness owns the controller, publish its
    // 61 values into the value half of the port's ActionInfo table; status words are untouched.
    void AutoTestManager::FillKeyBindings(CgsInput::InputIO::ActionInfo* lpActionInfo)
    {
        if (!mbControlPad)
            return;

        const f32* lpSource = maActions;
        CgsInput::InputIO::ActionInfo* lpDestination = lpActionInfo;
        for (u32 luGroup = 0; luGroup < 7; ++luGroup)
        {
            lpDestination[0].mfValue = lpSource[0];
            lpDestination[1].mfValue = lpSource[1];
            lpDestination[2].mfValue = lpSource[2];
            lpDestination[3].mfValue = lpSource[3];
            lpDestination[4].mfValue = lpSource[4];
            lpDestination[5].mfValue = lpSource[5];
            lpDestination[6].mfValue = lpSource[6];
            lpDestination[7].mfValue = lpSource[7];
            lpSource += 8;
            lpDestination += 8;
        }

        for (u32 luTail = 0; luTail < 5; ++luTail)
        {
            lpDestination->mfValue = *lpSource;
            ++lpSource;
            ++lpDestination;
        }
    }

    void* AutoTestManager::LuaAllocator(void*, void* lpPtr, size_t, size_t luNewSize)
    {
        if (luNewSize != 0)
            return realloc(lpPtr, luNewSize);
        free(lpPtr);
        return 0;
    }

    void AutoTestManager::LoadScripts()
    {
        if (CgsSystem::HardwareInit::HasDetectedAutomaticTestingFile())
        {
            char lacFileName[1024];
            char lacLine[1024];
            const char* lpcDirectory = CgsSystem::HardwareInit::GetFOPENDirectory();
            CgsCore::SPrintf(lacFileName, sizeof(lacFileName), "%sTESTINGSCRIPTS%c%s",
                lpcDirectory, lpcDirectory[strlen(lpcDirectory) - 1],
                CgsSystem::HardwareInit::GetAutoScriptToRun());
            const size_t luNameLength = strlen(lacFileName);
            for (size_t i = 0; i < luNameLength; ++i)
                if (lacFileName[i] == '\r' || lacFileName[i] == '\n')
                    lacFileName[i] = 0;

            FILE* lpFile = fopen(lacFileName, "r");
            if (lpFile)
            {
                memset(macScript, 0, sizeof(macScript));
                while (!feof(lpFile))
                {
                    fgets(lacLine, sizeof(lacLine), lpFile);
                    if (!feof(lpFile))
                    {
                        if (strlen(lacLine) + strlen(macScript) >= sizeof(macScript) - 1)
                        {
                            CgsDev::Assert::BeginAssert();
                            CgsDev::Assert::FireAssert("(strlen(lpcSource)+strlen(lpcDest))<luBytes - 1", "..\\..\\..\\GameShared\\GameClasses\\Core/CgsStringUtils.h", 75);
                            CgsDev::Assert::EndAssert();
                        }
                        strcat(macScript, lacLine);
                    }
                }
                fclose(lpFile);
                ++miNumScriptsLoaded;
                mpLuaHandle = lua_newstate(&AutoTestManager::LuaAllocator, 0);
                luaopen_base(mpLuaHandle);
                struct LuaLibrary { const char* mpcName; AutoTestLuaFunction mpFunction; };
                static const LuaLibrary kaLibraries[] = {
                    { "package", luaopen_package }, { "table", luaopen_table },
                    { "io", luaopen_io }, { "string", luaopen_string },
                    { "math", luaopen_math }, { "debug", luaopen_debug }, { 0, 0 }
                };
                for (const LuaLibrary* p = kaLibraries; p->mpFunction; ++p)
                {
                    lua_pushcclosure(mpLuaHandle, p->mpFunction, 0);
                    lua_pushstring(mpLuaHandle, p->mpcName);
                    lua_call(mpLuaHandle, 1, 0);
                }
                if (!mpLuaHandle)
                {
                    CgsDev::Assert::BeginAssert();
                    CgsDev::Assert::FireAssert("mpLuaHandle", "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../Game/BrnAutoTestManager.cpp", 238);
                    CgsDev::Assert::EndAssert();
                }
            }
            struct LuaCallback { const char* mpcName; AutoTestLuaFunction mpFunction; };
            static const LuaCallback kaCallbacks[] = {
                { "CallDebugFunction", &AutoTestManager::LUACBCallDebugFunction },
                { "RecordLog", &AutoTestManager::LUACBRecordLog },
                { "PressButton", &AutoTestManager::LUACBPressButton },
                { "AcquirePad", &AutoTestManager::LUACBAcquirePad },
                { "RelinquishPad", &AutoTestManager::LUACBRelinquishPad },
                { "ActivateDebugComponent", &AutoTestManager::LUACBActivateDebugComponent },
                { "DeactivateDebugComponent", &AutoTestManager::LUACBDeactivateDebugComponent },
                { 0, 0 }
            };
            for (const LuaCallback* p = kaCallbacks; p->mpcName; ++p)
            {
                if (!mpLuaHandle)
                {
                    CgsDev::Assert::BeginAssert();
                    CgsDev::Assert::FireAssert("mpLuaHandle", "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../Game/BrnAutoTestManager.cpp", 245);
                    CgsDev::Assert::EndAssert();
                }
                lua_pushcclosure(mpLuaHandle, p->mpFunction, 0);
                lua_setfield(mpLuaHandle, -10002, p->mpcName);
                lua_settop(mpLuaHandle, 0);
                if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
                    *CgsDev::Log::gpDebugPrint << "Registered function " << p->mpcName << "\n";
            }
        }
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
        return gpBurnoutGame->GuiAcceptsControllerInput() && mnCurrentGuiPage == KN_IN_GAME_GUI_PAGE;
    }

    void AutoTestManager::PerfMonCpuReportCallback(const CgsDev::PerfMonCpuMonitorData& lrData, void* lpUserData)
    {
        CgsDev::StrStreamBase& lrStream = *static_cast<CgsDev::StrStreamBase*>(lpUserData);

        lrStream << "        <cpumonitor";
        lrStream << " name=\"";
        lrStream << ((lrData.mpcName != 0) ? lrData.mpcName : "<NULLSTRING>");
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
        lrStream << ((lrData.mpcName != 0) ? lrData.mpcName : "<NULLSTRING>");
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
        lrStream << ((lrStats.mpcPoolName != 0) ? lrStats.mpcPoolName : "<NULLSTRING>");
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

        const char* lpcFileName = (lpcName != 0) ? lpcName : "gamestate.xml";

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
