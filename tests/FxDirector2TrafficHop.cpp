// FX-DIRECTOR2 (crash parity 2026-09-25): THE WORLD -> DIRECTOR TRAFFIC HOP AND THE TEAM WORDS.
//
// Four production pieces, extracted by run_fxdirector2_traffic_hop.py from the revision under test and run
// against the REVISION's real DirectorIO::InputBuffer (header shadowed, its TU linked) and its real
// AllVehicleData (header shadowed):
//   traffic_hop_step4.inc      BrnGameModule::BridgeWorldToDirector step 4 (0x823E3F5C..0x823E3F78): the world
//                              output's TrafficDirectorOutputInterface (0x823B6158) copied into the input at +0x6AC0
//                              (`lhz 0 ; sth 0x6AC0` + the Array copy helper sub_823B2368 into +0x6AD0).
//   traffic_hop_team_word.inc  BridgeGameStateToDirector 0x823CD428..0x823CD454: gameStateOut + 0x2AFC8 + 4*player
//   traffic_hop_team_loop.inc  (OnlineScoringOutputInterface @+0x2AF68 ::maePlayerTeam @+0x60) -> `stw 0x7AB0`; and
//                              0x823CD514..0x823CD56C: SetVehicleTeam @0x823B2938 (i, maePlayerTeam[i]) for i = 0..7.
//   traffic_hop_presq.inc      MainDirector::PreSceneQueryUpdate's first block (0x8225BC28..0x8225BC70): the five
//                              arguments of AllVehicleData::Update @0x8221D938 out of the input -- the used bits,
//                              the VehicleInfo[8] @+0x990, the live index, input + 0x6AC0 + 0x10 (the traffic array)
//                              and GetVehicleInfoArray() 0x82206EF0 (the team words @+0x3238).
// plus InputBuffer::Construct @0x822393D0 (0x82239444 `stw 0, 0x78D0`; 0x82239530..0x82239560 the team words;
// 0x82239520 `stw r30, 0x7AB0`) from the linked TU, and the AllVehicleData rows the console builds from them
// (`lwzx r10, 4 * car, lpaVehicleTeams` @0x8221DEE4). A revision without a piece compiles an empty block (or, for
// the API, a detection fallback), so it FAILS on behaviour rather than failing to build.
#include "types.hpp"
#include "BrnCommonTypes.h"
#include "GameSource/BurnoutConstants.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/SharedIO/BrnTrafficDirectorInterfaces.h"
#include "GameSource/Director/DirectorModule/BrnDirectorModuleIO.h"      // the REVISION's InputBuffer
#include "GameSource/Director/Utils/BrnDirectorAllVehicleData.h"         // the REVISION's AllVehicleData
#include "GameSource/Director/Camera/Utils/CameraUtils.h"
#include "GameSource/GameState/BrnGameStateSharedIO.h"                   // Scoring / OnlineScoring interfaces
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include <cfloat>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <type_traits>
#include <utility>

static unsigned gAsserts = 0, gChecks = 0, gFailures = 0;
static char     gacLastAssert[192] = { 0 };

namespace CgsDev
{
namespace Assert
{
    int   BeginAssert() { return 0; }
    int   FireAssert(const char* lpcText, const char*, int)
    {
        ++gAsserts;
        std::snprintf(gacLastAssert, sizeof(gacLastAssert), "%s", lpcText != 0 ? lpcText : "");
        std::fprintf(stderr, "assert: %s\n", lpcText != 0 ? lpcText : "");
        return 0;
    }
    void* EndAssert() { return nullptr; }
}
namespace Log
{
    DebugPrint* gpDebugPrint = nullptr;   // the InputBuffer TU's [takedown-cam] witness stays silent
}
namespace Message
{
    u64 gxMessageFilterFlags = 0;
}
}

// The traffic interface's const accessor (DWARF BrnTrafficDirectorInterfaces.h:98), extracted from the revision's
// BrnTrafficEntityModuleIO.cpp -- a build dependency of the extracted PreSceneQueryUpdate block, not under test.
namespace BrnTraffic
{
namespace BrnTrafficIO
{
#include "traffic_hop_accessor.inc"
}
}

// The vehicle input interface's queue bring-up lives in a TU that drags in AttribSys; Construct only needs to link.
namespace BrnDirector
{
    void BrnDirectorVehicleInputInterface::Construct() {}
}

