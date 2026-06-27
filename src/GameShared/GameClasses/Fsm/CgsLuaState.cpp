#include "GameShared/GameClasses/Fsm/CgsLuaState.h"

#include "GameShared/GameClasses/Fsm/Resources/CgsLuaCodeResource.h"  // CgsResource::LuaCodeResource
#include "GameShared/GameClasses/Memory/CgsHeapMalloc.h"              // CgsMemory::HeapMalloc
#include "GameShared/GameClasses/Core/CgsID.h"                        // CgsIDCompress / CgsIDUnCompress
#include "GameShared/GameClasses/Core/CgsAssert.h"                    // CGS_ASSERT (flagged assert substitute)

#include <cstring>   // strncpy

extern "C" {
#include "lua.h"
#include "lauxlib.h"
}

// ===========================================================================
//  CgsFsm::LuaState -- reconstructed from BURNOUT_X360_ARTIST.XEX (CgsFsm::LuaState::*).
//  Bodies are store-for-store from the asm; the X360's gpcMessageBuffer assert machinery
//  is replaced by the committed CGS_ASSERT per the project rule (the d:\p4 file/line cite
//  comes from the macro). NextState / Set* are ICF-folded in ARTIST -- reconstructed from
//  the uniform "call a Lua global" pattern the other methods establish + the DWARF
//  signatures (CgsLuaState.cpp).
// ===========================================================================

