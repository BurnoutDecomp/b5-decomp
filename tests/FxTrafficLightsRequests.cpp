// crash parity FX-TRAFFICLIGHTS (2026-09-25): the traffic's game-action dispatch, complete.
// run_fxtrafficlights_requests.py extracts the PRODUCTION bodies and compiles them against this fixture:
//   TrafficEntityModule::HandleExternalRequests @0x8274B660, HideAllTraffic @0x8273F418, UnhideAllTraffic @0x8274A500,
//   FireKillZone @0x827343B8 and PostPhysicsUpdate's two latched StreamingCompleteEvent answers (0x8274E744..0x8274E794,
//   0x8274EC74..0x8274ECB0) as members of ReqFixture (requests_bodies.inc);
//   TrafficData::FindKillZone @0x827570A0 / GetKillZoneRegions @0x82705D08 (BrnTrafficData.cpp) and
//   HullRuntime::GetFirstParamInSection @0x82706768 (BrnTrafficHullRuntime.cpp) as themselves (requests_other_bodies.inc).
// ReqFixture carries the module members those bodies read with their real types. KillAllTrafficInCylinder, RemoveVehicle
// and the callees of the arms not under test are recording doubles. The actions go through a real
// CgsModule::VariableEventQueue<13312,16>; the answers into a real VariableEventQueue<1536,16>.
// The two race-car interface accessors are their production bodies (BrnRCEntityActiveRaceCarOutputInterface.cpp,
// not linked). Every expected number is the ARTIST asm's (see the runner).
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficTrackWitness.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficHullRuntime.h"
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h"
#include "GameSource/GameState/BrnGameActions.h"
#include "GameSource/GameState/BrnGameEvents.h"
#include "GameSource/GameState/TriggerQueryManager/BrnKillzoneAction.h"
#include "SharedClasses/Traffic/BrnTrafficDataResourceType.h"
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "GameShared/GameClasses/System/PC/BrnNetHarnessPC.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <new>
#include <string>
#include <vector>

static unsigned    gAsserts = 0, gChecks = 0, gFailures = 0;
static std::string gAssertLog;

namespace CgsDev
{
namespace Assert
{
    char* gpcMessageBuffer = nullptr;
    int   BeginAssert() { return 0; }
    int   FireAssert(const char* lpcMessage, const char*, int)
    {
        ++gAsserts;
        gAssertLog += lpcMessage;
        gAssertLog += "|";
        return 0;
    }
    void* EndAssert() { return nullptr; }
}
namespace Log { DebugPrint* gpDebugPrint = nullptr; }
namespace Message { unsigned long long gxMessageFilterFlags = 0; }
}

namespace BrnNetHarnessPC
{
    void WitnessTag(const char*, const char*, const char*, ...) {}
}

// The two accessors arm 30 calls, as their production bodies (BrnRCEntityActiveRaceCarOutputInterface.cpp: IsRaceCarActive
// @0x82277B10 `lhzx +0x2780+2i ; clrlwi 31`, GetRaceCarState :220 &maRaceCarStates[i]). That TU is not linked.
namespace BrnWorld
{
namespace RaceCarEntityModuleIO
{
    bool RCEntityActiveRaceCarOutputInterface::IsRaceCarActive(EActiveRaceCarIndex leIndex) const
    {
        return (maxRaceCarFlags[leIndex] & 1) != 0;
    }
    const RCEntityActiveRaceCarOutputInterface::RaceCarState*
    RCEntityActiveRaceCarOutputInterface::GetRaceCarState(EActiveRaceCarIndex leIndex) const
    {
        return &maRaceCarStates[leIndex];
    }
}
}

namespace BrnTraffic
{
    const char* gpcTrafficRemoveReason = nullptr;

namespace
{
    // The complete handler logs no gate; an older revision's does.
    inline void LogMissingLeg_T6(bool& lrbAlreadyLogged, const char*) { lrbAlreadyLogged = true; }
    CgsDev::Log::DebugPrint* TrafficDiagStream() { return nullptr; }
    CgsDev::Log::DebugPrint* NetCrashDiagStream() { return nullptr; }
    const s32 KI_NETCRASH_HULL_DIAG_MAX_LINES = 48;
}

#include "requests_other_bodies.inc"

    struct FakeInput
    {
        typedef CgsModule::VariableEventQueue<13312, 16> GameActionQueueStorage;
        GameActionQueueStorage mQueue;
        const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* mpRaceCars = nullptr;
        const GameActionQueueStorage* GetGameActionQueue() const { return &mQueue; }
        const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* GetActiveRaceCarOutputInterface() const
        {
            return mpRaceCars;
        }
    };
    struct FakeOutput
    {
        typedef CgsModule::VariableEventQueue<1536, 16> GameEventQueue;
        GameEventQueue mGameEvents;
        GameEventQueue* GetGameEventQueue() { return &mGameEvents; }
    };
    struct FakeDataPtr
    {
        TrafficData* mp;
        TrafficData* operator->() { return mp; }
    };
    struct FakeLightManager
    {
        unsigned muCalls;
        void SetCountdownValue(s32) { ++muCalls; }
    };
    struct FakeStreamer
    {
        bool mbLoaded;
        bool AreAllAssetsLoaded() const { return mbLoaded; }
    };

    struct Cylinder
    {
        Vector3 mCentre;
        f32     mfRadius;
        f32     mfHeight;
        bool    mbStatic;
        std::string mReason;
    };

    static HullRuntime gaRuntimes[16];

    struct ReqFixture
    {
        typedef TrafficEntityModule M;
        typedef M::EState                EState;
        typedef M::ERunningState         ERunningState;
        typedef M::EEmptyTrafficPoolState EEmptyTrafficPoolState;
        static const EState                 E_STATE_STARTING_UP              = M::E_STATE_STARTING_UP;
        static const EState                 E_STATE_RUNNING                  = M::E_STATE_RUNNING;
        static const EState                 E_STATE_TEARING_DOWN             = M::E_STATE_TEARING_DOWN;
        static const ERunningState          E_RUNNINGSTATE_NORMAL            = M::E_RUNNINGSTATE_NORMAL;
        static const EEmptyTrafficPoolState E_EMPTYTRAFFICPOOLSTATE_IDLE     = M::E_EMPTYTRAFFICPOOLSTATE_IDLE;
        static const EEmptyTrafficPoolState E_EMPTYTRAFFICPOOLSTATE_EMPTYING = M::E_EMPTYTRAFFICPOOLSTATE_EMPTYING;
        static const EEmptyTrafficPoolState E_EMPTYTRAFFICPOOLSTATE_EMPTY    = M::E_EMPTYTRAFFICPOOLSTATE_EMPTY;
        static const EEmptyTrafficPoolState E_EMPTYTRAFFICPOOLSTATE_FILLING  = M::E_EMPTYTRAFFICPOOLSTATE_FILLING;

