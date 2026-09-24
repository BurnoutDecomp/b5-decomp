// FX-RCEM4 (crash-parity 2026-09-24): G68-D11 arms 4 and 74 of RaceCarEntityModule::HandleGameActions
// (ARTIST 0x8230BE08), RaceCarEntityModule::HandleSetPlayerOpponentsAction (0x822E96D8) and the whole of
// RaceCarEntityModule::UpdateStreaming (0x822FEFE0, for its junkyard-exit audio-wait leg), extracted
// VERBATIM from BrnRaceCarEntityModule.cpp by run_fxrcem4_game_actions.py and replayed against fixtures.
// A missing arm is replayed as the console's `default: break;` and a missing handler as an empty body,
// so the pre-fix source reports per-check failures instead of failing to build.
//   case 4   0x8230C700  HandleSetPlayerOpponentsAction(record): r20 = 7; per opponent i (signed
//                        `cmpw` against the count word +0x38, whose -1 fires the CgsArray.h:336 assert):
//                        SetDesiredVehicleData(r20--, id, wheel list entry(FindWheelIndexFromName(
//                        vehicle entry +0x10), -1 -> 0)->mID, -1)
//   case 74  0x8230C454  on byte 0 != 0: asserts :6762/:6765/:6767; streamer +0x17844 = 1; module
//                        +0x186D1 = 1; the "VEH_" tripwire :6778; HACKGetValidModelIds(model, wheel);
//                        audio RemoveEntry(player) then AddEntry(valid model, player, true);
//                        +0x18348 mbWaitingForStreaming = 1 (tail-merged with case 192)
//   UpdateStreaming 0x822FEFE0: `if (+0x186D1) { if (+0x17844) all-loaded = 0; else +0x186D1 = 0; }`
//                        between the car-select prefetch sweep and the streaming-complete edge
#include "types.hpp"
#include "GameSource/BurnoutConstants.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Core/CgsID.h"
#include "GameShared/GameClasses/Containers/CgsArray.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

static unsigned guAssertions = 0;
static std::vector<std::string> gaAssertMessages;
namespace CgsDev {
namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char* lpcMessage, const char*, int) {
    ++guAssertions; gaAssertMessages.push_back(lpcMessage ? lpcMessage : "");
    std::fprintf(stderr, "ASSERT: %s\n", lpcMessage); return 0;
}
void* EndAssert() { return nullptr; }
}
namespace Log { DebugPrint* gpDebugPrint = nullptr; void WriteToLog(const char*) {} }
namespace Message { u64 gxMessageFilterFlags = 0; }
}
namespace CgsModule { struct Event; }

