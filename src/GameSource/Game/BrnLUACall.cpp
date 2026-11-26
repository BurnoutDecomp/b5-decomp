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
#define CGS_ARTIST_FILE "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\game\\BrnLUACall.h"
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
                CGS_ASSERT_LOCATION("d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../Game/BrnLUACall.cpp", 60));
            CgsDev::Assert::EndAssert();
        }

        // stw r11(0), 0(r31) -- reset the pushed-parameter count.
        miNumParams = 0;

        // stw r3, 4(r31) -- cache lua_gettop(mpLuaHandle) as the result count.
        miNumResults = lua_gettop(mpLuaHandle);
    }

    // DWARF BrnLUACall.h:71/:74/:90 (and the IsNil/IsBool pair beside them). ARTIST has no
    // standalone copy of any of them: every accessor below inlines them, recomputing the stack
    // index from miNumResults after each Lua call (e.g. GetArgType 0x823C02D4/0x823C02F4).
    s32 LUACall::ResultIndexToStackIndex(s32 liResultIndex) const
    {
        return liResultIndex - miNumResults;
    }

    bool LUACall::IsNil(s32 liResultIndex) const
    {
        return lua_type(mpLuaHandle, ResultIndexToStackIndex(liResultIndex)) == 0;   // LUA_TNIL
    }

    bool LUACall::IsNumber(s32 liResultIndex) const
    {
        return lua_isnumber(mpLuaHandle, ResultIndexToStackIndex(liResultIndex)) != 0;
    }

    bool LUACall::IsString(s32 liResultIndex) const
    {
        return lua_isstring(mpLuaHandle, ResultIndexToStackIndex(liResultIndex)) != 0;
    }

    bool LUACall::IsBool(s32 liResultIndex) const
    {
        return lua_type(mpLuaHandle, ResultIndexToStackIndex(liResultIndex)) == 1;   // LUA_TBOOLEAN
    }

    // @ 0x823C0298
    LUACall::EArgType LUACall::GetArgType(s32 liResultIndex) const
    {
        if (IsNil(liResultIndex))
            return E_NIL;
        if (IsNumber(liResultIndex))
            return E_NUMBER;
        if (IsString(liResultIndex))
            return E_STRING;
        if (IsBool(liResultIndex))
            return E_BOOL;

        // Unconditional fall-through: fire the assert then return E_UNKNOWN.
        CGS_ASSERT(false, CGS_ASSERT_MESSAGE_AT("Could not derive arg type", CGS_ARTIST_FILE, 198));
        return E_UNKNOWN;
    }

    // @ 0x823C5C60
    s32 LUACall::GetInt32(s32 liResultIndex)
    {
        CGS_ASSERT(IsNumber(liResultIndex), CGS_ASSERT_MESSAGE_AT("IsNumber(liResultIndex)", CGS_ARTIST_FILE, 149));

        // fctiwz f0, f1 ; stfiwx -- truncate the Lua number to a 32-bit int.
        return static_cast<s32>(lua_tonumber(mpLuaHandle, ResultIndexToStackIndex(liResultIndex)));
    }

    // @ 0x823C5CF0
    const char* LUACall::GetString(s32 liResultIndex) const
    {
        CGS_ASSERT(IsString(liResultIndex), CGS_ASSERT_MESSAGE_AT("IsString(liResultIndex)", CGS_ARTIST_FILE, 156));

        // li r5, 0 -- length out-param is null.
        return lua_tolstring(mpLuaHandle, ResultIndexToStackIndex(liResultIndex), 0);
    }
}

#undef CGS_ARTIST_FILE
