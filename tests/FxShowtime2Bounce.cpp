// FX-SHOWTIME2 (crash parity 2026-09-24): the PRODUCTION GameStateModule::
// ProcessGameEventsShowtimeBounceBringUp -- the extracted CASE-52 / CASE-53 arms of
// GameStateModule::ProcessGameEvents @0x823A0A18 -- pulled out of
// src/GameSource/GameState/GameStateModule_gUI_00.cpp by run_fxshowtime2_bounce.py and driven with
// real queued events against:
//   * the REAL game-event records JustBouncedEvent / JustAppliedExtraSpinEvent (BrnGameEvents.h),
//     posted onto the REAL CgsModule::VariableEventQueue<1536,16> the pre-world pump walks,
//   * the REAL action records JustBouncedAction / JustAppliedExtraSpinAction (BrnGameActions.h),
//     read back BYTE BY BYTE from the REAL GameActionQueue (VariableEventQueue<13312,16>),
//   * the REAL CrashModeScoring layout with the PRODUCTION GetCurrentComboCount /
//     GetNumCarsCrashed bodies extracted from BrnCrashModeScoring.cpp.
// CrashModeScoring::DealWithPlayerBounced is a SPY here (it records its arguments and how many
// actions were already posted): the console's callee is a lone `blr` @0x8284CB38, so the call's
// ARGUMENTS and POSITION are all there is to check. The runner checks separately that the
// production body is empty.
//
// Checked against the ARTIST asm (r25 = the event, r22 = the action queue, r31 = this, r18 = 0,
// the record at r1+0x330):
//   case 52 @0x823A3D74..0x823A3E0C
//     rec+0x10 = lwz 0(ev)       rec+0x20..+0x23 = lbz 4 / 5 / 6 / 7 (ev)
//     rec+0x14 = lwz 0x20D0(this) (scorer+0x2E0, miCurrentComboCount)
//     rec+0x18 = r18 + the four words at this+0x20D8 (`li r11, 4` loop == GetNumCarsCrashed)
//     rec+0x1C = lwz 8(ev)       rec+0x00..+0x0F = `lvx128 v0, r25, 0x10 ; stvx128 v0, r0, rec`
//     AddEvent(r22, rec, 0x90, 0x30), THEN DealWithPlayerBounced(this+0x1DF0,
//       lbz rec+0x21, lbz rec+0x23, lwz rec+0x1C)
//   case 53 @0x823A3E10..0x823A3E20: AddEvent(r22, var, 0x91, 1)
#include "types.hpp"
#include "BrnCommonTypes.h"
#include "GameSource/GameState/BrnGameActions.h"                                     // the real action records + ids
#include "GameSource/GameState/BrnGameEvents.h"                                      // the real event records + ids
#include "GameSource/GameState/ModeManager/Scoring/BrnCrashModeScoringRecentCrash.h" // the real scorer
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

static unsigned gChecks = 0, gFailures = 0;
static unsigned gAsserts = 0;

namespace CgsDev
{
namespace Assert
{
    int   BeginAssert() { return 0; }
    int   FireAssert(const char*, const char*, int) { ++gAsserts; return 0; }
    void* EndAssert() { return nullptr; }
}
namespace Log
{
    DebugPrint* gpDebugPrint = nullptr;   // the [showtime-bounce] witness stays silent
}
namespace Message
{
    u64 gxMessageFilterFlags = 0;         // the queues' overflow dumps stay silent
}
}

typedef CgsModule::VariableEventQueue<1536, 16> GameEventQueue;

