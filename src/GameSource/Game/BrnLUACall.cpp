// ===================================================================================
// BrnGame::LUACall  -- implementation
//   class:BrnGame::LUACall
//
// Call @ 0x823AA320
//   Reconstructed store-for-store from the X360 pseudocode/asm.
//
// FLAGGED SUBSTITUTION: on a Lua error the X360 fetches the error text via
// lua_tolstring and routes it through CgsDev::Assert::BeginAssert / FireAssert /
// EndAssert. Per the project rule that substitutes that machinery, the failure path is
// reconstructed with the committed CGS_ASSERT macro (the X360 d:\p4 file/line cite at
// BrnLUACall.cpp:60 is supplied by the macro's __FILE__/__LINE__).
// ===================================================================================
#include "GameSource/Game/BrnLUACall.h"

#include "GameShared/GameClasses/Core/CgsAssert.h" // committed CGS_ASSERT (flagged substitute)

namespace BrnGame
{
    // @ 0x823AA320
    void LUACall::Call()
    {
        // v2 = lua_pcall(mpLuaHandle, miNumParams, -1, 0)
        //   nresults = -1 (LUA_MULTRET), errfunc = 0.
        const int liResult = lua_pcall(mpLuaHandle, miNumParams, -1, 0);

        // stb r11(1), 0xC(r31) -- mark that the function was invoked.
        mbCalledFunction = true;

        // if (v2) { ...assert with the Lua error string... }
        CGS_ASSERT(liResult == 0, lua_tolstring(mpLuaHandle, -1, nullptr));

        // stw r11(0), 0(r31) -- reset the pushed-parameter count.
        miNumParams = 0;

        // stw r3, 4(r31) -- cache lua_gettop(mpLuaHandle) as the result count.
        miNumResults = lua_gettop(mpLuaHandle);
    }

    // @ 0x823C0298
    LUACall::EArgType LUACall::GetArgType(s32 liResultIndex) const
    {
        // subf r4, r11, r30 -- ResultIndexToStackIndex(liResultIndex) inlined.
        const s32 liStackIndex = liResultIndex - miNumResults;

        if (lua_type(mpLuaHandle, liStackIndex) == 0)   // LUA_TNIL
            return E_NIL;
        if (lua_isnumber(mpLuaHandle, liStackIndex))
            return E_NUMBER;
        if (lua_isstring(mpLuaHandle, liStackIndex))
            return E_STRING;
        if (lua_type(mpLuaHandle, liStackIndex) == 1)   // LUA_TBOOLEAN
            return E_BOOL;

        // Unconditional fall-through: fire the assert then return E_UNKNOWN.
        CGS_ASSERT(false, "Could not derive arg type");
        return E_UNKNOWN;
    }

    // @ 0x823C5C60
    s32 LUACall::GetInt32(s32 liResultIndex)
    {
        // subf r4, r11, r30 -- ResultIndexToStackIndex(liResultIndex) inlined.
        const s32 liStackIndex = liResultIndex - miNumResults;

        CGS_ASSERT(lua_isnumber(mpLuaHandle, liStackIndex), "IsNumber(liResultIndex)");

        // fctiwz f0, f1 ; stfiwx -- truncate the Lua number to a 32-bit int.
        return static_cast<s32>(lua_tonumber(mpLuaHandle, liStackIndex));
    }

    // @ 0x823C5CF0
    const char* LUACall::GetString(s32 liResultIndex) const
    {
        // subf r4, r11, r30 -- ResultIndexToStackIndex(liResultIndex) inlined.
        const s32 liStackIndex = liResultIndex - miNumResults;

        CGS_ASSERT(lua_isstring(mpLuaHandle, liStackIndex), "IsString(liResultIndex)");

        // li r5, 0 -- length out-param is null.
        return lua_tolstring(mpLuaHandle, liStackIndex, nullptr);
    }
}
