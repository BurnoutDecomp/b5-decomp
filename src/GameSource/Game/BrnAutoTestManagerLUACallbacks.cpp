#include "GameSource/Game/BrnAutoTestManager.h"
#include "GameSource/Game/BrnLUACallbackHelper.h"                    // BrnGame::LUACallbackHelper
#include "GameSource/Game/BrnGameModule.hpp"                         // BrnGame::GetMainGameModule
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
// The pad-action store is addressed off the X360 game-module singleton
// (off_830102D0 == BrnGame::GetMainGameModule()): stfsx f0, (GetInt32Param(0)+0x10A6)*4,
// module -- i.e. byte offset 0x429C8 + GetInt32Param(0)*4. That f32 action array is not
// mapped in the incremental BrnGameModule layout, so -- mirroring the Option-B absolute-
// offset access in BrnGameMainFlowInGameState.cpp -- the write is expressed through the
// module base + the X360-attested byte offset. Intent: set one pad action to "pressed".

// lua_gettop / lua_settop are external Lua C API entry points used only here; declare the
// minimal surface (mirroring BrnLUACall.h). lua_State + the Get* Lua helpers come in via
// BrnLUACallbackHelper.h.
extern "C"
{
    int lua_gettop(lua_State* lpLuaState);              // top-of-stack index (== param count)
    int lua_settop(lua_State* lpLuaState, int liIndex); // set/clear the stack
}

namespace BrnGame
{
    // X360-attested byte offset of the pad-action f32 array within the BrnGameModule
    // singleton: stfsx uses (GetInt32Param(0) + 0x10A6) * 4 == 0x429C8 + index*4.
    static const u32 KU_PAD_ACTION_ARRAY_BYTE_OFFSET = 0x429C8;

    s32 AutoTestManager::LUACBPressButton(lua_State* lpLua)
    {
        // Inlined LUACallbackHelper(lua_State*): miNumParams = lua_gettop(L), miNumResults = 0.
        LUACallbackHelper lHelper(lpLua);

        CGS_ASSERT(lHelper.GetNumParams() == 1, "lHelper.GetNumParams() == 1");

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

        // ((f32*)module + byteOffset)[GetInt32Param(0)] = 1.0f -- mark the pad action pressed.
        u8* lpModuleBase = reinterpret_cast<u8*>(BrnGame::GetMainGameModule());
        f32* lpActions   = reinterpret_cast<f32*>(lpModuleBase + KU_PAD_ACTION_ARRAY_BYTE_OFFSET);
        lpActions[lHelper.GetInt32Param(0)] = 1.0f;

        lua_settop(lpLua, 0);
        return 0;
    }
}
