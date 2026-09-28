// L4 boot order (conductor 2026-09-28, L2's crash-sweep cell h225_s80) -- the start-of-game one-shot seeds the
// PROFILE's car pose with the track's player-start pose.
//
// GameStateModule::SendSetupPlayerCarEvent @0x8239A918 opens with
//     lvx128  v127, memory, 0x10        @0x8239A950   TriggerData player-start position
//     lvx128  v0,   memory, 0x20        @0x8239A978   TriggerData player-start direction
//     stvx128 v127, r31, r10 (0xBCD0)   @0x8239A97C   this+0xBCA0 (the Profile) + 0x30 = mCarPosition
//     stvx128 v0,   r31, r9  (0xBCE0)   @0x8239A980   this+0xBCA0 (the Profile) + 0x40 = mCarDirection
// and only then enters the junkyard nearest the start position. The PC dropped the two stores ("cached pose
// members with no reader"). Since the MemoryCard exit runs OnProfileLoaded @0x82397310 (b5 f935feb8), whose junkyard
// is FindNearestJunkyardID(profile+0x30), a profile whose pose was never seeded -- a fresh one, and every save a PC
// build wrote -- entered the junkyard nearest the ORIGIN (312262) instead of the start junkyard (250700).
//
// This fixture compiles the PRODUCTION SendSetupPlayerCarEvent (extracted by run_junkyard_start_pose.py into
// junkyard_sendsetup.inc) against minimal stand-ins, and replays the boot's two readers of the pose:
//   * FRESH profile:     Profile::Construct @0x823708A8 (pose (0,0,0,0), direction (1,0,0,0)) -> the one-shot ->
//                        no Deserialise -> OnProfileLoaded's junkyard == the start junkyard.
//   * RETURNING profile: the one-shot runs in the first loading-scripted frame, BEFORE the MemoryCard Deserialise
//                        (LoadingScriptedState::Update, bl @0x823F27BC), so the saved pose still wins.
// The two junkyards' positions are their logged spawn points (junkyard_last_car_reboot/20260928_084536 and
// scratch/flow_run/l2rdyoff_h225_s80_r1, "[CarSelectManager::EnterJunkyardAtStartOfGame] junkyard=... pos=") and
// the player start is the logged TriggerData value ("[GameStateModule::SendSetupPlayerCarEvent] ... playerStart=").
// They are TEST DATA for the nearest-junkyard stand-in, not production constants.
#include <cstdint>
#include <cstdio>
#include <cstring>

typedef uint8_t  u8;
typedef int32_t  s32;
typedef uint32_t u32;
typedef uint64_t u64;
typedef float    f32;
typedef u64      CgsID;

struct Vector3 { f32 x, y, z, w; };

namespace CgsDev {
    namespace Log {
        struct Stream
        {
            Stream& operator<<(const char*) { return *this; }
            Stream& operator<<(u64)         { return *this; }
            Stream& operator<<(s32)         { return *this; }
            Stream& operator<<(f32)         { return *this; }
        };
        static Stream  gStream;
        static Stream* gpDebugPrint = &gStream;   // the production witness line runs too
    }
}

namespace BrnTrigger
{
    struct TriggerData
    {
        Vector3 mPlayerStartPosition;
        Vector3 mPlayerStartDirection;
        Vector3 GetPlayerStartPosition() const  { return mPlayerStartPosition; }
        Vector3 GetPlayerStartDirection() const { return mPlayerStartDirection; }
    };
}

namespace BrnResource
{
    struct VehicleListEntry
    {
        CgsID       mId;
        const char* mpcDefaultWheelName;
        CgsID       GetId() const               { return mId; }
        const char* GetDefaultWheelName() const { return mpcDefaultWheelName; }
    };
    struct VehicleList
    {
        VehicleListEntry maEntries[2];
        const VehicleListEntry* GetVehicleData(s32 liIndex) const { return &maEntries[liIndex]; }
    };
    struct WheelListEntry { CgsID mID; const char* mpcName; };
    struct WheelList
    {
        WheelListEntry maEntries[2];
        s32 FindWheelIndexFromName(const char* lpcName) const
        {
            for (s32 li = 0; li < 2; ++li)
                if (std::strcmp(maEntries[li].mpcName, lpcName) == 0)
                    return li;
            return -1;
        }
        const WheelListEntry* GetWheelData(s32 liIndex) const { return &maEntries[liIndex]; }
    };
}

