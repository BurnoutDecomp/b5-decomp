#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/FSM/CgsFsm.h"            // CgsFsm::Fsm, CgsFsm::State
#include "GameShared/GameClasses/FSM/CgsScriptedState.h"  // CgsFsm::ScriptedState

// CgsFsm::ScriptedFsm - an FSM whose transitions are driven by a Lua script. It
// extends the plain CgsFsm::Fsm with the embedded Lua interpreter state and a
// sequence counter, and introduces the virtual state-table accessors (GetState /
// GetStateCount) that the concrete FSMs (e.g. CgsGui::StateMachine) override.
//
// LAYOUT (X360 authoritative -- pinned by CgsGui::StateMachine, which derives from
// this and stores its first own member miNumStates at this+0x14, so the base is
// exactly 0x14 = 20 bytes):
//   +0x00  vptr                       (ScriptedFsm introduces the virtuals; the base
//                                       CgsFsm::Fsm has none, so MSVC places a fresh
//                                       vfptr at offset 0 with the Fsm subobject after)
//   +0x04  CgsFsm::Fsm::mpCurrentState (the Fsm base subobject)
//   +0x08  LuaState mLuaState         (DWARF CgsScriptedFsm.h:162; 8 bytes here)
//   +0x10  u32      muSequenceNumber  (DWARF CgsScriptedFsm.h:164)
//
// This is a minimal-complete base: it carries the exact size + vtable shape the
// derived StateMachine needs. The full ScriptedFsm method set (Prepare/Release/the
// Lua variable getters/setters) and the real LuaState internals have their own home
// (CgsScriptedFsm.cpp); mLuaState is modelled here as correctly-sized opaque storage
// so the derived-class member offsets are exact. When the real LuaState lands this
// header should adopt the named type additively (same 8-byte span).
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
        bool IsLuaResourceValid();
        u32  GetSequenceNumber() const { return muSequenceNumber; }

    private:
        // FLAG (foreign type): CgsFsm::LuaState (DWARF CgsLuaState.h). 8-byte opaque
        // span so muSequenceNumber lands at +0x10 and the base ends at +0x14.
        u8  maLuaState[8];   // +0x08
        u32 muSequenceNumber; // +0x10
    };
}
