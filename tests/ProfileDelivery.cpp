// L4 boot order (owner's list 2026-09-28) -- the loaded profile reaches the game state.
//
// On the console the boot profile is handed to the game state by ProcessGameEvents @0x823A0A18 case 8 (the
// MemoryCard exit): `if (mbWaitForStreaming) { OnProfileLoaded(); WaitForStreaming(); }`. OnProfileLoaded
// @0x82397310 spawns the player in the car GetSpawnCar @0x823763C8 picks and calls ProgressionManager::
// OnLoadProfile @0x823893A8, which restores the rank cache and re-derives the max-car count. None of the three had
// a body on the PC. This fixture compiles the two PRODUCTION leaves (extracted by run_profile_delivery.py into
// profile_getspawncar.inc and profile_onloadprofile.inc) against minimal stand-ins and checks their answers:
//   GetSpawnCar   -- the saved car, unless the player's rank is below the car's unlock rank (VehicleListEntry
//                    +0x99): then PUSMC01. A car the list does not know asserts (cpp:6664).
//   OnLoadProfile -- the rank cache is the profile's rank; the medal / rival / drive-thru requests are raised and
//                    the trophy / ranked-up / newly-unlocked state cleared; miMaxCarCount = selectable - sponsor
//                    + one per owned sponsor car + one for an owned CARBEAGT; silver cars unlocked -> the elite
//                    completion sequence is marked seen.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

typedef uint8_t  u8;
typedef int8_t   s8;
typedef uint32_t u32;
typedef int32_t  s32;
typedef uint64_t u64;
typedef float    f32;
typedef u64      CgsID;

static u32 guAsserts = 0;
namespace CgsDev {
    namespace Assert {
        inline void BeginAssert() {}
        inline void FireAssert(const char*, const char*, s32) { ++guAsserts; }
        inline void EndAssert() {}
    }
    namespace Message { static u64 gxMessageFilterFlags = 0; }
    namespace Log {
        struct Stream
        {
            Stream& operator<<(const char*) { return *this; }
            Stream& operator<<(s32)         { return *this; }
        };
        static Stream* gpDebugPrint = 0;
    }
}

// A deterministic stand-in for the id packer: equal strings give equal ids, the production bodies compare ids only.
inline CgsID CgsIDCompress(const char* lpcName)
{
    u64 lu64Hash = 1469598103934665603ull;
    for (const char* lpc = lpcName; *lpc; ++lpc)
        lu64Hash = (lu64Hash ^ static_cast<u8>(*lpc)) * 1099511628211ull;
    return lu64Hash;
}

static const char* const KAC_GSM_FILE       = "BrnGameStateModule.cpp";
static const char* const KAC_PROFILE_H_FILE = "BrnProfile.h";

namespace BrnResource
{
    struct VehicleListEntry
    {
        CgsID mId;
        u8    mu8UnlockRank;
        u8    GetUnlockRank() const { return mu8UnlockRank; }
    };
    struct VehicleList
    {
        VehicleListEntry maEntries[8];
        s32              miCount;
        s32              miSelectable;
        s32              miSponsor;
        s32 GetVehicleIndex(CgsID lId) const
        {
            for (s32 li = 0; li < miCount; ++li)
                if (maEntries[li].mId == lId)
                    return li;
            return -1;
        }
        const VehicleListEntry* GetVehicleData(s32 liIndex) const { return &maEntries[liIndex]; }
        s32 GetSelectableVehicleCount() const { return miSelectable; }
        s32 GetSponsorVehicleCount() const    { return miSponsor; }
    };
}

namespace BrnProgression
{
    struct CarData
    {
        enum UnlockType { E_UNLOCK_TYPE_UNLOCK = 0, E_UNLOCK_TYPE_GIFT = 1, E_UNLOCK_TYPE_TROPHY = 2,
                          E_UNLOCK_TYPE_SHUTDOWN_RIVAL = 3, E_UNLOCK_TYPE_GOLD_SILVER = 4, E_UNLOCK_TYPE_SPONSOR = 5 };
        CgsID      mId;
        UnlockType meUnlockType;
        CgsID      GetId() const         { return mId; }
        UnlockType GetUnlockType() const { return meUnlockType; }
    };