// UpdatePlayerSpaces' look-at (CameraUtils.cpp): the player spaces are not what this test measures.
namespace BrnDirector { namespace Camera { namespace Utils {
    Matrix44Affine CreateLookAt(Vector3 lEyePosition, Vector3 lTargetPosition)
    {
        (void)lTargetPosition;
        Matrix44Affine lLookAt;
        lLookAt.SetIdentity();
        lLookAt.wAxis = lEyePosition;
        return lLookAt;
    }
} } }

using BrnDirector::AllVehicleData;
using BrnDirector::DirectorIO::InputBuffer;
using BrnTraffic::BrnTrafficIO::TrafficDirectorEntity;
using BrnTraffic::BrnTrafficIO::TrafficDirectorOutputInterface;
typedef Array<TrafficDirectorEntity, 32u>                          TrafficArray;
typedef BrnGameState::GameStateModuleIO::ScoringOutputInterface       ScoringOutputInterface;
typedef BrnGameState::GameStateModuleIO::OnlineScoringOutputInterface OnlineScoringOutputInterface;

// ---- the console's director-input offsets (identical on the PC below the hook-enumeration widening) ----
static const size_t KU_RACE_CARS         = 0x0990;
static const size_t KU_TEAMS             = 0x3238;
static const size_t KU_TRAFFIC           = 0x6AC0;
static const size_t KU_TRAFFIC_ARRAY     = 0x6AD0;
static const size_t KU_TRAFFIC_COUNT     = 0x78D0;
static const size_t KU_PLAYER_TEAM       = 0x7AB0 + InputBuffer::KU_HOOK_ENUMERATION_WIDENING;

alignas(16) static unsigned char gaInputStorage[sizeof(InputBuffer)];
static InputBuffer& Input() { return *reinterpret_cast<InputBuffer*>(gaInputStorage); }
template <typename T> static T Word(size_t luOffset) { T lValue; std::memcpy(&lValue, gaInputStorage + luOffset, sizeof(T)); return lValue; }

// ---- the world output: only the accessor step 4 reads (UpdateOutputBuffer::GetTrafficDirectorOutputInterface) ----
struct WorldOutputStandIn
{
    alignas(16) unsigned char maStorage[sizeof(TrafficDirectorOutputInterface)];
    mutable int miReads;
    TrafficDirectorOutputInterface& Traffic() { return *reinterpret_cast<TrafficDirectorOutputInterface*>(maStorage); }
    const TrafficDirectorOutputInterface* GetTrafficDirectorOutputInterface() const
    {
        ++miReads;
        return reinterpret_cast<const TrafficDirectorOutputInterface*>(maStorage);
    }
};

// Step 4, under its production parameter names.
static void Step4(InputBuffer* lpDirectorInput, const WorldOutputStandIn* lpWorldOutput)
{
    (void)lpDirectorInput;
    (void)lpWorldOutput;
#include "traffic_hop_step4.inc"
}

// ---- the game-state output: the two scoring snapshots the team leg reads ----
struct GameStateOutputStandIn
{
    alignas(16) unsigned char maScoring[sizeof(ScoringOutputInterface)];
    alignas(16) unsigned char maOnline[sizeof(OnlineScoringOutputInterface)];
    ScoringOutputInterface&       Scoring() { return *reinterpret_cast<ScoringOutputInterface*>(maScoring); }
    OnlineScoringOutputInterface& Online()  { return *reinterpret_cast<OnlineScoringOutputInterface*>(maOnline); }
    const ScoringOutputInterface* GetScoringOutputInterface() const
    {
        return reinterpret_cast<const ScoringOutputInterface*>(maScoring);
    }
    const OnlineScoringOutputInterface* GetOnlineScoringOutputInterface() const
    {
        return reinterpret_cast<const OnlineScoringOutputInterface*>(maOnline);
    }
};

// BridgeGameStateToDirector's two team blocks, under their production parameter names.
static void TeamLeg(InputBuffer* lpDirectorInput, const GameStateOutputStandIn* lpGameStateOutput)
{
    (void)lpDirectorInput;
    (void)lpGameStateOutput;
#include "traffic_hop_team_word.inc"
#include "traffic_hop_team_loop.inc"
}