namespace Fixture {

// ---- CgsIDUnCompress: a name table instead of the base-40 codec (the arm only strstr's the text) ----
struct NamedId { CgsID mId; const char* mpcName; };
static std::vector<NamedId> gaNames;
void CgsIDUnCompress(CgsID lId, char* lpcOut) {
    for (const NamedId& lrName : gaNames)
        if (lrName.mId == lId) { std::strncpy(lpcOut, lrName.mpcName, KI_CGSID_STRING_LEN - 1); lpcOut[KI_CGSID_STRING_LEN - 1] = 0; return; }
    std::strcpy(lpcOut, "UNKNOWN");
}

namespace BrnResource {
struct VehicleListEntry {
    CgsID mId; char macDefaultWheelName[32];
    const char* GetDefaultWheelName() const { return macDefaultWheelName; }
};
struct VehicleList {
    std::vector<VehicleListEntry> maEntries;
    s32 GetVehicleIndex(CgsID lId) const {
        for (size_t i = 0; i < maEntries.size(); ++i) if (maEntries[i].mId == lId) return static_cast<s32>(i);
        return -1;
    }
    const VehicleListEntry* GetVehicleData(s32 liIndex) const { return &maEntries[static_cast<size_t>(liIndex)]; }
};
struct WheelListEntry { CgsID mID; char macName[64]; };
struct WheelList {
    std::vector<WheelListEntry> maEntries;
    s32 FindWheelIndexFromName(const char* lpcName) const {
        for (size_t i = 0; i < maEntries.size(); ++i) if (std::strcmp(maEntries[i].macName, lpcName) == 0) return static_cast<s32>(i);
        return -1;
    }
    const WheelListEntry* GetWheelData(s32 liIndex) const { return &maEntries[static_cast<size_t>(liIndex)]; }
};
}

namespace RaceCarEntityModuleIO {
struct InputBuffer_PreScene {};
struct OutputBuffer_PreScene { void* GetVehicleInputInterface() { return this; } };
}

// HACKGetValidModelIds' real body prefixes "VEH_"/"WHE_" and re-compresses; the fixture marks the
// ids with recognisable high bits so the test can prove the VALIDATED ids reach AddEntry.
const CgsID KU_VALID_MODEL_MARK = 0x1000000000000000ull;
const CgsID KU_VALID_WHEEL_MARK = 0x2000000000000000ull;

struct AudioStreamer {
    std::vector<std::string> maCalls;
    s32 miRemovedSlot = -99; CgsID mAddedId = 0; u64 muAddedUser = 99; bool mbAddedIsPlayer = false;
    void RemoveEntry(s32 liSlot) { maCalls.push_back("remove"); miRemovedSlot = liSlot; }
    bool AddEntry(CgsID lId, u64 luUser, bool lbIsPlayer) {
        maCalls.push_back("add"); mAddedId = lId; muAddedUser = luUser; mbAddedIsPlayer = lbIsPlayer; return true;
    }
};
struct DesiredCall { s32 miSlot; CgsID mModelId; CgsID mWheelId; s32 miPriority; };
struct RaceCarStreamer {
    std::vector<DesiredCall> maDesired;
    bool mbHACK_WaitingForAudioAfterCarSelect = false;
    int  miValidCalls = 0; CgsID mValidModelIn = 0, mValidWheelIn = 0;
    int  miUpdates = 0; f32 mfUpdateStep = -1.0f;
    bool mbDesiredLoadedForCarSelect = true;
    AudioStreamer mAudio;
    void SetDesiredVehicleData(s32 liSlot, CgsID lModel, CgsID lWheel, s32 liPriority) { maDesired.push_back({ liSlot, lModel, lWheel, liPriority }); }
    bool HACK_IsWaitingForAudioAfterCarSelect() const { return mbHACK_WaitingForAudioAfterCarSelect; }
    void HACK_SetWaitingForAudioAfterCarSelect(bool lb) { mbHACK_WaitingForAudioAfterCarSelect = lb; }
    void HACKGetValidModelIds(CgsID& lrModel, CgsID& lrWheel) {
        ++miValidCalls; mValidModelIn = lrModel; mValidWheelIn = lrWheel; lrModel |= KU_VALID_MODEL_MARK; lrWheel |= KU_VALID_WHEEL_MARK;
    }
    AudioStreamer* GetAudioCarStreamer() { return &mAudio; }
    void Update(const RaceCarEntityModuleIO::InputBuffer_PreScene*, RaceCarEntityModuleIO::OutputBuffer_PreScene*, f32 lfStep) { ++miUpdates; mfUpdateStep = lfStep; }
    bool IsRaceCarLoadedForStateMachineBringUp(s32) const { return true; }
    bool IsDesiredRaceCarLoadedForCarSelect(s32) const { return mbDesiredLoadedForCarSelect; }
};

struct RaceCar {
    CgsID mModelId = 0, mWheelModelId = 0;
    CgsID GetModelId() const { return mModelId; }
    CgsID GetWheelModelId() const { return mWheelModelId; }
};
struct ActiveRaceCar {
    enum ERaceStartState : s32 { E_RACE_START_STATE_ON_START_LINE = 0, E_RACE_START_STATE_RACING = 2 };
    RaceCar mRaceCar;
    bool mbWaitingForLoad = false, mbIsPlayer = false;
    RaceCar* GetGlobalRaceCar() { return &mRaceCar; }
    bool IsWaitingForLoad() const { return mbWaitingForLoad; }
    bool IsPlayer() const { return mbIsPlayer; }
    bool IsOnRaceStartState(ERaceStartState) const { return false; }
};

// The [DIAG] gate UpdateStreaming's witness reads (armed: the witness must not change behaviour).
bool GameActionDiagEnabled() { return true; }

struct RaceCarEntityModule {
    ActiveRaceCar       maActiveRaceCars[E_ACTIVE_RACE_CAR_INDEX_COUNT];
    EActiveRaceCarIndex mePlayerActiveRaceCarIndex = E_ACTIVE_RACE_CAR_INDEX_3;
    RaceCarStreamer     mRaceCarStreamer;
    const BrnResource::VehicleList* mpVehicleList = nullptr;
    const BrnResource::WheelList*   mpWheelList = nullptr;
    f32  mfTimeStep = 0.0333f;
    bool mbWaitingForStreaming = false, mbSendStreamingComplete = false, mbInCarSelectScreen = false;
    bool mabCarSelectWaitForStreaming[E_ACTIVE_RACE_CAR_INDEX_COUNT] = {};
    bool mbHACK_ExitingCarSelectWaitForAudio = false;
    int  miResourcesLoaded = 0;
    struct SetPlayerOpponentsActionRecord;
    void HandleSetPlayerOpponentsAction(const SetPlayerOpponentsActionRecord* lpAction);
    ActiveRaceCar* GetActiveRaceCar(EActiveRaceCarIndex leIndex) {
        CGS_ASSERT(leIndex >= 0 && leIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT, "active index");
        return &maActiveRaceCars[leIndex];
    }
    void OnRaceCarResourcesLoaded(EActiveRaceCarIndex, void*) { ++miResourcesLoaded; }
    void UpdateStreaming(const RaceCarEntityModuleIO::InputBuffer_PreScene* lpInput,
                         RaceCarEntityModuleIO::OutputBuffer_PreScene* lpOutput);
    void Dispatch(s32 liType, const CgsModule::Event* lpEvent, RaceCarEntityModuleIO::OutputBuffer_PreScene* lpOutput);
};
#include "fxrcem4_game_actions.inc"
}   // namespace Fixture

