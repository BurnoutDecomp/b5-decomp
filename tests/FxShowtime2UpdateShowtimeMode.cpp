// FX-SHOWTIME2 (crash parity 2026-09-24): the PRODUCTION GameStateModule::UpdateShowtimeMode
// @0x82380EF8 (+ ToggleShowtimeBehaviour and the two file-scope constants), extracted from
// src/GameSource/GameState/GameStateModule_Showtime.cpp by run_fxshowtime2_update_showtime.py, driven
// frame by frame against:
//   * the REAL CrashModeScoring layout (BrnCrashModeScoringRecentCrash.h) with the PRODUCTION
//     DealWithScoreForVehicleClass / GetVehicleScoreData / GetRecentCrash / GetNumCarsCrashed /
//     GetScoreMultiplier bodies extracted from BrnCrashModeScoring.cpp,
//   * the REAL CgsContainers::Stack<u16,8>, CgsModule::EventQueue<TrafficTypeResponse,32> and
//     CgsModule::VariableEventQueue<13312,16> (the game-action queue the actions land on),
//   * the REAL action records (BrnGameActions.h), read back from the queue at the console offsets.
// The module itself is a stand-in carrying exactly the members the body names.
//
// Checked against the ARTIST asm (r30 = this, r25 = lpOutput, r26 = lpResponseQueue):
//   leg 1  0x82380F1C..0x82380F78  mbToggleShowtimeBehaviour -> (x+1)%3 stored to record+module,
//          AddEvent(138, 4), the flag cleared (`stb r29(=0)`)
//   leg 2  0x82380F84..0x82381084  index != 0xFFFF: first response whose `lhz 0` matches ->
//          DealWithScoreForVehicleClass(idx, lwz 4, ld 8, rec+4, rec+0xC, rec+0x10, rec+0x14, rec+0x1C);
//          rec+0x20 = idx (sth), rec+0x00 = class, rec+0x08 = the four-word sum, rec+0x18 = +0x2E4;
//          AddEvent(140, 0x24); index = 0xFFFF. Then `cmplwi 0xFFFF ; beq` else FireAssert
//          "muShowtimeRequestedTrafficIndex == K_INVALID_VEHICLE_INDEX" (:1611); index = 0xFFFF.
//   leg 3  0x82381098..0x82381128  count != 0 -> `addic. -1 ; stw ; bgt skip` -> Peek -> AddEvent(116, 2)
//          -> index = it -> Pop -> delay = 2
//   leg 4  0x8238112C..0x82381178  OnShowTimeMultiplier(lwz 0x20D4) every call
// and the scorer (DealWithScoreForVehicleClass @0x82338778 / GetVehicleScoreData @0x82312AB0):
//   chain bonus 1000 * count when the RecentCrash's count >= 2 (cmplwi 2 ; blt), base/mult/category
//   from the 24-row table @0x82020FA8 (else the class fallback: bus -> 3 / 5000 / 0),
//   miBaseScore += chain + base, miScoreMultiplier += mult, ++maiNumCarsCrashed[class].
#include "types.hpp"
#include "BrnCommonTypes.h"
#include "GameSource/GameState/BrnGameActions.h"                                   // the real records + ids
#include "GameSource/GameState/BrnGameStateTypes.h"                                // EShowtimeBehaviour
#include "GameSource/GameState/ModeManager/Scoring/BrnCrashModeScoringRecentCrash.h" // the real scorer
#include "GameSource/World/EntityModules/TrafficEntityModule/SharedIO/BrnTrafficTypeInterface.h"
#include "GameSource/Physics/ContactSpies/BrnContactSpyInterface.h"
#include "GameShared/GameClasses/Module/CgsEventQueue.h"
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"
#include "GameShared/GameClasses/Containers/CgsStack.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Core/CgsID.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

static unsigned gChecks = 0, gFailures = 0;
static unsigned gAsserts = 0, gIndexAsserts = 0;