// ---- the production scorer accessors + the DealWithPlayerBounced spy ----------------------------
namespace BrnGameState
{
#include "bounce_scoring_methods.inc"

struct BounceCall
{
    bool mbOnCar;
    bool mbWasGoodImpact;
    u32  muImpactEntity;
    s32  miActionsAlreadyPosted;
};
static std::vector<BounceCall>         gaBounceCalls;
static GameStateModuleIO::GameActionQueue* gpSpyActionQueue = nullptr;

static s32 CountActions(const GameStateModuleIO::GameActionQueue* lpQueue)
{
    s32 liCount = 0;
    const CgsModule::Event* lpEvent = nullptr;
    s32 liSize = 0;
    lpQueue->GetFirstEvent(&lpEvent, &liSize);
    while (lpEvent)
    {
        ++liCount;
        const CgsModule::Event* lpNext = nullptr;
        lpQueue->GetNextEvent(lpEvent, &lpNext, &liSize);
        lpEvent = lpNext;
    }
    return liCount;
}

void CrashModeScoring::DealWithPlayerBounced(bool lbOnCar, bool lbWasGoodImpact, EntityId lidImpactEntityId)
{
    BounceCall lCall;
    lCall.mbOnCar                = lbOnCar;
    lCall.mbWasGoodImpact        = lbWasGoodImpact;
    lCall.muImpactEntity         = lidImpactEntityId.muValue;
    lCall.miActionsAlreadyPosted = gpSpyActionQueue ? CountActions(gpSpyActionQueue) : -1;
    gaBounceCalls.push_back(lCall);
}
}

// ---- the stand-in module ------------------------------------------------------------------------
namespace BrnGameState
{
struct ScoringSystemStandIn
{
    CrashModeScoring* mpCrashScorer = nullptr;
    CrashModeScoring* GetCrashScorer() { return mpCrashScorer; }
};

struct ModeManagerStandIn
{
    ScoringSystemStandIn mScoringSystem;
    ScoringSystemStandIn* GetScoringSystem() { return &mScoringSystem; }
};

class GameStateModule
{
public:
    ModeManagerStandIn mModeManager;

    void ProcessGameEventsShowtimeBounceBringUp(const CgsModule::VariableEventQueue<1536, 16>* lpGameEventQueue,
                                                GameStateModuleIO::GameActionQueue*            lpActionQueue);
};

#include "bounce_methods.inc"
}

using namespace BrnGameState;

static void Check(bool lbPass, const char* lpcName)
{
    ++gChecks;
    if (!lbPass)
    {
        ++gFailures;
        std::printf("  FAIL %s\n", lpcName);
    }
    else
    {
        std::printf("  ok   %s\n", lpcName);
    }
}

// One posted action, copied out of the queue.
struct PostedAction
{
    s32 miType;
    s32 miSize;
    u8  maBytes[64];
};

static std::vector<PostedAction> Drain(GameStateModuleIO::GameActionQueue& lrQueue)
{
    std::vector<PostedAction> laActions;
    const CgsModule::Event* lpEvent = nullptr;
    s32 liSize = 0;
    s32 liType = lrQueue.GetFirstEvent(&lpEvent, &liSize);
    while (lpEvent)
    {
        PostedAction lAction;
        std::memset(&lAction, 0xCD, sizeof(lAction));
        lAction.miType = liType;
        lAction.miSize = liSize;
        std::memcpy(lAction.maBytes, lpEvent, (liSize < 64) ? liSize : 64);
        laActions.push_back(lAction);
        const CgsModule::Event* lpNext = nullptr;
        liType = lrQueue.GetNextEvent(lpEvent, &lpNext, &liSize);
        lpEvent = lpNext;
    }
    lrQueue.Clear();
    return laActions;
}

static s32 Word(const PostedAction& lrAction, s32 liOffset)
{
    s32 liValue = 0;
    std::memcpy(&liValue, lrAction.maBytes + liOffset, sizeof(liValue));
    return liValue;
}

