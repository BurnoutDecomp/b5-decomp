// L6 (owner list 2026-09-28): "Lot of Driver details are missing or wrong" -- CARS OWNED read 171/86.
//
// The PRODUCTION body of
//   ProgressionManager::GetGameStats @0x8238A6A0          (BrnProgressionManager_GameStats.cpp)
// with its TU's anonymous-namespace constants, GameStats::Construct @0x82354F38 (BrnGameActionData.cpp) and
// the three VehicleListEntry accessors it reads (IsTrophyCar / GetUnlockRank / GetLiveryType, with their
// offset constants, VehicleListEntry.cpp) are extracted by run_l6_game_stats_cars.py into
// l6_game_stats_cars.inc. They run against the REAL GameStats record (BrnGameActionData.h), the REAL
// VehicleListEntry (VehicleListEntry.h, raw 0xF0-byte entries written by resource offset) and the REAL
// CarData, on a ProgressionManager fixture whose profile / vehicle list / stunt and street managers are
// stand-ins returning distinct values.
//
// Every expectation is the ARTIST asm of 0x8238A6A0 (dumped 2026-09-28):
//   CARS_COLLECTED (@0x8238A73C..0x8238A8A8): per profile car, unlock type (CarData+0x10) == 5 -> count when
//     (s8)GetProgressionRank() >= entry+0x99; otherwise count when (entry+0x94 & 1) AND entry+0xE9 is NOT
//     1 / 3 / 4 (0x8238A870..0x8238A888: `cmplwi r11,0 ; li r11,1 ; beq 0x8238A880` counts the NO-MATCH arm).
//   The rest of the record: the float->int converts (fctiwz), the medal triple in GetTotalWinCount's order,
//   the three stunt sets and totals, the takedown block, both district grids, the roads, the nemesis
//   (strictly greater), the personal bests (two stfs), wins-to-next-rank (-1 on the last rank), events and
//   the X360-only challenges word. ACHIEVEMENTS (the manager's popcount) is not produced on PC and is
//   not checked here.
// The pre-fix body counted the colour / silver variants instead and FAILS the CARS_COLLECTED checks.
#include <cstdio>
#include <cstring>
#include "types.hpp"
#include "BrnCommonTypes.h"
#include "GameSource/GameState/SharedIO/BrnGameActionData.h"
#include "GameSource/GameState/BrnGameStateTypes.h"
#include "SharedClasses/World/BrnWorldRegion.h"
#include "GameSource/GameState/Progression/BrnProgressionCarData.h"
#include "SharedClasses/DataLists/VehicleListEntry.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"

static unsigned gChecks = 0, gFailures = 0, gAsserts = 0;

namespace CgsDev
{
namespace Assert
{
    int   BeginAssert() { return 0; }
    int   FireAssert(const char*, const char*, int) { ++gAsserts; return 0; }
    void* EndAssert() { return nullptr; }
}
namespace Log { DebugPrint* gpDebugPrint = nullptr; }
namespace Message { u64 gxMessageFilterFlags = 0; }
}

// ---- stand-ins, by their production names --------------------------------------------------------
namespace BrnStreetData
{
    class StreetData
    {
    public:
        s32 miRoadCount = 0;
        s32 GetRoadCount() const { return miRoadCount; }
    };
}

namespace BrnGameState
{
    class StuntManager
    {
    public:
        s16 maTotals[3] = {};
        s16 maaByCounty[3][5] = {};
        s16 GetTotalStuntElementCount(StuntElementType leType) const { return maTotals[leType]; }
        s16 GetTotalStuntElementCountByCounty(StuntElementType leType, s32 liCounty) const
        { return maaByCounty[leType][liCounty]; }
    };

    class StreetManager
    {
    public:
        s32 miParTime = 0, miParShowTime = 0, miComplete = 0;
        BrnStreetData::StreetData mStreetData;
        s32 GetNumberOfParTimeTrialRoadsRuledByLocalPlayer() { return miParTime; }
        s32 GetNumberOfParShowTimeRoadsRuledByLocalPlayer()  { return miParShowTime; }
        s32 GetNumberOfCompleteRoadsRuledByLocalPlayer()     { return miComplete; }
        const BrnStreetData::StreetData* GetStreetData() const { return &mStreetData; }
    };
}

