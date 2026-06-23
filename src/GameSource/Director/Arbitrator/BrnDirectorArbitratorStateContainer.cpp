#include "GameSource/Director/Arbitrator/BrnDirectorArbitratorStateContainer.h"
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
