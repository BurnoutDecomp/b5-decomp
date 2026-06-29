#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsID.h"                                 // CgsID (u64) -- mnCurrentGuiPage
#include "GameShared/GameClasses/Development/Log/CgsLogFile.h"                  // CgsDev::Log::LogFile          mAutoTestLogFile
#include "GameShared/GameClasses/Development/Log/CgsLogChannelOutput.h"         // CgsDev::Log::LogChannelOutput mAutoTestLogChannel
#include "GameShared/GameClasses/Development/Log/CgsLogCombined.h"              // CgsDev::Log::LogCombined      mAutoTestLog

// BrnGame::AutoTestManager - the automated-test driver. It owns a Lua-scripted test harness
// (macScript holds one loaded test script, KI_MAX_SCRIPT_SIZE bytes) plus a three-way logging
// fan-out: a raw log file (mAutoTestLogFile), a channelled console output (mAutoTestLogChannel)
// and a combined sink (mAutoTestLog) that AddStream()s the other two so every test line lands in
// both places. The script FSM (meScriptState) presses pad buttons through maActions / mbControlPad.
//
// Reconstructed from the DecFIGS DWARF (GameSource/Game/BrnAutoTestManager.h) gated on the X360
// ledger, with the constructor (this TU) grounded in BURNOUT_X360_ARTIST.XEX @ 0x827DB4C0. The
// ctor default-constructs the three log members in declaration order (the X360 emits each member's
// base StrStreamBase vtable store + value-init then patches in the derived vtable): LogFile sets
// its handle to INVALID_HANDLE_VALUE (-1), LogChannelOutput sets miChannel to -1, and LogCombined
// zeroes its four mapStreams slots. The members past mAutoTestLog are left for Init() (the ctor
// does not touch them in the asm).

// Forward declarations for types referenced only by method DECLARATIONS bodied in other TUs (no
// layout dependency here, so the heavy headers stay out of this constructor TU).
struct lua_State;
namespace BrnGame { class BrnGameModule; }
namespace CgsDev { struct PerfMonCpuMonitorData; struct PerfMonGpuMonitorData; }
namespace CgsResource { struct PoolStats; }
namespace CgsInput { namespace InputIO { struct ActionInfo; } }

namespace BrnGame
{
    // BrnAutoTestManager.h:48 (DWARF) -- the loaded-script buffer size (15360 == 15 KiB).
    const s32 KI_MAX_SCRIPT_SIZE = 15360;

    struct AutoTestManager
    {
        // BrnAutoTestManager.h:93 (DWARF) -- the script FSM states.
        enum EScriptState
        {
            eScriptFinished   = 0,
            eScriptRunning    = 1,
            eScriptLoading    = 2,
            eScriptInitialise = 3,
        };

        // X360 0x827DB4C0. Default-construct the harness: brings up the three log streams (file,
        // channel, combined) and leaves the script/FSM/pad state for Init().
        AutoTestManager();

        // --- public interface (DWARF, bodied in other TUs) ---
        void Init(BrnGame::BrnGameModule* lpGameModule);
        void Update();
        void FillKeyBindings(CgsInput::InputIO::ActionInfo* lpActionInfo);

        // --- LUA C callbacks (DWARF, bodied in other TUs) ---
        s32 LUACBCallDebugFunction(lua_State* lpLua);
        s32 LUACBRecordLog(lua_State* lpLua);
        s32 LUACBPressButton(lua_State* lpLua);
        s32 LUACBAcquirePad(lua_State* lpLua);
        s32 LUACBRelinquishPad(lua_State* lpLua);
        s32 LUACBActivateDebugComponent(lua_State* lpLua);
        s32 LUACBDeactivateDebugComponent(lua_State* lpLua);

    private:
        // --- private helpers (DWARF, bodied in other TUs) ---
        void PerfMonCpuReportCallback(const CgsDev::PerfMonCpuMonitorData& lrData, void* lpUserData);
        void PerfMonGpuReportCallback(const CgsDev::PerfMonGpuMonitorData& lrData, void* lpUserData);
        void PoolReportCallback(const CgsResource::PoolStats& lrStats, void* lpUserData);
        void DumpGameState(const char* lpcName);
        bool IsInGame();
        void GetCurrentGameState();
        void* LuaAllocator(void* lpUserData, void* lpPtr, size_t luOldSize, size_t luNewSize);
        const char* ScriptReader(lua_State* lpLua, void* lpUserData, size_t* lpuSize);
        void LoadScripts();
        void UpdateScriptFSM();
        void ScriptStateRunning();
        void ScriptStateLoading();
        void ScriptStateFinished();
        void ScriptStateInitialise();

    private:
        // Layout from DWARF (BrnAutoTestManager.h). On X360 the log members begin at this+0x3C04
        // (macScript == 0x3C00 bytes, then the 4-byte mpGameModule pointer), which the ctor asm
        // confirms by writing the first log vtable at 0x3C04.
        char                          macScript[KI_MAX_SCRIPT_SIZE];   // [+0x0000] loaded test script
        BrnGame::BrnGameModule*       mpGameModule;                    // [+0x3C00] set by Init()
        CgsDev::Log::LogFile          mAutoTestLogFile;                // [+0x3C04] raw log-file sink
        CgsDev::Log::LogChannelOutput mAutoTestLogChannel;             // [+0x3C10] channelled console sink
        CgsDev::Log::LogCombined      mAutoTestLog;                    // [+0x3C1C] combined fan-out sink
        s32                           miFramesSinceLastGameStateSave;
        CgsID                         mnCurrentGuiPage;
        s32                           miNumScriptsLoaded;
        lua_State*                    mpLuaHandle;
        EScriptState                  meScriptState;
        bool                          mbLoadComplete;
        f32                           maActions[61];
        bool                          mbControlPad;
    };
}