// A 32-byte event 52 exactly as VehicleManager::ProcessAftertouchEvents posts it: GetRecentBounce's
// seven outputs at +0x00 / +0x04..+0x07 / +0x08 / +0x10, the contact point's fourth lane included.
static GameStateModuleIO::JustBouncedEvent Bounce(s32 liChain, bool lbFromStationary, bool lbOnCar, bool lbBoosted,
                                                   bool lbGoodImpact, u32 luEntity, const f32 (&lafPoint)[4])
{
    GameStateModuleIO::JustBouncedEvent lEvent;
    std::memset(&lEvent, 0, sizeof(lEvent));
    lEvent.miBounceChain             = liChain;
    lEvent.mbFromStationary          = lbFromStationary;
    lEvent.mbOnCar                   = lbOnCar;
    lEvent.mbBoostedBounce           = lbBoosted;
    lEvent.mbGoodImpact              = lbGoodImpact;
    lEvent.midImpactEntityId.muValue = luEntity;
    std::memcpy(&lEvent.mContactPoint, lafPoint, 16);
    return lEvent;
}

static void Post(GameEventQueue& lrQueue, const GameStateModuleIO::JustBouncedEvent& lrEvent)
{
    lrQueue.AddEvent(reinterpret_cast<const CgsModule::Event*>(&lrEvent), GameStateModuleIO::E_EVENT_JUST_BOUNCED,
                     static_cast<s32>(sizeof(lrEvent)));
}

static void PostSpin(GameEventQueue& lrQueue)
{
    const GameStateModuleIO::JustAppliedExtraSpinEvent lEvent = {};
    lrQueue.AddEvent(reinterpret_cast<const CgsModule::Event*>(&lEvent),
                     GameStateModuleIO::E_EVENT_JUST_APPLIED_EXTRA_SPIN, static_cast<s32>(sizeof(lEvent)));
}

static void PostOther(GameEventQueue& lrQueue, s32 liType, s32 liSize)
{
    u8 laBytes[32];
    std::memset(laBytes, 0x5A, sizeof(laBytes));
    lrQueue.AddEvent(reinterpret_cast<const CgsModule::Event*>(laBytes), liType, liSize);
}

// Checks one posted 144 against the event it came from and the scorer state it read.
static void CheckRecord(const PostedAction& lr, const GameStateModuleIO::JustBouncedEvent& lrEvent,
                        s32 liCombo, s32 liCars, const char* lpcTag)
{
    char lacName[256];
    std::snprintf(lacName, sizeof(lacName), "%s: action 144, size 48 (li r5, 0x90 ; li r6, 0x30)", lpcTag);
    Check(lr.miType == 144 && lr.miSize == 48, lacName);
    std::snprintf(lacName, sizeof(lacName),
                  "%s: rec+0x00..+0x0F = the event's +0x10 vector, all 16 bytes (lvx128 / stvx128)", lpcTag);
    Check(std::memcmp(lr.maBytes + 0x00, reinterpret_cast<const u8*>(&lrEvent) + 0x10, 16) == 0, lacName);
    std::snprintf(lacName, sizeof(lacName), "%s: rec+0x10 = the event's word 0, the bounce chain (lwz 0)", lpcTag);
    Check(Word(lr, 0x10) == lrEvent.miBounceChain, lacName);
    std::snprintf(lacName, sizeof(lacName), "%s: rec+0x14 = the scorer's miCurrentComboCount (lwz 0x20D0)", lpcTag);
    Check(Word(lr, 0x14) == liCombo, lacName);
    std::snprintf(lacName, sizeof(lacName), "%s: rec+0x18 = maiNumCarsCrashed[0..3] summed (the li r11, 4 loop)", lpcTag);
    Check(Word(lr, 0x18) == liCars, lacName);
    std::snprintf(lacName, sizeof(lacName), "%s: rec+0x1C = the event's word 2, the impact entity (lwz 8)", lpcTag);
    Check(static_cast<u32>(Word(lr, 0x1C)) == lrEvent.midImpactEntityId.muValue, lacName);
    std::snprintf(lacName, sizeof(lacName),
                  "%s: rec+0x20..+0x23 = the event's bytes 4..7 in order (fromStationary, onCar, boosted, goodImpact)",
                  lpcTag);
    Check(lr.maBytes[0x20] == (lrEvent.mbFromStationary ? 1 : 0) && lr.maBytes[0x21] == (lrEvent.mbOnCar ? 1 : 0)
              && lr.maBytes[0x22] == (lrEvent.mbBoostedBounce ? 1 : 0)
              && lr.maBytes[0x23] == (lrEvent.mbGoodImpact ? 1 : 0),
          lacName);
    bool lbTailZero = true;
    for (s32 liByte = 0x24; liByte < 0x30; ++liByte)
    {
        lbTailZero = lbTailZero && (lr.maBytes[liByte] == 0);
    }
    std::snprintf(lacName, sizeof(lacName),
                  "%s: rec+0x24..+0x2F carry nothing from the event or the scorer (never stored by the console; zero here)",
                  lpcTag);
    Check(lbTailZero, lacName);
}

