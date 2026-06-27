#include "GameShared/GameClasses/Fsm/CgsScriptedFsm.h"

#include "GameShared/GameClasses/Fsm/Resources/CgsLuaCodeResource.h"  // CgsResource::LuaCodeResource
#include "GameShared/GameClasses/Memory/CgsHeapMalloc.h"              // CgsMemory::HeapMalloc
#include "GameShared/GameClasses/Core/CgsID.h"                        // CgsIDCompress / CgsIDUnCompress
#include "GameShared/GameClasses/Core/CgsAssert.h"                    // CGS_ASSERT (flagged assert substitute)

// ===========================================================================
//  CgsFsm::ScriptedFsm -- reconstructed from BURNOUT_X360_ARTIST.XEX. The transition
//  logic lives in the FSM's Lua script (run through mLuaState); this C++ drives it:
//  Prepare brings the script up and enters the start state; SetState looks a state up by
//  id in the concrete FSM's table (GetState/GetStateCount) and swaps it (OnLeave/OnEnter);
//  SendEvent (next pass) marshals an event's variables into Lua then NextState->SetState.
//  The X360 gpcMessageBuffer assert machinery is replaced by CGS_ASSERT per the project rule.
// ===========================================================================

namespace CgsFsm
{

ScriptedFsm::ScriptedFsm() { Construct(); }

void ScriptedFsm::Construct()
{
    mpCurrentState   = nullptr;
    mLuaState.Construct();
    muSequenceNumber = 0;
}

void ScriptedFsm::Destruct()
{
    if (mLuaState.IsLuaResourceValid())
        Release();
}

// @ 0x82836668
bool ScriptedFsm::Prepare(CgsResource::LuaCodeResource* lpLuaCodeResource,
                          CgsMemory::HeapMalloc* lpHeapMalloc, CgsID lInitialStateId)
{
    mLuaState.Prepare(lpLuaCodeResource, lpHeapMalloc);
    mpCurrentState = nullptr;
    // No explicit start state -> ask the script which state it starts in; otherwise force it.
    if (lInitialStateId == 0)
        SetState(mLuaState.GetCurrentState());
    else
        SetState(lInitialStateId);
    return true;
}

// @ 0x828366D0
bool ScriptedFsm::Release()
{
    if (mpCurrentState != nullptr)
        mpCurrentState->OnLeave();
    mpCurrentState = nullptr;
    CGS_ASSERT(mLuaState.IsLuaResourceValid(), "mLuaState.IsLuaResourceValid()");
    mLuaState.Release();
    return true;
}

// @ 0x82835E90 -- find the state with this id in the table and make it current.
void ScriptedFsm::SetState(CgsID lStateId)
{
    ++muSequenceNumber;

    ScriptedState* lpTarget = nullptr;
    const s32 liCount = GetStateCount();
    for (s32 li = 0; li < liCount; ++li)
    {
        ScriptedState* lpState = GetState(li);
        if (lpState != nullptr && lpState->GetId() == lStateId)
        {
            lpTarget = lpState;
            break;
        }
    }

    if (lpTarget == nullptr)
    {
        char lacId[KI_CGSID_STRING_LEN];
        CgsIDUnCompress(lStateId, lacId);
        CGS_ASSERT(false, "Could not find state with id");   // X360 appends the id (lacId)
        return;
    }

    if (lpTarget == mpCurrentState)
        return;                                              // already current
    if (mpCurrentState != nullptr)
        mpCurrentState->OnLeave();
    mpCurrentState = lpTarget;
    lpTarget->OnEnter();
}

// @ 0x82836BA8 -- the script's numeric index for the current state.
s32 ScriptedFsm::GetCurrentStateIndex()
{
    CGS_ASSERT(mLuaState.IsLuaResourceValid(), "mLuaState.IsLuaResourceValid()");
    return mLuaState.GetInt("GetCurrentStateIndex", CgsIDCompress("0"));
}

// Base state-table accessors. The concrete FSM (CgsGui::StateMachine) overrides these to
// index into its fixed state array; the base has no table.
ScriptedState* ScriptedFsm::GetState(s32 /*liIndex*/) { return nullptr; }
s32            ScriptedFsm::GetStateCount()           { return 0; }

// @ 0x82836790 -- FLAG (incomplete): the faithful body marshals each of the event's
// variables into the Lua state (variable -> SetEventVariable) then runs the transition
// (mLuaState.NextState(event.id) -> GetCurrentState -> SetState). It needs the CgsFsm event
// type (event id @ +0x40, variable count @ +0x48, polymorphic variable objects) which is not
// modelled yet -- reconstruct in the next pass. Kept as a checked no-op so the vtable is whole.
void ScriptedFsm::SendEvent(const State* lpEvent)
{
    CGS_ASSERT(lpEvent != nullptr, "lpEvent != NULL");
    CGS_ASSERT(mLuaState.IsLuaResourceValid(), "mLuaState.IsLuaResourceValid()");
}

} // namespace CgsFsm
