// Actual complete bridge body and IO/getter/queue/checkpoint implementations.
// The three existing translators are observed at their subsystem boundaries.
#include <cstdio>
#include <cstring>
#include <cmath>
#include <limits>
#include <vector>
#include "GameSource/Game/GameBridgeGameStateToX.h"
#include "GameSource/GameState/BrnGameStateSharedIO.h"
#include "GameSource/GameState/BrnGameStateModuleIO.h"
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"
#include "GameSource/Gui/BrnGuiDemangledEventTypes.h"
#include "GameSource/Gui/Events/BrnGuiEventOnlinePostEvent.h"
#include "GameShared/GameClasses/Gui/CgsGuiEventTypeDefs.h"
#include "GameShared/GameClasses/System/Timer/CgsTimerStatusInterface.h"
#include "GameShared/GameClasses/Module/CgsModuleUtils.h"

static unsigned checks, failures, assertions;
static void Check(bool ok, const char* message)
{
    ++checks;
    if (!ok) { ++failures; std::printf("FAIL %s\n", message); }
}
namespace CgsDev { namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char*, const char*, int) { ++assertions; return 0; }
void* EndAssert() { return nullptr; }
} namespace Log { DebugPrint* gpDebugPrint = nullptr; }
namespace Message { u64 gxMessageFilterFlags = 0; } }
using GSOutput = BrnGameState::GameStateModuleIO::OutputBuffer;
using GuiInput = CgsGui::CgsGuiModuleIO::InputBuffer;
static GSOutput* expectedGS;
static GuiInput* expectedGui;
static std::vector<unsigned> translators;
static void Translator(GuiInput* gui, unsigned id)
{
    translators.push_back(id);
    Check(gui == expectedGui && gui->IsBufferLockedForWriting() && expectedGS->IsBufferLockedForReading(),
          "original translator retains the complete source/destination lock bracket");
    const u32 payload = id;
    gui->GetGuiEvents()->AddEvent(reinterpret_cast<const CgsModule::Event*>(&payload), id, sizeof(payload));
}
namespace BrnGame {
namespace {
void TranslateGuiInterfaceToGuiEvents(GuiInput* gui,
    const BrnGameState::GameStateModuleIO::GameStateToGuiInterface* source)
{
    Check(source == static_cast<const GSOutput*>(expectedGS)->GetGameStateToGuiInterface(), "actual GUI interface reaches its translator");
    Translator(gui, 902);
}
}
class BrnGameModule
{
public:
    CgsSystem::TimerStatusInterface mTimerStatusInterface;
    void BridgeGameStateToGui(GuiInput*, const GSOutput*);
    void TranslateGameActionsToGuiEvents(GuiInput* gui, const GSOutput* source)
    { Check(source == expectedGS, "actual action output reaches its translator"); Translator(gui, 900); }
    void TranslateTakedownsToGuiEvents(GuiInput* gui,
        const CgsModule::BaseEventQueue<BrnGameState::TakedownEvent>* source, s32 player)
    { Check(source == reinterpret_cast<const CgsModule::BaseEventQueue<BrnGameState::TakedownEvent>*>(
          static_cast<const GSOutput*>(expectedGS)->GetTakedownEventOutputQueue()) && player == 3,
          "actual takedown queue and original player index reach translator"); Translator(gui, 901); }
};
}
#include "game_state_gui_bridge.inc"

