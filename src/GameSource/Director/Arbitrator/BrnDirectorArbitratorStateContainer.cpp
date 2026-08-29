#include "GameSource/Director/Arbitrator/BrnDirectorArbitratorStateContainer.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // [diag] gpDebugPrint
#include <cstdlib>                                          // [diag] getenv
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnDirector::ArbitratorStateContainer::ConstructAll @ 0x8224F020
//   BrnDirector::ArbitratorStateContainer::UpdateAll    @ 0x821F5DC0
//   BrnDirector::ArbitratorStateContainer::ReleaseAll   @ 0x821F5EA0
//
// ConstructAll constructs the embedded SharedPlaylists, then seeds the per-EState
// pointer table with the address of each embedded state, sanity-checks the table
// (no nulls, no duplicates), runs every state's Construct, and clears the current
// state. UpdateAll/ReleaseAll walk the table calling Update / Release on each state.
// (The original streamed the assert messages through CgsDev::StrStream; our CGS_ASSERT
// forwards the plain message string -- behaviourally identical for a non-firing assert.)

namespace BrnDirector
{
    // X360 0x827E2D18. The source build's synthesized constructor: it default-constructs
    // every embedded subobject -- writing each ArbitratorState subobject's vtable pointer
    // (the off_820C... loads) and initialising its interior members to the -1 sentinel
    // (the `stw -1, <subobject-offset>(this)` stores), and running mSharedPlaylists' ctor
    // (the inlined sub_827DC838 + the playlist vtable stores). Those are per-subobject
    // interior writes at the source build's absolute offsets, which our by-name layout
    // reproduces by letting each member run its own constructor: every ArbStateXxx runs
    // ArbitratorState(), and mSharedPlaylists default-constructs. The constructor does NOT
    // touch mArrayOfStatePointers / mpCurrentState (ConstructAll seeds those), matching the
    // asm -- so the body is the implicit member-wise construction with nothing further.
    ArbitratorStateContainer::ArbitratorStateContainer()
    {
    }

    // X360 0x821F5A60. Return the active state, asserting it has been set
    // (BrnDirectorArbitratorStateContainer.h:131 "mpCurrentState != NULL"). The source
    // reads/returns the +0x35CC word; here that is mpCurrentState, accessed by name.
    ArbitratorState* ArbitratorStateContainer::GetCurrentState() const
    {
        CGS_ASSERT(mpCurrentState != 0, "mpCurrentState != NULL");
        return mpCurrentState;
    }

    // The state registered for a given EState index. The X360 reaches a sibling state either
    // through this table (`container + 0x35A0 + 4*index`) or by its fixed interior offset when
    // the index is a compile-time constant -- e.g. ArbStateAttractMode::Update's hand-off reads
    // `container + 0x1890` for ROAMING, which is table slot 1's pointee. Both spellings resolve
    // to the same object; the table form is the one modelled here.
    ArbitratorState* ArbitratorStateContainer::GetState(EState leState) const
    {
        CGS_ASSERT(leState >= 0 && leState < E_NUM_STATES, "leState < E_NUM_STATES");
        CGS_ASSERT(mArrayOfStatePointers[leState] != 0, "mArrayOfStatePointers[leState] != NULL");
        return mArrayOfStatePointers[leState];
    }

    // Make the given EState the active state. The X360 spells it as the +0x35A0 table load
    // followed by the +0x35CC store (e.g. Arbitrator::Update's PRE_NORMAL case does exactly
    // `*(container + 0x35CC) = *(container + 0x35A4)`, i.e. SetCurrentState(E_STATE_ROAMING);
    // ArbStateAttractMode::Update does the same on its roaming hand-off).
    void ArbitratorStateContainer::SetCurrentState(EState leState)
    {
        // [diag] BRN_CRASHCAM_DIAG -- NOT IN THE X360 BINARY. This function is the SINGLE writer
        // of mpCurrentState, i.e. the one place that decides which arbitrator state owns the
        // frame, and nothing in the build reported it. Off unless the variable is set.
        if (getenv("BRN_CRASHCAM_DIAG") != 0 && CgsDev::Log::gpDebugPrint != 0)
        {
            *CgsDev::Log::gpDebugPrint
                << "[crashcam] container current state -> " << static_cast<s32>(leState)
                << " (" << GetState(leState)->GetName() << ")\n";
        }

        mpCurrentState = GetState(leState);
    }