        // ---- the members the bodies read (names and types as in BrnTrafficEntityModule.h) -----------------------
        decltype(M::meState)                                  meState;
        decltype(M::meRunningState)                           meRunningState;
        decltype(M::meRunningStateToUseAfterStartup)          meRunningStateToUseAfterStartup;
        decltype(M::meEmptyTrafficPoolState)                  meEmptyTrafficPoolState;
        decltype(M::mbIsOnlineGameMode)                       mbIsOnlineGameMode;
        decltype(M::mbAllowDivergentBehaviour)                mbAllowDivergentBehaviour;
        decltype(M::mbGameModeClearsTraffic)                  mbGameModeClearsTraffic;
        decltype(M::mbAtStartLineSoProtectRaceCarsFromTraffic) mbAtStartLineSoProtectRaceCarsFromTraffic;
        decltype(M::mbGameModeAllowsKillzones)                mbGameModeAllowsKillzones;
        decltype(M::mbNeedToSetUpLightsForEventStart)         mbNeedToSetUpLightsForEventStart;
        decltype(M::mbDEBUGTurnTrafficOff)                    mbDEBUGTurnTrafficOff;
        decltype(M::mbDEBUGEnableKillzones)                   mbDEBUGEnableKillzones;
        decltype(M::mbWaitingForStreaming)                    mbWaitingForStreaming;
        decltype(M::mbTrafficIsHidden)                        mbTrafficIsHidden;
        decltype(M::mbInPictureParadise)                      mbInPictureParadise;
        decltype(M::mfBaseDensityScale)                       mfBaseDensityScale;
        decltype(M::mfGameModeDensityScale)                   mfGameModeDensityScale;
        decltype(M::mfTrafficSimRadius)                       mfTrafficSimRadius;
        decltype(M::muMaxVehiclesToRender)                    muMaxVehiclesToRender;
        decltype(M::mfRenderCullDistanceSq)                   mfRenderCullDistanceSq;
        decltype(M::mbInOfflineCarSelect)                     mbInOfflineCarSelect;
        decltype(M::mLocalPlayerPosition)                     mLocalPlayerPosition;
        decltype(M::meLocalPlayerIndex)                       meLocalPlayerIndex;
        decltype(M::muCurrentlyPredictedHull)                 muCurrentlyPredictedHull;
        decltype(M::mbActivateOnlineHullsAfterReset)          mbActivateOnlineHullsAfterReset;
        decltype(M::mau16HullsToActivateAfterReset)           mau16HullsToActivateAfterReset;
        decltype(M::mCameraLastFrame)                         mCameraLastFrame;
        decltype(M::mActiveHulls)                             mActiveHulls;
        decltype(M::maParamListNodes)                         maParamListNodes;
        decltype(M::mVehicleSoaData)                          mVehicleSoaData;
        decltype(M::mDEBUGRecentlyFiredKillZones)             mDEBUGRecentlyFiredKillZones;
        Vehicle                                               maVehicles[KU_MAX_TOTAL_TRAFFIC];
        FakeDataPtr                                           mpData;
        FakeLightManager                                      mTrafficLightManager;
        FakeStreamer                                          mStreamer;

        // ---- recording doubles ----------------------------------------------------------------------------------
        std::vector<Cylinder> maCylinders;
        std::vector<u32>      mauRemoved;
        std::vector<u64>      mauFiredForwarded;   // unused: FireKillZone is the production body
        unsigned muPrepareCalls = 0, muStopCalls = 0, muClearupCalls = 0, muRestarts = 0, muGetHullCalls = 0;

        void HandlePrepareForModeAction(const FakeInput*, const BrnGameState::GameStateModuleIO::PrepareForModeAction*)
        { ++muPrepareCalls; }
        void HandleStopModeAction(const FakeInput*, const BrnGameState::GameStateModuleIO::StopModeAction*)
        { ++muStopCalls; }
        void ClearupCrashedTraffic() { ++muClearupCalls; }
        void RestartTraffic() { ++muRestarts; }
        bool IsPaused() { return false; }
        void KillAllTrafficInCylinder(Vector3 lvCentre, f32 lfRadius, f32 lfHeight, bool lbIncludeStatic)
        {
            Cylinder lCylinder;
            lCylinder.mCentre  = lvCentre;
            lCylinder.mfRadius = lfRadius;
            lCylinder.mfHeight = lfHeight;
            lCylinder.mbStatic = lbIncludeStatic;
            lCylinder.mReason  = gpcTrafficRemoveReason ? gpcTrafficRemoveReason : "";
            maCylinders.push_back(lCylinder);
        }
        void RemoveVehicle(u32 luVehicle) { mauRemoved.push_back(luVehicle); }
        const Hull* GetHull(u32) { ++muGetHullCalls; return nullptr; }
        HullRuntime* GetHullRuntime(u32 luHull)
        {
            CGS_ASSERT(luHull < 16u, "GetHullRuntime: hull out of the fixture's range");
            return &gaRuntimes[luHull];
        }
        Vehicle* GetVehicle(u32 luIndex)
        {
            CGS_ASSERT(luIndex < KU_MAX_TOTAL_TRAFFIC, "luIndex < KU_MAX_TOTAL_TRAFFIC");
            return &maVehicles[luIndex];
        }

        // ---- the production bodies ------------------------------------------------------------------------------
        void HandleExternalRequests(const FakeInput* lpInput, FakeOutput* lpOutput);
        void HideAllTraffic();
        void UnhideAllTraffic();
        void FireKillZone(u64 lKillZoneId);
        void LatchedAnswerHead(FakeOutput* lpOutput);
        void LatchedAnswerStartup(FakeOutput* lpOutput);
    };

#include "requests_bodies.inc"
}

using namespace BrnTraffic;
using namespace BrnGameState::GameStateModuleIO;

static void Check(bool lbPass, const char* lpcWhat)
{
    ++gChecks;
    if (!lbPass)
    {
        ++gFailures;
        std::printf("FAIL: %s\n", lpcWhat);
    }
}