struct EventView { s32 id, size; const CgsModule::Event* data; };
int main()
{
    using namespace BrnGameState::GameStateModuleIO;
    alignas(16) static unsigned char gsBytes[sizeof(GSOutput)], guiBytes[sizeof(GuiInput)];
    auto* gs = reinterpret_cast<GSOutput*>(gsBytes);
    auto* gui = reinterpret_cast<GuiInput*>(guiBytes);
    std::memset(gsBytes, 0, sizeof(gsBytes)); std::memset(guiBytes, 0, sizeof(guiBytes));
    gs->CgsModule::IOBuffer::Construct(); gui->CgsModule::IOBuffer::Construct();
    gs->mGuiEventQueue.Construct();
    gui->mInputQueue.CgsModule::VariableEventQueue<32768, 16>::Construct();
    const u64 token = UINT64_C(0x1234567887654321);
    gs->mGuiEventQueue.AddEvent(reinterpret_cast<const CgsModule::Event*>(&token), 777, sizeof(token));
    auto* scoring = reinterpret_cast<ScoringOutputInterface*>(gs->mScoringOutputInterfaceStorage);
    auto* online = reinterpret_cast<OnlineScoringOutputInterface*>(gs->mOnlineScoringOutputInterfaceStorage);
    scoring->mePlayerRaceCarIndex = E_ACTIVE_RACE_CAR_INDEX_3;
    scoring->miNumPlayersInGame = 3;
    scoring->mfModeTimeElapsed = 12.5f; scoring->mfModeTimeRemaining = -7.0f;
    scoring->mfCurrentTargetModeTime = 3.0f; scoring->mfDistanceDrivenInCurrentCar = 765.0f;
    scoring->miRoadRageNumTakedowns = 9; scoring->miRoadRageTakedownTarget = 12;
    scoring->miPursuitCarDamageLeft = 77; scoring->miShowtimeCarsCrashed = 11;
    scoring->miShowtimeComboMultiplier = 3; scoring->miShowtimeScoreMultiplier = 4;
    scoring->mfShowtimeDistanceTravelled = 66.25f;
    scoring->miCurrentScore = 123; scoring->miTargetScore = 456;
    scoring->miComboScore = 78; scoring->miComboMultiplier = 5;
    scoring->muCurrentStunts = 9; scoring->muAllStunts = 17;
    scoring->mfComboWarningTimeActive = 0.75f;
    scoring->mbComboWarningActive = true; scoring->mbComboInProgress = false;
    scoring->mbTimerActive = true;
    for (s32 team = 0; team < 9; ++team) scoring->maiTeamStuntScores[team] = 1000 + team;
    for (s32 car = 0; car < 8; ++car)
    {
        scoring->maCarScoreData[car].ClearData();
        auto& data = scoring->maCarScoreData[car];
        data.SetDistanceToFinishLive(42.0f + car); data.SetDistanceToNextCheckpointLive(10.0f + car);
        data.SetDistanceToFinish(24.0f + car); data.SetFinishTime(CgsSystem::Time(car + 1, 0.25f));
        data.SetOnlineStuntScore(300 + car); data.SetOnlineRacePoints(200 + car);
        data.SetOnlineFinishPosition(car - 1); data.SetNumEliminations(car + 2);
        data.SetTimedOut(car == 3); data.SetDisconnected(car == 6);
        data.muOnlinePostEventValueC0 = 0xFEDCBA00u + car;
        data.miCumulativeCheckpoints = 40 + car;
        scoring->maCarIds[car] = UINT64_C(0xA123456788765400) + car;
        scoring->maiNumRoadsRuled[car] = 20 + car;
        scoring->maiCumulativeScoreData[car] = 500 + car;
        scoring->mabValid[car] = car == 2 || car == 3 || car == 6;
        scoring->mabPlayerEliminated[car] = car == 6;
        online->maePlayerTeam[car] = static_cast<EPlayerTeam>(car == 2 ? 2 : 1);
        online->maOnlineAwards[car] = static_cast<BrnGameState::EOnlineAwardID>(car == 2 ? 3 : car == 6 ? 5 : -1);
        online->maiOnlineAwardVariables[car] = 400 + car;
        scoring->maCarCheckpointData[car].SetupCheckpoints(5);
        scoring->maCarCheckpointData[car].MarkCheckpointAsHit(1);
        scoring->maCarCheckpointData[car].MarkCheckpointAsHit(3);
    }
    std::memset(&gs->mSetUpAllEventStartsInterface, 0x53, sizeof(gs->mSetUpAllEventStartsInterface));
    std::memset(&gs->mSpecificGameModeEventInterface, 0x68, sizeof(gs->mSpecificGameModeEventInterface));
    gs->mbSetUpAllEventStartsInterfaceIsValid = true;
    gs->mbSpecificGameModeEventInterfaceIsValid = true;
    BrnGame::BrnGameModule game;
    game.mTimerStatusInterface.Clear();
    auto* timer = game.mTimerStatusInterface.GetGameTimerStatus();
    timer->miFrameCount = 77; timer->mfBaseTimeStep = 0.032f; timer->mfTimeStepMultiplier = 0.25f;
    timer->mTime.SetSeconds(123); timer->mTime.SetFraction(0.5f);
    expectedGS = gs; expectedGui = gui;
    const s32 modes[] = {-1, 2, 3, 4, 7, 9, 12, 13, 14, 15, 16, 17};
    for (s32 mode : modes)
    {
        scoring->meGameModeType = static_cast<EGameModeType>(mode);
        scoring->mbIsOnlineGameMode = mode >= 12;
        scoring->mfModeTimeRemaining = mode == 7 ? std::numeric_limits<f32>::quiet_NaN() : -7.0f;
        for (unsigned repeat = 0; repeat < 2; ++repeat)
        {
            gui->mInputQueue.CgsModule::VariableEventQueue<32768, 16>::Clear(); translators.clear();
            CgsModule::LockBuffersForIO(gui, gs);
            game.BridgeGameStateToGui(gui, gs);
            CgsModule::UnlockBuffersForIO(gui, gs);
            Check(translators == std::vector<unsigned>({900, 901, 902}), "original translator order follows complete status publication");
            std::vector<EventView> events;
            const CgsModule::Event* event = nullptr; s32 size = 0;
            s32 id = gui->mInputQueue.CgsModule::VariableEventQueue<32768, 16>::GetFirstEvent(&event, &size);
            while (event)
            {
                events.push_back({id, size, event}); const CgsModule::Event* next = nullptr;
                id = gui->mInputQueue.CgsModule::VariableEventQueue<32768, 16>::GetNextEvent(event, &next, &size); event = next;
            }
            auto take = [&](s32 wanted, s32 wantedSize) -> const CgsModule::Event* {
                for (const auto& item : events) if (item.id == wanted)
                { Check(item.size == wantedSize, "original event payload width"); return item.data; }
                Check(false, "original event is present"); return nullptr;
            };
            Check(!events.empty() && events.front().id == 777 && events.front().size == 8
                  && *reinterpret_cast<const u64*>(events.front().data) == token,
                  "append actual GS GUI queue before all bridge-generated records");
            auto* distance = reinterpret_cast<const BrnGui::GuiEventRaceDistanceRemaining*>(take(239, 144));
            if (distance) for (s32 car = 0; car < 8; ++car)
            {
                const bool valid = scoring->mabValid[car];
                const f32 expectedDistance = !valid ? 0.0f : (mode == 15 || mode == 16) ? 20.0f + car : 42.0f + car;
                Check(distance->maCarId[car] == (valid ? scoring->maCarIds[car] : 0)
                      && distance->mafDistanceToFinish[car] == expectedDistance
                      && distance->maiOnlineStuntScore[car] == (valid ? 300 + car : 0)
                      && distance->mabValid[car] == valid, "complete per-car distance/id/stunt/valid lanes");
            }
            auto* checkpoint = reinterpret_cast<const f32*>(take(240, 4));
            Check(checkpoint && *checkpoint == 13.0f, "publish actual player's next checkpoint distance");
            auto* status = reinterpret_cast<const BrnGui::GuiEventCurrentStatus*>(take(492, 120));
            Check(status && status->miGameTimeSeconds == 123 && status->mfGameTimeFraction == 0.5f
                  && status->mfGameTimeStep == 0.008f && status->mfDistanceToFinishLive == 45.0f
                  && status->mfDistanceDrivenInCurrentCar == 765.0f, "complete original timer/distance status fields");
            if (status) Check(mode != 13 ? status->miNumRemainingCheckpoints == 0
                : status->miNumRemainingCheckpoints == 3 && status->maiRemainingCheckpointIndexes[0] == 0
                  && status->maiRemainingCheckpointIndexes[1] == 2 && status->maiRemainingCheckpointIndexes[2] == 4,
                "only original online runner mode publishes the real remaining bitmap indices");
            std::vector<s32> order = {777, 239, 240, 492};
            if (mode != -1)
            {
                order.push_back(424);
                auto* score = reinterpret_cast<const BrnGui::GuiEventScoreUpdate*>(take(424, 20));
                const bool countdown = mode == 3 || mode == 7 || mode == 12 || mode == 14 || mode == 17;
                Check(score && score->mfModeTime == (countdown ? 0.0f : 12.5f)
                      && score->mfDistanceToNextCheckpoint == 13.0f, "original mode time selection, including fsel NaN to zero");
                if (mode == 2 || mode == 16) { order.push_back(434); take(434, 16); }
                if (mode == 3) { order.push_back(426); take(426, 8); }
                if (mode == 4) { order.push_back(432); const auto* p = reinterpret_cast<const s32*>(take(432, 4));
                    Check(p && *p == 77, "original Pursuit score arm is not omitted"); }
                if (mode == 7 || mode == 9 || mode == 12 || mode == 14 || mode == 17)
                { order.push_back(428); take(428, 40); }
            }
            if (mode >= 12)
            {
                order.push_back(318);
                const auto* result = reinterpret_cast<const BrnGui::GuiEventOnlinePostEvent*>(take(318, 568));
                Check(result && result->maTail[0] == 3 && result->maTail[1] == 3 && result->maTail[2] == 2,
                      "original online player/result/award counts");
                if (result)
                {
                    const s32 active[] = {2, 3, 6};
                    for (s32 row = 0; row < 3; ++row)
                    {
                        const s32 car = active[row]; const auto& r = result->maRecords[row];
                        Check(r.miIndex == car && r.mfValue04 == car + 1.25f && r.mTime.GetSeconds() == car + 1
                              && r.muValue14 == 300u + car && r.muValue18 == 200u + car
                              && r.muValue1C == static_cast<u32>(car - 1) && r.muValue20 == 500u + car
                              && r.muValue24 == static_cast<u32>(car + 2) && r.muValue28 == 0xFEDCBA00u + car
                              && r.muValue30 == 40u + car && r.mbFlag34 == (car == 3) && r.mbFlag35 == (car == 6),
                              "online results preserve actual packed per-car fields and all C0 bits");
                    }
                    Check(result->maIndexTriplets[0].miIndexA == 3 && result->maIndexTriplets[0].miIndexB == 2
                          && result->maIndexTriplets[0].muValue08 == 402
                          && result->maIndexTriplets[1].miIndexA == 5 && result->maIndexTriplets[1].miIndexB == 6,
                          "original award list packs only actual awarded players");
                }
            }
            order.insert(order.end(), {203, 194, 900, 901, 902, 26});
            std::vector<s32> actual; for (const auto& item : events) actual.push_back(item.id);
            Check(actual == order, "all original bridge outputs retain their exact producer order");
            const auto* starts = take(203, 8416); const auto* presets = take(194, 7704);
            Check(starts && !std::memcmp(starts, &gs->mSetUpAllEventStartsInterface, 8416),
                  "copy the complete actual event-start interface on every valid update");
            Check(presets && !std::memcmp(presets, &gs->mSpecificGameModeEventInterface, 7704),
                  "copy the complete actual preset interface");
            const auto* time = reinterpret_cast<const f32*>(take(26, 8));
            Check(time && time[0] == 0.008f && time[1] == 123.5f, "original GUI time record closes the bridge");
        }
    }
    Check(assertions == 0, "complete original bridge obeys the actual IO lock contracts");
    std::printf("GameStateGuiBridge: %u checks, %u failures\n", checks, failures);
    return failures ? 1 : 0;
}