    void ArbitratorStateContainer::ConstructAll()
    {
        mSharedPlaylists.Construct();

        // Zero the table, then point each EState slot at its embedded state object.
        for (u32 luLoop = 0; luLoop < E_NUM_STATES; ++luLoop)
        {
            mArrayOfStatePointers[luLoop] = 0;
        }

        mArrayOfStatePointers[E_STATE_DRIVETHRU]         = &mArbStateDriveThru;
        mArrayOfStatePointers[E_STATE_ROAMING]           = &mArbStateRoaming;
        mArrayOfStatePointers[E_STATE_CRASHING]          = &mArbStateCrashing;
        mArrayOfStatePointers[E_STATE_TAKEDOWN]          = &mArbStateTakedown;
        mArrayOfStatePointers[E_STATE_CRASH_MODE]        = &mArbStateCrashMode;
        mArrayOfStatePointers[E_STATE_POST_EVENT]        = &mArbStatePostEvent;
        mArrayOfStatePointers[E_STATE_RACE_INTRO]        = &mArbStateRaceIntro;
        mArrayOfStatePointers[E_STATE_ONLINE_RACE_INTRO] = &mArbStateOnlineRaceIntro;
        mArrayOfStatePointers[E_STATE_CAR_SELECT]        = &mArbStateCarSelect;
        mArrayOfStatePointers[E_STATE_RANK_UP]           = &mArbStateRankUp;
        mArrayOfStatePointers[E_STATE_ONLINE_CAR_SELECT] = &mArbStateOnlineCarSelect;

        // Every slot must be set; construct each state in EState order.
        for (u32 luLoop = 0; luLoop < E_NUM_STATES; ++luLoop)
        {
            CGS_ASSERT(mArrayOfStatePointers[luLoop] != 0, "Uninitialised state pointer");
            mArrayOfStatePointers[luLoop]->Construct();
        }

        // No two slots may alias the same state object.
        for (u32 luLoop = 0; luLoop < E_NUM_STATES; ++luLoop)
        {
            for (u32 luLoopStates = luLoop + 1; luLoopStates < E_NUM_STATES; ++luLoopStates)
            {
                CGS_ASSERT(mArrayOfStatePointers[luLoop] != mArrayOfStatePointers[luLoopStates],
                           "Duplicate state pointer");
            }
        }

        mpCurrentState = 0;
    }

    void ArbitratorStateContainer::UpdateAll(ArbStateSharedInfo& lrSharedInfo)
    {
        for (u32 luLoop = 0; luLoop < E_NUM_STATES; ++luLoop)
        {
            CGS_ASSERT(mArrayOfStatePointers[luLoop] != 0, "Uninitialised state pointer");
            mArrayOfStatePointers[luLoop]->Update(lrSharedInfo);
            mArrayOfStatePointers[luLoop]->ClearCycleCameraThisFrame();
        }
    }

    void ArbitratorStateContainer::ReleaseAll(ArbStateSharedInfo& lrSharedInfo)
    {
        for (u32 luLoop = 0; luLoop < E_NUM_STATES; ++luLoop)
        {
            CGS_ASSERT(mArrayOfStatePointers[luLoop] != 0, "Uninitialised state pointer");
            bool lbReleased = mArrayOfStatePointers[luLoop]->Release(lrSharedInfo);
            CGS_ASSERT(lbReleased, "mArrayOfStatePointers[luLoop]->Release(lSharedInfo)");
        }
    }
}
