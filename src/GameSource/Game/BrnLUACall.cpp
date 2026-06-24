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
}
