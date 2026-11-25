#include "GameShared/GameClasses/Fsm/CgsScriptedFsm.h"
#define CGS_ARTIST_FILE "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\fsm\\CgsScriptedFsm.cpp"
#include "GameShared/GameClasses/Fsm/Resources/CgsLuaCodeResource.h"  // CgsResource::LuaCodeResource
#include "GameShared/GameClasses/Fsm/CgsEvent.h"                       // CgsFsm::Event
#include "GameShared/GameClasses/Fsm/CgsVariable.h"                    // CgsFsm::Variable
#include "GameShared/GameClasses/Memory/CgsHeapMalloc.h"              // CgsMemory::HeapMalloc
#include "GameShared/GameClasses/Core/CgsID.h"                        // CgsIDCompress / CgsIDUnCompress
#include "GameShared/GameClasses/Core/CgsStringUtils.h"               // CgsCore::SPrintf
#include "GameShared/GameClasses/Core/CgsAssert.h"                    // CGS_ASSERT (flagged assert substitute)
#include "GameShared/GameClasses/Development/CgsStrStream.h"
#include <cstdio>                                                      // std::sscanf

// ===========================================================================
//  CgsFsm::ScriptedFsm -- reconstructed from BURNOUT_X360_ARTIST.XEX. The transition
//  logic lives in the FSM's Lua script (run through mLuaState); this C++ drives it:
//  Prepare brings the script up and enters the start state; SetState looks a state up by
//  id in the concrete FSM's table (GetState/GetStateCount) and swaps it (OnLeave/OnEnter);
//  SendEvent (next pass) marshals an event's variables into Lua then NextState->SetState.
// ===========================================================================

namespace CgsFsm
{

const char* ScriptedFsm::KPC_SET_EVENT_VARIABLE_FUNCTION_NAME = "SetEventVariable";
const char* ScriptedFsm::KPC_GET_STATE_NAME_FUNCTION_NAME = "GetStateName";
const char* ScriptedFsm::KPC_GET_NEXT_STATES_FUNCTION_NAME = "GetNextStates";
const char* ScriptedFsm::KPC_GET_CURRENT_STATE_INDEX_FUNCTION_NAME = "GetCurrentStateIndex";

ScriptedFsm::ScriptedFsm() { Construct(); }

void ScriptedFsm::Construct()
{
    mpCurrentState   = 0;
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
    mpCurrentState = 0;
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
    if (mpCurrentState != 0)
    {
        mpCurrentState->OnLeave();
        mpCurrentState = 0;
    }
    CGS_ASSERT_AT(mLuaState.IsLuaResourceValid(), "mLuaState.IsLuaResourceValid()", CGS_ARTIST_FILE, 99);
    mLuaState.Release();
    return true;
}

// @ 0x82835E90 -- find the state with this id in the table and make it current.
void ScriptedFsm::SetState(CgsID lStateId)
{
    ++muSequenceNumber;

    const s32 liStateCount = GetStateCount();
    for (s32 li = 0; li < liStateCount; ++li)
    {
        ScriptedState* lpState = GetState(li);
        if (lpState->GetId() == lStateId)
        {
            if (lpState != mpCurrentState)
            {
                if (mpCurrentState != 0)
                    mpCurrentState->OnLeave();
                mpCurrentState = lpState;
                lpState->OnEnter();
            }
            return;
        }
    }

    char lacId[KI_CGSID_STRING_LEN];
    CgsIDUnCompress(lStateId, lacId);
    CGS_ASSERT_STREAM_AT(false, "Could not find state with id: " << lacId, CGS_ARTIST_FILE, 385);
}

// @ 0x828368A8 -- Lua indices name authored graph nodes, not C++ state-table slots.
const ScriptedState* ScriptedFsm::GetStateByIndex(s32 liStateIndex)
{
    CGS_ASSERT_AT(mLuaState.IsLuaResourceValid(), "mLuaState.IsLuaResourceValid()", CGS_ARTIST_FILE, 220);

    const ScriptedState* lpResult = 0;
    char lacStateIndex[128];
    CgsCore::SPrintf(lacStateIndex, 128, "%d", liStateIndex);
    const CgsID lStateIndexId = CgsIDCompress(lacStateIndex);
    char lacStateName[128];
    mLuaState.GetString(KPC_GET_STATE_NAME_FUNCTION_NAME, lStateIndexId, lacStateName, 128);
    const CgsID lStateId = CgsIDCompress(lacStateName);

    const s32 liNumStates = GetStateCount();
    for (s32 liState = 0; liState < liNumStates; ++liState)
    {
        const ScriptedState* lpState = GetState(liState);
        if (lpState->GetId() == lStateId)
        {
            lpResult = lpState;
            break;
        }
    }
    return lpResult;
}

// @ 0x82836BA8 -- the script's numeric index for the current state.
s32 ScriptedFsm::GetCurrentStateIndex()
{
    CGS_ASSERT_AT(mLuaState.IsLuaResourceValid(), "mLuaState.IsLuaResourceValid()", CGS_ARTIST_FILE, 312);
    return mLuaState.GetInt(KPC_GET_CURRENT_STATE_INDEX_FUNCTION_NAME, CgsIDCompress("0"));
}

// Base state-table accessors. The concrete FSM (CgsGui::StateMachine) overrides these to
// index into its fixed state array; the base has no table.
ScriptedState* ScriptedFsm::GetState(s32 /*liIndex*/) { return 0; }
s32            ScriptedFsm::GetStateCount()           { return 0; }

// Push event variables through the Lua setter before running the transition.
void ScriptedFsm::SendEvent(const Event* lpEvent)
{
    CGS_ASSERT_AT(lpEvent != 0, "lpEvent != NULL", CGS_ARTIST_FILE, 337);
    CGS_ASSERT_AT(mLuaState.IsLuaResourceValid(), "mLuaState.IsLuaResourceValid()", CGS_ARTIST_FILE, 338);

    for (s32 li = 0; li < lpEvent->GetVariableCount(); ++li)
    {
        lpEvent->GetVariable(li)->ToLua(&mLuaState, KPC_SET_EVENT_VARIABLE_FUNCTION_NAME);
    }

    mLuaState.NextState(lpEvent->GetId());
    SetState(mLuaState.GetCurrentState());
}

// @ 0x82836998 -- retrieve the authored next-state list for a Lua FSM state.
void ScriptedFsm::GetNextStates(s32 liStateIndex,
                                s32* lpaiNextStates,
                                s32* lpiNumNextStates)
{
    if (!mLuaState.IsLuaResourceValid())
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("mLuaState.IsLuaResourceValid()",
            CGS_ASSERT_LOCATION(CGS_ARTIST_FILE, 263));
        CgsDev::Assert::EndAssert();
    }