namespace BrnResource
{
    // The vehicle list the loop walks: GetVehicleIndex(id) then GetVehicleData(index), as the console.
    class VehicleList
    {
    public:
        VehicleListEntry maEntries[16];
        s32 miCount = 0;
        s32 GetVehicleIndex(CgsID lId) const
        {
            for (s32 li = 0; li < miCount; ++li)
                if (maEntries[li].GetId() == lId)
                    return li;
            return -1;
        }
        const VehicleListEntry* GetVehicleData(s32 liIndex) const { return &maEntries[liIndex]; }
    };
}

namespace BrnProgression
{
    namespace GsmIO = BrnGameState::GameStateModuleIO;

    struct RivalData
    {
        CgsID mRivalId;
        s32   miTakedownFromCount;
    };

    struct ProfileEvent
    {
        bool mbFound;
        bool IsFound() const { return mbFound; }
    };

    class Profile
    {
    public:
        CarData      maCars[256];
        s32          miCarCount = 0;
        RivalData    maRivals[4] = {};
        s32          miRivalCount = 0;
        ProfileEvent maEvents[8] = {};
        u32          muEventCount = 0;
        s32          maiStunts[3] = {};
        s16          maaStuntsByCounty[3][5] = {};
        s32          maiTakedownTypes[13] = {};

        f32  GetDistanceDrivenOnline() const  { return 139688.546875f; }
        f32  GetDistanceDrivenOffline() const { return 5657635.5f; }
        f32  GetRealTimePlayed() const        { return 143418.125f; }
        s8   GetPowerParkingBestRating() const { return static_cast<s8>(-3); }
        s8   GetPowerParkingBetweenOtherPlayersBestRating() const { return static_cast<s8>(77); }
        s32  GetCarCount() const              { return miCarCount; }
        const CarData* GetCarData(s32 liIndex) const { return &maCars[liIndex]; }
        u32  GetTotalWinCount(u32& lruRankWins, u32& lruNonRankWins, u32& lruSpecialEventWins) const
        { lruRankWins = 31; lruNonRankWins = 32; lruSpecialEventWins = 33; return 96; }
        s32  GetStuntElementCount(BrnGameState::StuntElementType leType) const { return maiStunts[leType]; }
        s32  GetTotalTakedownCount() const    { return 3731; }
        s32  GetStuntElementCountByCounty(BrnGameState::StuntElementType leType, BrnWorld::ECounty leCounty) const
        { return maaStuntsByCounty[leType][leCounty]; }
        s32  GetTakedownTypeCount(s32 liType) const { return maiTakedownTypes[liType]; }
        s32  GetRivalCount() const            { return miRivalCount; }
        const RivalData* GetRivalData(s32 liIndex) const { return &maRivals[liIndex]; }
        s32  GetCompletedBarrelRolls() const  { return 2; }
        f32  GetCompletedAirSpinAngle() const { return 373.14398193359375f; }
        f32  GetCompletedDriftDistance() const { return 414.8996887207031f; }
        u32  GetBestNewBurnoutChainScore() const { return 999u; }
        f32  GetOncomingDistance() const      { return 3167.010986328125f; }
        f32  GetAirMaximum() const            { return 7.433375835418701f; }
        s32  GetNewHighShowtimeScore() const  { return 6180200; }
        s32  GetTotalCarsToShutDown() const   { return 4; }
        u32  GetEventCount() const            { return muEventCount; }
        const ProfileEvent* GetEvent(u32 luIndex) const { return &maEvents[luIndex]; }
        s32  GetBestStuntRunScore() const     { return 1799200; }
    };

    class ProgressionManager
    {
    public:
        Profile                           mProfile;
        const BrnResource::VehicleList*   mpVehicleList = nullptr;
        BrnGameState::StreetManager*      mpStreetManager = nullptr;
        void*                             mpAchievementManager = nullptr;
        s8                                mi8ProgressionRank = 5;
        bool                              mbFinishedLastRank = false;

        s32  GetProgressionRank() const        { return mi8ProgressionRank; }
        bool PlayerHasFinishedLastRank() const { return mbFinishedLastRank; }
        u32  GetTotalWinsForNextRank()         { return 26u; }
        f32  ComputeCompletionPercentage()     { return 73.25f; }

