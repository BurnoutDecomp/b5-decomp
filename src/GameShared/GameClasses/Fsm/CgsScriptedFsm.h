#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsID.h"             // CgsID
#include "GameShared/GameClasses/FSM/CgsFsm.h"             // CgsFsm::Fsm, CgsFsm::State
#include "GameShared/GameClasses/FSM/CgsScriptedState.h"   // CgsFsm::ScriptedState
#include "GameShared/GameClasses/FSM/CgsLuaState.h"        // CgsFsm::LuaState

namespace CgsResource { struct LuaCodeResource; }
namespace CgsMemory   { class  HeapMalloc; }

// CgsFsm::ScriptedFsm - an FSM whose transitions are driven by a Lua script (the "LuaCode"
// resource). It extends CgsFsm::Fsm with an embedded CgsFsm::LuaState (which runs the script)
// and a sequence counter, and introduces the virtual state-table accessors (GetState /
// GetStateCount) that the concrete FSMs (e.g. CgsGui::StateMachine) override to index into
// their fixed state array. Reconstructed from BURNOUT_X360_ARTIST.XEX (CgsFsm::ScriptedFsm::*).
//
// LAYOUT (X360 authoritative -- pinned by CgsGui::StateMachine, which derives from this and
// stores its first own member miNumStates at this+0x14, so the base is exactly 0x14 = 20 bytes):
//   +0x00  vptr                       (ScriptedFsm introduces the virtuals; CgsFsm::Fsm has none)
//   +0x04  CgsFsm::Fsm::mpCurrentState (the Fsm base subobject)
//   +0x08  CgsFsm::LuaState mLuaState  (8 bytes: lua_State* + HeapMalloc*)
//   +0x10  u32              muSequenceNumber
namespace CgsFsm
{
    struct ScriptedFsm : public Fsm
    {
        ScriptedFsm();

        virtual void SendEvent(const State* lpEvent);
        // The script-FSM state table. Derived FSMs that own a fixed state array
        // (CgsGui::StateMachine) override these to index into it.
        virtual ScriptedState* GetState(s32 liIndex);
        virtual s32 GetStateCount();

        void Construct();
        void Destruct();

        // @ 0x82836668 -- bring the Lua script up (mLuaState.Prepare) then enter the initial
        // state: the explicit lInitialStateId if non-zero, else the script's GetCurrentState().
        bool Prepare(CgsResource::LuaCodeResource* lpLuaCodeResource,
                     CgsMemory::HeapMalloc* lpHeapMalloc, CgsID lInitialStateId);
        // @ 0x828366D0 -- lua_close the script + clear.
        bool Release();
        // @ 0x82835E90 -- transition to the state whose id matches: scan the table (GetState/
        // GetStateCount), run the old state's OnLeave + the new state's OnEnter, bump the sequence.
        void SetState(CgsID lStateId);
        // @ 0x82836BA8 -- the script's index for the current state (GetInt "GetCurrentStateIndex").
        s32  GetCurrentStateIndex();

        bool IsLuaResourceValid() { return mLuaState.IsLuaResourceValid(); }
        u32  GetSequenceNumber() const { return muSequenceNumber; }

    protected:
        CgsFsm::LuaState mLuaState;        // +0x08
        u32              muSequenceNumber; // +0x10
    };
}