// ---- MainDirector's first PreSceneQueryUpdate block, as a member of a stand-in with the real AllVehicleData ----
namespace BrnDirector
{
    static int giTrafficDiagCalls = 0;
    void DirectorTrafficDiag(const AllVehicleData&) { ++giTrafficDiagCalls; }   // the production PC witness

    struct DirectorInputOutputStandIn
    {
        const DirectorIO::InputBuffer* mpInputBuffer;
    };

    struct PreSceneStandIn
    {
        AllVehicleData mAllVehicleData;

        void Line1(const DirectorInputOutputStandIn* lpIO, s32 liPlayerCarIndex)
        {
            (void)lpIO;
            (void)liPlayerCarIndex;
#include "traffic_hop_presq.inc"
        }
    };
}

// ---- detection: the API a revision may lack (it then takes the fallback and fails on behaviour) ----
template <class B, class = void> struct HasTrafficGetter : std::false_type {};
template <class B>
struct HasTrafficGetter<B, std::void_t<decltype(std::declval<const B&>().GetTrafficOutputInterface())>> : std::true_type {};

template <class W, class = void> struct HasConsoleUpdate : std::false_type {};
template <class W>
struct HasConsoleUpdate<W, std::void_t<decltype(std::declval<W&>().Update(
    std::declval<CgsContainers::BitArray<8u>>(), std::declval<const BrnDirector::Camera::VehicleInfo*>(),
    std::declval<EActiveRaceCarIndex>(), std::declval<const TrafficArray*>(), std::declval<const u32*>()))>>
    : std::true_type {};

template <class B> static const void* TrafficGetterAddress(const B& lrBuffer)
{
    if constexpr (HasTrafficGetter<B>::value)
        return lrBuffer.GetTrafficOutputInterface();
    else
        return nullptr;
}

// Update with a NULL traffic array: true when the revision's Update exists and its :66 tripwire fired.
template <class W> static bool NullTrafficTrips(W& lrWorld, const CgsContainers::BitArray<8u>& lrUsed,
                                                const BrnDirector::Camera::VehicleInfo* lpCars, const u32* lpaTeams)
{
    if constexpr (HasConsoleUpdate<W>::value)
    {
        gacLastAssert[0] = 0;
        const unsigned luBefore = gAsserts;
        lrWorld.Update(lrUsed, lpCars, static_cast<EActiveRaceCarIndex>(0), static_cast<const TrafficArray*>(nullptr),
                       lpaTeams);
        return gAsserts > luBefore && std::strcmp(gacLastAssert, "lpTrafficVehicleArray != NULL") == 0;
    }
    else
    {
        (void)lrWorld; (void)lrUsed; (void)lpCars; (void)lpaTeams;
        return false;
    }
}

static void Check(bool lbPass, const char* lpcName)
{
    ++gChecks;
    if (!lbPass)
    {
        ++gFailures;
        std::printf("FAIL  %s\n", lpcName);
    }
}

static Vector3 V3(f32 lfX, f32 lfY, f32 lfZ)
{
    Vector3 lVector;
    lVector.x = lfX; lVector.y = lfY; lVector.z = lfZ; lVector.w = 0.0f;
    return lVector;
}

// One traffic record, distinct in every field, the way TrafficEntityModule::ProcessNearbyTrafficSceneQueryResults
// fills it (box-centre transform, velocity along At, half extents, the asset's vehicle id, the vehicle index).
static TrafficDirectorEntity Record(int liSeed)
{
    TrafficDirectorEntity lRecord;
    std::memset(static_cast<void*>(&lRecord), 0, sizeof(lRecord));
    lRecord.mLocalTransform.SetIdentity();
    lRecord.mLocalTransform.wAxis = V3(100.0f + liSeed, 2.0f, -50.0f - 2.0f * liSeed);
    lRecord.mVelocity             = V3(0.5f * liSeed, 0.0f, 3.0f + liSeed);
    lRecord.mHalfExtents          = V3(1.0f, 0.75f, 2.25f + 0.125f * liSeed);
    lRecord.mVehicleId            = CgsID(0x5000u + static_cast<u32>(liSeed));
    lRecord.mu16EntityIndex       = static_cast<u16>(200 + 3 * liSeed);
    return lRecord;
}

static WorldOutputStandIn gWorld;