static int giChecks = 0, giFailures = 0;
static void Check(bool lbPass, const char* lpcName) {
    ++giChecks;
    if (!lbPass) { ++giFailures; std::fprintf(stderr, "FAIL: %s\n", lpcName); }
}

typedef Array<CgsID, 7u> OpponentArray;   // the 64-byte wire record of action 4 (DWARF Array<CgsID,7u>)
static_assert(sizeof(OpponentArray) == 64, "action 4 is posted with size 64 (li r6, 0x40)");

static Fixture::BrnResource::VehicleList MakeVehicles() {
    Fixture::BrnResource::VehicleList lList;
    lList.maEntries.push_back({ 0x1111, "WHEELS_A" });
    lList.maEntries.push_back({ 0x2222, "WHEELS_B" });
    lList.maEntries.push_back({ 0x3333, "WHEELS_MISSING" });
    for (CgsID lId = 0x4001; lId <= 0x4007; ++lId) lList.maEntries.push_back({ lId, "WHEELS_B" });
    return lList;
}
static Fixture::BrnResource::WheelList MakeWheels() {
    Fixture::BrnResource::WheelList lList;
    lList.maEntries.push_back({ 0xAAA0, "WHEELS_DEFAULT" });
    lList.maEntries.push_back({ 0xAAA1, "WHEELS_A" });
    lList.maEntries.push_back({ 0xAAA2, "WHEELS_B" });
    return lList;
}