namespace BrnProgression
{
    class Profile
    {
    public:
        // Profile::Construct @0x823708A8: `stvx128 v0(0,0,0,0), r30, 0x30` / `stvx128 v13(1,0,0,0), r30, 0x40`.
        void Construct()
        {
            mCarPosition  = Vector3{0.0f, 0.0f, 0.0f, 0.0f};
            mCarDirection = Vector3{1.0f, 0.0f, 0.0f, 0.0f};
        }
        void    SetCarPosition(Vector3 lPosition)   { mCarPosition = lPosition; }
        void    SetCarDirection(Vector3 lDirection) { mCarDirection = lDirection; }
        Vector3 GetCarPosition() const              { return mCarPosition; }

        Vector3 mCarPosition;
        Vector3 mCarDirection;
    };

    class ProgressionManager
    {
    public:
        Profile* GetProfile() { return &mProfile; }
        Profile  mProfile;
    };
}

namespace BrnGameState
{
    namespace GameStateModuleIO
    {
        struct GameActionQueue { s32 miUnused; };
        enum EPlayerScoringIndex { E_PLAYER_SCORING_INDEX_0 = 0, E_PLAYER_SCORING_INDEX_COUNT = 8 };
    }
    struct CarSelectChangedActionStandIn { s32 miUnused; };

    class CarSelectManager
    {
    public:
        void EnterJunkyardAtStartOfGame(GameStateModuleIO::GameActionQueue* lpQueue, CgsID lJunkyardId, CgsID lCarId,
                                        CgsID lWheelId, GameStateModuleIO::EPlayerScoringIndex leScoringIndex,
                                        CarSelectChangedActionStandIn* lpCached)
        {
            ++miEntries;
            mpQueue = lpQueue; mJunkyardId = lJunkyardId; mCarId = lCarId; mWheelId = lWheelId;
            meScoringIndex = leScoringIndex; mpCached = lpCached;
        }
        s32   miEntries = 0;
        GameStateModuleIO::GameActionQueue* mpQueue = 0;
        CgsID mJunkyardId = 0, mCarId = 0, mWheelId = 0;
        GameStateModuleIO::EPlayerScoringIndex meScoringIndex = GameStateModuleIO::E_PLAYER_SCORING_INDEX_COUNT;
        CarSelectChangedActionStandIn* mpCached = 0;
    };

    struct TriggerQueryManager
    {
        const BrnTrigger::TriggerData* mpTriggerData = 0;
        const BrnTrigger::TriggerData* GetTriggerData() const { return mpTriggerData; }
    };

    static const CgsID KU_JUNKYARD_START  = 250700;   // logged: spawn[1] pos=(2986.933105, 1.009405, -2011.417969)
    static const CgsID KU_JUNKYARD_ORIGIN = 312262;   // logged: spawn[1] pos=(-381.583313, 13.091520, 915.523804)

    class GameStateModule
    {
    public:
        void SendSetupPlayerCarEvent(GameStateModuleIO::GameActionQueue* lpActionQueue);