static void FillWorld(int liCount, int liSeedBase)
{
    std::memset(gWorld.maStorage, 0xAB, sizeof(gWorld.maStorage));
    gWorld.Traffic().mu16EntityCount = static_cast<u16>(liCount);
    gWorld.Traffic().GetTrafficDirectorEntityArray().Construct();
    for (int i = 0; i < liCount; ++i)
        gWorld.Traffic().GetTrafficDirectorEntityArray().Append(Record(liSeedBase + i));
    gWorld.miReads = 0;
}

static bool SameXYZ(const Vector3& a, const Vector3& b) { return a.x == b.x && a.y == b.y && a.z == b.z; }

// Field by field (the record's padding is not part of the copy's contract).
static bool SameRecord(const TrafficDirectorEntity& a, const TrafficDirectorEntity& b)
{
    return SameXYZ(a.mLocalTransform.xAxis, b.mLocalTransform.xAxis) && SameXYZ(a.mLocalTransform.yAxis, b.mLocalTransform.yAxis)
        && SameXYZ(a.mLocalTransform.zAxis, b.mLocalTransform.zAxis) && SameXYZ(a.mLocalTransform.wAxis, b.mLocalTransform.wAxis)
        && SameXYZ(a.mVelocity, b.mVelocity) && SameXYZ(a.mHalfExtents, b.mHalfExtents)
        && a.mVehicleId == b.mVehicleId && a.mu16EntityIndex == b.mu16EntityIndex;
}

static bool InputRecordsAre(int liCount, int liSeedBase)
{
    const TrafficDirectorEntity* lpRecords = reinterpret_cast<const TrafficDirectorEntity*>(gaInputStorage + KU_TRAFFIC_ARRAY);
    for (int i = 0; i < liCount; ++i)
    {
        if (!SameRecord(lpRecords[i], Record(liSeedBase + i)))
            return false;
    }
    return true;
}

static GameStateOutputStandIn gState;

static void Scores(s32 liPlayer, const s32 (&laiTeams)[8], s32 liAwardVariable7)
{
    std::memset(gState.maScoring, 0xCD, sizeof(gState.maScoring));
    std::memset(gState.maOnline, 0xCD, sizeof(gState.maOnline));
    gState.Scoring().mePlayerRaceCarIndex = static_cast<EActiveRaceCarIndex>(liPlayer);
    for (int i = 0; i < 8; ++i)
        gState.Online().maePlayerTeam[i] = static_cast<BrnGameState::GameStateModuleIO::EPlayerTeam>(laiTeams[i]);
    gState.Online().maiOnlineAwardVariables[7] = liAwardVariable7;
}

static void RunTeamLeg()
{
    Input().LockForWrite();
    TeamLeg(&Input(), &gState);
    Input().UnlockForWrite();
}

// A used race car at a world position (the rest of its record zeroed: not crashing, no ground test).
static void PlaceCar(u32 luCar, f32 lfX, f32 lfY, f32 lfZ)
{
    BrnDirector::Camera::VehicleInfo& lrCar = Input().mRaceCarInfo[luCar];
    std::memset(static_cast<void*>(&lrCar), 0, sizeof(lrCar));
    lrCar.mRaceCarState.mTransform.SetIdentity();
    lrCar.mRaceCarState.mTransform.wAxis = V3(lfX, lfY, lfZ);
    Input().mUsedRaceCars.SetBit(luCar);
}

static BrnDirector::PreSceneStandIn gDirector;

static void RunLine1(s32 liPlayer)
{
    BrnDirector::DirectorInputOutputStandIn lIO = { &Input() };
    Input().LockForRead();
    gDirector.Line1(&lIO, liPlayer);
    Input().UnlockForRead();
}