int main()
{
    static CrashModeScoring lScorer;
    std::memset(&lScorer, 0, sizeof(lScorer));
    lScorer.miCurrentComboCount  = 3;
    lScorer.miScoreMultiplier    = 5;
    lScorer.maiNumCarsCrashed[0] = 2;
    lScorer.maiNumCarsCrashed[1] = 1;
    lScorer.maiNumCarsCrashed[2] = 0;
    lScorer.maiNumCarsCrashed[3] = 4;   // GetNumCarsCrashed() == 7

    static GameStateModule lModule;
    lModule.mModeManager.mScoringSystem.mpCrashScorer = &lScorer;

    static GameEventQueue lEvents;
    lEvents.Construct();
    static GameStateModuleIO::GameActionQueue lActions;
    lActions.Construct();
    gpSpyActionQueue = &lActions;

    const f32 lafPointA[4] = { 1.5f, -2.25f, 3.125f, 7.0f };        // the fourth lane rides along
    const f32 lafPointB[4] = { -40.0f, 0.5f, 1234.75f, -0.0f };
    const GameStateModuleIO::JustBouncedEvent lBounceA = Bounce(2, true, false, true, true, 0x00C0FFEEu, lafPointA);
    const GameStateModuleIO::JustBouncedEvent lBounceB = Bounce(5, false, true, false, false, 0x80000011u, lafPointB);

    // ---- one bounce -----------------------------------------------------------------------------
    std::printf("-- one event 52\n");
    Post(lEvents, lBounceA);
    lModule.ProcessGameEventsShowtimeBounceBringUp(&lEvents, &lActions);
    std::vector<PostedAction> laPosted = Drain(lActions);
    Check(laPosted.size() == 1, "one event 52 posts exactly one action");
    if (laPosted.size() == 1)
    {
        CheckRecord(laPosted[0], lBounceA, 3, 7, "bounce A");
    }
    Check(gaBounceCalls.size() == 1, "DealWithPlayerBounced is called once per bounce (bl @0x823A3E08)");
    if (gaBounceCalls.size() == 1)
    {
        Check(!gaBounceCalls[0].mbOnCar && gaBounceCalls[0].mbWasGoodImpact
                  && gaBounceCalls[0].muImpactEntity == 0x00C0FFEEu,
              "...with (lbz rec+0x21 onCar, lbz rec+0x23 goodImpact, lwz rec+0x1C entity)");
        Check(gaBounceCalls[0].miActionsAlreadyPosted == 1,
              "...AFTER the 144 is posted (the AddEvent @0x823A3DF4 precedes the call)");
    }
    Check(lScorer.miCurrentComboCount == 3 && lScorer.GetNumCarsCrashed() == 7 && lScorer.miScoreMultiplier == 5,
          "the arm only reads the scorer: combo, cars and multiplier unchanged");

    // ---- the opposite flags, and the scorer read live -------------------------------------------
    std::printf("-- the opposite flags, a different scorer state\n");
    lEvents.Clear();
    lScorer.miCurrentComboCount  = 4;
    lScorer.maiNumCarsCrashed[2] = 2;   // 9
    Post(lEvents, lBounceB);
    lModule.ProcessGameEventsShowtimeBounceBringUp(&lEvents, &lActions);
    laPosted = Drain(lActions);
    Check(laPosted.size() == 1, "one event 52 posts exactly one action (again)");
    if (laPosted.size() == 1)
    {
        CheckRecord(laPosted[0], lBounceB, 4, 9, "bounce B");
    }
    Check(gaBounceCalls.size() == 2 && gaBounceCalls[1].mbOnCar && !gaBounceCalls[1].mbWasGoodImpact
              && gaBounceCalls[1].muImpactEntity == 0x80000011u,
          "DealWithPlayerBounced gets bounce B's own onCar / goodImpact / entity");

    // ---- event 53 -------------------------------------------------------------------------------
    std::printf("-- one event 53\n");
    lEvents.Clear();
    PostSpin(lEvents);
    lModule.ProcessGameEventsShowtimeBounceBringUp(&lEvents, &lActions);
    laPosted = Drain(lActions);
    Check(laPosted.size() == 1 && laPosted[0].miType == 145 && laPosted[0].miSize == 1,
          "event 53 posts exactly action 145, size 1 (li r5, 0x91 ; li r6, 1)");
    Check(gaBounceCalls.size() == 2, "event 53 does not call DealWithPlayerBounced");

    // ---- nothing else is relayed ----------------------------------------------------------------
    std::printf("-- other events\n");
    lEvents.Clear();
    PostOther(lEvents, GameStateModuleIO::E_EVENT_VEHICLE_IMPACT, 12);   // 31, the neighbouring arm's
    PostOther(lEvents, 51, 4);
    PostOther(lEvents, 54, 8);
    lModule.ProcessGameEventsShowtimeBounceBringUp(&lEvents, &lActions);
    laPosted = Drain(lActions);
    Check(laPosted.empty() && gaBounceCalls.size() == 2, "events 31 / 51 / 54 post nothing through these two arms");

    // ---- one walk, the queue's order ------------------------------------------------------------
    std::printf("-- one walk over 52, 53, 31, 52\n");
    lEvents.Clear();
    Post(lEvents, lBounceA);
    PostSpin(lEvents);
    PostOther(lEvents, GameStateModuleIO::E_EVENT_VEHICLE_IMPACT, 12);
    Post(lEvents, lBounceB);
    lModule.ProcessGameEventsShowtimeBounceBringUp(&lEvents, &lActions);
    laPosted = Drain(lActions);
    Check(laPosted.size() == 3 && laPosted[0].miType == 144 && laPosted[1].miType == 145 && laPosted[2].miType == 144,
          "the walk relays in queue order: 144, 145, 144 (one walk, one arm per case)");
    if (laPosted.size() == 3)
    {
        Check(Word(laPosted[0], 0x10) == 2 && Word(laPosted[2], 0x10) == 5,
              "each 144 carries its own event's chain (2, then 5)");
    }
    Check(gaBounceCalls.size() == 4 && gaBounceCalls[2].miActionsAlreadyPosted == 1
              && gaBounceCalls[3].miActionsAlreadyPosted == 3,
          "DealWithPlayerBounced follows each 144 in the walk (1 posted, then 3)");
    {
        const CgsModule::Event* lpFirst = nullptr;
        s32 liFirstSize = 0;
        const s32 liFirstType = lEvents.GetFirstEvent(&lpFirst, &liFirstSize);
        Check(lpFirst != nullptr && liFirstType == GameStateModuleIO::E_EVENT_JUST_BOUNCED && liFirstSize == 32,
              "the arms do not Clear the event queue (PreWorldUpdateStuntBringUp owns the Clear)");
    }

    Check(gAsserts == 0, "no assert fired");

    std::printf("FxShowtime2Bounce: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