static u32 Bits(f32 lfValue)
{
    u32 luBits;
    std::memcpy(&luBits, &lfValue, sizeof(luBits));
    return luBits;
}
static f32 FromBits(u32 luBits)
{
    f32 lfValue;
    std::memcpy(&lfValue, &luBits, sizeof(lfValue));
    return lfValue;
}
static bool Same(const Vector3& lrA, f32 lfX, f32 lfY, f32 lfZ)
{
    return Bits(lrA.x) == Bits(lfX) && Bits(lrA.y) == Bits(lfY) && Bits(lrA.z) == Bits(lfZ);
}
static bool Splat(const decltype(ReqFixture::mfTrafficSimRadius)& lrV, f32 lfValue)
{
    return Bits(lrV.x) == Bits(lfValue) && Bits(lrV.y) == Bits(lfValue) && Bits(lrV.z) == Bits(lfValue)
        && Bits(lrV.w) == Bits(lfValue);
}

static ReqFixture                                                  gM;
static FakeInput                                                   gIn;
static FakeOutput                                                  gOut;
// The interface is used as raw, zeroed storage: its constructor's RaceCarState::Clear lives in a TU this test does
// not link, and arm 30 reads only maxRaceCarFlags and maRaceCarStates[i].mTransform.
alignas(16) static unsigned char gau8RaceCarsStorage[sizeof(BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface)];
static BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface& gRaceCars =
    *reinterpret_cast<BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface*>(gau8RaceCarsStorage);
static TrafficData                                                 gData;

static void Fresh()
{
    gM.~ReqFixture();
    new (&gM) ReqFixture();
    gM.meState                         = ReqFixture::E_STATE_RUNNING;
    gM.meEmptyTrafficPoolState         = ReqFixture::E_EMPTYTRAFFICPOOLSTATE_IDLE;
    gM.mbIsOnlineGameMode              = false;
    gM.mbAllowDivergentBehaviour       = true;
    gM.mbGameModeAllowsKillzones       = true;
    gM.mbDEBUGEnableKillzones          = true;
    gM.mbDEBUGTurnTrafficOff           = false;
    gM.mbWaitingForStreaming           = false;
    gM.mbTrafficIsHidden               = false;
    gM.mbInPictureParadise             = false;
    gM.mbGameModeClearsTraffic         = false;
    gM.mbAtStartLineSoProtectRaceCarsFromTraffic = true;
    gM.mfBaseDensityScale              = 1.0f;
    gM.mfGameModeDensityScale          = 0.8f;
    gM.mfTrafficSimRadius.x = gM.mfTrafficSimRadius.y = gM.mfTrafficSimRadius.z = gM.mfTrafficSimRadius.w = 195.0f;
    gM.muMaxVehiclesToRender           = 32;
    gM.mfRenderCullDistanceSq          = 62500.0f;
    gM.mbInOfflineCarSelect            = false;
    gM.mLocalPlayerPosition            = Vector3{ 100.0f, 5.0f, 200.0f, 0.0f };
    gM.meLocalPlayerIndex              = E_ACTIVE_RACE_CAR_INDEX_0;
    gM.mActiveHulls.Construct();
    gM.mDEBUGRecentlyFiredKillZones.Clear();
    gM.mVehicleSoaData.mAliveVehicles.Construct();
    gM.mVehicleSoaData.mPhysicalVehicles.Construct();
    std::memset(&gM.mCameraLastFrame, 0, sizeof(gM.mCameraLastFrame));
    std::memset(gM.maVehicles, 0, sizeof(gM.maVehicles));
    for (u32 luParam = 0; luParam < KU_MAX_PARAMS; ++luParam)
    {
        gM.maParamListNodes[luParam].muNextParam = static_cast<u16>(KU_INVALID_PARAM);
        gM.maParamListNodes[luParam].muPrevParam = static_cast<u16>(KU_INVALID_PARAM);
        gM.maParamListNodes[luParam].mfParamAlong = 0.0f;
    }
    gM.mpData.mp = &gData;
    gM.mStreamer.mbLoaded = false;
    gIn.mQueue.Construct();
    gIn.mQueue.Clear();
    gIn.mpRaceCars = &gRaceCars;
    gOut.mGameEvents.Construct();
    gOut.mGameEvents.Clear();
}

template <typename T>
static void Post(const T& lrRecord, s32 liType)
{
    gIn.mQueue.AddEvent(reinterpret_cast<const CgsModule::Event*>(&lrRecord), liType, static_cast<s32>(sizeof(T)));
}
static void PostBytes(const void* lpData, s32 liType, s32 liSize)
{
    gIn.mQueue.AddEvent(reinterpret_cast<const CgsModule::Event*>(lpData), liType, liSize);
}
static void Run()
{
    gM.HandleExternalRequests(&gIn, &gOut);
    gIn.mQueue.Clear();
}

// The one game event the output queue holds, or type -1.
static s32 OnlyGameEvent(const StreamingCompleteEvent** lppEvent, s32* lpiSize, s32* lpiCount)
{
    const CgsModule::Event* lpEvent = nullptr;
    s32 liSize = 0;
    s32 liType = gOut.mGameEvents.GetFirstEvent(&lpEvent, &liSize);
    s32 liFirstType = lpEvent ? liType : -1;
    *lppEvent = reinterpret_cast<const StreamingCompleteEvent*>(lpEvent);
    *lpiSize = liSize;
    s32 liCount = 0;
    while (lpEvent != nullptr)
    {
        ++liCount;
        liType = gOut.mGameEvents.GetNextEvent(lpEvent, &lpEvent, &liSize);
    }
    *lpiCount = liCount;
    return liFirstType;
}

// ---- the kill-zone data -------------------------------------------------------------------------------------------
// Ids sorted as the builder writes them; the last one has bit 63 set, so a SIGNED compare would search the wrong half.
static u64            gau64Ids[5] = { 0x100ull, 0x2000ull, 0x30000ull, 0x7FFFFFFFFFFFFFFFull, 0x8000000000000001ull };
static KillZone       gaZones[5];
static KillZoneRegion gaRegions[6];

