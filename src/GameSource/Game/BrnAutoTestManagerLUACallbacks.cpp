#include "GameSource/Game/BrnAutoTestManager.h"
#include "GameSource/Game/BrnLUACallbackHelper.h"                    // BrnGame::LUACallbackHelper
#include "GameSource/Game/BrnGameModule.hpp"                         // gpBurnoutGame
#include "GameShared/GameClasses/Core/CgsAssert.h"                   // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"           // gpDebugPrint / gxMessageFilterFlags

// BrnGame::AutoTestManager::LUACBPressButton @ 0x823C6150 (BURNOUT_X360_ARTIST.XEX).
//
// The scripted-test Lua callback "PressButton(actionIndex)". A LUACallbackHelper is
// constructed on the stack over the incoming lua_State (its inlined ctor caches
// miNumParams = lua_gettop(L), miNumResults = 0, mpLUAHandle = L). The callback asserts
// exactly one argument, optionally logs the argument's type (and its string form when it
// is a string), then stamps 1.0f into the game-module's pad-action array at the requested
// action index, and finally clears the Lua stack.
//
// ARTIST823C6264..6278 addresses gpBurnoutGame + (index+0x10A6)*4.
// The actual named destination is mAutoTestManager.maActions, at +0x4298.

// lua_gettop / lua_settop are external Lua C API entry points used only here; declare the
// minimal surface (mirroring BrnLUACall.h). lua_State + the Get* Lua helpers come in via
// BrnLUACallbackHelper.h.
extern "C"
{
    int lua_gettop(lua_State* lpLuaState);              // top-of-stack index (== param count)
    void lua_settop(lua_State* lpLuaState, int liIndex); // set/clear the stack
}

namespace BrnGame
{
    s32 AutoTestManager::LUACBPressButton(lua_State* lpLua)
    {
        // Inlined LUACallbackHelper(lua_State*): miNumParams = lua_gettop(L), miNumResults = 0.
        LUACallbackHelper lHelper(lpLua);

        if (lHelper.GetNumParams() != 1)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("lHelper.GetNumParams() == 1",
                "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../Game/BrnAutoTestManager.cpp", 635);
            CgsDev::Assert::EndAssert();
        }

        if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
        {
            *CgsDev::Log::gpDebugPrint << "ArgType: "
                                       << static_cast<s32>(lHelper.GetParamType(0)) << "\n";
        }

        // lua_isstring(L, 1) is evaluated first, then the message-filter gate.
        if (lua_isstring(lpLua, 1) && (CgsDev::Message::gxMessageFilterFlags & 1) != 0)
        {
            *CgsDev::Log::gpDebugPrint << lHelper.GetStringParam(0) << "\n";
        }

        gpBurnoutGame->mAutoTestManager.maActions[lHelper.GetInt32Param(0)] = 1.0f;

        lua_settop(lpLua, 0);
        return 0;
    }
}