    char lacStateIndex[128];
    CgsCore::SPrintf(lacStateIndex, 128, "%d", liStateIndex);
    const CgsID lStateIndexId = CgsIDCompress(lacStateIndex);
    char lacNextStateIndices[128];
    mLuaState.GetString(KPC_GET_NEXT_STATES_FUNCTION_NAME, lStateIndexId, lacNextStateIndices, 128);

    s32 liOverflowScratch; // ARTIST sp+var_140 receives conversion 33, outside the 32-slot output.
    const s32 liParsedCount = std::sscanf(lacNextStateIndices,
        "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
        &lpaiNextStates[0],
        &lpaiNextStates[1],
        &lpaiNextStates[2],
        &lpaiNextStates[3],
        &lpaiNextStates[4],
        &lpaiNextStates[5],
        &lpaiNextStates[6],
        &lpaiNextStates[7],
        &lpaiNextStates[8],
        &lpaiNextStates[9],
        &lpaiNextStates[10],
        &lpaiNextStates[11],
        &lpaiNextStates[12],
        &lpaiNextStates[13],
        &lpaiNextStates[14],
        &lpaiNextStates[15],
        &lpaiNextStates[16],
        &lpaiNextStates[17],
        &lpaiNextStates[18],
        &lpaiNextStates[19],
        &lpaiNextStates[20],
        &lpaiNextStates[21],
        &lpaiNextStates[22],
        &lpaiNextStates[23],
        &lpaiNextStates[24],
        &lpaiNextStates[25],
        &lpaiNextStates[26],
        &lpaiNextStates[27],
        &lpaiNextStates[28],
        &lpaiNextStates[29],
        &lpaiNextStates[30],
        &lpaiNextStates[31],
        &liOverflowScratch);
    *lpiNumNextStates = liParsedCount;

    CGS_ASSERT_STREAM_AT(*lpiNumNextStates <= 32, "Too many state transitions in FSM\n",
                         CGS_ARTIST_FILE, 296);
}

} // namespace CgsFsm

#undef CGS_ARTIST_FILE
