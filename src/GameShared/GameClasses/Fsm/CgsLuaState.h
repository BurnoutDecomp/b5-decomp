#ifndef CGS_FSM_LUASTATE_H
#define CGS_FSM_LUASTATE_H

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsID.h"   // CgsID (u64 packed identity)

// CgsFsm::LuaState -- reconstructed from BURNOUT_X360_ARTIST.XEX (CgsFsm::LuaState::*)
// + DWARF CgsLuaState.{h,cpp}.
//
// Wraps a Lua 5.1 lua_State that runs an FSM's "LuaCode" script. Each FSM script defines
// the state machine as Lua globals -- GetCurrentState(), NextState(eventId),
// GetInt/GetString(fn,id), SetInt/SetBool/SetString(fn,id,value) -- which these C++
// wrappers invoke via lua_getfield(L, LUA_GLOBALSINDEX, name) + lua_pcall. CgsFsm::ScriptedFsm
// owns one of these (mLuaState @ +0x08) and drives transitions through it.
//
// LAYOUT (X360-authoritative, 8 bytes -- matches the ScriptedFsm mLuaState span):
//   +0x00  lua_State*             mpLuaState
//   +0x04  CgsMemory::HeapMalloc* mpHeapMalloc   (the allocator passed to lua_newstate)

struct lua_State;   // opaque here; the real type comes from <lua.h> in the .cpp

namespace CgsMemory { class HeapMalloc; }
namespace CgsResource { struct LuaCodeResource; }

namespace CgsFsm
{
    class LuaState
    {
    public:
        // @ 0x82835D50 -- zero the handle + allocator.
        void Construct();
        // @ 0x82836000 -- create the lua_State (LuaAllocator over lpHeapMalloc), load the
        // script chunk via luaL_loadbuffer, and run it (lua_pcall) to define its globals.
        bool Prepare(CgsResource::LuaCodeResource* lpLuaCodeResource, CgsMemory::HeapMalloc* lpHeapMalloc);
        // @ DWARF CgsLuaState.cpp:103 -- lua_close + clear (ICF-folded in ARTIST).
        void Release();
        // @ 0x82835E78 -- true while the lua_State exists.
        bool IsLuaResourceValid() const;

        // Query/transition the script (each calls the matching Lua global).
        CgsID GetCurrentState();                                            // @ 0x82836514
        void  NextState(CgsID lEventId);                                    // calls Lua NextState(eventId)
        int   GetInt(const char* lpcFunctionName, CgsID lVariableId);       // @ 0x828360EC
        void  GetString(const char* lpcFunctionName, CgsID lVariableId, char* lpcOut, int liOutSize); // @ 0x82836234
        void  SetInt(const char* lpcFunctionName, CgsID lVariableId, s32 liValue);
        void  SetBool(const char* lpcFunctionName, CgsID lVariableId, bool lbValue);
        void  SetString(const char* lpcFunctionName, CgsID lVariableId, const char* lpcValue);

    private:
        // @ 0x82835DD0 -- uncompress a CgsID to its printable form (trim trailing pad) and
        // push it as a Lua string argument.
        void LuaPushId(CgsID lId);
        // @ 0x82835D60 -- the lua_Alloc: free when newsize==0, else realloc, through HeapMalloc.
        static void* LuaAllocator(void* lpData, void* lpPointer, usize luOldSize, usize luNewSize);

        lua_State*             mpLuaState;     // +0x00
        CgsMemory::HeapMalloc* mpHeapMalloc;   // +0x04
    };
}

#endif // CGS_FSM_LUASTATE_H
