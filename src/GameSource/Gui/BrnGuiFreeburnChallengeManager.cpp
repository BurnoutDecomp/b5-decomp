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
// Matching builds use the original assertion paths and lines where recovered.

#include "GameSource/Gui/BrnGuiFreeburnChallengeManager.h"
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"
#include "GameSource/Gui/BrnGuiCache.h"                 // BrnGui::GuiCache::GetFreeburnChallengeList
#include "GameSource/Gui/Events/BrnGuiChallengeEvents.h" // Gui challenge event payloads
#include "SharedClasses/DataLists/ChallengeList.h"      // BrnResource::ChallengeList

#include <cstring>                                      // std::memcpy (X360 XMemCpy)

#define CGS_ARTIST_FILE "..\\..\\..\\GameSource\\Gui/BrnGuiFreeburnChallengeManager.cpp"

static const char KAC_ARTIST_ASSERT_FILE[] = "..\\..\\..\\GameSource\\Gui/BrnGuiFreeburnChallengeManager.h";

namespace BrnGui
{

// The auto-rotate dwell the X360 Update adds to the clock (`*(a1 + 8) = Time + 5.0`,
// @0x824F3F10). FLAG: constant name not DWARF-attested; the VALUE is X360-attested.
static const f32 KF_PAGE_ROTATE_SECONDS = 5.0f;

// @ 0x82501C80 — Construct. Latch the owning GuiCache and reset every tracked word to the
// "no challenge" resting state. Store-for-store from the X360 body (byte offsets shown as the
// members they are):
//     if (!a2) FireAssert("lpCache", "...BrnGuiFreeburnChallengeManager.cpp", 46);
//     *a1        = a2;    // mpGuiCache
//     *(a1+8)    = 0.0;   // mfTimeToNextChange
//     *(a1+12)   = 1;     // mePageState        = E_PAGE_STATE_AUTO_ROTATE
//     *(a1+20)   = 0;     // mpCurrentChallenge
//     *(a1+4)    = 0;     // meInternalState    = E_INTERNAL_STATE_OFF
//     *(a1+16)   = 0;     // miCurrentTargetIndex
//     *(a1+28)   = 0;     // miTargetsCount
//     *(a1+24)   = 0;     // mbIsLocalHost
//     *(a1+176)  = 0;     // miCurrentAction
//     XMemSet(a1+32, 0, 8);                               // maeChallengeTargetTypes[2]
//     FburnChallengeEveryPlayerStatusData::Construct(a1+184);   // mCompletedData
//
// E_INTERNAL_STATE_OFF is the resting state: every consumer of this manager gates on
// IsActive()/IsNotActive()/IsRunning()/IsShowingResults() first, and all four are false while
// the state is OFF, so a constructed-but-idle manager is exactly what an offline event
// (E_MODE_STUNT_ATTACK included) sees for its whole run.
//
void FreeburnChallengeManager::Construct( GuiCache* lpCache )
{
    CGS_ASSERT( lpCache, "lpCache" );   // BrnGuiFreeburnChallengeManager.cpp:46

    mpGuiCache           = lpCache;
    mfTimeToNextChange   = 0.0f;
    mePageState          = E_PAGE_STATE_AUTO_ROTATE;
    mpCurrentChallenge   = 0;
    meInternalState      = E_INTERNAL_STATE_OFF;
    miCurrentTargetIndex = 0;
    miTargetsCount       = 0;
    mbIsLocalHost        = false;
    miCurrentAction      = 0;
    mCompletedData.Construct();

    // the X360 XMemSet(a1 + 32, 0, 8) -- the two target-type words
    for ( s32 liTarget = 0; liTarget < KI_MAX_TARGETS; ++liTarget )
    {
        maeChallengeTargetTypes[ liTarget ] =
            static_cast<BrnResource::ChallengeListEntryAction::EChallengeDataType>( 0 );
    }
}

// @ 0x824F3F10 — Update. The AUTO_ROTATE page timer: while a challenge is live and the page
// state is auto-rotate, step miCurrentTargetIndex (wrapping at miTargetsCount) every five
// seconds of GuiCache time. The X360 reads miTargetsCount and the incremented index BEFORE the
// stores and re-reads GetTime for the new deadline; the shape below is store-for-store with it.
// The "mpGuiCache" assert is the console's own (cpp:75) and is NON-GATING -- but the cache is
// bound by Construct and this manager has exactly one construction site, so it is a tripwire.
// Note the whole body is behind `meInternalState != E_INTERNAL_STATE_OFF`, so an idle manager
// does not even read the clock.
void FreeburnChallengeManager::Update()
{
    CGS_ASSERT( mpGuiCache, "mpGuiCache" );   // BrnGuiFreeburnChallengeManager.cpp:75

    if ( meInternalState != E_INTERNAL_STATE_OFF &&
         mePageState == E_PAGE_STATE_AUTO_ROTATE &&
         mpGuiCache->GetTime() >= mfTimeToNextChange )
    {
        const s32 liTargetsCount = miTargetsCount;
        const s32 liNextTarget   = miCurrentTargetIndex + 1;

        mfTimeToNextChange   = mpGuiCache->GetTime() + KF_PAGE_ROTATE_SECONDS;
        miCurrentTargetIndex = liNextTarget;
        if ( liNextTarget >= liTargetsCount )
            miCurrentTargetIndex = 0;
    }
}

// @ 0x8240EC30 — guard meInternalState != OFF, return mpCurrentChallenge.
const BrnResource::ChallengeListEntry* FreeburnChallengeManager::GetCurrentChallenge() const
{
    CGS_ASSERT( meInternalState != E_INTERNAL_STATE_OFF,
        CGS_ASSERT_MESSAGE_AT(
            "meInternalState != E_INTERNAL_STATE_OFF",
            "..\\..\\..\\GameSource\\Gui/BrnGuiFreeburnChallengeManager.h", 235 /* ARTIST 0x8240EC58 */));
    return mpCurrentChallenge;
}

// @ 0x8240EC88 — guards meInternalState != OFF; mpCurrentChallenge non-null;
// miCurrentAction < mpCurrentChallenge->GetNumActions(); then return the action.
const BrnResource::ChallengeListEntryAction* FreeburnChallengeManager::GetCurrentAction() const
{
    CGS_ASSERT( meInternalState != E_INTERNAL_STATE_OFF,
        CGS_ASSERT_MESSAGE_AT(
            "meInternalState != E_INTERNAL_STATE_OFF",
            KAC_ARTIST_ASSERT_FILE, 251 /* ARTIST 0x8240ECBC */));
    CGS_ASSERT( mpCurrentChallenge,
        CGS_ASSERT_MESSAGE_AT("mpCurrentChallenge", KAC_ARTIST_ASSERT_FILE, 252 /* ARTIST 0x8240ECE4 */));
    CGS_ASSERT( miCurrentAction < mpCurrentChallenge->GetNumActions(),
        CGS_ASSERT_MESSAGE_AT(
            "miCurrentAction < mpCurrentChallenge->GetNumActions()",
            KAC_ARTIST_ASSERT_FILE, 253 /* ARTIST 0x8240ED14 */));
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
    CGS_ASSERT( meInternalState != E_INTERNAL_STATE_OFF,
        CGS_ASSERT_MESSAGE_AT(
            "meInternalState != E_INTERNAL_STATE_OFF",
            KAC_ARTIST_ASSERT_FILE, 291 /* ARTIST 0x8240EE80 */));
    CGS_ASSERT( miCurrentTargetIndex < miTargetsCount,
        CGS_ASSERT_MESSAGE_AT(
            "miCurrentTargetIndex < miTargetsCount",
            KAC_ARTIST_ASSERT_FILE, 292 /* ARTIST 0x8240EEAC */));
    CGS_ASSERT( miCurrentTargetIndex < BrnResource::ChallengeListEntry::KI_MAX_ACTIONS_PER_CHALLENGE,
        CGS_ASSERT_MESSAGE_AT(
            "miCurrentTargetIndex < BrnResource::ChallengeListEntry::KI_MAX_ACTIONS_PER_CHALLENGE",
            KAC_ARTIST_ASSERT_FILE, 293 /* ARTIST 0x8240EED4 */));
    CGS_ASSERT( leARCI >= E_ACTIVE_RACE_CAR_INDEX_0,
        CGS_ASSERT_MESSAGE_AT(
            "leARCI >= E_ACTIVE_RACE_CAR_INDEX_0",
            KAC_ARTIST_ASSERT_FILE, 294 /* ARTIST 0x8240EEF8 */));
    CGS_ASSERT( leARCI < E_ACTIVE_RACE_CAR_INDEX_COUNT,
        CGS_ASSERT_MESSAGE_AT(
            "leARCI < E_ACTIVE_RACE_CAR_INDEX_COUNT",
            KAC_ARTIST_ASSERT_FILE, 295 /* ARTIST 0x8240EF1C */));
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
    const CgsID lChallengeId = lpEvent->mChallengeID;
    mpCurrentChallenge = mpGuiCache->GetFreeburnChallengeList()->GetChallengeData(lChallengeId);

    miTargetsCount       = 0;
    miCurrentTargetIndex = 0;
    miCurrentAction      = 0;
    meInternalState      = E_INTERNAL_STATE_INITIALISED;
    mbIsLocalHost        = lpEvent->mbIsLocalHost;
    mePageState          = E_PAGE_STATE_AUTO_ROTATE;

    const BrnResource::ChallengeListEntryAction* lpCurrentAction = GetCurrentAction();

    // ARTIST compares the meet-up action byte to 22 at 0x82509F28.
    const u8 KU_X360_MEET_UP_ACTION_TYPE = 22;

    bool lbHasMeetUp = false;
    for ( s32 liIndex = 0; liIndex < mpCurrentChallenge->GetNumActions(); ++liIndex )
    {
        CGS_ASSERT_AT( mpCurrentChallenge, "mpCurrentChallenge", CGS_ARTIST_FILE, 134 );
        CGS_ASSERT_AT( mpCurrentChallenge->GetAction( liIndex ), "mpCurrentChallenge->GetAction(liIndex)",
                       CGS_ARTIST_FILE, 135 );
        if ( mpCurrentChallenge->GetAction( liIndex )->GetActionType()
                == KU_X360_MEET_UP_ACTION_TYPE )
        {
            lbHasMeetUp = true;
            break;
        }
    }

    const u32 luCombine = lpCurrentAction->GetCombineAction();
    if ( luCombine == BrnResource::ChallengeListEntryAction::E_COMBINE_ACTION_COUNT ||
         luCombine == BrnResource::ChallengeListEntryAction::E_COMBINE_ACTION_INDEPENDENT )
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
    const CgsID lChallengeId = lpEvent->mChallengeID;
    mpCurrentChallenge = mpGuiCache->GetFreeburnChallengeList()->GetChallengeData(lChallengeId);

    const bool lbIsLocalHost = lpEvent->mbIsLocalHost;
    miTargetsCount       = 0;
    miCurrentTargetIndex = 0;
    miCurrentAction      = 0;
    mePageState          = E_PAGE_STATE_AUTO_ROTATE;
    meInternalState      = E_INTERNAL_STATE_RUNNING;
    mbIsLocalHost        = lbIsLocalHost;

    const BrnResource::ChallengeListEntryAction* lpCurrentAction = GetCurrentAction();
    CGS_ASSERT_AT( lpCurrentAction, "lpCurrentAction", CGS_ARTIST_FILE, 248 );

    // ARTIST compares the meet-up action byte to 22 at 0x8250A34C.
    const u8 KU_X360_MEET_UP_ACTION_TYPE = 22;

    bool lbHasMeetUp = false;
    for ( s32 liIndex = 0; liIndex < mpCurrentChallenge->GetNumActions(); ++liIndex )
    {
        CGS_ASSERT_AT( mpCurrentChallenge, "mpCurrentChallenge", CGS_ARTIST_FILE, 254 );
        CGS_ASSERT_AT( mpCurrentChallenge->GetAction( liIndex ), "mpCurrentChallenge->GetAction(liIndex)",
                       CGS_ARTIST_FILE, 255 );
        if ( mpCurrentChallenge->GetAction( liIndex )->GetActionType()
                == KU_X360_MEET_UP_ACTION_TYPE )
        {
            lbHasMeetUp = true;
            break;
        }
    }

    const u32 luCombine = lpCurrentAction->GetCombineAction();
    if ( luCombine == BrnResource::ChallengeListEntryAction::E_COMBINE_ACTION_COUNT ||
         luCombine == BrnResource::ChallengeListEntryAction::E_COMBINE_ACTION_INDEPENDENT )
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

// StartNotActiveChallenge (GUI 583): a challenge is running that this player is not in.
// Latch NOT_ACTIVE and resolve the record's challenge id against the cache's list; a miss
// leaves no current challenge. The id is the record's first 8 bytes (the network bridge
// writes it there; the demangled 583 type is opaque, so it is read from the record base).
void FreeburnChallengeManager::StartNotActiveChallenge( const GuiChallengeNotActiveStartEvent* lpEvent )
{
    meInternalState = E_INTERNAL_STATE_NOT_ACTIVE;
    const CgsID lChallengeID = *reinterpret_cast<const CgsID*>( lpEvent );

    const BrnResource::ChallengeList* lpList = mpGuiCache->GetFreeburnChallengeList();
    const s32 liChallengeIndex = lpList->GetChallengeIndex( lChallengeID );
    mpCurrentChallenge = ( liChallengeIndex < 0 )
                       ? 0
                       : lpList->GetChallengeData( liChallengeIndex );
}

// FinishChallenge (GUI 579): the challenge is over; the tracker goes back to OFF. The assert
// is non-gating, the store always happens.
void FreeburnChallengeManager::FinishChallenge()
{
    CGS_ASSERT( meInternalState != E_INTERNAL_STATE_OFF,
        CGS_ASSERT_MESSAGE_AT(
            "meInternalState != E_INTERNAL_STATE_OFF",
            "..\\..\\..\\GameSource\\Gui/BrnGuiFreeburnChallengeManager.cpp", 358 /* ARTIST 0x824F40D8 */));
    meInternalState = E_INTERNAL_STATE_OFF;
}

// SelectNext (GUI 544): step to the next target page by hand. Ignored while OFF and on the
// meet-up page (state 3); otherwise the page state becomes SELECT, the target index wraps at
// miTargetsCount and the auto-rotate clock is zeroed.
void FreeburnChallengeManager::SelectNext()
{
    if ( meInternalState == E_INTERNAL_STATE_OFF || mePageState == E_PAGE_STATE_COUNT )
        return;

    const s32 liTargetsCount = miTargetsCount;
    const s32 liNextTarget   = miCurrentTargetIndex + 1;
    mePageState          = E_PAGE_STATE_SELECT;
    miCurrentTargetIndex = liNextTarget;
    if ( liNextTarget >= liTargetsCount )
        miCurrentTargetIndex = 0;
    mfTimeToNextChange = 0.0f;
}

// HandleCompletionStatus (GUI 581): the every-player completion block is copied whole into
// mCompletedData. GuiModule::Update inlines this as a 2104-byte memcpy of the record.
void FreeburnChallengeManager::HandleCompletionStatus(
    const GuiEventFburnChallengeEveryPlayerStatus* lpCompletionEvent )
{
    CGS_STATIC_ASSERT(( sizeof( BrnGameState::GameStateModuleIO::FburnChallengeEveryPlayerStatusData ) == 2104),
                   "GUI 581 carries the 2104-byte every-player status block" );
    std::memcpy( &mCompletedData, lpCompletionEvent, sizeof( mCompletedData ) );
}

} // namespace BrnGui

#undef CGS_ARTIST_FILE
