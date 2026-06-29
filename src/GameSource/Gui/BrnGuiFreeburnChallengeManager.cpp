// BrnGuiFreeburnChallengeManager.cpp
// Reconstructed from BURNOUT_X360_ARTIST.XEX. Two groups of out-of-line members of
// BrnGui::FreeburnChallengeManager live here:
//   * the read-only accessors (GetCurrentChallenge/Action/SuccessForARCI/
//     ContributionForARCI/ContributionOverall/TargetType) -- each runs the project
//     CGS_ASSERT range guards (non-fatal: the X360 returns the value even on a failed
//     guard) then returns a member read; and
//   * the state-transition handlers (StartChallenge @0x82509D60, TriggerChallenge
//     @0x8250A160, HandleNewData @0x824F3FC8) -- which latch a challenge / fold a live
//     progress update into the tracked per-target state.
// The X360-baked assert file/line are discarded per project convention; the stringized
// condition matches the X360 assert message text.

#include "GameSource/Gui/BrnGuiFreeburnChallengeManager.h"
#include "GameSource/Gui/BrnGuiCache.h"                 // BrnGui::GuiCache::GetFreeburnChallengeList
#include "GameSource/Gui/Events/BrnGuiChallengeEvents.h" // Gui challenge event payloads
#include "SharedClasses/DataLists/ChallengeList.h"      // BrnResource::ChallengeList

#include <cstring>                                      // std::memcpy (X360 XMemCpy)

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

// @ 0x82509D60 — StartChallenge. Resolve the event's challenge id against the cache's
// freeburn challenge list, latch it as the current challenge, reset the per-run state to
// INITIALISED/AUTO_ROTATE, then build the target-type list from the challenge's actions.
//
// Target-list rule (recovered from the trailing three-way branch):
//   * if the CURRENT action's combine type is INDEPENDENT or COUNT (the X360 compares the
//     raw byte to 4 and 5) -> one target slot per action that declares targets;
//   * else if any action is a MEET_UP -> same per-action fill, but the page state is moved
//     to the value 3 (E_PAGE_STATE_COUNT; the X360 stores a literal 3 here -- the DWARF
//     EPageState has no named state for 3, so this is the attested "meet-up" sentinel);
//   * else (a plain single-action challenge) -> one slot from the current action only.
// The per-slot store writes maeChallengeTargetTypes[miTargetsCount++] = action's target
// data type 0 (the X360 reads the raw mau8TargetDataType[0] byte, modelled via the named
// GetTargetDataType(0) accessor).
void FreeburnChallengeManager::StartChallenge( const GuiChallengeStartEvent* lpEvent )
{
    const BrnResource::ChallengeList* lpList = mpGuiCache->GetFreeburnChallengeList();
    const s32 liChallengeIndex = lpList->GetChallengeIndex( lpEvent->mChallengeID );
    mpCurrentChallenge = ( liChallengeIndex < 0 )
                       ? nullptr
                       : lpList->GetChallengeData( liChallengeIndex );

    miTargetsCount       = 0;
    miCurrentTargetIndex = 0;
    miCurrentAction      = 0;
    meInternalState      = E_INTERNAL_STATE_INITIALISED;
    mbIsLocalHost        = lpEvent->mbIsLocalHost;
    mePageState          = E_PAGE_STATE_AUTO_ROTATE;

    const BrnResource::ChallengeListEntryAction* lpCurrentAction = GetCurrentAction();

    // X360/DWARF DRIFT (binary authoritative): the X360 breaks the scan when the action
    // type byte == 22 (asm `cmplwi r11, 0x16` @0x82509F28). The X360 action-type enum is
    // EXPANDED relative to the committed PS3-DWARF EChallengeActionType (whose meet-up is
    // 16 and whose E_ACTION_COUNT is 22; the binary asserts the byte < 0x29 == 41). We do
    // NOT redefine the committed enum (ODR); the literal 22 is the attested X360 meet-up
    // ordinal -- mirrors the documented `< 0x29` drift on GetActionType/GetChallengeStyle.
    const u8 KU_X360_MEET_UP_ACTION_TYPE = 22;

    bool lbHasMeetUp = false;
    for ( s32 liIndex = 0; liIndex < mpCurrentChallenge->GetNumActions(); ++liIndex )
    {
        CGS_ASSERT( mpCurrentChallenge, "mpCurrentChallenge" );
        CGS_ASSERT( mpCurrentChallenge->GetAction( liIndex ), "mpCurrentChallenge->GetAction(liIndex)" );
        if ( mpCurrentChallenge->GetAction( liIndex )->GetActionType()
                == KU_X360_MEET_UP_ACTION_TYPE )
        {
            lbHasMeetUp = true;
            break;
        }
    }

    const BrnResource::ChallengeListEntryAction::ECombineActionType leCombine =
        lpCurrentAction->GetCombineAction();
    if ( leCombine == BrnResource::ChallengeListEntryAction::E_COMBINE_ACTION_INDEPENDENT ||
         leCombine == BrnResource::ChallengeListEntryAction::E_COMBINE_ACTION_COUNT )
    {
        for ( s32 liIndex = 0; liIndex < mpCurrentChallenge->GetNumActions(); ++liIndex )
        {
            const BrnResource::ChallengeListEntryAction* lpAction = mpCurrentChallenge->GetAction( liIndex );
            if ( lpAction->GetNumTargets() != 0 )
            {
                maeChallengeTargetTypes[ miTargetsCount ] = lpAction->GetTargetDataType( 0 );
                ++miTargetsCount;
            }
        }
    }
    else if ( lbHasMeetUp )
    {
        mePageState = E_PAGE_STATE_COUNT; // X360 literal 3 (attested meet-up sentinel)
        for ( s32 liIndex = 0; liIndex < mpCurrentChallenge->GetNumActions(); ++liIndex )
        {
            const BrnResource::ChallengeListEntryAction* lpAction = mpCurrentChallenge->GetAction( liIndex );
            if ( lpAction->GetNumTargets() != 0 )
            {
                maeChallengeTargetTypes[ miTargetsCount ] = lpAction->GetTargetDataType( 0 );
                ++miTargetsCount;
            }
        }
    }
    else if ( lpCurrentAction->GetNumTargets() != 0 )
    {
        maeChallengeTargetTypes[ miTargetsCount ] = lpCurrentAction->GetTargetDataType( 0 );
        ++miTargetsCount;
    }
}