namespace CgsDev
{
namespace Assert
{
    int   BeginAssert() { return 0; }
    int   FireAssert(const char* lpcExpression, const char*, int)
    {
        ++gAsserts;
        if (lpcExpression != 0 &&
            std::strcmp(lpcExpression, "muShowtimeRequestedTrafficIndex == K_INVALID_VEHICLE_INDEX") == 0)
        {
            ++gIndexAsserts;
        }
        return 0;
    }
    void* EndAssert() { return nullptr; }
}
namespace Log
{
    DebugPrint* gpDebugPrint = nullptr;   // the [showtime-score] witness stays silent
}
namespace Message
{
    u64 gxMessageFilterFlags = 0;         // the queue's overflow dump and the scorer's miss print stay silent
}
}

// GetVehicleScoreData's miss tail un-compresses the id for its log line; nothing here reads it.
void CgsIDUnCompress(CgsID, char* lpcString) { lpcString[0] = 0; }

// ---- the production scorer bodies ------------------------------------------------------------
namespace BrnGameState
{
#include "showtime_scoring_methods.inc"
}

// ---- the stand-in module ---------------------------------------------------------------------
namespace BrnGameState
{
namespace GameStateModuleIO
{
    struct PreWorldInputBuffer;

    struct OutputBuffer
    {
        GameActionQueue mGameActionQueue;
        s32             miQueueAccessCount = 0;

        GameActionQueue* GetGameActionQueue()
        {
            ++miQueueAccessCount;
            return &mGameActionQueue;
        }
    };
}

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

struct AchievementManagerStandIn
{
    std::vector<s32> maMultipliers;
    void OnShowTimeMultiplier(s32 liMultiplier) { maMultipliers.push_back(liMultiplier); }
};

class GameStateModule
{
public:
    bool                            mbToggleShowtimeBehaviour = false;
    EShowtimeBehaviour              meShowtimeBehaviour       = E_SHOWTIME_MODE_ON_SIXAXIS;
    CgsContainers::Stack<u16, 8>    mShowtimePendingTrafficIndexStack;
    s32                             miShowtimePendingFrameDelay = 1;       // ClearData's seed
    u16                             muShowtimeRequestedTrafficIndex = 0xFFFF;
    ModeManagerStandIn              mModeManager;
    AchievementManagerStandIn       mAchievementManager;

    void ToggleShowtimeBehaviour();
    void UpdateShowtimeMode(const GameStateModuleIO::PreWorldInputBuffer*       lpInput,
                            GameStateModuleIO::OutputBuffer*                    lpOutput,
                            const BrnPhysics::ContactSpy::ContactSpyInterface*  lpContacts,
                            const CgsModule::BaseEventQueue<BrnTraffic::BrnTrafficIO::TrafficTypeResponse>*
                                                                                lpResponseQueue);
};

namespace
{
#include "showtime_constants.inc"
}
#include "showtime_methods.inc"
}

using namespace BrnGameState;
typedef CgsModule::EventQueue<BrnTraffic::BrnTrafficIO::TrafficTypeResponse, 32> ResponseQueue;

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
        std::memset(&lAction, 0, sizeof(lAction));
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

static u16 Half(const PostedAction& lrAction, s32 liOffset)
{
    u16 luValue = 0;
    std::memcpy(&luValue, lrAction.maBytes + liOffset, sizeof(luValue));
    return luValue;
}

static BrnTraffic::BrnTrafficIO::TrafficTypeResponse Response(u16 luIndex, BrnTraffic::VehicleClass leClass, CgsID lId)
{
    BrnTraffic::BrnTrafficIO::TrafficTypeResponse lResponse;
    std::memset(&lResponse, 0, sizeof(lResponse));
    lResponse.muVehicleIndex = luIndex;
    lResponse.meType         = leClass;
    lResponse.mTypeId        = lId;
    return lResponse;
}

// The ClearData state the scorer is in at a showtime start (0x82320D10: multiplier 1, counts 0).
static void ResetScorer(CrashModeScoring& lrScorer)
{
    std::memset(&lrScorer, 0, sizeof(lrScorer));
    lrScorer.maRecentCrashes.Clear();
    lrScorer.miScoreMultiplier = 1;
}