        // The nearest of the two logged junkyards (squared XZ-plane distance is enough to pick between them).
        CgsID FindNearestJunkyardID(Vector3 lPosition)
        {
            ++miNearestCalls;
            mLastNearestQuery = lPosition;
            const f32 lfStartDx = lPosition.x - 2986.933105f, lfStartDz = lPosition.z - (-2011.417969f);
            const f32 lfOriginDx = lPosition.x - (-381.583313f), lfOriginDz = lPosition.z - 915.523804f;
            return (lfStartDx * lfStartDx + lfStartDz * lfStartDz) <= (lfOriginDx * lfOriginDx + lfOriginDz * lfOriginDz)
                ? KU_JUNKYARD_START : KU_JUNKYARD_ORIGIN;
        }
        GameStateModuleIO::EPlayerScoringIndex FindPlayerScoringIndexForActiveRaceCar(s32)
        {
            return GameStateModuleIO::E_PLAYER_SCORING_INDEX_0;
        }

        TriggerQueryManager                  mTriggerQueryManager;
        const BrnResource::VehicleList*      mpVehicleList = 0;
        const BrnResource::WheelList*        mpWheelList = 0;
        s32                                  mePlayerActiveRaceCarIndex = 0;
        CarSelectManager                     mCarSelectManager;
        CarSelectChangedActionStandIn        mCachedCarSelectChangedAction = {};
        bool                                 mbWaitingToPutPlayerInJunkyard = false;
        BrnProgression::ProgressionManager   mProgressionManager;
        s32                                  miNearestCalls = 0;
        Vector3                              mLastNearestQuery = {};
    };

#include "junkyard_sendsetup.inc"      // the production GameStateModule::SendSetupPlayerCarEvent
}

using namespace BrnGameState;

static s32 giChecks = 0, giFailures = 0;
static void Check(bool lbOk, const char* lpcName)
{
    ++giChecks;
    if (!lbOk) ++giFailures;
    std::printf("%s  %s\n", lbOk ? "PASS" : "FAIL", lpcName);
}
static bool Same(const Vector3& a, const Vector3& b)
{
    return std::memcmp(&a.x, &b.x, sizeof(f32)) == 0 && std::memcmp(&a.y, &b.y, sizeof(f32)) == 0
        && std::memcmp(&a.z, &b.z, sizeof(f32)) == 0;
}

// The logged TriggerData start pose (the direction is illustrative: only its identity is checked).
static const Vector3 KV_START_POSITION  = {2960.891113f, 1.866618f, -1658.474976f, 0.0f};
static const Vector3 KV_START_DIRECTION = {-0.25f, 0.0f, 0.96825f, 0.0f};

static BrnTrigger::TriggerData gTriggerData = {KV_START_POSITION, KV_START_DIRECTION};
static BrnResource::VehicleList gVehicleList = {{{0xC5DE4F3E1A2B0000ull, "WHE_DEFAULT"}, {0x1111ull, "WHE_OTHER"}}};
static BrnResource::WheelList   gWheelList   = {{{0x2F2E000000000000ull, "WHE_OTHER_WHEEL"}, {0x2F31C1D5B7E80000ull, "WHE_DEFAULT"}}};

static void Boot(GameStateModule& lrModule)
{
    lrModule.mTriggerQueryManager.mpTriggerData = &gTriggerData;
    lrModule.mpVehicleList = &gVehicleList;
    lrModule.mpWheelList = &gWheelList;
    lrModule.mProgressionManager.mProfile.Construct();     // ProgressionManager::Prepare -> Profile::Construct
}

