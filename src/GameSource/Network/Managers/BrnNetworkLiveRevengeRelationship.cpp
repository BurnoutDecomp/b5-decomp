#include "GameSource/Network/Managers/BrnNetworkLiveRevengeRelationship.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// @ 0x82355540  int __fastcall BrnNetwork::LiveRevengeRelationship::GetTotalTakedowns(_DWORD *a1)
// Sum of the local player's and the rival's lifetime takedowns across the whole
// relationship. X360: return *a1 + a1[9]  ->  mOverallStats.mPlayerStats.miTakedowns
// (+0) + mOverallStats.mRivalStats.miTakedowns (+36). The X360 build asserts the
// total is non-negative before returning it.
// Original assert site: GameSource/Network/Managers/BrnNetworkLiveRevengeRelationship.h:389
s32 BrnNetwork::LiveRevengeRelationship::GetTotalTakedowns() const
{
    const s32 liTotal = mOverallStats.mPlayerStats.miTakedowns +
                        mOverallStats.mRivalStats.miTakedowns;

    CGS_ASSERT(liTotal >= 0,
               "mOverallStats.mPlayerStats.miTakedowns + mOverallStats.mRivalStats.miTakedowns >= 0");

    return liTotal;
}

namespace BrnNetwork
{
    // ----------------------------------------------------------------------------------
    // The relationship "clear" family. All three zero the SAME field set, store-for-store:
    //   mOverallStats (the 18-word +0..+68 block), mLastTimeChanged's two FILETIME halves
    //   (+76/+80), the un-homed mUniqueID byte @+88 and 8-byte field @+104, and the two
    //   trailing ints miCurrentScoreForPlayersPointOfView (+112) / miTotalEvents (+116).
    //   The X360 leaves mLastTimeChanged.mbIsLocal (+72) untouched here (it is owned by the
    //   wall-clock Update path), and the opaque mUniqueID head/interior bytes untouched.
    // Each member function below zeroes the set by name (members are private; expressed
    // inline so no extra header surface is added).
    // ----------------------------------------------------------------------------------

    // @ 0x82547BE8  -- reset the relationship in place (no return value).
    void LiveRevengeRelationship::Destruct()
    {
        // mOverallStats: zero both 36-byte stat blocks (the +0..+68 word stores).
        mOverallStats.mPlayerStats = CommonRelationshipStats();
        mOverallStats.mRivalStats  = CommonRelationshipStats();
        // mLastTimeChanged: zero the two FILETIME halves (+76 / +80); DateAndTime::Clear
        // zeros exactly those and leaves mbIsLocal (+72) untouched (no +72 store in the asm).
        mLastTimeChanged.Clear();
        // mUniqueID (un-homed): the byte @+88 and the 8-byte field @+104 the X360 zeroes.
        mbUniqueID_88  = 0;
        muUniqueID_104 = 0;
        miCurrentScoreForPlayersPointOfView = 0;   // +112
        miTotalEvents                       = 0;   // +116
    }

    // @ 0x82547B78  -- reset the relationship in place; returns true (the X360 li r3,1).
    bool LiveRevengeRelationship::Release()
    {
        mOverallStats.mPlayerStats = CommonRelationshipStats();
        mOverallStats.mRivalStats  = CommonRelationshipStats();
        mLastTimeChanged.Clear();
        mbUniqueID_88  = 0;
        muUniqueID_104 = 0;
        miCurrentScoreForPlayersPointOfView = 0;
        miTotalEvents                       = 0;
        return true;
    }

    // @ 0x82547D70  -- static debug callback: cast the void* to the relationship, clear it,
    // then refresh the timestamp (X360 tail-calls CgsSystem::DateAndTime::Update(this+0x48)).
    void LiveRevengeRelationship::DEBUGClearRelationship(void* lpParameter)
    {
        LiveRevengeRelationship* lpThis = static_cast<LiveRevengeRelationship*>(lpParameter);
        lpThis->mOverallStats.mPlayerStats = CommonRelationshipStats();
        lpThis->mOverallStats.mRivalStats  = CommonRelationshipStats();
        lpThis->mLastTimeChanged.Clear();
        lpThis->mbUniqueID_88  = 0;
        lpThis->muUniqueID_104 = 0;
        lpThis->miCurrentScoreForPlayersPointOfView = 0;
        lpThis->miTotalEvents                       = 0;
        lpThis->mLastTimeChanged.Update();
    }
}