// @ 0x8250A160 — TriggerChallenge. Identical shape to StartChallenge except the run is
// latched straight into RUNNING (not INITIALISED) and the resolved current action is
// guarded non-null (the X360 fires the "lpCurrentAction" assert here). The same
// combine-type / meet-up / single-action target-list build follows.
void FreeburnChallengeManager::TriggerChallenge( const GuiChallengeTriggerResponse* lpEvent )
{
    const BrnResource::ChallengeList* lpList = mpGuiCache->GetFreeburnChallengeList();
    const s32 liChallengeIndex = lpList->GetChallengeIndex( lpEvent->mChallengeID );
    mpCurrentChallenge = ( liChallengeIndex < 0 )
                       ? nullptr
                       : lpList->GetChallengeData( liChallengeIndex );

    miTargetsCount       = 0;
    miCurrentTargetIndex = 0;
    miCurrentAction      = 0;
    mePageState          = E_PAGE_STATE_AUTO_ROTATE;
    meInternalState      = E_INTERNAL_STATE_RUNNING;
    mbIsLocalHost        = lpEvent->mbIsLocalHost;

    const BrnResource::ChallengeListEntryAction* lpCurrentAction = GetCurrentAction();
    CGS_ASSERT( lpCurrentAction, "lpCurrentAction" );

    // X360/DWARF DRIFT (binary authoritative): the X360 breaks the scan when the action
    // type byte == 22 (asm `cmplwi r11, 0x16` @0x8250A34C). The X360 action-type enum is
    // EXPANDED relative to the committed PS3-DWARF EChallengeActionType (whose meet-up is
    // 16 and whose E_ACTION_COUNT is 22; the binary asserts the byte < 0x29 == 41). We do
    // NOT redefine the committed enum (ODR); the literal 22 is the attested X360 meet-up
    // ordinal -- mirrors the documented `< 0x29` drift on GetActionType/GetChallengeStyle.
    const u8 KU_X360_MEET_UP_ACTION_TYPE = 22;

    bool lbHasMeetUp = false;
    for ( s32 liIndex = 0; liIndex < mpCurrentChallenge->GetNumActions(); ++liIndex )
    {
        CGS_ASSERT( mpCurrentChallenge, "mpCurrentChallenge" );
        CGS_ASSERT( mpCurrentChallenge->GetAction( liIndex ), "mpCurrentChallenge->GetAction(liIndex)" );
        if ( mpCurrentChallenge->GetAction( liIndex )->GetActionType()
                == KU_X360_MEET_UP_ACTION_TYPE )
        {
            lbHasMeetUp = true;
            break;
        }
    }

    const BrnResource::ChallengeListEntryAction::ECombineActionType leCombine =
        lpCurrentAction->GetCombineAction();
    if ( leCombine == BrnResource::ChallengeListEntryAction::E_COMBINE_ACTION_INDEPENDENT ||
         leCombine == BrnResource::ChallengeListEntryAction::E_COMBINE_ACTION_COUNT )
    {
        for ( s32 liIndex = 0; liIndex < mpCurrentChallenge->GetNumActions(); ++liIndex )
        {
            const BrnResource::ChallengeListEntryAction* lpAction = mpCurrentChallenge->GetAction( liIndex );
            if ( lpAction->GetNumTargets() != 0 )
            {
                maeChallengeTargetTypes[ miTargetsCount ] = lpAction->GetTargetDataType( 0 );
                ++miTargetsCount;
            }
        }
    }
    else if ( lbHasMeetUp )
    {
        mePageState = E_PAGE_STATE_COUNT; // X360 literal 3 (attested meet-up sentinel)
        for ( s32 liIndex = 0; liIndex < mpCurrentChallenge->GetNumActions(); ++liIndex )
        {
            const BrnResource::ChallengeListEntryAction* lpAction = mpCurrentChallenge->GetAction( liIndex );
            if ( lpAction->GetNumTargets() != 0 )
            {
                maeChallengeTargetTypes[ miTargetsCount ] = lpAction->GetTargetDataType( 0 );
                ++miTargetsCount;
            }
        }
    }
    else if ( lpCurrentAction->GetNumTargets() != 0 )
    {
        maeChallengeTargetTypes[ miTargetsCount ] = lpCurrentAction->GetTargetDataType( 0 );
        ++miTargetsCount;
    }
}