    class Profile
    {
    public:
        CgsID          GetSpawnCarId() const             { return mSpawnCarId; }
        s8             GetCurrentProgressionRank() const { return mi8CurrentProgressionRank; }
        s32            GetCarCount() const               { return miCarCount; }
        const CarData* GetCarData(s32 liIndex) const
        {
            if (!(liIndex >= 0 && liIndex < miCarCount))
                ++guAsserts;                              // "liCarIndex >= 0 && liCarIndex < miCarCount"
            return &maCars[liIndex];
        }
        bool AreSilverCarsUnlocked() const    { return mbSilverCarsUnlocked; }
        void SetSeenEliteCompletionSequence() { mbHaveSeenEliteCompletionSequence = true; }

        CgsID   mSpawnCarId = 0;
        s8      mi8CurrentProgressionRank = 0;
        CarData maCars[8] = {};
        s32     miCarCount = 0;
        bool    mbSilverCarsUnlocked = false;
        bool    mbHaveSeenEliteCompletionSequence = false;
    };

    class ProgressionManager
    {
    public:
        void OnLoadProfile();
        void UnlockDefaultPlayerCars() { ++miUnlockDefaultPlayerCarsCalls; }
        s32  GetProgressionRank() const { return mi8ProgressionRank < 0 ? 0 : mi8ProgressionRank; }

        Profile                         mProfile;
        const BrnResource::VehicleList* mpVehicleList = 0;
        s32   miMaxCarCount = -1;
        CgsID mNewlyUnlockedCarID = 0x1234;
        s8    mi8ProgressionRank = -2;
        bool  mbHasJustRankedUp = true;
        bool  mbUpdateRivalsRequested = false;
        bool  mbPlayerMedalsUpdateRequired = false;
        bool  mbPlayerJustWonATrophyUpdateRequired = true;
        bool  mbDriveThrusDirty = false;
        s32   miUnlockDefaultPlayerCarsCalls = 0;
    };

#include "profile_onloadprofile.inc"       // the production ProgressionManager::OnLoadProfile
}

namespace BrnGameState
{
    class GameStateModule
    {
    public:
        CgsID GetSpawnCar(const BrnProgression::Profile* lpProfile);
        const BrnResource::VehicleList*    mpVehicleList = 0;
        BrnProgression::ProgressionManager mProgressionManager;
    };

#include "profile_getspawncar.inc"         // the production GameStateModule::GetSpawnCar
}

static u32 guChecks   = 0;
static u32 guFailures = 0;

static void Check(bool lbPassed, const char* lpcLabel)
{
    ++guChecks;
    if (!lbPassed)
        ++guFailures;
    std::printf("%s  %s\n", lbPassed ? "PASS" : "FAIL", lpcLabel);
}