int main()
{
    // Two table rows of GetVehicleScoreData @0x82312AB0 (rodata 0x82020FA8), and one id the table lacks.
    const CgsID KU_ID_TARGET = 0xBF2E42A8A7700000ULL;   // TARGETVEHICLE, 6000, multiplier 1
    const CgsID KU_ID_UNKNOWN = 0x0000000000001234ULL;  // -> the class fallback

    static CrashModeScoring lScorer;
    ResetScorer(lScorer);
    CrashModeScoring::RecentCrash lChain3 = { 30, 3, 1.0f };   // chain count 3 -> 3000
    CrashModeScoring::RecentCrash lChain1 = { 20, 1, 1.5f };   // chain count 1 -> no bonus
    lScorer.maRecentCrashes.Append(lChain3);
    lScorer.maRecentCrashes.Append(lChain1);

    static GameStateModule lModule;
    lModule.mModeManager.mScoringSystem.mpCrashScorer = &lScorer;
    lModule.mShowtimePendingTrafficIndexStack.Construct();
    lModule.mShowtimePendingTrafficIndexStack.Push(10);
    lModule.mShowtimePendingTrafficIndexStack.Push(20);
    lModule.mShowtimePendingTrafficIndexStack.Push(30);   // the top: the first pop

    static GameStateModuleIO::OutputBuffer lOutput;
    lOutput.mGameActionQueue.Construct();
    static ResponseQueue lResponses;
    lResponses.Construct();

    std::printf("-- the constants\n");
    Check(K_INVALID_VEHICLE_INDEX == 0xFFFF, "K_INVALID_VEHICLE_INDEX == 65535 (DWARF .cpp:154; ori 0xFFFF)");
    Check(KI_SHOWTIME_TRAFFIC_RESPONSE_FRAMES == 2, "KI_SHOWTIME_TRAFFIC_RESPONSE_FRAMES == 2 (DWARF .cpp:155; li r11, 2)");

    // ---- frame 1: nothing outstanding; the first pop (delay 1 -> 0) --------------------------
    std::printf("-- frame 1: pop\n");
    lModule.UpdateShowtimeMode(nullptr, &lOutput, nullptr, &lResponses);
    std::vector<PostedAction> laFrame = Drain(lOutput.mGameActionQueue);
    Check(laFrame.size() == 1 && laFrame[0].miType == 116 && laFrame[0].miSize == 2,
          "frame 1 posts exactly action 116, size 2 (li r5, 0x74 ; li r6, 2)");
    Check(laFrame.size() == 1 && Half(laFrame[0], 0) == 30,
          "the request carries the stack's TOP (Peek) -- 30");
    Check(lModule.muShowtimeRequestedTrafficIndex == 30, "the request is parked in muShowtimeRequestedTrafficIndex");
    Check(lModule.mShowtimePendingTrafficIndexStack.GetLength() == 2, "Pop: two left");
    Check(lModule.miShowtimePendingFrameDelay == 2, "the delay is re-seeded to 2 (li r11, 2)");

    // ---- frame 2: the answer arrives; delay 2 -> 1, no pop -----------------------------------
    std::printf("-- frame 2: answer -> score -> 140\n");
    lResponses.Clear();
    lResponses.AddEvent(Response(7,  BrnTraffic::E_VEHICLECLASS_CAR, 0));             // not ours
    lResponses.AddEvent(Response(30, BrnTraffic::E_VEHICLECLASS_VAN, KU_ID_TARGET));  // ours
    lModule.UpdateShowtimeMode(nullptr, &lOutput, nullptr, &lResponses);
    laFrame = Drain(lOutput.mGameActionQueue);
    Check(laFrame.size() == 1 && laFrame[0].miType == 140 && laFrame[0].miSize == 36,
          "frame 2 posts exactly action 140, size 36 (li r5, 0x8C ; li r6, 0x24) and no 116");
    if (laFrame.size() == 1 && laFrame[0].miType == 140)
    {
        const PostedAction& lr = laFrame[0];
        Check(Word(lr, 0x00) == BrnTraffic::E_VEHICLECLASS_VAN, "rec+0x00 = the response's class (lwz 4(r31)) -- VAN");
        Check(Word(lr, 0x04) == 1, "rec+0x04 = maiNumCarsCrashed[VAN] after the ++ (lpiVehicleTypeCrashed)");
        Check(Word(lr, 0x08) == 1, "rec+0x08 = the four-word sum (GetNumCarsCrashed)");
        Check(Word(lr, 0x0C) == 6000, "rec+0x0C = the table's base score for the target vehicle");
        Check(Word(lr, 0x10) == BrnTraffic::E_VEHICLESCORE_TARGETVEHICLE, "rec+0x10 = the table's category (TARGETVEHICLE)");
        Check(Word(lr, 0x14) == 1, "rec+0x14 = the multiplier earned (1)");
        Check(Word(lr, 0x18) == 2, "rec+0x18 = miScoreMultiplier AFTER the += (1 + 1)");
        Check(Word(lr, 0x1C) == 3000, "rec+0x1C = the chain bonus, 1000 * the RecentCrash's count 3");
        Check(Half(lr, 0x20) == 30, "rec+0x20 = the traffic index (sth)");
    }
    Check(lScorer.miBaseScore == 9000, "miBaseScore += chain + base (3000 + 6000)");
    Check(lScorer.maiNumCarsCrashed[BrnTraffic::E_VEHICLECLASS_VAN] == 1 && lScorer.GetNumCarsCrashed() == 1,
          "maiNumCarsCrashed[VAN] moved -- 'Cars Crashed' is 1");
    Check(lModule.muShowtimeRequestedTrafficIndex == 0xFFFF, "the answered index is invalidated");
    Check(lModule.miShowtimePendingFrameDelay == 1 && lModule.mShowtimePendingTrafficIndexStack.GetLength() == 2,
          "frame 2 only counts the delay down (2 -> 1), no pop");
    Check(gIndexAsserts == 0, "an answered request fires no :1611 assert");

    // ---- frame 3: the second pop -------------------------------------------------------------
    std::printf("-- frame 3: pop\n");
    lResponses.Clear();
    lModule.UpdateShowtimeMode(nullptr, &lOutput, nullptr, &lResponses);
    laFrame = Drain(lOutput.mGameActionQueue);
    Check(laFrame.size() == 1 && laFrame[0].miType == 116 && Half(laFrame[0], 0) == 20,
          "frame 3 posts 116 for the next victim (20): one pop every second frame");

    // ---- frame 4: the fallback-class answer -----------------------------------------------------
    std::printf("-- frame 4: answer (class fallback)\n");
    lResponses.AddEvent(Response(20, BrnTraffic::E_VEHICLECLASS_BUS, KU_ID_UNKNOWN));
    lModule.UpdateShowtimeMode(nullptr, &lOutput, nullptr, &lResponses);
    laFrame = Drain(lOutput.mGameActionQueue);
    Check(laFrame.size() == 1 && laFrame[0].miType == 140, "frame 4 posts 140");
    if (laFrame.size() == 1 && laFrame[0].miType == 140)
    {
        const PostedAction& lr = laFrame[0];
        Check(Word(lr, 0x00) == BrnTraffic::E_VEHICLECLASS_BUS && Word(lr, 0x04) == 1,
              "BUS, class tally 1");
        Check(Word(lr, 0x08) == 2, "the sum over all classes is now 2");
        Check(Word(lr, 0x0C) == 5000 && Word(lr, 0x10) == BrnTraffic::E_VEHICLESCORE_BUS && Word(lr, 0x14) == 0,
              "unknown type id -> the class fallback (bus: category 3, 5000, x0)");
        Check(Word(lr, 0x18) == 2, "the multiplier total stays 2");
        Check(Word(lr, 0x1C) == 0, "chain count 1 (< 2) earns no bonus (cmplwi 2 ; blt)");
        Check(Half(lr, 0x20) == 20, "rec+0x20 = 20");
    }

    // ---- frame 5: the last pop; frame 6: NO answer -> the console's assert ---------------------
    std::printf("-- frames 5/6: pop, then a missing answer\n");
    lResponses.Clear();
    lModule.UpdateShowtimeMode(nullptr, &lOutput, nullptr, &lResponses);
    laFrame = Drain(lOutput.mGameActionQueue);
    Check(laFrame.size() == 1 && laFrame[0].miType == 116 && Half(laFrame[0], 0) == 10, "frame 5 pops 10");
    lResponses.AddEvent(Response(11, BrnTraffic::E_VEHICLECLASS_CAR, 0));    // someone else's answer
    lModule.UpdateShowtimeMode(nullptr, &lOutput, nullptr, &lResponses);
    laFrame = Drain(lOutput.mGameActionQueue);
    Check(laFrame.empty(), "frame 6 posts nothing: no match, and the stack is empty");
    Check(gIndexAsserts == 1, "the miss fires \"muShowtimeRequestedTrafficIndex == K_INVALID_VEHICLE_INDEX\" once (:1611)");
    Check(lModule.muShowtimeRequestedTrafficIndex == 0xFFFF, "and the index is invalidated anyway (no retry)");
    Check(lScorer.GetNumCarsCrashed() == 2, "a missed answer scores nothing");
    Check(lModule.miShowtimePendingFrameDelay == 2, "an EMPTY stack does not count the delay down");

    // ---- leg 4: the achievement tail ran every frame, with the scorer's live multiplier --------
    const std::vector<s32>& laMult = lModule.mAchievementManager.maMultipliers;
    Check(laMult.size() == 6, "OnShowTimeMultiplier runs on every call (6 frames, 6 calls)");
    Check(laMult.size() == 6 && laMult[0] == 1 && laMult[1] == 2 && laMult[5] == 2,
          "...with miScoreMultiplier (lwz 0x20D4) as it stands after the frame's scoring");

    // ---- leg 1: the toggle, and the console's in-frame order ------------------------------------
    std::printf("-- the toggle and the leg order\n");
    static GameStateModule lToggle;
    lToggle.mModeManager.mScoringSystem.mpCrashScorer = &lScorer;
    lToggle.mShowtimePendingTrafficIndexStack.Construct();
    lToggle.ToggleShowtimeBehaviour();
    Check(lToggle.mbToggleShowtimeBehaviour, "ToggleShowtimeBehaviour sets the byte (*(module + 284512) = 1)");
    lToggle.UpdateShowtimeMode(nullptr, &lOutput, nullptr, &lResponses);
    laFrame = Drain(lOutput.mGameActionQueue);
    Check(laFrame.size() == 1 && laFrame[0].miType == 138 && laFrame[0].miSize == 4 && Word(laFrame[0], 0) == 0,
          "ON_SIXAXIS (2) -> (2+1)%3 == OFF (0), posted as action 138 size 4");
    Check(lToggle.meShowtimeBehaviour == E_SHOWTIME_MODE_OFF && !lToggle.mbToggleShowtimeBehaviour,
          "the module holds the new behaviour and the byte is cleared");
    lToggle.ToggleShowtimeBehaviour();
    lToggle.UpdateShowtimeMode(nullptr, &lOutput, nullptr, &lResponses);
    laFrame = Drain(lOutput.mGameActionQueue);
    Check(laFrame.size() == 1 && Word(laFrame[0], 0) == E_SHOWTIME_MODE_ON, "OFF (0) -> ON (1)");

    // All three legs in one frame: toggle, an answer, and a due pop -> 138, 140, 116 in that order.
    lToggle.ToggleShowtimeBehaviour();
    lToggle.muShowtimeRequestedTrafficIndex = 30;
    lToggle.miShowtimePendingFrameDelay     = 1;
    lToggle.mShowtimePendingTrafficIndexStack.Push(44);
    lResponses.Clear();
    lResponses.AddEvent(Response(30, BrnTraffic::E_VEHICLECLASS_CAR, KU_ID_UNKNOWN));
    lToggle.UpdateShowtimeMode(nullptr, &lOutput, nullptr, &lResponses);
    laFrame = Drain(lOutput.mGameActionQueue);
    Check(laFrame.size() == 3 && laFrame[0].miType == 138 && laFrame[1].miType == 140 && laFrame[2].miType == 116,
          "one frame with all three legs posts 138, then 140, then 116 (the console's leg order)");
    Check(lOutput.miQueueAccessCount > 0, "every post goes through OutputBuffer::GetGameActionQueue (0x8231D4B8)");

    Check(gAsserts == gIndexAsserts, "no other assert fired");

    std::printf("FxShowtime2UpdateShowtimeMode: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