int main()
{
    GameStateModuleIO::GameActionQueue lQueue = {};

    // 1. The one-shot itself.
    {
        GameStateModule lModule;
        Boot(lModule);
        lModule.SendSetupPlayerCarEvent(&lQueue);
        const BrnProgression::Profile& lrProfile = lModule.mProgressionManager.mProfile;
        Check(Same(lrProfile.mCarPosition, KV_START_POSITION),
              "the one-shot stores the TriggerData start position into Profile::mCarPosition (stvx128 v127, r31, 0xBCD0 @0x8239A97C)");
        Check(Same(lrProfile.mCarDirection, KV_START_DIRECTION),
              "the one-shot stores the TriggerData start direction into Profile::mCarDirection (stvx128 v0, r31, 0xBCE0 @0x8239A980)");
        Check(lModule.miNearestCalls == 1 && Same(lModule.mLastNearestQuery, KV_START_POSITION),
              "FindNearestJunkyardID is handed the start position (the value step 1 stored)");
        Check(lModule.mCarSelectManager.miEntries == 1 && lModule.mCarSelectManager.mJunkyardId == KU_JUNKYARD_START
              && lModule.mCarSelectManager.mCarId == gVehicleList.maEntries[0].mId
              && lModule.mCarSelectManager.mWheelId == gWheelList.maEntries[1].mID
              && lModule.mCarSelectManager.mpQueue == &lQueue
              && lModule.mCarSelectManager.mpCached == &lModule.mCachedCarSelectChangedAction,
              "EnterJunkyardAtStartOfGame(queue, the start junkyard, VehicleList[0], its default wheel, idx, &cached)");
        Check(lModule.mbWaitingToPutPlayerInJunkyard,
              "the case-78 latch mbWaitingToPutPlayerInJunkyard (+0x38B72) is armed");
    }

    // 2. FRESH profile: Construct -> the one-shot -> no Deserialise -> OnProfileLoaded reads profile+0x30.
    {
        GameStateModule lModule;
        Boot(lModule);
        lModule.SendSetupPlayerCarEvent(&lQueue);
        const CgsID lOnProfileLoadedJunkyard =
            lModule.FindNearestJunkyardID(lModule.mProgressionManager.GetProfile()->GetCarPosition());
        Check(lOnProfileLoadedJunkyard == KU_JUNKYARD_START,
              "a FRESH profile's OnProfileLoaded (FindNearestJunkyardID(profile+0x30) @0x823973D0) enters the START "
              "junkyard 250700, not the junkyard nearest the origin (312262)");
    }

    // 3. RETURNING profile: the one-shot runs BEFORE the Deserialise, so the saved pose wins.
    {
        GameStateModule lModule;
        Boot(lModule);
        lModule.SendSetupPlayerCarEvent(&lQueue);
        BrnProgression::Profile lSaved;
        lSaved.Construct();
        lSaved.SetCarPosition(Vector3{-379.25f, 12.5f, 913.0f, 0.0f});      // a save made at junkyard 312262
        lModule.mProgressionManager.mProfile = lSaved;                     // the MemoryCard Deserialise
        const CgsID lJunkyard =
            lModule.FindNearestJunkyardID(lModule.mProgressionManager.GetProfile()->GetCarPosition());
        Check(lJunkyard == KU_JUNKYARD_ORIGIN,
              "a RETURNING profile keeps its SAVED pose: the Deserialise after the one-shot restores it, and "
              "OnProfileLoaded enters the saved junkyard");
    }

    // 4. The guards: no TriggerData -> nothing; no vehicle list -> the pose is still seeded (step 1 precedes the
    //    console's list reads), but there is no junkyard entry.
    {
        GameStateModule lModule;
        Boot(lModule);
        lModule.mTriggerQueryManager.mpTriggerData = 0;
        lModule.SendSetupPlayerCarEvent(&lQueue);
        Check(Same(lModule.mProgressionManager.mProfile.mCarPosition, Vector3{0.0f, 0.0f, 0.0f, 0.0f})
              && lModule.mCarSelectManager.miEntries == 0 && !lModule.mbWaitingToPutPlayerInJunkyard,
              "[PC guard] no TriggerData: no store, no junkyard entry");
    }
    {
        GameStateModule lModule;
        Boot(lModule);
        lModule.mpVehicleList = 0;
        lModule.SendSetupPlayerCarEvent(&lQueue);
        Check(Same(lModule.mProgressionManager.mProfile.mCarPosition, KV_START_POSITION)
              && lModule.mCarSelectManager.miEntries == 0,
              "[PC guard] no vehicle list: the pose is seeded first (as the console orders it), no junkyard entry");
    }

    std::printf("JunkyardStartPose: %d checks, %d failures\n", giChecks, giFailures);
    return giFailures == 0 ? 0 : 1;
}