int main()
{
    const CgsID lDefault  = CgsIDCompress("PUSMC01");
    const CgsID lSavedCar = CgsIDCompress("PUSRC01");
    const CgsID lSponsorA = CgsIDCompress("PUSSP01");
    const CgsID lSponsorB = CgsIDCompress("PUSSP02");
    const CgsID lBeagt    = CgsIDCompress("CARBEAGT");

    static BrnResource::VehicleList sList = {};
    sList.maEntries[0] = { lDefault,  0 };
    sList.maEntries[1] = { lSavedCar, 3 };
    sList.maEntries[2] = { lSponsorA, 0 };
    sList.miCount = 3;
    sList.miSelectable = 120;
    sList.miSponsor = 12;

    // ---- GetSpawnCar @0x823763C8 --------------------------------------------------------------------------------
    {
        static BrnGameState::GameStateModule sModule;
        sModule.mpVehicleList = &sList;
        BrnProgression::Profile lProfile;
        lProfile.mSpawnCarId = lSavedCar;

        sModule.mProgressionManager.mi8ProgressionRank = 5;
        guAsserts = 0;
        Check(sModule.GetSpawnCar(&lProfile) == lSavedCar && guAsserts == 0,
              "rank 5 >= the saved car's unlock rank 3: GetSpawnCar keeps the saved car");
        sModule.mProgressionManager.mi8ProgressionRank = 3;
        Check(sModule.GetSpawnCar(&lProfile) == lSavedCar,
              "rank 3 == unlock rank 3: kept (the console's test is `blt`, strictly below)");
        sModule.mProgressionManager.mi8ProgressionRank = 2;
        Check(sModule.GetSpawnCar(&lProfile) == lDefault,
              "rank 2 < unlock rank 3: GetSpawnCar falls back to CgsIDCompress(\"PUSMC01\")");
        lProfile.mSpawnCarId = lSponsorA;
        sModule.mProgressionManager.mi8ProgressionRank = 0;
        Check(sModule.GetSpawnCar(&lProfile) == lSponsorA && guAsserts == 0,
              "a car with unlock rank 0 is kept at rank 0, without an assert");
        lProfile.mSpawnCarId = CgsIDCompress("NOTINLIST");
        guAsserts = 0;
        Check(sModule.GetSpawnCar(&lProfile) == lDefault && guAsserts == 1,
              "a car the vehicle list does not know fires lpVehicleListEntry != NULL (cpp:6664) and answers PUSMC01");
    }

    // ---- ProgressionManager::OnLoadProfile @0x823893A8 ----------------------------------------------------------
    {
        static BrnProgression::ProgressionManager sManager;
        sManager.mpVehicleList = &sList;
        BrnProgression::Profile& lrProfile = sManager.mProfile;
        lrProfile.mi8CurrentProgressionRank = 6;
        lrProfile.maCars[0] = { lDefault,  BrnProgression::CarData::E_UNLOCK_TYPE_UNLOCK };
        lrProfile.maCars[1] = { lSponsorA, BrnProgression::CarData::E_UNLOCK_TYPE_SPONSOR };
        lrProfile.maCars[2] = { lBeagt,    BrnProgression::CarData::E_UNLOCK_TYPE_TROPHY };
        lrProfile.maCars[3] = { lSponsorB, BrnProgression::CarData::E_UNLOCK_TYPE_SPONSOR };
        lrProfile.maCars[4] = { lSavedCar, BrnProgression::CarData::E_UNLOCK_TYPE_GIFT };
        lrProfile.miCarCount = 5;
        lrProfile.mbSilverCarsUnlocked = true;

        guAsserts = 0;
        sManager.OnLoadProfile();
        Check(sManager.miUnlockDefaultPlayerCarsCalls == 1, "OnLoadProfile runs UnlockDefaultPlayerCars first (0x823893B8)");
        Check(sManager.mi8ProgressionRank == 6, "the rank cache (+0x2096C) is the loaded profile's rank (Profile+0x70)");
        Check(sManager.miMaxCarCount == 120 - 12 + 2 + 1,
              "miMaxCarCount = selectable 120 - sponsor 12, + 1 per owned sponsor car (2) + 1 for CARBEAGT = 111");
        Check(sManager.mbPlayerMedalsUpdateRequired && sManager.mbUpdateRivalsRequested && sManager.mbDriveThrusDirty,
              "the medal (+0x20973), rival (+0x20971) and drive-thru (+0x20988) requests are raised");
        Check(!sManager.mbPlayerJustWonATrophyUpdateRequired && !sManager.mbHasJustRankedUp
              && sManager.mNewlyUnlockedCarID == 0,
              "the trophy request (+0x20974), mbHasJustRankedUp (+0x20970) and mNewlyUnlockedCarID (+0x20960) are cleared");
        Check(lrProfile.mbHaveSeenEliteCompletionSequence,
              "silver cars unlocked (Profile+42516): the elite completion sequence (Profile+118038) is marked seen");
        Check(guAsserts == 0, "the car walk stays inside miCarCount (no GetCarData bounds assert)");

        lrProfile.mi8CurrentProgressionRank = -2;
        lrProfile.mbSilverCarsUnlocked = false;
        lrProfile.mbHaveSeenEliteCompletionSequence = false;
        lrProfile.miCarCount = 1;
        sManager.mbPlayerMedalsUpdateRequired = false;
        sManager.OnLoadProfile();
        Check(sManager.mi8ProgressionRank == -2 && sManager.mbPlayerMedalsUpdateRequired,
              "a profile with no rank yet (-2): the cache holds -2 and the medal request is raised");
        Check(sManager.miMaxCarCount == 108 && !lrProfile.mbHaveSeenEliteCompletionSequence,
              "no sponsor car owned: miMaxCarCount is the list's 108; silver cars locked: the elite flag is untouched");
    }

    std::printf("ProfileDelivery: %u checks, %u failures\n", guChecks, guFailures);
    return guFailures == 0 ? 0 : 1;
}