        void GetGameStats(GsmIO::GameStats* lpGameStats, const BrnGameState::StuntManager* lpStuntManager,
                          s32 liNumChallengesCompleted);
    };
}

// The production bodies under test.
#include "l6_game_stats_cars.inc"

using namespace BrnProgression;
typedef BrnGameState::GameStateModuleIO::GameStats GS;

static void Check(bool lbPass, const char* lpcWhat)
{
    ++gChecks;
    if (!lbPass)
    {
        ++gFailures;
        std::printf("FAIL  %s\n", lpcWhat);
    }
}

static void CheckInt(s32 liGot, s32 liWant, const char* lpcWhat)
{
    char lac[256];
    std::snprintf(lac, sizeof(lac), "%s: %d (want %d)", lpcWhat, liGot, liWant);
    Check(liGot == liWant, lac);
}

// A vehicle-list entry as the resource carries it: id @+0x00, flags word @+0x94, unlock rank @+0x99,
// livery tag @+0xE9 (host byte order, as the PC resource is converted).
static void MakeEntry(BrnResource::VehicleListEntry& lrEntry, CgsID lId, u32 luFlags, u8 lu8UnlockRank,
                      u8 lu8Livery)
{
    u8* lpBytes = reinterpret_cast<u8*>(&lrEntry);
    std::memset(lpBytes, 0, sizeof(lrEntry));
    std::memcpy(lpBytes + 0x00, &lId, 8);
    std::memcpy(lpBytes + 0x94, &luFlags, 4);
    lpBytes[0x99] = lu8UnlockRank;
    lpBytes[0xE9] = lu8Livery;
}

static void AddCar(Profile& lrProfile, CgsID lId, CarData::UnlockType leType)
{
    CarData& lrCar = lrProfile.maCars[lrProfile.miCarCount++];
    std::memset(&lrCar, 0, sizeof(lrCar));
    lrCar.mId = lId;
    lrCar.meUnlockType = leType;
}

static s32 RunCarsCollected(ProgressionManager& lrManager)
{
    BrnGameState::StuntManager lStunts;
    static GS sStats;
    lrManager.GetGameStats(&sStats, &lStunts, 0);
    return sStats.GetValue(GS::E_INT_VALUE_TYPE_CARS_COLLECTED);
}