namespace CgsFsm
{

// @ 0x82835D50
void LuaState::Construct()
{
    mpLuaState   = nullptr;
    mpHeapMalloc = nullptr;
}

// @ 0x82835D60 -- the Lua allocator. lua_Alloc contract: nsize==0 frees, else (re)allocates.
void* LuaState::LuaAllocator(void* lpData, void* lpPointer, usize /*luOldSize*/, usize luNewSize)
{
    CGS_ASSERT(lpData != nullptr, "lpData != NULL");
    CgsMemory::HeapMalloc* lpHeap = static_cast<CgsMemory::HeapMalloc*>(lpData);
    if (luNewSize == 0)
    {
        lpHeap->Free(lpPointer);
        return nullptr;
    }
    return lpHeap->ReAlloc(lpPointer, static_cast<s32>(luNewSize));
}

// @ 0x82836000
bool LuaState::Prepare(CgsResource::LuaCodeResource* lpLuaCodeResource, CgsMemory::HeapMalloc* lpHeapMalloc)
{
    mpHeapMalloc = lpHeapMalloc;
    mpLuaState   = lua_newstate(reinterpret_cast<lua_Alloc>(&LuaState::LuaAllocator), lpHeapMalloc);
    CGS_ASSERT(mpLuaState != nullptr, "Failed to allocate LUA state, out of memory");

    // Load the script chunk and run it (defining the FSM's Lua globals).
    luaL_loadbuffer(mpLuaState, lpLuaCodeResource->GetSource(),
                    lpLuaCodeResource->GetSourceSize(), "LuaCode");
    lua_pcall(mpLuaState, 0, LUA_MULTRET, 0);
    return true;
}

// @ DWARF CgsLuaState.cpp:103 (folded in ARTIST).
void LuaState::Release()
{
    if (mpLuaState != nullptr)
    {
        lua_close(mpLuaState);
        mpLuaState = nullptr;
    }
}

// @ 0x82835E78
bool LuaState::IsLuaResourceValid() const
{
    return mpLuaState != nullptr;
}

// @ 0x82835DD0 -- uncompress the CgsID to its printable form, drop the space padding, push it.
void LuaState::LuaPushId(CgsID lId)
{
    CGS_ASSERT(mpLuaState != nullptr, "mpLuaState");
    char lacId[KI_CGSID_STRING_LEN];
    CgsIDUnCompress(lId, lacId);
    for (int li = 0; li < KI_CGSID_STRING_LEN; ++li)
    {
        if (lacId[li] == ' ') { lacId[li] = 0; break; }
    }
    lua_pushstring(mpLuaState, lacId);
}

// @ 0x82836514 -- call the Lua global GetCurrentState() and compress its returned name.
CgsID LuaState::GetCurrentState()
{
    CGS_ASSERT(mpLuaState != nullptr, "mpLuaState");
    CgsID lResult = 0;
    lua_getfield(mpLuaState, LUA_GLOBALSINDEX, "GetCurrentState");
    lua_pcall(mpLuaState, 0, 1, 0);
    if (lua_isstring(mpLuaState, -1))
        lResult = CgsIDCompress(lua_tolstring(mpLuaState, -1, nullptr));
    else
        CGS_ASSERT(false, "Could not get the current state from Lua");
    lua_settop(mpLuaState, 0);
    return lResult;
}

// Call the Lua global NextState(eventId) to advance the machine.
void LuaState::NextState(CgsID lEventId)
{
    CGS_ASSERT(mpLuaState != nullptr, "mpLuaState");
    lua_getfield(mpLuaState, LUA_GLOBALSINDEX, "NextState");
    LuaPushId(lEventId);
    lua_pcall(mpLuaState, 1, 0, 0);
    lua_settop(mpLuaState, 0);
}

// @ 0x828360EC -- call <fn>(id) and read an integer result.
int LuaState::GetInt(const char* lpcFunctionName, CgsID lVariableId)
{
    CGS_ASSERT(mpLuaState != nullptr, "mpLuaState");
    int liResult = 0;
    lua_getfield(mpLuaState, LUA_GLOBALSINDEX, lpcFunctionName);
    LuaPushId(lVariableId);
    lua_pcall(mpLuaState, 1, 1, 0);
    if (lua_isnumber(mpLuaState, -1))
        liResult = static_cast<int>(lua_tonumber(mpLuaState, -1));
    else
        CGS_ASSERT(false, "Could not find int variable in Lua");
    lua_settop(mpLuaState, 0);
    return liResult;
}

// @ 0x82836234 -- call <fn>(id) and copy the string result into the caller's buffer.
void LuaState::GetString(const char* lpcFunctionName, CgsID lVariableId, char* lpcOut, int liOutSize)
{
    CGS_ASSERT(mpLuaState != nullptr, "mpLuaState");
    lua_getfield(mpLuaState, LUA_GLOBALSINDEX, lpcFunctionName);
    LuaPushId(lVariableId);
    lua_pcall(mpLuaState, 1, 1, 0);
    if (lua_isstring(mpLuaState, -1))
    {
        usize luLen = 0;
        const char* lpcStr = lua_tolstring(mpLuaState, -1, &luLen);
        CGS_ASSERT(static_cast<int>(luLen) < liOutSize, "Lua string variable too long for buffer");
        if (lpcOut != nullptr && liOutSize > 0)
        {
            std::strncpy(lpcOut, lpcStr, static_cast<usize>(liOutSize) - 1);
            lpcOut[liOutSize - 1] = 0;
        }
    }
    else
    {
        CGS_ASSERT(false, "Could not find string variable in Lua");
    }
    lua_settop(mpLuaState, 0);
}

// Call <fn>(id, value) -- the value setters (folded in ARTIST; uniform pattern).
void LuaState::SetInt(const char* lpcFunctionName, CgsID lVariableId, s32 liValue)
{
    CGS_ASSERT(mpLuaState != nullptr, "mpLuaState");
    lua_getfield(mpLuaState, LUA_GLOBALSINDEX, lpcFunctionName);
    LuaPushId(lVariableId);
    lua_pushinteger(mpLuaState, liValue);
    lua_pcall(mpLuaState, 2, 0, 0);
    lua_settop(mpLuaState, 0);
}

void LuaState::SetBool(const char* lpcFunctionName, CgsID lVariableId, bool lbValue)
{
    CGS_ASSERT(mpLuaState != nullptr, "mpLuaState");
    lua_getfield(mpLuaState, LUA_GLOBALSINDEX, lpcFunctionName);
    LuaPushId(lVariableId);
    lua_pushboolean(mpLuaState, lbValue ? 1 : 0);
    lua_pcall(mpLuaState, 2, 0, 0);
    lua_settop(mpLuaState, 0);
}

void LuaState::SetString(const char* lpcFunctionName, CgsID lVariableId, const char* lpcValue)
{
    CGS_ASSERT(mpLuaState != nullptr, "mpLuaState");
    lua_getfield(mpLuaState, LUA_GLOBALSINDEX, lpcFunctionName);
    LuaPushId(lVariableId);
    lua_pushstring(mpLuaState, lpcValue);
    lua_pcall(mpLuaState, 2, 0, 0);
    lua_settop(mpLuaState, 0);
}

} // namespace CgsFsm
