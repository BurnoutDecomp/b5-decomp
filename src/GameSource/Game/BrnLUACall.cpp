// ===================================================================================
// BrnGame::LUACall  -- implementation
//   class:BrnGame::LUACall
//
// Call @ 0x823AA320
//   Reconstructed store-for-store from the X360 pseudocode/asm.
//
// On a Lua error the X360 fetches the error text via lua_tolstring and routes it through
// CgsDev::Assert::BeginAssert / FireAssert / EndAssert with the d:\p4 file/line cite
// BrnLUACall.cpp:60, written out below.
// ===================================================================================
#include "GameSource/Game/BrnLUACall.h"

#include "GameShared/GameClasses/Core/CgsAssert.h" // CgsDev::Assert::BeginAssert / FireAssert / EndAssert
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"

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
        // The CGS_ASSERT expansion with ARTIST's own file and line (BrnLUACall.cpp:60, unity path,
        // `li r5, 0x3C` @0x823AA37C): the file is loaded before the message call, as for any
        // __FILE__ argument, so it is passed directly rather than through MessageAt.
        if (liResult != 0)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(lua_tolstring(mpLuaHandle, -1, 0),
                "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../Game/BrnLUACall.cpp", 60);
            CgsDev::Assert::EndAssert();
        }

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
        CGS_ASSERT(false, CgsDev::Assert::MessageAt("Could not derive arg type", "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\game\\BrnLUACall.h", 198));
        return E_UNKNOWN;
    }

    // @ 0x823C5C60
    s32 LUACall::GetInt32(s32 liResultIndex)
    {
        // subf r4, r11, r30 -- ResultIndexToStackIndex(liResultIndex) inlined.
        const s32 liStackIndex = liResultIndex - miNumResults;

        CGS_ASSERT(lua_isnumber(mpLuaHandle, liStackIndex), CgsDev::Assert::MessageAt("IsNumber(liResultIndex)", "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\game\\BrnLUACall.h", 149));

        // fctiwz f0, f1 ; stfiwx -- truncate the Lua number to a 32-bit int.
        return static_cast<s32>(lua_tonumber(mpLuaHandle, liStackIndex));
    }

    // @ 0x823C5CF0
    const char* LUACall::GetString(s32 liResultIndex) const
    {
        // subf r4, r11, r30 -- ResultIndexToStackIndex(liResultIndex) inlined.
        const s32 liStackIndex = liResultIndex - miNumResults;

        CGS_ASSERT(lua_isstring(mpLuaHandle, liStackIndex), CgsDev::Assert::MessageAt("IsString(liResultIndex)", "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\game\\BrnLUACall.h", 156));

        // li r5, 0 -- length out-param is null.
        return lua_tolstring(mpLuaHandle, liStackIndex, 0);
    }
}
