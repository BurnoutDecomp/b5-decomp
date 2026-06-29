// BrnGuiFreeburnChallengeManager.cpp
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The five out-of-line accessors of
// BrnGui::FreeburnChallengeManager. Each runs the project CGS_ASSERT range guards
// (non-fatal: the X360 returns the value even on a failed guard) then returns a
// member read. The X360-baked assert file/line are discarded per project convention;
// the stringized condition matches the X360 assert message text.

#include "GameSource/Gui/BrnGuiFreeburnChallengeManager.h"

namespace BrnGui
{

// @ 0x8240EC30 — guard meInternalState != OFF, return mpCurrentChallenge.
const BrnResource::ChallengeListEntry* FreeburnChallengeManager::GetCurrentChallenge() const
{
    CGS_ASSERT( meInternalState != E_INTERNAL_STATE_OFF, "meInternalState != E_INTERNAL_STATE_OFF" );
    return mpCurrentChallenge;
}

// @ 0x8240EC88 — guards meInternalState != OFF; mpCurrentChallenge non-null;
// miCurrentAction < mpCurrentChallenge->GetNumActions(); then return the action.
const BrnResource::ChallengeListEntryAction* FreeburnChallengeManager::GetCurrentAction() const
{
    CGS_ASSERT( meInternalState != E_INTERNAL_STATE_OFF, "meInternalState != E_INTERNAL_STATE_OFF" );
    CGS_ASSERT( mpCurrentChallenge, "mpCurrentChallenge" );
    CGS_ASSERT( miCurrentAction < mpCurrentChallenge->GetNumActions(),
                "miCurrentAction < mpCurrentChallenge->GetNumActions()" );
    return mpCurrentChallenge->GetAction( miCurrentAction );
}

// @ 0x8240ED50 (IDA "GetCu") — guards internal-state/target-index/ARCI bounds, then
// return maaeComplete[miCurrentTargetIndex][leARCI].
FreeburnChallengeManager::EFreeburnChallengeSuccess
FreeburnChallengeManager::GetCurrentSuccessForARCI( EActiveRaceCarIndex leARCI ) const
{
    CGS_ASSERT( meInternalState != E_INTERNAL_STATE_OFF, "meInternalState != E_INTERNAL_STATE_OFF" );
    CGS_ASSERT( miCurrentTargetIndex < miTargetsCount, "miCurrentTargetIndex < miTargetsCount" );
    CGS_ASSERT( miCurrentTargetIndex < BrnResource::ChallengeListEntry::KI_MAX_ACTIONS_PER_CHALLENGE,
                "miCurrentTargetIndex < BrnResource::ChallengeListEntry::KI_MAX_ACTIONS_PER_CHALLENGE" );
    CGS_ASSERT( leARCI >= E_ACTIVE_RACE_CAR_INDEX_0, "leARCI >= E_ACTIVE_RACE_CAR_INDEX_0" );
    CGS_ASSERT( leARCI < E_ACTIVE_RACE_CAR_INDEX_COUNT, "leARCI < E_ACTIVE_RACE_CAR_INDEX_COUNT" );
    return maaeComplete[ miCurrentTargetIndex ][ leARCI ];
}

// @ 0x8240EE50 — guards internal-state/target-index/ARCI bounds, then return
// maafIndividualTargetContributions[miCurrentTargetIndex][leARCI].
f32 FreeburnChallengeManager::GetCurrentContributionForARCI( EActiveRaceCarIndex leARCI ) const
{
    CGS_ASSERT( meInternalState != E_INTERNAL_STATE_OFF, "meInternalState != E_INTERNAL_STATE_OFF" );
    CGS_ASSERT( miCurrentTargetIndex < miTargetsCount, "miCurrentTargetIndex < miTargetsCount" );
    CGS_ASSERT( miCurrentTargetIndex < BrnResource::ChallengeListEntry::KI_MAX_ACTIONS_PER_CHALLENGE,
                "miCurrentTargetIndex < BrnResource::ChallengeListEntry::KI_MAX_ACTIONS_PER_CHALLENGE" );
    CGS_ASSERT( leARCI >= E_ACTIVE_RACE_CAR_INDEX_0, "leARCI >= E_ACTIVE_RACE_CAR_INDEX_0" );
    CGS_ASSERT( leARCI < E_ACTIVE_RACE_CAR_INDEX_COUNT, "leARCI < E_ACTIVE_RACE_CAR_INDEX_COUNT" );
    return maafIndividualTargetContributions[ miCurrentTargetIndex ][ leARCI ];
}

// @ 0x8240EF50 (IDA "GetCur") — guards internal-state/target-index, then return
// maeChallengeTargetTypes[miCurrentTargetIndex].
BrnResource::ChallengeListEntryAction::EChallengeDataType
FreeburnChallengeManager::GetCurrentTargetType() const
{
    CGS_ASSERT( meInternalState != E_INTERNAL_STATE_OFF, "meInternalState != E_INTERNAL_STATE_OFF" );
    CGS_ASSERT( miCurrentTargetIndex < miTargetsCount, "miCurrentTargetIndex < miTargetsCount" );
    CGS_ASSERT( miCurrentTargetIndex < BrnResource::ChallengeListEntry::KI_MAX_ACTIONS_PER_CHALLENGE,
                "miCurrentTargetIndex < BrnResource::ChallengeListEntry::KI_MAX_ACTIONS_PER_CHALLENGE" );
    return maeChallengeTargetTypes[ miCurrentTargetIndex ];
}

// @ 0x824F4160 — sum the per-ARCI contributions over all eight active-race-car slots.
// The X360 walks leCar = 0..7 with the post-increment operator++(EActiveRaceCarIndex&,int)
// (GameSource/BurnoutConstants.h:39) inlined: that operator carries the range guard whose
// failed assert ("leEnumIndex <= E_ACTIVE_RACE_CAR_INDEX_COUNT", BurnoutConstants.h line 39)
// shows in this function's asm. The guard is non-fatal. Hex-Rays renders the prototype as
// void because it loses the fp31 return; the function returns the accumulated f32 total.
f32 FreeburnChallengeManager::GetCurrentContributionOverall() const
{
    f32 lfCurrentTotal = 0.0f;
    for ( EActiveRaceCarIndex leCurrentCar = E_ACTIVE_RACE_CAR_INDEX_0;
          leCurrentCar < E_ACTIVE_RACE_CAR_INDEX_COUNT;
          leCurrentCar++ )
    {
        lfCurrentTotal += GetCurrentContributionForARCI( leCurrentCar );
    }
    return lfCurrentTotal;
}

} // namespace BrnGui