int main()
{
    Check(sizeof(BrnResource::VehicleListEntry) == 0xF0, "VehicleListEntry is the 0xF0-byte resource record");

    // ---- 1) one car per arm of the console's loop -------------------------------------------------
    {
        static ProgressionManager sManager;
        static BrnResource::VehicleList sList;
        struct Row { CgsID id; u32 flags; u8 rank; u8 livery; CarData::UnlockType type; bool counts; const char* why; };
        const Row laRows[] =
        {
            { 101, 0x41, 0, 0, CarData::E_UNLOCK_TYPE_GIFT,           true,  "flag bit 0, livery 0 (base car): counted" },
            { 102, 0x41, 0, 1, CarData::E_UNLOCK_TYPE_GIFT,           false, "flag bit 0, livery 1 (colour variant): NOT counted" },
            { 103, 0x41, 0, 2, CarData::E_UNLOCK_TYPE_GIFT,           true,  "flag bit 0, livery 2 (pattern car): counted" },
            { 104, 0x41, 0, 3, CarData::E_UNLOCK_TYPE_SHUTDOWN_RIVAL, false, "flag bit 0, livery 3: NOT counted" },
            { 105, 0x41, 0, 4, CarData::E_UNLOCK_TYPE_GOLD_SILVER,    false, "flag bit 0, livery 4 (silver): NOT counted" },
            { 106, 0x40, 0, 0, CarData::E_UNLOCK_TYPE_GIFT,           false, "flag bit 0 CLEAR, livery 0: NOT counted" },
            { 107, 0x41, 3, 0, CarData::E_UNLOCK_TYPE_SPONSOR,        true,  "sponsor, unlock rank 3 <= rank 5: counted" },
            { 108, 0x41, 6, 0, CarData::E_UNLOCK_TYPE_SPONSOR,        false, "sponsor, unlock rank 6 > rank 5: NOT counted (the livery arm is not taken)" },
            { 109, 0x41, 2, 1, CarData::E_UNLOCK_TYPE_SPONSOR,        true,  "sponsor with a colour livery, rank reached: counted" },
            { 110, 0x41, 5, 4, CarData::E_UNLOCK_TYPE_SPONSOR,        true,  "sponsor, unlock rank 5 == rank 5 (bge): counted" },
        };
        const s32 liRows = static_cast<s32>(sizeof(laRows) / sizeof(laRows[0]));
        for (s32 li = 0; li < liRows; ++li)
            MakeEntry(sList.maEntries[li], laRows[li].id, laRows[li].flags, laRows[li].rank, laRows[li].livery);
        sList.miCount = liRows;
        sManager.mpVehicleList = &sList;
        sManager.mi8ProgressionRank = 5;
        for (s32 li = 0; li < liRows; ++li)
        {
            sManager.mProfile.miCarCount = 0;
            AddCar(sManager.mProfile, laRows[li].id, laRows[li].type);
            CheckInt(RunCarsCollected(sManager), laRows[li].counts ? 1 : 0, laRows[li].why);
        }
    }

    // ---- 2) the owner's slot-0 profile mix (decoded 2026-09-28): 241 cars -> console 70 ------------
    //   UNLOCK base 1, GIFT base 5, GIFT colour 101, GIFT pattern 35, SHUTDOWN_RIVAL base 29, GOLD_SILVER silver 70.
    {
        static ProgressionManager sManager;
        static BrnResource::VehicleList sList;
        MakeEntry(sList.maEntries[0], 201, 0x41, 0, 0);   // base
        MakeEntry(sList.maEntries[1], 202, 0x41, 0, 1);   // colour
        MakeEntry(sList.maEntries[2], 203, 0x41, 0, 2);   // pattern
        MakeEntry(sList.maEntries[3], 204, 0x41, 0, 4);   // silver
        sList.miCount = 4;
        sManager.mpVehicleList = &sList;
        Profile& P = sManager.mProfile;
        P.miCarCount = 0;
        AddCar(P, 201, CarData::E_UNLOCK_TYPE_UNLOCK);
        for (s32 li = 0; li < 5; ++li)   AddCar(P, 201, CarData::E_UNLOCK_TYPE_GIFT);
        for (s32 li = 0; li < 101; ++li) AddCar(P, 202, CarData::E_UNLOCK_TYPE_GIFT);
        for (s32 li = 0; li < 35; ++li)  AddCar(P, 203, CarData::E_UNLOCK_TYPE_GIFT);
        for (s32 li = 0; li < 29; ++li)  AddCar(P, 201, CarData::E_UNLOCK_TYPE_SHUTDOWN_RIVAL);
        for (s32 li = 0; li < 70; ++li)  AddCar(P, 204, CarData::E_UNLOCK_TYPE_GOLD_SILVER);
        CheckInt(P.miCarCount, 241, "the slot-0 mix holds 241 cars");
        CheckInt(RunCarsCollected(sManager), 70, "CARS_COLLECTED on the owner's slot-0 mix (the PC showed 171)");
    }

    // ---- 3) the rest of the record, against the console's store list --------------------------------
    {
        static ProgressionManager sManager;
        static BrnResource::VehicleList sList;
        static BrnGameState::StreetManager sStreets;
        BrnGameState::StuntManager lStunts;
        sManager.mpVehicleList = &sList;
        sManager.mpStreetManager = &sStreets;
        Profile& P = sManager.mProfile;
        P.miCarCount = 0;
        P.maiStunts[0] = 35; P.maiStunts[1] = 281; P.maiStunts[2] = 71;
        lStunts.maTotals[0] = 50; lStunts.maTotals[1] = 400; lStunts.maTotals[2] = 120;
        for (s32 lt = 0; lt < 3; ++lt)
            for (s32 lc = 0; lc < 5; ++lc)
            {
                P.maaStuntsByCounty[lt][lc] = static_cast<s16>(10 * lt + lc + 1);
                lStunts.maaByCounty[lt][lc] = static_cast<s16>(100 * lt + 10 * lc + 7);
            }
        for (s32 li = 0; li < 13; ++li) P.maiTakedownTypes[li] = 1000 + li;
        P.miRivalCount = 4;
        P.maRivals[0] = { 501, 3 }; P.maRivals[1] = { 502, 9 }; P.maRivals[2] = { 503, 9 }; P.maRivals[3] = { 504, 1 };
        P.muEventCount = 7;
        for (u32 li = 0; li < 7; ++li) P.maEvents[li].mbFound = (li % 3) != 1;
        sStreets.miParTime = 12; sStreets.miParShowTime = 13; sStreets.miComplete = 11; sStreets.mStreetData.miRoadCount = 64;

        static GS sStats;
        sManager.GetGameStats(&sStats, &lStunts, 17);
        CheckInt(sStats.GetValue(GS::E_INT_VALUE_TYPE_DISTANCE_DRIVEN_ONLINE), 139688, "DISTANCE_DRIVEN_ONLINE = fctiwz(Profile+0x64)");
        CheckInt(sStats.GetValue(GS::E_INT_VALUE_TYPE_DISTANCE_DRIVEN_OFFLINE), 5657635, "DISTANCE_DRIVEN_OFFLINE = fctiwz(Profile+0x68)");
        CheckInt(sStats.GetValue(GS::E_INT_VALUE_TYPE_TIME_PLAYED), 143418, "TIME_PLAYED = fctiwz(Profile+0x1CD28)");
        CheckInt(sStats.GetValue(GS::E_INT_VALUE_TYPE_BEST_POWER_PARKING), -3, "BEST_POWER_PARKING = extsb(Profile+0x71)");
        CheckInt(sStats.GetValue(GS::E_INT_VALUE_TYPE_BEST_POWER_PARKING_BETWEEN_OTHER_PLAYERS), 77, "BEST_POWER_PARKING_BETWEEN = extsb(Profile+0x72)");
        CheckInt(sStats.GetValue(GS::E_INT_VALUE_TYPE_MEDALS_GOLD), 31, "MEDALS_GOLD = GetTotalWinCount out 1 (stw +0x30)");
        CheckInt(sStats.GetValue(GS::E_INT_VALUE_TYPE_MEDALS_SILVER), 32, "MEDALS_SILVER = out 2 (stw +0x34)");
        CheckInt(sStats.GetValue(GS::E_INT_VALUE_TYPE_MEDALS_BRONZE), 33, "MEDALS_BRONZE = out 3 (stw +0x38)");
        CheckInt(sStats.GetValue(GS::E_INT_VALUE_TYPE_NUM_EVENT_MEDALS), 0, "NUM_EVENT_MEDALS never written (Construct's 0)");
        CheckInt(sStats.GetValue(GS::E_INT_VALUE_TYPE_JUMPS), 35, "JUMPS = the first stunt set (Profile+0x75F8)");
        CheckInt(sStats.GetValue(GS::E_INT_VALUE_TYPE_SMASHES), 281, "SMASHES = the second set (+0x8600)");
        CheckInt(sStats.GetValue(GS::E_INT_VALUE_TYPE_STUNTS), 71, "STUNTS = the billboard set (+0x9608)");
        CheckInt(sStats.GetValue(GS::E_INT_VALUE_TYPE_JUMPS_MAX), 50, "JUMPS_MAX = lhz 0x5C4");
        CheckInt(sStats.GetValue(GS::E_INT_VALUE_TYPE_SMASHES_MAX), 400, "SMASHES_MAX = lhz 0x5C6");
        CheckInt(sStats.GetValue(GS::E_INT_VALUE_TYPE_STUNTS_MAX), 120, "STUNTS_MAX = lhz 0x5C8");
        CheckInt(sStats.GetValue(GS::E_INT_VALUE_TYPE_TAKEDOWNS), 3731, "TAKEDOWNS = lwz Profile+0x198");
        CheckInt(sStats.GetValue(GS::E_INT_VALUE_TYPE_BEST_SHOWTIME), 6180200, "BEST_SHOWTIME = lwz Profile+0x264");
        CheckInt(sStats.GetValue(GS::E_INT_VALUE_TYPE_BEST_BOOST_CHAIN), 999, "BEST_BOOST_CHAIN = lwz Profile+0x74");
        CheckInt(sStats.GetValue(GS::E_INT_VALUE_TYPE_BEST_DRIFT), 414, "BEST_DRIFT = fctiwz(Profile+0x258)");
        CheckInt(sStats.GetValue(GS::E_INT_VALUE_TYPE_BEST_ONCOMING), 3167, "BEST_ONCOMING = fctiwz(Profile+0x25C)");
        CheckInt(sStats.GetValue(GS::E_INT_VALUE_TYPE_BEST_NO_BARREL_ROLLS), 2, "BEST_NO_BARREL_ROLLS = lwz Profile+0x24C");
        CheckInt(sStats.GetValue(GS::E_INT_VALUE_TYPE_TOTAL_WINS_FOR_NEXT_RANK), 26, "TOTAL_WINS_FOR_NEXT_RANK = GetTotalWinsForNextRank");
        CheckInt(sStats.GetValue(GS::E_INT_VALUE_TYPE_TOTAL_CARS_TO_SHUTDOWN), 4, "TOTAL_CARS_TO_SHUTDOWN");
        CheckInt(sStats.GetValue(GS::E_INT_VALUE_TYPE_EVENTS_FOUND), 5, "EVENTS_FOUND = events with the found bit");
        CheckInt(sStats.GetValue(GS::E_INT_VALUE_TYPE_EVENTS_TOTAL), 7, "EVENTS_TOTAL = the event count");
        CheckInt(sStats.GetValue(GS::E_INT_VALUE_TYPE_HIGHEST_STUNT_SCORE), 1799200, "HIGHEST_STUNT_SCORE = Profile+0x268");
        CheckInt(sStats.GetValue(GS::E_INT_VALUE_TYPE_TOTALROADSRULED), 11, "TOTALROADSRULED = complete roads (stw +0x90)");
        CheckInt(sStats.GetValue(GS::E_INT_VALUE_TYPE_FREEBURN_CHALLENGES_COMPLETE), 17, "FREEBURN_CHALLENGES_COMPLETE = the 3rd argument (stw +0x98)");
        Check(sStats.GetValue(GS::E_FLOAT_VALUE_TYPE_BEST_AIRTIME) == 7.433375835418701f, "BEST_AIRTIME = stfs Profile+0x260");
        Check(sStats.GetValue(GS::E_FLOAT_VALUE_TYPE_BEST_SPIN) == 373.14398193359375f, "BEST_SPIN = stfs Profile+0x250");
        Check(sStats.GetValue(GS::E_FLOAT_VALUE_PERCENTAGE_COMPLETE) == 73.25f, "PERCENTAGE_COMPLETE = stfs f1 (+0xA4)");
        Check(sStats.GetValue(GS::E_ID_VALUE_TYPE_NEMESIS) == 502, "NEMESIS = the FIRST rival at the maximum takedown count");
        Check(sStats.GetValue(GS::E_ID_VALUE_TYPE_FAVOURITE_CAR) == 0 && sStats.GetValue(GS::E_ID_VALUE_TYPE_FORGOTTEN_CAR) == 0,
              "FAVOURITE / FORGOTTEN car ids re-zeroed");
        bool lbTakedowns = true;
        for (s32 li = 0; li < 13; ++li) lbTakedowns = lbTakedowns && sStats.GetTakedownTypeCount(li) == 1000 + li;
        Check(lbTakedowns, "the 13 takedown-type tallies copied from Profile+0x1A0");
        CheckInt(sStats.GetRoadsRuledCount(0), 12, "roads ruled [0] = par time trial (stw +0xDC)");
        CheckInt(sStats.GetRoadsRuledCount(1), 13, "roads ruled [1] = par showtime (stw +0xE0)");
        CheckInt(sStats.GetTotalRoads(), 64, "total roads = StreetData road count (stw +0x15C)");
        bool lbGrids = true;
        for (s32 lt = 0; lt < 3; ++lt)
            for (s32 lc = 0; lc < 5; ++lc)
                lbGrids = lbGrids && sStats.GetCurrentStuntElementPerCounty(lt, lc) == 10 * lt + lc + 1
                                  && sStats.GetMaxStuntElementPerCounty(lt, lc) == 100 * lt + 10 * lc + 7;
        Check(lbGrids, "both 3x5 district grids (current from Profile+96440, max from StuntManager+0x5CA)");

        sManager.mbFinishedLastRank = true;
        sManager.GetGameStats(&sStats, &lStunts, 17);
        CheckInt(sStats.GetValue(GS::E_INT_VALUE_TYPE_TOTAL_WINS_FOR_NEXT_RANK), -1, "TOTAL_WINS_FOR_NEXT_RANK = -1 on the last rank");
    }

    std::printf("L6GameStatsCars: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
