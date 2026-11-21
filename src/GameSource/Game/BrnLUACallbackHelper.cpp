#include "GameSource/Game/BrnLUACallbackHelper.h"

#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"   // CGS_ASSERT

extern "C" int lua_gettop(lua_State* lpLuaState);

// BrnGame::LUACallbackHelper -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//   GetParamType    @ 0x823C0370
//   GetInt32Param   @ 0x823C5D70
//   GetStringParam  @ 0x823C5DF8
// ParamIndexToStackIndex is inlined at every call site (stack index == param index + 1).

static const char KAC_ARTIST_ASSERT_FILE[] =
    "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\game\\BrnLUACallbackHelper.h";

namespace BrnGame
{
    // Inlined at ARTIST 0x823C6160..0x823C6174 in LUACBPressButton.
    LUACallbackHelper::LUACallbackHelper(lua_State* lpLUAHandle)
    {
        mpLUAHandle = lpLUAHandle;
        miNumParams = lua_gettop(lpLUAHandle);
        miNumResults = 0;
    }

    s32 LUACallbackHelper::GetNumParams() const
    {
        return miNumParams;
    }

    // @ 0x823C0370
    LUACallbackHelper::EArgType LUACallbackHelper::GetParamType(s32 liParamIndex) const
    {
        // ParamIndexToStackIndex is inlined: stack index == param index + 1.
        const s32 liStackIndex = liParamIndex + 1;

        // lua_type == 0 (LUA_TNIL) -> the argument was not supplied.
        if (!lua_type(mpLUAHandle, liStackIndex))
        {
            return E_NIL;
        }
        if (lua_isnumber(mpLUAHandle, liStackIndex))
        {
            return E_NUMBER;
        }
        if (lua_isstring(mpLUAHandle, liStackIndex))
        {
            return E_STRING;
        }
        // lua_type == 1 (LUA_TBOOLEAN).
        if (lua_type(mpLUAHandle, liStackIndex) == 1)
        {
            return E_BOOL;
        }

        CGS_ASSERT(false,
            CgsDev::Assert::MessageAt(
                "Could not derive arg type",
                "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\game\\BrnLUACallbackHelper.h", 197 /* ARTIST 0x823C0404 */));
        return E_UNKNOWN;
    }

    // @ 0x823C5D70
    s32 LUACallbackHelper::GetInt32Param(s32 liParamIndex)
    {
        // ParamIndexToStackIndex is inlined: stack index == param index + 1.
        const s32 liStackIndex = liParamIndex + 1;

        // if (!lua_isnumber(mpLUAHandle, liStackIndex)) assert.
        CGS_ASSERT(lua_isnumber(mpLUAHandle, liStackIndex) != 0,
            CgsDev::Assert::MessageAt(
                "IsParamNumber(liParamIndex)",
                KAC_ARTIST_ASSERT_FILE, 148 /* ARTIST 0x823C5DA8 */));

        // lua_tonumber returns a double; fctiwz truncates it to a signed 32-bit int.
        return static_cast<s32>(lua_tonumber(mpLUAHandle, liStackIndex));
    }

    // @ 0x823C5DF8
    const char* LUACallbackHelper::GetStringParam(s32 liParamIndex) const
    {
        // ParamIndexToStackIndex is inlined: stack index == param index + 1.
        const s32 liStackIndex = liParamIndex + 1;

        // if (!lua_isstring(mpLUAHandle, liStackIndex)) assert.
        CGS_ASSERT(lua_isstring(mpLUAHandle, liStackIndex) != 0,
            CgsDev::Assert::MessageAt(
                "IsParamString(liParamIndex)",
                KAC_ARTIST_ASSERT_FILE, 155 /* ARTIST 0x823C5E30 */));

        // lua_tolstring(L, idx, len) with a null length pointer (li r5,0).
        return lua_tolstring(mpLUAHandle, liStackIndex, 0);
    }
}