int main() {
    const auto AsEvent = [](const void* lp) { return reinterpret_cast<const CgsModule::Event*>(lp); };
    const Fixture::BrnResource::VehicleList lVehicles = MakeVehicles();
    const Fixture::BrnResource::WheelList lWheels = MakeWheels();
    Fixture::RaceCarEntityModuleIO::OutputBuffer_PreScene lOut;
    Fixture::RaceCarEntityModuleIO::InputBuffer_PreScene lIn;

    // ---- case 4: HandleSetPlayerOpponentsAction ------------------------------------------------------
    {
        OpponentArray laOpponents; laOpponents.Construct();
        laOpponents.Append(0x1111); laOpponents.Append(0x2222); laOpponents.Append(0x3333);
        Fixture::RaceCarEntityModule lModule; lModule.mpVehicleList = &lVehicles; lModule.mpWheelList = &lWheels;
        lModule.Dispatch(4, AsEvent(&laOpponents), &lOut);
        const std::vector<Fixture::DesiredCall>& lrCalls = lModule.mRaceCarStreamer.maDesired;
        Check(lrCalls.size() == 3, "G68-D11 4: one SetDesiredVehicleData per opponent (bl @0x822E98FC in the count loop)");
        if (lrCalls.size() == 3) {
            Check(lrCalls[0].miSlot == 7 && lrCalls[1].miSlot == 6 && lrCalls[2].miSlot == 5,
                  "G68-D11 4: streamer slots 7, 6, 5 (li r20, 7 ; addi r20, r20, -1)");
            Check(lrCalls[0].mModelId == 0x1111 && lrCalls[1].mModelId == 0x2222 && lrCalls[2].mModelId == 0x3333,
                  "G68-D11 4: the opponent ids, in array order (GetItem(i) ; ld r29, 0(r3))");
            Check(lrCalls[0].mWheelId == 0xAAA1 && lrCalls[1].mWheelId == 0xAAA2,
                  "G68-D11 4: wheel = the wheel-list entry named by the vehicle's default wheel (entry +0x10) -> its mID");
            Check(lrCalls[2].mWheelId == 0xAAA0,
                  "G68-D11 4: unknown wheel name -> FindWheelIndexFromName -1 -> wheel entry 0 (li r4, 0 @0x822E98D8)");
            Check(lrCalls[0].miPriority == -1 && lrCalls[1].miPriority == -1 && lrCalls[2].miPriority == -1,
                  "G68-D11 4: priority -1 (li r7, -1 @0x822E98E8)");
        }
        Check(!lModule.mbWaitingForStreaming, "G68-D11 4: the arm sets no streaming latch");

        OpponentArray laSeven; laSeven.Construct();
        for (CgsID lId = 0x4001; lId <= 0x4007; ++lId) laSeven.Append(lId);
        Fixture::RaceCarEntityModule lFull; lFull.mpVehicleList = &lVehicles; lFull.mpWheelList = &lWheels;
        lFull.Dispatch(4, AsEvent(&laSeven), &lOut);
        bool lbSlots = lFull.mRaceCarStreamer.maDesired.size() == 7;
        for (size_t i = 0; lbSlots && i < 7; ++i)
            lbSlots = lFull.mRaceCarStreamer.maDesired[i].miSlot == static_cast<s32>(7 - i)
                   && lFull.mRaceCarStreamer.maDesired[i].mModelId == 0x4001 + i;
        Check(lbSlots, "G68-D11 4: a full set of 7 fills slots 7..1 (the player's slot 0 is never touched)");

        OpponentArray laNone; laNone.Construct();
        Fixture::RaceCarEntityModule lEmpty; lEmpty.mpVehicleList = &lVehicles; lEmpty.mpWheelList = &lWheels;
        lEmpty.Dispatch(4, AsEvent(&laNone), &lOut);
        Check(lEmpty.mRaceCarStreamer.maDesired.empty(), "G68-D11 4: no opponents -> no request");

        const unsigned luBefore = guAssertions;
        OpponentArray laRaw; laRaw.MarkUnconstructed();
        Fixture::RaceCarEntityModule lRaw; lRaw.mpVehicleList = &lVehicles; lRaw.mpWheelList = &lWheels;
        lRaw.Dispatch(4, AsEvent(&laRaw), &lOut);
        Check(guAssertions == luBefore + 1 && lRaw.mRaceCarStreamer.maDesired.empty(),
              "G68-D11 4: an unconstructed array (count -1) fires the CgsArray.h:336 assert once and runs NO "
              "iteration (signed cmpw r26, r11 @0x822E9780)");
        guAssertions = luBefore;
    }

    // ---- case 74: the junkyard exit's audio wait ------------------------------------------------------
    const CgsID KU_MODEL = 0x5151, KU_WHEEL = 0x6161, KU_OLD_DESIRED = 0x7171;
    Fixture::gaNames.push_back({ KU_MODEL, "PRO_TAXI" });
    Fixture::gaNames.push_back({ 0x5252, "VEH_PRO_TAXI" });
    {
        Fixture::RaceCarEntityModule lModule;
        lModule.maActiveRaceCars[3].mRaceCar.mModelId = KU_MODEL; lModule.maActiveRaceCars[3].mRaceCar.mWheelModelId = KU_WHEEL;
        (void)KU_OLD_DESIRED;
        const bool lbNotWaiting = false;
        lModule.Dispatch(74, AsEvent(&lbNotWaiting), &lOut);
        Check(!lModule.mRaceCarStreamer.mbHACK_WaitingForAudioAfterCarSelect && !lModule.mbHACK_ExitingCarSelectWaitForAudio
                  && !lModule.mbWaitingForStreaming && lModule.mRaceCarStreamer.mAudio.maCalls.empty()
                  && lModule.mRaceCarStreamer.miValidCalls == 0,
              "G68-D11 74: byte 0 == 0 -> nothing (beq @0x8230C45C)");

        const bool lbWaiting = true;
        lModule.Dispatch(74, AsEvent(&lbWaiting), &lOut);
        Check(lModule.mRaceCarStreamer.mbHACK_WaitingForAudioAfterCarSelect,
              "G68-D11 74: streamer mbHACK_WaitingForAudioAfterCarSelect = 1 (stbx r23, +0x17844 @0x8230C504)");
        Check(lModule.mbHACK_ExitingCarSelectWaitForAudio,
              "G68-D11 74: module mbHACK_ExitingCarSelectWaitForAudio = 1 (stbx r23, +0x186D1 @0x8230C508)");
        Check(lModule.mRaceCarStreamer.miValidCalls == 1 && lModule.mRaceCarStreamer.mValidModelIn == KU_MODEL
                  && lModule.mRaceCarStreamer.mValidWheelIn == KU_WHEEL,
              "G68-D11 74: HACKGetValidModelIds(&car model id, &car wheel id) (@0x8230C56C)");
        const Fixture::AudioStreamer& lrAudio = lModule.mRaceCarStreamer.mAudio;
        Check(lrAudio.maCalls.size() == 2 && lrAudio.maCalls[0] == "remove" && lrAudio.maCalls[1] == "add",
              "G68-D11 74: audio RemoveEntry (@0x8230C59C) THEN AddEntry (@0x8230C5B4)");
        Check(lrAudio.miRemovedSlot == 3, "G68-D11 74: RemoveEntry on the player's slot (lwz r11, 0(r29) = +0x182F8)");
        Check(lrAudio.mAddedId == (KU_MODEL | Fixture::KU_VALID_MODEL_MARK) && lrAudio.muAddedUser == 3 && lrAudio.mbAddedIsPlayer,
              "G68-D11 74: AddEntry(the VALIDATED model id, player slot, true) (ld r4 var_2F8 ; extsw r5 ; li r6, 1)");
        Check(lModule.mbWaitingForStreaming, "G68-D11 74: mbWaitingForStreaming = 1 (stbx r23, +0x18348 @0x8230C5C0)");
        Check(guAssertions == 0, "G68-D11 74: a valid player car with a bare model id fires no assert");

        const unsigned luBefore = guAssertions;
        Fixture::RaceCarEntityModule lVeh;
        lVeh.maActiveRaceCars[3].mRaceCar.mModelId = 0x5252;
        lVeh.Dispatch(74, AsEvent(&lbWaiting), &lOut);
        Check(guAssertions == luBefore + 1 && lVeh.mbWaitingForStreaming && lVeh.mRaceCarStreamer.mAudio.maCalls.size() == 2,
              "G68-D11 74: a model id already carrying \"VEH_\" trips the :6778 assert once; the arm still completes");
        guAssertions = luBefore;
    }

    // ---- UpdateStreaming: the +0x186D1 audio-wait leg ---------------------------------------------------
    {
        Fixture::RaceCarEntityModule lModule;
        lModule.mbWaitingForStreaming = true;
        lModule.mbHACK_ExitingCarSelectWaitForAudio = true;
        lModule.mRaceCarStreamer.mbHACK_WaitingForAudioAfterCarSelect = true;
        lModule.UpdateStreaming(&lIn, &lOut);
        Check(lModule.mRaceCarStreamer.miUpdates == 1 && lModule.mRaceCarStreamer.mfUpdateStep == lModule.mfTimeStep,
              "UpdateStreaming: the streamer pump runs once with mfTimeStep (unchanged head)");
        Check(lModule.mbWaitingForStreaming && !lModule.mbSendStreamingComplete,
              "G68-D11 74 leg: latch up + streamer still waiting for audio -> all-loaded 0, NO streaming-complete edge");
        Check(lModule.mbHACK_ExitingCarSelectWaitForAudio, "G68-D11 74 leg: the latch holds while the streamer waits");

        lModule.mRaceCarStreamer.mbHACK_WaitingForAudioAfterCarSelect = false;
        lModule.UpdateStreaming(&lIn, &lOut);
        Check(!lModule.mbHACK_ExitingCarSelectWaitForAudio,
              "G68-D11 74 leg: streamer wait released -> the module latch clears itself (a1[100049] = 0)");
        Check(!lModule.mbWaitingForStreaming && lModule.mbSendStreamingComplete,
              "G68-D11 74 leg: ...and the streaming-complete edge fires in that same update");

        Fixture::RaceCarEntityModule lNoLatch;
        lNoLatch.mbWaitingForStreaming = true;
        lNoLatch.mRaceCarStreamer.mbHACK_WaitingForAudioAfterCarSelect = true;
        lNoLatch.UpdateStreaming(&lIn, &lOut);
        Check(!lNoLatch.mbWaitingForStreaming && lNoLatch.mbSendStreamingComplete,
              "G68-D11 74 leg: without the module latch the streamer flag alone does not hold the edge");

        Fixture::RaceCarEntityModule lSelect;
        lSelect.mbWaitingForStreaming = true; lSelect.mbInCarSelectScreen = true;
        lSelect.mabCarSelectWaitForStreaming[2] = true; lSelect.mRaceCarStreamer.mbDesiredLoadedForCarSelect = false;
        lSelect.UpdateStreaming(&lIn, &lOut);
        Check(lSelect.mbWaitingForStreaming && !lSelect.mbSendStreamingComplete && lSelect.mabCarSelectWaitForStreaming[2],
              "UpdateStreaming: the car-select prefetch wait still holds the edge (unchanged leg)");
    }

    // ---- end-to-end: 74 then the pump ---------------------------------------------------------------------
    {
        Fixture::RaceCarEntityModule lModule;
        lModule.maActiveRaceCars[3].mRaceCar.mModelId = KU_MODEL;
        const bool lbWaiting = true;
        lModule.Dispatch(74, AsEvent(&lbWaiting), &lOut);
        lModule.UpdateStreaming(&lIn, &lOut);
        Check(lModule.mbWaitingForStreaming && !lModule.mbSendStreamingComplete,
              "G68-D11 74 end-to-end: the junkyard exit's streaming-complete waits for the player's audio");
        lModule.mRaceCarStreamer.mbHACK_WaitingForAudioAfterCarSelect = false;   // the audio streamer's release
        lModule.UpdateStreaming(&lIn, &lOut);
        Check(!lModule.mbWaitingForStreaming && lModule.mbSendStreamingComplete && !lModule.mbHACK_ExitingCarSelectWaitForAudio,
              "G68-D11 74 end-to-end: released once the audio streamer drops its wait");
    }

    Check(guAssertions == 0, "valid fixtures fire no stray assertions");
    std::printf("FxRcem4GameActions: %d checks, %d failures\n", giChecks, giFailures);
    return giFailures ? 1 : 0;
}