static void SetUpKillZoneData()
{
    std::memset(&gData, 0, sizeof(gData));
    gData.muNumKillZones       = 5;
    gData.muNumKillZoneRegions = 6;
    gData.mpaKillZoneIds       = gau64Ids;
    gData.mpaKillZones         = gaZones;
    gData.mpaKillZoneRegions   = gaRegions;
    // 0x100: one region in hull 9 (never active)
    gaZones[0].muOffset = 0; gaZones[0].muCount = 1;
    gaRegions[0].muHull = 9; gaRegions[0].muSection = 0; gaRegions[0].muStartRung = 0; gaRegions[0].muEndRung = 10;
    // 0x2000: hull 5 section 1 rungs 2..4, then hull 5 section 3 (an empty section)
    gaZones[1].muOffset = 1; gaZones[1].muCount = 2;
    gaRegions[1].muHull = 5; gaRegions[1].muSection = 1; gaRegions[1].muStartRung = 2; gaRegions[1].muEndRung = 4;
    gaRegions[2].muHull = 5; gaRegions[2].muSection = 3; gaRegions[2].muStartRung = 0; gaRegions[2].muEndRung = 9;
    // 0x30000: hull 5 section 2 rungs 5..6 (a NaN param)
    gaZones[2].muOffset = 3; gaZones[2].muCount = 1;
    gaRegions[3].muHull = 5; gaRegions[3].muSection = 2; gaRegions[3].muStartRung = 5; gaRegions[3].muEndRung = 6;
    // 0x7FFF...: hull 6 section 0 rungs 0..255
    gaZones[3].muOffset = 4; gaZones[3].muCount = 1;
    gaRegions[4].muHull = 6; gaRegions[4].muSection = 0; gaRegions[4].muStartRung = 0; gaRegions[4].muEndRung = 255;
    // 0x8000...1: hull 6 section 1 rungs 0..1
    gaZones[4].muOffset = 5; gaZones[4].muCount = 1;
    gaRegions[5].muHull = 6; gaRegions[5].muSection = 1; gaRegions[5].muStartRung = 0; gaRegions[5].muEndRung = 1;
}

static void Link(const u16* lpau16Params, const f32* lpafAlong, u32 luCount)
{
    for (u32 i = 0; i < luCount; ++i)
    {
        ParamListNode& lrNode = gM.maParamListNodes[lpau16Params[i]];
        lrNode.mfParamAlong = lpafAlong[i];
        lrNode.muNextParam  = (i + 1 < luCount) ? lpau16Params[i + 1] : static_cast<u16>(KU_INVALID_PARAM);
    }
}

static void SetUpLanes()
{
    for (u32 luHull = 0; luHull < 16; ++luHull)
    {
        std::memset(&gaRuntimes[luHull], 0, sizeof(gaRuntimes[luHull]));
        gaRuntimes[luHull].mbPrepared          = true;
        gaRuntimes[luHull].muNumSectionsInHull = 4;
        for (u32 luSection = 0; luSection < 4; ++luSection)
        {
            gaRuntimes[luHull].mauFirstParamInSection[luSection] = static_cast<u16>(KU_INVALID_PARAM);
        }
    }
    // hull 5 section 1: 10 (1.0) 11 (1.99) 12 (2.0) 20 (3.0, dead) 13 (3.5, physical) 14 (4.0) 15 (4.01)
    const u16 kau16Section1[7] = { 10, 11, 12, 20, 13, 14, 15 };
    const f32 kafSection1[7]   = { 1.0f, 1.99f, 2.0f, 3.0f, 3.5f, 4.0f, 4.01f };
    Link(kau16Section1, kafSection1, 7);
    gaRuntimes[5].mauFirstParamInSection[1] = 10;
    // hull 5 section 2: 30 (NaN) 31 (7.0)
    const u16 kau16Section2[2] = { 30, 31 };
    const f32 kafSection2[2]   = { std::numeric_limits<f32>::quiet_NaN(), 7.0f };
    Link(kau16Section2, kafSection2, 2);
    gaRuntimes[5].mauFirstParamInSection[2] = 30;
    // hull 6 section 0: 40 (0.5) ; hull 6 section 1: 41 (0.25)
    const u16 kau16Hull6a[1] = { 40 };
    const f32 kafHull6a[1]   = { 0.5f };
    Link(kau16Hull6a, kafHull6a, 1);
    gaRuntimes[6].mauFirstParamInSection[0] = 40;
    const u16 kau16Hull6b[1] = { 41 };
    const f32 kafHull6b[1]   = { 0.25f };
    Link(kau16Hull6b, kafHull6b, 1);
    gaRuntimes[6].mauFirstParamInSection[1] = 41;
    // the vehicles: alive (0x01) unless named otherwise
    const u32 kauAlive[] = { 10, 11, 12, 14, 15, 30, 31, 40, 41 };
    for (u32 luVehicle : kauAlive)
    {
        gM.maVehicles[luVehicle].mxFlags = Vehicle::E_FLAG_ALIVE;
    }
    gM.maVehicles[13].mxFlags = Vehicle::E_FLAG_ALIVE | Vehicle::E_FLAG_PHYSICAL;
    gM.maVehicles[20].mxFlags = 0;
    // hulls 5 and 6 are active; 9 is not
    gM.mActiveHulls.Insert(5);
    gM.mActiveHulls.Insert(6);
}

static bool Removed(const std::vector<u32>& lrRemoved, std::initializer_list<u32> lExpected)
{
    return lrRemoved == std::vector<u32>(lExpected);
}