// @ 0x824F3FC8 — HandleNewData. Fold a live progress update into the tracked state. The
// X360 inlines IsStarted() (meInternalState is RUNNING or RESULTS) for the first guard and
// asserts the event's target count matches miTargetsCount. It then latches miCurrentAction
// from the event, and -- only when the page state is the meet-up sentinel (value 3) --
// also latches it as the current target index. The three per-target blocks (overall
// remaining, per-ARCI contributions, per-ARCI completion) are XMemCpy'd in (count =
// miTargetsCount), at the event source offsets recovered from the asm.
void FreeburnChallengeManager::HandleNewData( const GuiChallengeUpdateEvent* lpEvent )
{
    CGS_ASSERT( meInternalState == E_INTERNAL_STATE_RUNNING ||
                meInternalState == E_INTERNAL_STATE_RESULTS,
                "IsStarted()" ); // X360 inlines IsStarted()
    CGS_ASSERT( lpEvent->miNumTargetsUsed == miTargetsCount,
                "lpEvent->miNumTargetsUsed == miTargetsCount" );

    miCurrentAction = lpEvent->miCurrentAction;
    if ( mePageState == E_PAGE_STATE_COUNT ) // value 3: the meet-up single-target page
    {
        miCurrentTargetIndex = lpEvent->miCurrentAction;
    }

    // Order and byte counts match the X360 store sequence: remaining (count << 2 == 4
    // bytes/target), then contributions (count << 5 == 32 bytes/target == 8 f32), then
    // completion (count << 5 == 32 bytes/target == 8 enums).
    std::memcpy( maiOverallTargetRemaining,
                 lpEvent->maiOverallTargetRemaining,
                 static_cast<unsigned int>( miTargetsCount ) * sizeof( s32 ) );
    std::memcpy( maafIndividualTargetContributions,
                 lpEvent->maafIndividualTargetContributions,
                 static_cast<unsigned int>( miTargetsCount ) * KI_MAX_ARCI * sizeof( f32 ) );
    std::memcpy( maaeComplete,
                 lpEvent->maaeComplete,
                 static_cast<unsigned int>( miTargetsCount ) * KI_MAX_ARCI * sizeof( EFreeburnChallengeSuccess ) );
}

} // namespace BrnGui
