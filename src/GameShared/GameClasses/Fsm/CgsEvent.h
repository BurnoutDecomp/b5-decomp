#ifndef CGS_FSM_EVENT_H
#define CGS_FSM_EVENT_H

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsID.h"   // CgsID

// CgsFsm::Event - an event delivered to a CgsFsm::ScriptedFsm. Carries an id (which Lua
// transition to run) plus up to KI_MAX_VARIABLE_COUNT typed variables that are marshalled
// into the Lua state before the transition. Reconstructed from the DecFIGS DWARF (CgsEvent.h);
// the layout (variables[16] @ +0x00, id @ +0x40, count @ +0x48) matches the X360
// ScriptedFsm::SendEvent asm.

namespace CgsFsm
{
    struct Variable;

    struct Event
    {
        static const s32 KI_MAX_VARIABLE_COUNT = 16;

        void  Construct(CgsID lId);
        CgsID GetId() const { return mId; }
        void  AddVariable(const Variable* lpVariable);
        s32   GetVariableCount() const { return miVariableCount; }
        const Variable* GetVariable(s32 liIndex) const { return maVariables[liIndex]; }

    private:
        const Variable* maVariables[KI_MAX_VARIABLE_COUNT];  // +0x00 (16 ptrs = 0x40 bytes)
        CgsID           mId;                                 // +0x40
        s32             miVariableCount;                     // +0x48
    };
}

#endif // CGS_FSM_EVENT_H