int main()
{
    // ================= A. the typed +0x6AC0 span =================
    Check(std::is_same<decltype(InputBuffer::mTrafficOutputInterface), TrafficDirectorOutputInterface>::value,
          "A1 the director input's +0x6AC0 member IS a BrnTraffic::BrnTrafficIO::TrafficDirectorOutputInterface "
          "(DWARF BrnDirectorModuleIO.h:338), not opaque bytes");
    Check(offsetof(InputBuffer, mTrafficOutputInterface) == KU_TRAFFIC
              && sizeof(Input().mTrafficOutputInterface) == 0x78E0 - KU_TRAFFIC,
          "A2 it sits at +0x6AC0 and spans 0xE20 bytes, up to mPlayerCrashInfo @0x78E0");
    Check(KU_TRAFFIC + offsetof(TrafficDirectorOutputInterface, maActiveEntityArray) == KU_TRAFFIC_ARRAY
              && sizeof(TrafficDirectorEntity) == 0x70
              && KU_TRAFFIC_ARRAY + offsetof(TrafficArray, miCount) == KU_TRAFFIC_COUNT,
          "A3 its entity array at +0x6AD0 (PreSceneQueryUpdate `addi +0x10` 0x8225BC4C), 0x70-byte records, the "
          "array's count word at +0x78D0");

    // ================= B. InputBuffer::Construct @0x822393D0 =================
    std::memset(gaInputStorage, 0xCD, sizeof(gaInputStorage));
    Input().Construct();
    Check(Word<s32>(KU_TRAFFIC_COUNT) == 0,
          "B1 Construct empties the traffic array: `stw 0, 0x78D0` (0x82239444)");
    {
        bool lbAllZero = true;
        for (size_t i = 0; i < 8; ++i)
            lbAllZero = lbAllZero && Word<u32>(KU_TEAMS + 4 * i) == 0u;
        Check(lbAllZero, "B2 Construct clears the eight team words @+0x3238 to E_PLAYER_TEAM_NONE (0x82239530..0x82239560)");
    }
    Check(Word<s32>(KU_PLAYER_TEAM) == 0, "B3 Construct seeds the player's team word @0x7AB0 to 0 (`stw r30, 0x7AB0`)");

    // ================= C. BridgeWorldToDirector step 4 =================
    FillWorld(5, 0);
    Step4(&Input(), &gWorld);
    Check(gWorld.miReads == 1,
          "C1 step 4 reads the world output's traffic interface once (GetTrafficDirectorOutputInterface 0x823B6158)");
    Check(Word<u16>(KU_TRAFFIC) == 5, "C2 the world's u16 entity count lands at +0x6AC0 (`lhz 0 ; sth 0x6AC0`)");
    Check(Word<s32>(KU_TRAFFIC_COUNT) == 5 && InputRecordsAre(5, 0),
          "C3 the five records land at +0x6AD0 field for field, and the array count word at +0x78D0 (sub_823B2368)");
    FillWorld(2, 10);
    Step4(&Input(), &gWorld);
    Check(Word<u16>(KU_TRAFFIC) == 2 && Word<s32>(KU_TRAFFIC_COUNT) == 2 && InputRecordsAre(2, 10),
          "C4 the next frame's copy REPLACES the list (2 new records, count 2), it does not append");
    Check(TrafficGetterAddress(Input()) == gaInputStorage + KU_TRAFFIC,
          "C5 the DWARF :290 getter is the input + 0x6AC0 (PreSceneQueryUpdate `addi r30, r31, 0x6AC0` 0x8225BC30)");

    // ================= D. BridgeGameStateToDirector's team leg =================
    {
        const s32 KAI_TEAMS[8] = { 2, 1, 1, 2, 0, 2, 1, 2 };
        bool lbWord = true;
        for (s32 liPlayer = 0; liPlayer < 8; ++liPlayer)
        {
            Scores(liPlayer, KAI_TEAMS, 0x1234567);
            RunTeamLeg();
            lbWord = lbWord && Word<s32>(KU_PLAYER_TEAM) == KAI_TEAMS[liPlayer];
        }
        Check(lbWord, "D1 the player's team word @0x7AB0 = maePlayerTeam[mePlayerRaceCarIndex] (`lwzx gameStateOut + "
                      "0x2AFC8 + 4*player` 0x823CD450 -> `stw 0x7AB0` 0x823CD454), players 0..7");
        Scores(-1, KAI_TEAMS, 0x1234567);
        RunTeamLeg();
        Check(Word<s32>(KU_PLAYER_TEAM) == 0x1234567,
              "D2 a -1 player slot reads the word before the array: maiOnlineAwardVariables[7] (gameStateOut + 0x2AFC4)");
        bool lbTeams = true;
        for (size_t i = 0; i < 8; ++i)
            lbTeams = lbTeams && Word<s32>(KU_TEAMS + 4 * i) == KAI_TEAMS[i];
        Check(lbTeams, "D3 SetVehicleTeam @0x823B2938 publishes maePlayerTeam[i] into +0x3238 + 4i for i = 0..7 "
                       "(0x823CD514..0x823CD56C)");

        const s32 KAI_OFFLINE[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
        for (size_t i = 0; i < 8; ++i)
        {
            const u32 luStale = 0x77u;
            std::memcpy(gaInputStorage + KU_TEAMS + 4 * i, &luStale, sizeof(luStale));
        }
        Scores(0, KAI_OFFLINE, 0x1234567);
        std::memset(gState.maOnline, 0, sizeof(gState.maOnline));   // offline: the online scorers never wrote it
        gState.Scoring().mePlayerRaceCarIndex = static_cast<EActiveRaceCarIndex>(0);
        RunTeamLeg();
        bool lbOffline = Word<s32>(KU_PLAYER_TEAM) == 0;
        for (size_t i = 0; i < 8; ++i)
            lbOffline = lbOffline && Word<s32>(KU_TEAMS + 4 * i) == 0;
        Check(lbOffline, "D4 offline (every maePlayerTeam E_PLAYER_TEAM_NONE) the leg rewrites a stale frame's words "
                         "to 0, the player word too");
        Input().LockForRead();
        const u32* lpaTeams = Input().GetVehicleInfoArray();
        Input().UnlockForRead();
        Check(reinterpret_cast<const unsigned char*>(lpaTeams) == gaInputStorage + KU_TEAMS,
              "D5 GetVehicleInfoArray 0x82206EF0 is `addi r3, this, 0x3238`: the team words, AllVehicleData's lpaVehicleTeams");
    }

    // ================= E. PreSceneQueryUpdate's AllVehicleData::Update call =================
    std::memset(gaInputStorage, 0xCD, sizeof(gaInputStorage));
    Input().Construct();
    PlaceCar(0, 10.0f, 1.0f, 20.0f);   // the player
    PlaceCar(2, 13.0f, 1.0f, 24.0f);   // 3-4-5: squared distance 25
    PlaceCar(5, 16.0f, 1.0f, 28.0f);   // 6-8-10: squared distance 100
    {
        const s32 KAI_TEAMS[8] = { 1, 0, 1, 0, 0, 2, 0, 0 };   // the player and car 2 red, car 5 blue
        Scores(0, KAI_TEAMS, 0);
        RunTeamLeg();
    }
    FillWorld(3, 20);
    Step4(&Input(), &gWorld);
    gDirector.mAllVehicleData.Construct();
    RunLine1(0);
    {
        const AllVehicleData& lrWorld = gDirector.mAllVehicleData;
        Check(static_cast<const void*>(lrWorld.mpTrafficVehicleArray) == gaInputStorage + KU_TRAFFIC_ARRAY,
              "E1 Update's lpTrafficVehicleArray is the input's entity array, input + 0x6AC0 + 0x10 (0x8225BC30 / "
              "0x8225BC4C), stored at +0xD0 (0x8221D998)");
        bool lbSeen = lrWorld.mpTrafficVehicleArray != 0
                      && static_cast<const void*>(lrWorld.mpTrafficVehicleArray) == gaInputStorage + KU_TRAFFIC_ARRAY;
        if (lbSeen)
        {
            lbSeen = lrWorld.mpTrafficVehicleArray->GetCount() == 3;
            for (u32 i = 0; lbSeen && i < 3u; ++i)
                lbSeen = (*lrWorld.mpTrafficVehicleArray)[i].mu16EntityIndex == Record(20 + static_cast<int>(i)).mu16EntityIndex;
        }
        Check(lbSeen, "E2 AllVehicleData sees the three traffic records the world published this frame (N = 3)");

        const AllVehicleData::NearestCarInfoArray& lrRows = lrWorld.maNearestRaceCarsToPlayer;
        const bool lbRows = lrRows.GetCount() == 3
            && lrRows[0u].meRaceCarIndex == 0 && lrRows[1u].meRaceCarIndex == 2 && lrRows[2u].meRaceCarIndex == 5
            && lrRows[0u].mfDistance == 0.0f && lrRows[1u].mfDistance == 25.0f && lrRows[2u].mfDistance == 100.0f;
        Check(lbRows, "E3 one row per used car in ascending order, the player included at 0; squared distances 25 / 100 "
                      "(`vsubfp ; vmsum3fp128`)");
        Check(lbRows && lrRows[0u].miTeam == 1 && lrRows[1u].miTeam == 1 && lrRows[2u].miTeam == 2,
              "E4 each row carries the input's team word (`lwzx r10, 4 * car, lpaVehicleTeams` @0x8221DEE4): 1 / 1 / 2 "
              "-- non-zero where the console has them");
        Check(static_cast<const void*>(lrWorld.mpRaceCars) == gaInputStorage + KU_RACE_CARS
                  && lrWorld.mePlayerRaceCarIndex == 0
                  && lrWorld.mUsedRaceCars.IsBitSet(0u) && lrWorld.mUsedRaceCars.IsBitSet(2u)
                  && lrWorld.mUsedRaceCars.IsBitSet(5u) && !lrWorld.mUsedRaceCars.IsBitSet(1u),
              "E5 the three stores: mpRaceCars = the input's VehicleInfo[8] @+0x990 (0x8221D95C), the player index "
              "(0x8221D960), the used bits (0x8221D96C)");
        Check(!lrWorld.mbSorteddNearestRaceCarsToPlayer && lrWorld.mbShouldUpdateNearestRaceCars,
              "E6 the table is left unsorted (`stb 0, 0x138` 0x8221E0E0) and flagged for update (`stb 1, 0x139`)");
        Check(lrWorld.SqDistanceOfNearestOpposingTeamMember(1) == 100.0f,
              "E7 the opposing-team query (0x822334E0) skips the red team-mate at 25 and answers the blue car's 100");
        Check(lrWorld.GetSqDistanceOfNearestCarToPlayer() == 25.0f,
              "E8 the nearest OTHER car is sorted row 1 at 25 (0x82233488; row 0 is the player)");
    }
    {
        // The next frame, offline: car 2 gone, every team E_PLAYER_TEAM_NONE, no traffic.
        Input().mUsedRaceCars.UnSetBit(2u);
        const s32 KAI_OFFLINE[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
        Scores(0, KAI_OFFLINE, 0);
        std::memset(gState.maOnline, 0, sizeof(gState.maOnline));
        gState.Scoring().mePlayerRaceCarIndex = static_cast<EActiveRaceCarIndex>(0);
        RunTeamLeg();
        FillWorld(0, 0);
        Step4(&Input(), &gWorld);
        RunLine1(0);
        const AllVehicleData& lrWorld = gDirector.mAllVehicleData;
        const AllVehicleData::NearestCarInfoArray& lrRows = lrWorld.maNearestRaceCarsToPlayer;
        Check(lrRows.GetCount() == 2 && lrRows[0u].meRaceCarIndex == 0 && lrRows[1u].meRaceCarIndex == 5
                  && lrRows[0u].miTeam == 0 && lrRows[1u].miTeam == 0
                  && lrWorld.mpTrafficVehicleArray != 0 && lrWorld.mpTrafficVehicleArray->GetCount() == 0,
              "E9 the next frame rebuilds from scratch (`stw 0, 0x134` first): rows 0 / 5, both team 0, and an empty "
              "traffic list is still handed over");
        Check(lrWorld.SqDistanceOfNearestOpposingTeamMember(0) == FLT_MAX,
              "E10 offline every row shares team 0, so the opposing-team query answers FLT_MAX (flt_8200173C)");
    }
    Check(gAsserts == 0, "E11 no assert fired on the whole hop (the :66 / :134 tripwires, the lock asserts)");

    // ================= F. the console's Update signature =================
    Check(HasConsoleUpdate<AllVehicleData>::value,
          "F1 AllVehicleData::Update takes the console's five arguments (PreSceneQueryUpdate loads r4..r8, 0x8225BC54..0x8225BC70)");
    {
        AllVehicleData lWorld;
        lWorld.Construct();
        CgsContainers::BitArray<8u> lUsed;
        lUsed.UnSetAll();
        lUsed.SetBit(0u);
        const u32 KAU_TEAMS[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
        Check(NullTrafficTrips(lWorld, lUsed, &Input().mRaceCarInfo[0], KAU_TEAMS),
              "F2 a NULL traffic array trips Update's own \"lpTrafficVehicleArray != NULL\" (:66, 0x8221D974)");
    }

    std::printf("FxDirector2TrafficHop: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
