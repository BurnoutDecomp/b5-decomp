#pragma once

#include "BrnCommonTypes.h"   // CgsID
#include "GameShared/GameClasses/FSM/CgsState.h"

// CgsFsm::ScriptedState - a State that is part of a script-driven FSM. Adds the
// state id and a back-pointer to its owning ScriptedFsm, plus the Construct hook
// the fsm calls to wire the state up. Layout/method set from the DecFIGS DWARF
// (CgsScriptedState.h).
namespace CgsFsm
{
    struct ScriptedFsm;   // defined as `struct` in CgsScriptedFsm.h -- tag must match (MSVC mangles
                          // `class T*` and `struct T*` differently -> a class/struct mismatch makes
                          // ScriptedState/State::Construct callers fail to link against the definition).

    struct ScriptedState : public State
    {
        ScriptedState();

        virtual void Construct(CgsID liId, ScriptedFsm* lpFsm);

        CgsID GetId() const { return mId; }

    protected:
        CgsID        mId;
        ScriptedFsm* mpFsm;
    };
}
