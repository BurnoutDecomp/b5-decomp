#include "GameShared/GameClasses/Fsm/CgsLuaState.h"

#include "GameShared/GameClasses/Fsm/Resources/CgsLuaCodeResource.h"  // CgsResource::LuaCodeResource
#include "GameShared/GameClasses/Memory/CgsHeapMalloc.h"              // CgsMemory::HeapMalloc
#include "GameShared/GameClasses/Core/CgsID.h"                        // CgsIDCompress / CgsIDUnCompress
#include "GameShared/GameClasses/Core/CgsAssertStreamMessageAt.h"                    // CGS_ASSERT (flagged assert substitute)

#include "GameShared/GameClasses/Core/CgsStringUtils.h"

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

static const char KAC_ARTIST_ASSERT_FILE[] =
    "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\fsm\\CgsLuaState.cpp";

namespace CgsFsm
{

const char* LuaState::KPC_NEXT_STATE_FUNCTION_NAME = "NextState";
const char* LuaState::KPC_GET_STATE_FUNCTION_NAME = "GetCurrentState";

// @ 0x82835D50
void LuaState::Construct()
{
    mpHeapMalloc = 0;
    mpLuaState   = 0;
}

// @ 0x82835D60 -- the Lua allocator. lua_Alloc contract: nsize==0 frees, else (re)allocates.
void* LuaState::LuaAllocator(void* lpData, void* lpPointer, usize /*luOldSize*/, usize luNewSize)
{
    CGS_ASSERT_AT(lpData != 0,
        "lpData != NULL", KAC_ARTIST_ASSERT_FILE, 412 /* ARTIST 0x82835D88 */);
    CgsMemory::HeapMalloc* lpHeap = static_cast<CgsMemory::HeapMalloc*>(lpData);
    if (luNewSize == 0)
    {
        lpHeap->Free(lpPointer);
        return 0;
    }
    return lpHeap->ReAlloc(lpPointer, static_cast<s32>(luNewSize));
}

// @ 0x82836000
bool LuaState::Prepare(CgsResource::LuaCodeResource* lpLuaCodeResource, CgsMemory::HeapMalloc* lpHeapMalloc)
{
    mpHeapMalloc = lpHeapMalloc;
    mpLuaState   = lua_newstate(reinterpret_cast<lua_Alloc>(&LuaState::LuaAllocator), lpHeapMalloc);
    CGS_ASSERT_STREAM_AT(mpLuaState != 0, "Failed to allocate LUA state, out of memory", KAC_ARTIST_ASSERT_FILE, 85);

    // Load the script chunk and run it (defining the FSM's Lua globals).
    luaL_loadbuffer(mpLuaState, lpLuaCodeResource->GetSource(),
                    lpLuaCodeResource->GetSourceSize(), "LuaCode");
    lua_pcall(mpLuaState, 0, LUA_MULTRET, 0);
    return true;
}

// @ DWARF CgsLuaState.cpp:103 (folded in ARTIST).
bool LuaState::Release()
{
    CGS_ASSERT_AT(mpLuaState != 0, "mpLuaState", KAC_ARTIST_ASSERT_FILE, 105);
    lua_close(mpLuaState);
    mpLuaState = 0;
    return true;
}

// @ 0x82835E78
bool LuaState::IsLuaResourceValid() const
{
    return mpLuaState != 0;
}

// @ 0x82835DD0 -- uncompress the CgsID to its printable form, drop the space padding, push it.
void LuaState::LuaPushId(CgsID lId)
{
    CGS_ASSERT_AT(mpLuaState != 0,
        "mpLuaState", KAC_ARTIST_ASSERT_FILE, 438 /* ARTIST 0x82835E00 */);
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
    CGS_ASSERT_AT(mpLuaState != 0, "mpLuaState", KAC_ARTIST_ASSERT_FILE, 369);
    CgsID lResult = CgsIDCompress("UNKNOWN");
    lua_getfield(mpLuaState, LUA_GLOBALSINDEX, KPC_GET_STATE_FUNCTION_NAME);
    lua_pcall(mpLuaState, 0, 1, 0);
    if (lua_isstring(mpLuaState, -1))
        lResult = CgsIDCompress(lua_tolstring(mpLuaState, -1, 0));
    else
        CGS_FAIL_STREAM_AT("Could not find state ID: " << lResult << " in Lua",
                             KAC_ARTIST_ASSERT_FILE, 386);
    lua_settop(mpLuaState, 0);
    return lResult;
}

// Call the Lua global NextState(eventId) to advance the machine.
void LuaState::NextState(CgsID lEventId)
{
    CGS_ASSERT_AT(mpLuaState != 0, "mpLuaState", KAC_ARTIST_ASSERT_FILE, 341);
    lua_getfield(mpLuaState, LUA_GLOBALSINDEX, KPC_NEXT_STATE_FUNCTION_NAME);
    LuaPushId(lEventId);
    lua_pcall(mpLuaState, 1, 0, 0);
    lua_settop(mpLuaState, 0);
    lua_gc(mpLuaState, LUA_GCCOLLECT, 0);
}

// @ 0x828360EC -- call <fn>(id) and read an integer result.
int LuaState::GetInt(const char* lpcFunctionName, CgsID lVariableId)
{
    CGS_ASSERT_AT(mpLuaState != 0, "mpLuaState", KAC_ARTIST_ASSERT_FILE, 169);
    int liResult = 0;
    lua_getfield(mpLuaState, LUA_GLOBALSINDEX, lpcFunctionName);
    LuaPushId(lVariableId);
    lua_pcall(mpLuaState, 1, 1, 0);
    if (lua_isnumber(mpLuaState, -1))
        liResult = static_cast<int>(lua_tonumber(mpLuaState, -1));
    else
        CGS_FAIL_STREAM_AT("Could not find int variable in Lua: " << lVariableId,
                             KAC_ARTIST_ASSERT_FILE, 187);
    lua_settop(mpLuaState, 0);
    return liResult;
}

// @ 0x82836234 -- call <fn>(id) and copy the string result into the caller's buffer.
const char* LuaState::GetString(const char* lpcFunctionName, CgsID lVariableId, char* lpcOut, int liOutSize)
{
    CGS_ASSERT_AT(mpLuaState != 0, "mpLuaState", KAC_ARTIST_ASSERT_FILE, 212);
    lpcOut[0] = '\0';
    lua_getfield(mpLuaState, LUA_GLOBALSINDEX, lpcFunctionName);
    LuaPushId(lVariableId);
    lua_pcall(mpLuaState, 1, 1, 0);
    if (lua_isstring(mpLuaState, -1))
    {
        const char* lpcString = lua_tostring(mpLuaState, -1);
        CgsCore::StrnCpy(lpcOut, lpcString, static_cast<u32>(liOutSize - 1));
        lpcOut[liOutSize - 1] = '\0';
    }
    else
    {
        CGS_FAIL_STREAM_AT("Could not find string variable in Lua: " << lVariableId,
                             KAC_ARTIST_ASSERT_FILE, 232);
    }
    lua_settop(mpLuaState, 0);
    return lpcOut;
}

// Call <fn>(id, value) -- the value setters (folded in ARTIST; uniform pattern).
void LuaState::SetInt(const char* lpcFunctionName, CgsID lVariableId, s32 liValue)
{
    CGS_ASSERT(mpLuaState != 0, "mpLuaState");
    lua_getfield(mpLuaState, LUA_GLOBALSINDEX, lpcFunctionName);
    LuaPushId(lVariableId);
    lua_pushinteger(mpLuaState, liValue);
    lua_pcall(mpLuaState, 2, 0, 0);
    lua_settop(mpLuaState, 0);
}

void LuaState::SetBool(const char* lpcFunctionName, CgsID lVariableId, bool lbValue)
{
    CGS_ASSERT(mpLuaState != 0, "mpLuaState");
    lua_getfield(mpLuaState, LUA_GLOBALSINDEX, lpcFunctionName);
    LuaPushId(lVariableId);
    lua_pushboolean(mpLuaState, lbValue ? 1 : 0);
    lua_pcall(mpLuaState, 2, 0, 0);
    lua_settop(mpLuaState, 0);
}

void LuaState::SetString(const char* lpcFunctionName, CgsID lVariableId, const char* lpcValue)
{
    CGS_ASSERT(mpLuaState != 0, "mpLuaState");
    lua_getfield(mpLuaState, LUA_GLOBALSINDEX, lpcFunctionName);
    LuaPushId(lVariableId);
    lua_pushstring(mpLuaState, lpcValue);
    lua_pcall(mpLuaState, 2, 0, 0);
    lua_settop(mpLuaState, 0);
}

} // namespace CgsFsm