int main()
{
    std::memset(&gRaceCars, 0, sizeof(gRaceCars));
    SetUpKillZoneData();

    // ================================================================================================================
    // 13 EMPTY_TRAFFIC_POOL (0x8274BD34)
    // ================================================================================================================
    {
        Fresh();
        const u8 lu8Empty = 1;
        PostBytes(&lu8Empty, 13, 1);
        unsigned luAsserts = gAsserts;
        Run();
        Check(gM.meEmptyTrafficPoolState == ReqFixture::E_EMPTYTRAFFICPOOLSTATE_EMPTYING && gAsserts == luAsserts,
              "13 mbEmptyPool from IDLE: EMPTYING (stw 1), no tripwire");
        gM.meEmptyTrafficPoolState = ReqFixture::E_EMPTYTRAFFICPOOLSTATE_EMPTY;
        const u8 lu8Fill = 0;
        PostBytes(&lu8Fill, 13, 1);
        Run();
        Check(gM.meEmptyTrafficPoolState == ReqFixture::E_EMPTYTRAFFICPOOLSTATE_FILLING && gAsserts == luAsserts,
              "13 !mbEmptyPool from EMPTY: FILLING (stw 3), no tripwire");
        gAssertLog.clear();
        const u8 lu8EmptyAgain = 2;   // any non-zero byte (lbz ; cmplwi 0)
        PostBytes(&lu8EmptyAgain, 13, 1);
        Run();
        Check(gM.meEmptyTrafficPoolState == ReqFixture::E_EMPTYTRAFFICPOOLSTATE_EMPTYING && gAsserts == luAsserts + 1
              && gAssertLog == "meEmptyTrafficPoolState == E_EMPTYTRAFFICPOOLSTATE_IDLE|",
              "13 empty from FILLING: the .cpp 5937 tripwire fires once and does not gate (EMPTYING); byte 2 is true");
        gAssertLog.clear();
        PostBytes(&lu8Fill, 13, 1);
        Run();
        Check(gM.meEmptyTrafficPoolState == ReqFixture::E_EMPTYTRAFFICPOOLSTATE_FILLING && gAsserts == luAsserts + 2
              && gAssertLog == "meEmptyTrafficPoolState == E_EMPTYTRAFFICPOOLSTATE_EMPTY|",
              "13 fill from EMPTYING: the .cpp 5943 tripwire fires once and does not gate (FILLING)");
        gAsserts = luAsserts;   // the two above are the scenario's own
    }

    // ================================================================================================================
    // 30 STOP_MODE_INTRO (0x8274BC84)
    // ================================================================================================================
    {
        Fresh();
        gRaceCars.maxRaceCarFlags[0] = 1;
        gRaceCars.maxRaceCarFlags[3] = 1;
        gRaceCars.maxRaceCarFlags[5] = 2;   // not active (bit 0 clear)
        gRaceCars.maRaceCarStates[0].mTransform.wAxis = Vector3{ 11.0f, 1.0f, -7.0f, 1.0f };
        gRaceCars.maRaceCarStates[3].mTransform.wAxis = Vector3{ -40.5f, 2.25f, 18.0f, 1.0f };
        gRaceCars.maRaceCarStates[5].mTransform.wAxis = Vector3{ 999.0f, 0.0f, 999.0f, 1.0f };
        gM.mbGameModeClearsTraffic = true;
        StopModeIntroAction lStop;
        std::memset(&lStop, 0, sizeof(lStop));
        Post(lStop, E_ACTION_STOP_MODE_INTRO);
        Run();
        Check(gM.maCylinders.size() == 2, "30 clears + offline: one cylinder per ACTIVE race car (slots 0 and 3)");
        Check(gM.maCylinders.size() == 2 && Same(gM.maCylinders[0].mCentre, 11.0f, 1.0f, -7.0f)
              && Same(gM.maCylinders[1].mCentre, -40.5f, 2.25f, 18.0f),
              "30: each cylinder is centred on the car's GetRaceCarState(i)->mTransform.wAxis (0x8227D690, +0x220)");
        Check(gM.maCylinders.size() == 2 && gM.maCylinders[0].mfRadius == 30.0f && gM.maCylinders[0].mfHeight == 10.0f
              && gM.maCylinders[0].mbStatic && gM.maCylinders[1].mfRadius == 30.0f && gM.maCylinders[1].mbStatic,
              "30: radius 30.0 (flt_820BA5E8), height 10.0 (flt_820BA5E4), parked cars too (li r6, 1)");
        Check(!gM.mbAtStartLineSoProtectRaceCarsFromTraffic, "30 offline: the start-line protection ends (stb 0, +0x717E1)");

        Fresh();
        gM.mbGameModeClearsTraffic = false;
        Post(lStop, E_ACTION_STOP_MODE_INTRO);
        Run();
        Check(gM.maCylinders.empty() && !gM.mbAtStartLineSoProtectRaceCarsFromTraffic,
              "30 without mbGameModeClearsTraffic: no sweep, the protection still ends offline");

        Fresh();
        gM.mbGameModeClearsTraffic   = true;
        gM.mbIsOnlineGameMode        = true;
        gM.mbAllowDivergentBehaviour = false;
        Post(lStop, E_ACTION_STOP_MODE_INTRO);
        Run();
        Check(gM.maCylinders.empty() && gM.mbAtStartLineSoProtectRaceCarsFromTraffic,
              "30 online: no sweep (lbzx +0x717E7) and the protection stays (lbzx +0x717DC ; bne)");
        std::memset(&gRaceCars, 0, sizeof(gRaceCars));
    }

    // ================================================================================================================
    // 73 CAR_SELECT_TRANSITION_IN (0x8274C018) and 77 CAR_SELECT_EXIT, offline (0x8274C068)
    // ================================================================================================================
    {
        Fresh();
        const u8 kau8Start[2] = { 1, 0 };
        PostBytes(kau8Start, 73, 2);
        Run();
        Check(Splat(gM.mfTrafficSimRadius, 395.0f), "73 start: mfTrafficSimRadius = splat(lane 1 of unk_8300CF10) = 395.0");
        Check(gM.muMaxVehiclesToRender == 64, "73 start: muMaxVehiclesToRender = 64 (li r11, 0x40)");
        Check(gM.mbInOfflineCarSelect, "73 start: mbInOfflineCarSelect = true (stbx r15, +0x713C8)");
        Check(Bits(gM.mfRenderCullDistanceSq) == Bits(160000.0f), "73 start: mfRenderCullDistanceSq = lane 3 = 160000.0");

        const u8 kau8End[2] = { 0, 1 };
        PostBytes(kau8End, 73, 2);
        Run();
        Check(Splat(gM.mfTrafficSimRadius, 395.0f) && gM.muMaxVehiclesToRender == 64 && gM.mbInOfflineCarSelect,
              "73 end (mbStart clear): nothing changes (lbz 0(record) ; beq)");

        CarSelectExitAction lExit = {};
        lExit.mExitSpawnLocation = Vector3{ 3040.75f, -5.8125f, -1937.875f, 1.0f };
        lExit.mbOnlineCarSelect  = false;
        Post(lExit, E_ACTION_CAR_SELECT_FINISHED);
        Run();
        Check(gM.maCylinders.size() == 1 && Same(gM.maCylinders[0].mCentre, 3040.75f, -5.8125f, -1937.875f),
              "77 offline: one cylinder centred on the record's mExitSpawnLocation (lvx128 0(record))");
        Check(gM.maCylinders.size() == 1 && gM.maCylinders[0].mfRadius == 150.0f && gM.maCylinders[0].mfHeight == 1000.0f
              && !gM.maCylinders[0].mbStatic,
              "77 offline: radius 150.0 (flt_820BA5A4), height 1000.0 (flt_820BA604), parked cars stay (li r6, 0)");
        Check(Splat(gM.mfTrafficSimRadius, 195.0f) && gM.muMaxVehiclesToRender == 32 && !gM.mbInOfflineCarSelect
              && Bits(gM.mfRenderCullDistanceSq) == Bits(62500.0f),
              "77 offline after 73: Construct's box comes back (splat 195.0, 32, false, 62500.0)");

        Fresh();
        gM.muMaxVehiclesToRender  = 17;   // not in car select: arm 77 must leave these alone
        gM.mfRenderCullDistanceSq = 1.0f;
        Post(lExit, E_ACTION_CAR_SELECT_FINISHED);
        Run();
        Check(gM.maCylinders.size() == 1 && gM.muMaxVehiclesToRender == 17 && gM.mfRenderCullDistanceSq == 1.0f,
              "77 offline outside the car select: the cylinder only (lbz +0x713C8 ; beq)");

        Fresh();
        gM.mbIsOnlineGameMode        = true;
        gM.mbAllowDivergentBehaviour = false;
        PostBytes(kau8Start, 73, 2);
        Run();
        Check(Splat(gM.mfTrafficSimRadius, 195.0f) && gM.muMaxVehiclesToRender == 32 && !gM.mbInOfflineCarSelect,
              "73 online: nothing changes (lbzx +0x717E7 ; beq)");
    }

    // ================================================================================================================
    // 75 CAR_SELECT_READY -> HideAllTraffic (0x8274C000, 0x8273F418); 77 online -> UnhideAllTraffic (0x8274A500)
    // ================================================================================================================
    {
        Fresh();
        const u32 kauAlive[]    = { 3, 7, 64, 599 };
        const u32 kauPhysical[] = { 7, 64, 100 };
        for (u32 i : kauAlive) gM.mVehicleSoaData.mAliveVehicles.SetBit(i);
        for (u32 i : kauPhysical) gM.mVehicleSoaData.mPhysicalVehicles.SetBit(i);
        CarSelectReadyAction lReady;
        lReady.miCarSelectFlow = 1;   // the offline junkyard
        Post(lReady, E_ACTION_CAR_SELECT_READY);
        Run();
        Check(!gM.mbTrafficIsHidden && gM.mauRemoved.empty(), "75 type 1 (junkyard): the traffic is not hidden (cmpwi 2)");

        lReady.miCarSelectFlow = 2;   // E_CAR_SELECT_TYPE_ONLINE_EVENT_START
        Post(lReady, E_ACTION_CAR_SELECT_READY);
        Run();
        Check(gM.mbTrafficIsHidden, "75 type 2 (online event start): HideAllTraffic raises mbTrafficIsHidden (stb 1)");
        Check(Removed(gM.mauRemoved, { 7, 64 }),
              "HideAllTraffic removes every alive AND physical car, in index order (the +0x5078 & +0x505A snapshot)");
        Post(lReady, E_ACTION_CAR_SELECT_READY);
        Run();
        Check(Removed(gM.mauRemoved, { 7, 64 }), "HideAllTraffic while hidden: nothing (lbz +0x725E8 ; bne)");

        CarSelectExitAction lOnlineExit = {};
        lOnlineExit.mExitSpawnLocation = Vector3{ 1.0f, 2.0f, 3.0f, 0.0f };
        lOnlineExit.mbOnlineCarSelect  = true;
        Post(lOnlineExit, E_ACTION_CAR_SELECT_FINISHED);
        Run();
        Check(!gM.mbTrafficIsHidden, "77 online: UnhideAllTraffic lowers mbTrafficIsHidden (stb 0)");
        Check(gM.maCylinders.size() == 1 && Same(gM.maCylinders[0].mCentre, 100.0f, 5.0f, 200.0f)
              && gM.maCylinders[0].mfRadius == 150.0f && gM.maCylinders[0].mfHeight == 1000.0f
              && !gM.maCylinders[0].mbStatic,
              "UnhideAllTraffic clears 150 m / 1000 m around mLocalPlayerPosition (+0x713D0), not the record's vector");

        Post(lOnlineExit, E_ACTION_CAR_SELECT_FINISHED);
        Run();
        Check(gM.maCylinders.size() == 1, "UnhideAllTraffic when not hidden: nothing (lbz +0x725E8 ; beq)");

        gM.mbTrafficIsHidden  = true;
        gM.meLocalPlayerIndex = E_ACTIVE_RACE_CAR_INDEX_INVALID;
        Post(lOnlineExit, E_ACTION_CAR_SELECT_FINISHED);
        Run();
        Check(gM.maCylinders.size() == 1 && !gM.mbTrafficIsHidden,
              "UnhideAllTraffic with no local player (-1): no clear, the traffic still comes back");
    }

    // ================================================================================================================
    // 110 KILLZONE -> FireKillZone (0x8274BE64, 0x827343B8) and TrafficData::FindKillZone (0x827570A0)
    // ================================================================================================================
    {
        Fresh();
        Check(gData.FindKillZone(0x100ull) == &gaZones[0] && gData.FindKillZone(0x30000ull) == &gaZones[2],
              "FindKillZone finds the first id and a middle one");
        Check(gData.FindKillZone(0x8000000000000001ull) == &gaZones[4]
              && gData.FindKillZone(0x7FFFFFFFFFFFFFFFull) == &gaZones[3],
              "FindKillZone compares UNSIGNED (cmpld): the id with bit 63 set is found, and its neighbour");
        const unsigned luAsserts = gAsserts;
        gAssertLog.clear();
        Check(gData.FindKillZone(0x999ull) == nullptr && gAsserts == luAsserts + 1
              && gAssertLog.find("Failed to find traffic kill zone") == 0,
              "FindKillZone misses: the BrnTrafficData.cpp:303 tripwire, then NULL");
        gAsserts = luAsserts;

        SetUpLanes();
        KillzoneAction lKillzone;
        lKillzone.maRegionIds.Construct();
        lKillzone.maRegionIds.Append(0x2000ull);
        lKillzone.maRegionIds.Append(0x100ull);
        Post(lKillzone, 110);
        Run();
        Check(Removed(gM.mauRemoved, { 12, 14 }),
              "110 -> FireKillZone(0x2000): rungs 2..4 of hull 5 section 1 -- 12 (2.0) and 14 (4.0); 11 (1.99) and 15 "
              "(4.01) are outside, 20 is dead, 13 is physical; hull 9 (0x100) is not active; an empty section is skipped");
        Check(gM.mDEBUGRecentlyFiredKillZones.GetLength() == 2
              && gM.mDEBUGRecentlyFiredKillZones[0].mKillZoneId == 0x2000ull
              && gM.mDEBUGRecentlyFiredKillZones[0].miFramesLeftToRemember == 300
              && gM.mDEBUGRecentlyFiredKillZones[1].mKillZoneId == 0x100ull,
              "FireKillZone remembers each fired id for 300 frames (li r11, 0x12C ; Append), in firing order");
        Check(gM.muGetHullCalls == 1,
              "FireKillZone calls GetHull once per region it walks (0x82734530): not for an inactive hull or an "
              "empty section");

        Fresh();
        SetUpLanes();
        KillzoneAction lNaN;
        lNaN.maRegionIds.Construct();
        lNaN.maRegionIds.Append(0x30000ull);
        Post(lNaN, 110);
        Run();
        Check(Removed(gM.mauRemoved, { 30 }),
              "FireKillZone: a NaN param stops the skip (bge) and counts as inside the span (bgt); 31 (7.0) is past 6");

        Fresh();
        SetUpLanes();
        gM.mbGameModeAllowsKillzones = false;
        Post(lKillzone, 110);
        Run();
        Check(gM.mauRemoved.empty() && gM.mDEBUGRecentlyFiredKillZones.GetLength() == 0,
              "110 while the mode forbids kill zones (lbzx +0x717E0): nothing fires");

        Fresh();
        SetUpLanes();
        gM.mbDEBUGEnableKillzones = false;
        Post(lKillzone, 110);
        Run();
        Check(gM.mauRemoved.empty(), "110 with the debug switch off (lbzx +0x727D4): nothing fires");

        Fresh();
        SetUpLanes();
        gM.mbIsOnlineGameMode        = true;
        gM.mbAllowDivergentBehaviour = false;
        Post(lKillzone, 110);
        Run();
        Check(gM.mauRemoved.empty() && gAsserts == luAsserts, "110 online (lbzx +0x717E7): nothing fires, no tripwire");

        Fresh();
        SetUpLanes();
        for (u32 luFire = 0; luFire < 9; ++luFire)
        {
            gM.FireKillZone(luFire < 8 ? 0x7FFFFFFFFFFFFFFFull : 0x8000000000000001ull);
        }
        Check(gM.mDEBUGRecentlyFiredKillZones.GetLength() == 8
              && gM.mDEBUGRecentlyFiredKillZones[7].mKillZoneId == 0x8000000000000001ull
              && gM.mDEBUGRecentlyFiredKillZones[0].mKillZoneId == 0x7FFFFFFFFFFFFFFFull,
              "the ninth fire erases the oldest record (cmplwi 8 ; Erase(0)) and appends the newest");
        Check(gM.mauRemoved.size() == 9 && gM.mauRemoved[0] == 40 && gM.mauRemoved[8] == 41,
              "a zone spanning rungs 0..255 removes its alive car on every fire (RemoveVehicle is a double here)");
    }

    // ================================================================================================================
    // 192 WAIT_FOR_STREAMING (0x8274BC30) and PostPhysicsUpdate's latched answers (0x8274E744, 0x8274EC74)
    // ================================================================================================================
    {
        const StreamingCompleteEvent* lpEvent = nullptr;
        s32 liSize = 0, liCount = 0;

        Fresh();
        gM.meState = ReqFixture::E_STATE_STARTING_UP;
        const u8 lu8Unused = 0;
        PostBytes(&lu8Unused, 192, 1);
        Run();
        Check(gM.mbWaitingForStreaming && OnlyGameEvent(&lpEvent, &liSize, &liCount) == -1,
              "192 while STARTING_UP: the answer is latched (stbx 1, +0x7180E), nothing posted");

        gM.LatchedAnswerHead(&gOut);
        Check(gM.mbWaitingForStreaming && OnlyGameEvent(&lpEvent, &liSize, &liCount) == -1,
              "the latched answer waits for the streamer (AreAllAssetsLoaded false)");
        gM.mStreamer.mbLoaded = true;
        gM.LatchedAnswerHead(&gOut);
        const s32 liType = OnlyGameEvent(&lpEvent, &liSize, &liCount);
        Check(!gM.mbWaitingForStreaming && liType == 9 && liSize == 16 && liCount == 1,
              "loaded: PostPhysicsUpdate answers once -- game event 9, 16 bytes (0x8274E788 li r6, 0x10 ; li r5, 9)");
        Check(liType == 9 && lpEvent->meModule == StreamingCompleteEvent::E_MODULE_TRAFFIC_ENTITY
              && static_cast<s32>(lpEvent->meModule) == 0,
              "the answer names module 0, E_MODULE_TRAFFIC_ENTITY (stw 0 -> record+0)");
        gM.LatchedAnswerHead(&gOut);
        Check(OnlyGameEvent(&lpEvent, &liSize, &liCount) == 9 && liCount == 1, "the latch is consumed: no second answer");

        Fresh();
        gM.meState = ReqFixture::E_STATE_TEARING_DOWN;
        PostBytes(&lu8Unused, 192, 1);
        Run();
        gM.mStreamer.mbLoaded = true;
        gM.LatchedAnswerStartup(&gOut);
        Check(gM.mbWaitingForStreaming == false && OnlyGameEvent(&lpEvent, &liSize, &liCount) == 9 && liCount == 1
              && lpEvent->meModule == StreamingCompleteEvent::E_MODULE_TRAFFIC_ENTITY,
              "192 while TEARING_DOWN latches too; the start-up site's answer (0x8274EC94) is the same record");

        Fresh();
        gM.meState = ReqFixture::E_STATE_RUNNING;
        PostBytes(&lu8Unused, 192, 1);
        Run();
        Check(!gM.mbWaitingForStreaming && OnlyGameEvent(&lpEvent, &liSize, &liCount) == 9 && liSize == 16
              && lpEvent->meModule == StreamingCompleteEvent::E_MODULE_TRAFFIC_ENTITY,
              "192 while RUNNING: answered at once (GetGameEventQueue 0x82711CE8 ; AddEvent 9, 16)");

        Fresh();
        gM.meState               = ReqFixture::E_STATE_STARTING_UP;
        gM.mbDEBUGTurnTrafficOff = true;
        PostBytes(&lu8Unused, 192, 1);
        Run();
        Check(!gM.mbWaitingForStreaming && OnlyGameEvent(&lpEvent, &liSize, &liCount) == 9,
              "192 STARTING_UP with the traffic switched off (lbzx +0x7287E ; bne): answered at once");
    }

    // ================================================================================================================
    // 244 HUD_MESSAGE_DIST_TO_FINISH (0x8274BF74)
    // ================================================================================================================
    {
        Fresh();
        HUDMessageDistanceToFinishAction lDistance;
        lDistance.mfDistanceToFinish = 1500.0f;
        lDistance.miPlayerPosition   = 1;
        Post(lDistance, E_ACTION_HUD_MESSAGE_DIST_TO_FINISH);
        Run();
        Check(Bits(gM.mfGameModeDensityScale) == Bits(0.4f), "244 at 1500 m: mfGameModeDensityScale 0.8 -> 0.4 (fmuls 0.5)");
        Post(lDistance, E_ACTION_HUD_MESSAGE_DIST_TO_FINISH);
        Run();
        Check(Bits(gM.mfGameModeDensityScale) == Bits(0.2f), "244 again: halved again (0.2)");
        lDistance.mfDistanceToFinish = 1505.0f;
        Post(lDistance, E_ACTION_HUD_MESSAGE_DIST_TO_FINISH);
        Run();
        Check(Bits(gM.mfGameModeDensityScale) == Bits(0.2f), "244 at exactly 1505.0 (flt_820BA814): kept (bge)");
        lDistance.mfDistanceToFinish = std::numeric_limits<f32>::quiet_NaN();
        Post(lDistance, E_ACTION_HUD_MESSAGE_DIST_TO_FINISH);
        Run();
        Check(Bits(gM.mfGameModeDensityScale) == Bits(0.2f), "244 with a NaN distance: kept (bge is taken on unordered)");

        Fresh();
        gM.mbIsOnlineGameMode        = true;
        gM.mbAllowDivergentBehaviour = false;
        lDistance.mfDistanceToFinish = 10.0f;
        Post(lDistance, E_ACTION_HUD_MESSAGE_DIST_TO_FINISH);
        Run();
        Check(Bits(gM.mfGameModeDensityScale) == Bits(0.8f), "244 online (lbzx +0x717E7): kept");
    }

    // ================================================================================================================
    // the tail: leaving Picture Paradise (0x8274C0D0)
    // ================================================================================================================
    {
        const u32 KU_PICTURE_PARADISE = BrnDirector::Camera::CameraState::E_FLAG_IS_PICTURE_PARADISE;
        Check(KU_PICTURE_PARADISE == 14, "E_FLAG_IS_PICTURE_PARADISE is bit 14 (rlwinm 0,17,17 == 0x4000)");

        // far: the camera 150 m from the player, looking away
        Fresh();
        gM.mbInPictureParadise = true;
        gM.mCameraLastFrame.mTransform.wAxis = Vector3{ 250.0f, 5.0f, 200.0f, 1.0f };
        gM.mCameraLastFrame.mTransform.zAxis = Vector3{ 1.0f, 0.0f, 0.0f, 0.0f };
        Run();
        Check(!gM.mbInPictureParadise, "tail: mbInPictureParadise follows the camera flag (stb +0x725EB)");
        Check(gM.maCylinders.size() == 1 && Same(gM.maCylinders[0].mCentre, 100.0f, 5.0f, 200.0f)
              && gM.maCylinders[0].mfRadius == 90.0f && gM.maCylinders[0].mfHeight == 1000.0f
              && !gM.maCylinders[0].mbStatic,
              "tail, leaving Picture Paradise 150 m from the player: clear 90 m (flt_820BA60C) around the player");

        // near (50 m), the player in front of the camera
        Fresh();
        gM.mbInPictureParadise = true;
        gM.mCameraLastFrame.mTransform.wAxis = Vector3{ 50.0f, 5.0f, 200.0f, 1.0f };
        gM.mCameraLastFrame.mTransform.zAxis = Vector3{ 1.0f, 0.0f, 0.0f, 0.0f };
        Run();
        Check(gM.maCylinders.size() == 1, "tail near, the player in front (dot > 0, vcmpgtfp.): clear");

        // near, the player behind the camera
        Fresh();
        gM.mbInPictureParadise = true;
        gM.mCameraLastFrame.mTransform.wAxis = Vector3{ 50.0f, 5.0f, 200.0f, 1.0f };
        gM.mCameraLastFrame.mTransform.zAxis = Vector3{ -1.0f, 0.0f, 0.0f, 0.0f };
        Run();
        Check(gM.maCylinders.empty() && !gM.mbInPictureParadise, "tail near, the player behind the camera: no clear");

        // still in Picture Paradise; and entering it
        Fresh();
        gM.mbInPictureParadise = true;
        gM.mCameraLastFrame.mState.mCurrentFlags.SetBit(KU_PICTURE_PARADISE);
        gM.mCameraLastFrame.mTransform.wAxis = Vector3{ 900.0f, 5.0f, 200.0f, 1.0f };
        Run();
        Check(gM.maCylinders.empty() && gM.mbInPictureParadise, "tail while still in Picture Paradise: nothing");
        gM.mbInPictureParadise = false;
        Run();
        Check(gM.maCylinders.empty() && gM.mbInPictureParadise, "tail on entering Picture Paradise: nothing (only the exit)");

        // ROUNDING (rule 1, vmsum3fp128 0x8274C14C): |d|^2 with d = (0x426E1CB5, 0x4228B13E, 0x4288CA1E) is
        // 9999.9990234375 as ONE rounding of the f64 sum, 10000.0 summed in f32 left to right. The camera looks away,
        // so only the distance can clear: the console does NOT clear.
        Fresh();
        gM.mbInPictureParadise = true;
        gM.mCameraLastFrame.mTransform.wAxis = Vector3{ 0.0f, 0.0f, 0.0f, 1.0f };
        gM.mCameraLastFrame.mTransform.zAxis = Vector3{ -1.0f, 0.0f, 0.0f, 0.0f };
        gM.mLocalPlayerPosition = Vector3{ FromBits(0x426E1CB5u), FromBits(0x4228B13Eu), FromBits(0x4288CA1Eu), 0.0f };
        Run();
        Check(gM.maCylinders.empty(),
              "tail rounding: |d|^2 = 9999.999 as one rounding of the f64 sum (FLAG model, rule 1), so no clear "
              "(a sequential f32 sum gives 10000.0 and would clear)");
    }

    Check(gAsserts == 0, "no tripwire fired outside the scenarios that expect one");
    std::printf("FxTrafficLightsRequests: %u checks, %u failures (%u asserts)\n", gChecks, gFailures, gAsserts);
    return gFailures ? 1 : 0;
}
