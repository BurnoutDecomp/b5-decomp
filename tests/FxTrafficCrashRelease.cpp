// FX-TRAFFIC (crash parity 2026-09-23, G58-X2 / G58-X3 / G58-X4): the PRODUCTION
// StopVehicleBeingPhysical, KillParam and StaticVehicles_KillParam, with the production
// EnsureVehicleRemovedFromCrashModule they call, extracted from
// src/GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.cpp by
// run_fxtraffic_crash_release.py and hosted on a fixture that carries the real member types.
//
// Checked against the ARTIST asm:
//   StopVehicleBeingPhysical @0x8271FED0
//     0x8271FF98  Append to maNewRemovedVehicles only when lbSuppressPhysicsRemoval == 0
//     0x8271FFA4  bl EnsureVehicleRemovedFromCrashModule(luVehicle) -- unconditional, and
//                 BEFORE SetNotPhysical @0x82720024 stores 0xFF over muCrashTrafficType
//   KillParam @0x82721FB8 (the SetDead arm)
//     0x82722318  SetDead(luParam) ; 0x82722324 Ensure(luParam)
//     0x8272232C  GetVehicleSpecies == STANDARD ; 0x82722344 GetTrailerIndex != 0xFFFF
//     0x82722370  trailer E_FLAG_PHYSICAL (lbz 5 & 0x08) set -> the trailer tail is skipped
//     0x82722384  Detach(trailer) ; 0x82722394 SetDead(trailer) ; 0x827223A0 Ensure(trailer)
//     0x827223B0  Detach(cab) -- LAST
//     (the SetOrphan arm 0x827222AC has no Ensure)
//   StaticVehicles_KillParam @0x82721C50
//     0x82721E00  SetDead ; 0x82721E18 Ensure(GetVehicleIndexFromStaticIndex(luParam)) ; return
//     (the zombie arm has no Ensure)
//
// Vehicle's four mutators are RECORDERS here (with the production effects), so the call
// order is checked, not only the end state.
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include <cstdio>
#include <cstring>
#include <vector>

static unsigned gAsserts = 0, gChecks = 0, gFailures = 0;

namespace CgsDev
{
namespace Assert
{
    int   BeginAssert() { return 0; }
    int   FireAssert(const char* lpcMessage, const char*, int)
    {
        ++gAsserts;
        std::fprintf(stderr, "ASSERT: %s\n", lpcMessage);
        return 0;
    }
    void* EndAssert() { return nullptr; }
}
}

namespace BrnTraffic
{
    // The production file-scope helpers the bodies (or their pre-fix revision) name.
    inline CgsDev::Log::DebugPrint* TrafficDiagStream() { return nullptr; }
    inline void LogMissingLeg_T1(bool&, const char*) {}
    inline void LogMissingLeg_T2(bool&, const char*) {}
    inline void LogMissingLeg_T3Drive(bool&, const char*) {}
    static const s32 KI_DEMOTE_PRINT_CAP = 60;
    static s32 giDemoteCalls = 0, giDemoteFromClearup = 0, giDemoteFromRecycle = 0, giDemoteFromReturn = 0;

    struct CrashReleaseFixture
    {
        typedef TrafficEntityModule M;
        typedef M::EState EState;
        static const EState E_STATE_STARTING_UP  = M::E_STATE_STARTING_UP;
        static const EState E_STATE_RUNNING      = M::E_STATE_RUNNING;
        static const EState E_STATE_TEARING_DOWN = M::E_STATE_TEARING_DOWN;

        decltype(M::meState)                           meState;
        decltype(M::mbDecisionFrame)                   mbDecisionFrame;
        decltype(M::mbWaitingForStreaming)             mbWaitingForStreaming;
        decltype(M::mbAllowDivergentBehaviour)         mbAllowDivergentBehaviour;
        decltype(M::maVehicles)                        maVehicles;
        decltype(M::mVehiclesAddedToCrashModule)       mVehiclesAddedToCrashModule;
        decltype(M::mVehicleSoaData)                   mVehicleSoaData;
        decltype(M::maParams)                          maParams;
        decltype(M::mParamSoaData)                     mParamSoaData;
        decltype(M::maPurgatoryList)                   maPurgatoryList;
        decltype(M::maStaticTrafficParams)             maStaticTrafficParams;
        decltype(M::maRecentlyRemovedVehicles)         maRecentlyRemovedVehicles;
        decltype(M::maNewRemovedVehicles)              maNewRemovedVehicles;
        decltype(M::maRecentlyRecoveredSlammedTraffic) maRecentlyRecoveredSlammedTraffic;
        decltype(M::maTrafficPhysicsInfoList)          maTrafficPhysicsInfoList;
        decltype(M::maTrafficPhysicsInfoListBits)      maTrafficPhysicsInfoListBits;

        Vehicle*            GetVehicle(u32 luIndex);
        Vehicle*            GetStaticVehicle(u32 luIndex);
        StaticTrafficParam* GetStaticTrafficParam(u32 luIndex);
        u32                 GetVehicleIndexFromStaticIndex(u32 luStaticVehicle);
        Param*              GetParam(u32 luParam);
        bool                IsDecisionFrame();
        void                PutParamInPurgatory(u32 luParam);
        void                EnsureVehicleRemovedFromCrashModule(u32 luVehicle);
        void                StopVehicleBeingPhysical(u32 luVehicle, bool lbSuppressPhysicsRemoval);
        void                KillParam(u32 luParam);
        void                StaticVehicles_KillParam(u32 luParam);
    };

    // ---- call recorder -------------------------------------------------------------------
    struct CallRecord
    {
        char mcKind;        // 'D' SetDead, 'N' SetNotPhysical, 'O' SetOrphan, 'A' DetachArticulation
        u32  muVehicle;
        s32  miRemovedLength;   // maRecentlyRemovedVehicles' length when the call happened
        s32  miSlamLength;      // maRecentlyRecoveredSlammedTraffic's length when the call happened
    };
    static std::vector<CallRecord> gCalls;
    static CrashReleaseFixture*    gpFixture = nullptr;

    static void Record(char lcKind, u32 luVehicle)
    {
        CallRecord lRecord = { lcKind, luVehicle,
                               static_cast<s32>(gpFixture->maRecentlyRemovedVehicles.GetLength()),
                               static_cast<s32>(gpFixture->maRecentlyRecoveredSlammedTraffic.GetLength()) };
        gCalls.push_back(lRecord);
    }

    // The production effects (BrnTrafficVehicle.cpp), plus the record.
    void Vehicle::SetDead(u32 luVehicle, VehicleSoaData& lSoaData)
    {
        Record('D', luVehicle);
        CGS_ASSERT(IsAlive(), "IsAlive()");
        CGS_ASSERT(lSoaData.mAliveVehicles.IsBitSet(luVehicle), "lSoaData.mAliveVehicles.IsBitSet( luVehicle )");
        mxFlags &= 0xDE;
        lSoaData.mAliveVehicles.UnSetBit(luVehicle);
    }

    void Vehicle::SetNotPhysical(u32 luVehicle, VehicleSoaData& lSoaData)
    {
        Record('N', luVehicle);
        CGS_ASSERT(IsPhysical(), "IsPhysical()");
        mxFlags &= 0xE7;
        lSoaData.mPhysicalVehicles.UnSetBit(luVehicle);
        miPhysicalPartsIndex = -1;
        miPhysicalReason     = -1;
        muCrashTrafficType   = static_cast<u8>(-1);
    }

    void Vehicle::SetOrphan()
    {
        Record('O', static_cast<u32>(this - &gpFixture->maVehicles[0]));
        CGS_ASSERT(IsAlive(), "IsAlive()");
        mxFlags |= E_FLAG_ORPHAN;
    }

    void Vehicle::DetachArticulation(u32 luVehicle, VehicleSoaData& lSoaData)
    {
        Record('A', luVehicle);
        CGS_ASSERT(muOtherHalfIndex != static_cast<u16>(KU_INVALID_VEHICLE), "muOtherHalfIndex != KU_INVALID_VEHICLE");
        CGS_ASSERT(lSoaData.mArticulatedVehicles.IsBitSet(luVehicle), "lSoaData.mArticulatedVehicles.IsBitSet( luVehicle )");
        muOtherHalfIndex = static_cast<u16>(KU_INVALID_VEHICLE);
        lSoaData.mArticulatedVehicles.UnSetBit(luVehicle);
    }

    u16 Vehicle::GetTrailerIndex() const
    {
        CGS_ASSERT(IsOfStandardSpecies(), "IsOfStandardSpecies()");
        return muOtherHalfIndex;
    }
}

// The production bodies under test.
#include "crash_release.inc"

using namespace BrnTraffic;
typedef CrashReleaseFixture Fixture;

static void Check(bool lbPass, const char* lpcName)
{
    ++gChecks;
    if (!lbPass)
    {
        ++gFailures;
        std::fprintf(stderr, "FAIL: %s\n", lpcName);
    }
}

alignas(64) static unsigned char gaStorage[sizeof(Fixture)];

static Fixture& Fresh()
{
    std::memset(gaStorage, 0, sizeof(gaStorage));
    Fixture& lr = *reinterpret_cast<Fixture*>(gaStorage);
    gpFixture = &lr;
    lr.meState                   = Fixture::E_STATE_RUNNING;
    lr.mbDecisionFrame           = true;
    lr.mbAllowDivergentBehaviour = true;   // offline
    // The SoA bit sets and mVehiclesAddedToCrashModule are all-zero == empty after the memset.
    lr.maPurgatoryList.Construct();
    lr.maRecentlyRemovedVehicles.Construct();
    lr.maNewRemovedVehicles.Construct();
    lr.maRecentlyRecoveredSlammedTraffic.Construct();
    for (u32 luVehicle = 0; luVehicle < KU_MAX_TOTAL_TRAFFIC; ++luVehicle)
    {
        lr.maVehicles[luVehicle].muOtherHalfIndex   = static_cast<u16>(KU_INVALID_VEHICLE);
        lr.maVehicles[luVehicle].muCrashTrafficType = static_cast<u8>(-1);
        lr.maVehicles[luVehicle].miPhysicalPartsIndex = -1;
        lr.maVehicles[luVehicle].muSpecies = static_cast<u8>(GetVehicleSpecies(luVehicle));
    }
    gCalls.clear();
    return lr;
}

// An alive vehicle with an entity; physical in slot liParts with crash type lu8Type when
// liParts >= 0; its crash-module bit set when lbAnnounced.
static void Seed(Fixture& lr, u32 luVehicle, s8 liParts, u8 lu8Type, bool lbAnnounced)
{
    Vehicle& lrVehicle = lr.maVehicles[luVehicle];
    lrVehicle.mxFlags = Vehicle::E_FLAG_ALIVE | Vehicle::E_FLAG_HASENTITY;
    lr.mVehicleSoaData.mAliveVehicles.SetBit(luVehicle);
    lr.mVehicleSoaData.mVehiclesWithEntities.SetBit(luVehicle);
    if (liParts >= 0)
    {
        lrVehicle.mxFlags |= Vehicle::E_FLAG_PHYSICAL;
        lrVehicle.miPhysicalPartsIndex = liParts;
        lrVehicle.muCrashTrafficType   = lu8Type;
        lr.mVehicleSoaData.mPhysicalVehicles.SetBit(luVehicle);
        lr.maTrafficPhysicsInfoListBits.SetBit(static_cast<u32>(liParts));
        lr.maTrafficPhysicsInfoList[liParts].muOwningVehicleIndex = static_cast<u16>(luVehicle);
    }
    if (lbAnnounced)
    {
        lr.mVehiclesAddedToCrashModule.SetBit(luVehicle);
    }
}

static void Articulate(Fixture& lr, u32 luCab, u32 luTrailer)
{
    lr.maVehicles[luCab].muOtherHalfIndex     = static_cast<u16>(luTrailer);
    lr.maVehicles[luTrailer].muOtherHalfIndex = static_cast<u16>(luCab);
    lr.mVehicleSoaData.mArticulatedVehicles.SetBit(luCab);
    lr.mVehicleSoaData.mArticulatedVehicles.SetBit(luTrailer);
}

static void SeedParam(Fixture& lr, u32 luParam, u8 lxExtraFlags)
{
    lr.maParams[luParam].mxFlags = static_cast<u8>(Param::E_FLAG_ALIVE | lxExtraFlags);
    lr.mParamSoaData.mAliveParams.SetBit(luParam);
    if (lxExtraFlags & Param::E_FLAG_ZOMBIE)
    {
        lr.mParamSoaData.mZombieParams.SetBit(luParam);
    }
}

template <typename TArray>
static bool Holds(const TArray& lrArray, std::initializer_list<u32> lItems)
{
    if (static_cast<s32>(lrArray.GetLength()) != static_cast<s32>(lItems.size()))
        return false;
    u32 luIndex = 0;
    for (u32 luItem : lItems)
    {
        if (static_cast<u32>(const_cast<TArray&>(lrArray).GetItem(luIndex++)) != luItem)
            return false;
    }
    return true;
}

static bool CallsAre(std::initializer_list<CallRecord> lExpected, bool lbCheckLengths)
{
    if (gCalls.size() != lExpected.size())
        return false;
    size_t luIndex = 0;
    for (const CallRecord& lrWant : lExpected)
    {
        const CallRecord& lrGot = gCalls[luIndex++];
        if (lrGot.mcKind != lrWant.mcKind || lrGot.muVehicle != lrWant.muVehicle)
            return false;
        if (lbCheckLengths && (lrGot.miRemovedLength != lrWant.miRemovedLength || lrGot.miSlamLength != lrWant.miSlamLength))
            return false;
    }
    return true;
}

int main()
{
    // ================= StopVehicleBeingPhysical (G58-X2) =================
    {
        // A slammed (type 3), announced car returned to traffic.
        Fixture& lr = Fresh();
        Seed(lr, 17, 3, 3, true);
        lr.StopVehicleBeingPhysical(17, false);
        Check(!lr.mVehiclesAddedToCrashModule.IsBitSet(17), "Stop: the crash-module bit is dropped  @0x8271FFA4");
        Check(Holds(lr.maRecentlyRemovedVehicles, { 17 }), "Stop: the car is queued on maRecentlyRemovedVehicles");
        Check(Holds(lr.maRecentlyRecoveredSlammedTraffic, { 17 }),
              "Stop: a slammed car is queued on maRecentlyRecoveredSlammedTraffic (Ensure runs BEFORE SetNotPhysical)");
        Check(lr.maVehicles[17].GetCrashTrafficTypeRaw() == 0xFF && !lr.maVehicles[17].IsPhysical(),
              "Stop: the car ends non-physical with crash type 0xFF");
        Check(Holds(lr.maNewRemovedVehicles, { 17 }), "Stop: not suppressed -> the physics removal is queued  @0x8271FF98");
        Check(CallsAre({ { 'N', 17, 1, 1 } }, true),
              "Stop: SetNotPhysical is the only Vehicle mutator and runs AFTER Ensure (queues already hold the car)");
    }
    {
        Fixture& lr = Fresh();
        Seed(lr, 18, 4, 3, true);
        lr.StopVehicleBeingPhysical(18, true);
        Check(!lr.mVehiclesAddedToCrashModule.IsBitSet(18) && Holds(lr.maRecentlyRemovedVehicles, { 18 })
                  && Holds(lr.maRecentlyRecoveredSlammedTraffic, { 18 }) && lr.maNewRemovedVehicles.GetLength() == 0,
              "Stop(suppressed): Ensure is UNCONDITIONAL, only the physics Append is skipped");
    }
    {
        Fixture& lr = Fresh();
        Seed(lr, 19, 5, 0, false);
        lr.StopVehicleBeingPhysical(19, false);
        Check(lr.maRecentlyRemovedVehicles.GetLength() == 0 && lr.maRecentlyRecoveredSlammedTraffic.GetLength() == 0
                  && Holds(lr.maNewRemovedVehicles, { 19 }),
              "Stop: an unannounced, unslammed car queues nothing on the crash side");
    }

    // ================= KillParam (G58-X3) =================
    {
        // Offline removal: the param was flagged should-be-removed (RemoveVehicle's divergent
        // STANDARD arm), not divorced -> lbKillVehicle false -> the SetDead arm, even physical.
        Fixture& lr = Fresh();
        SeedParam(lr, 5, Param::E_FLAG_SHOULD_BE_REMOVED);
        Seed(lr, 5, 6, 3, true);
        lr.KillParam(5);
        Check(!lr.maVehicles[5].IsAlive() && !lr.mVehicleSoaData.mAliveVehicles.IsBitSet(5), "KillParam: the vehicle is killed  @0x82722318");
        Check(!lr.mVehiclesAddedToCrashModule.IsBitSet(5), "KillParam: the crash-module bit is dropped  @0x82722324");
        Check(Holds(lr.maRecentlyRemovedVehicles, { 5 }), "KillParam: the car is queued on maRecentlyRemovedVehicles");
        Check(Holds(lr.maRecentlyRecoveredSlammedTraffic, { 5 }) && lr.maVehicles[5].GetCrashTrafficTypeRaw() == 0xFF,
              "KillParam: a slammed car reports its slam recovery, type -> 0xFF");
        Check(CallsAre({ { 'D', 5, 0, 0 } }, true), "KillParam: SetDead, then Ensure (SetDead saw empty queues)");
    }
    {
        // Not flagged -> lbKillVehicle true; the car is not physical -> SetDead arm.
        Fixture& lr = Fresh();
        SeedParam(lr, 6, 0);
        Seed(lr, 6, -1, 0xFF, true);
        lr.KillParam(6);
        Check(!lr.maVehicles[6].IsAlive() && !lr.mVehiclesAddedToCrashModule.IsBitSet(6)
                  && Holds(lr.maRecentlyRemovedVehicles, { 6 }) && lr.maRecentlyRecoveredSlammedTraffic.GetLength() == 0,
              "KillParam: a non-physical announced car is released (no slam report)");
    }
    {
        // A cab (7) towing a non-physical trailer (599): the trailer tail, in console order.
        Fixture& lr = Fresh();
        SeedParam(lr, 7, Param::E_FLAG_SHOULD_BE_REMOVED);
        Seed(lr, 7, -1, 0xFF, true);
        Seed(lr, 599, -1, 0xFF, true);
        Articulate(lr, 7, 599);
        lr.KillParam(7);
        Check(CallsAre({ { 'D', 7, 0, 0 }, { 'A', 599, 1, 0 }, { 'D', 599, 1, 0 }, { 'A', 7, 2, 0 } }, false),
              "KillParam trailer: SetDead(cab), Detach(trailer), SetDead(trailer), Detach(cab)  @0x82722318..0x827223B0");
        Check(gCalls.size() == 4 && gCalls[1].miRemovedLength == 1,
              "KillParam trailer: Ensure(cab) runs before the trailer is detached  @0x82722324");
        Check(gCalls.size() == 4 && gCalls[3].miRemovedLength == 2,
              "KillParam trailer: Ensure(trailer) runs before the cab is detached  @0x827223A0");
        Check(Holds(lr.maRecentlyRemovedVehicles, { 7, 599 }) && !lr.mVehiclesAddedToCrashModule.IsBitSet(599),
              "KillParam trailer: both halves are released from the crash module");
        Check(!lr.maVehicles[599].IsAlive() && lr.maVehicles[7].muOtherHalfIndex == 0xFFFF
                  && lr.maVehicles[599].muOtherHalfIndex == 0xFFFF
                  && !lr.mVehicleSoaData.mArticulatedVehicles.IsBitSet(7)
                  && !lr.mVehicleSoaData.mArticulatedVehicles.IsBitSet(599),
              "KillParam trailer: the trailer is dead and the pair is broken");
    }
    {
        // The same cab with a PHYSICAL trailer: the tail is skipped (0x82722370).
        Fixture& lr = Fresh();
        SeedParam(lr, 8, Param::E_FLAG_SHOULD_BE_REMOVED);
        Seed(lr, 8, -1, 0xFF, true);
        Seed(lr, 599, 2, 0, true);
        Articulate(lr, 8, 599);
        lr.KillParam(8);
        Check(CallsAre({ { 'D', 8, 0, 0 } }, true) && lr.maVehicles[599].IsAlive()
                  && lr.mVehicleSoaData.mArticulatedVehicles.IsBitSet(599),
              "KillParam: a physical trailer is left alone  @0x82722370");
        Check(Holds(lr.maRecentlyRemovedVehicles, { 8 }) && lr.mVehiclesAddedToCrashModule.IsBitSet(599),
              "KillParam: ...but the cab itself is still released");
    }
    {
        // lbKillVehicle && physical && !divorced -> SetOrphan, and NO Ensure (0x827222AC).
        Fixture& lr = Fresh();
        SeedParam(lr, 9, 0);
        Seed(lr, 9, 7, 3, true);
        lr.KillParam(9);
        Check(CallsAre({ { 'O', 9, 0, 0 } }, true) && lr.mVehiclesAddedToCrashModule.IsBitSet(9)
                  && lr.maRecentlyRemovedVehicles.GetLength() == 0,
              "KillParam: the orphan arm does not release the car");
    }
    {
        // Zombie param whose vehicle is already dead: no vehicle arm at all.
        Fixture& lr = Fresh();
        SeedParam(lr, 10, Param::E_FLAG_ZOMBIE);
        Seed(lr, 10, -1, 0xFF, true);
        lr.maVehicles[10].mxFlags = 0;
        lr.mVehicleSoaData.mAliveVehicles.UnSetBit(10);
        lr.KillParam(10);
        Check(gCalls.empty() && lr.mVehiclesAddedToCrashModule.IsBitSet(10) && lr.maRecentlyRemovedVehicles.GetLength() == 0,
              "KillParam: a dead vehicle is not touched");
    }
    {
        // Online (no divergence): the purgatory tail still runs after the release.
        Fixture& lr = Fresh();
        lr.mbAllowDivergentBehaviour = false;
        SeedParam(lr, 11, Param::E_FLAG_SHOULD_BE_REMOVED);
        Seed(lr, 11, -1, 0xFF, true);
        lr.KillParam(11);
        Check(Holds(lr.maRecentlyRemovedVehicles, { 11 }) && lr.maPurgatoryList.GetLength() == 1
                  && !lr.maParams[11].IsDying(),
              "KillParam: the online tail (ClearDying + PutParamInPurgatory) follows the release");
    }

    // ================= StaticVehicles_KillParam (G58-X4) =================
    {
        Fixture& lr = Fresh();
        lr.maStaticTrafficParams[3].mxFlags = StaticTrafficParam::E_FLAG_ALIVE | StaticTrafficParam::E_FLAG_SHOULD_BE_REMOVED;
        Seed(lr, 403, 8, 3, true);
        lr.StaticVehicles_KillParam(3);
        Check(!lr.maVehicles[403].IsAlive(), "Static: the parked car is killed  @0x82721E00");
        Check(!lr.mVehiclesAddedToCrashModule.IsBitSet(403) && Holds(lr.maRecentlyRemovedVehicles, { 403 }),
              "Static: the parked car is released at its FULL index (400 + 3)  @0x82721E18");
        Check(Holds(lr.maRecentlyRecoveredSlammedTraffic, { 403 }) && lr.maVehicles[403].GetCrashTrafficTypeRaw() == 0xFF,
              "Static: a slammed parked car reports its slam recovery");
        Check(CallsAre({ { 'D', 403, 0, 0 } }, true), "Static: SetDead, then Ensure");
    }
    {
        Fixture& lr = Fresh();
        lr.maStaticTrafficParams[4].mxFlags = StaticTrafficParam::E_FLAG_ALIVE;
        Seed(lr, 404, -1, 0xFF, true);
        lr.StaticVehicles_KillParam(4);
        Check(!lr.mVehiclesAddedToCrashModule.IsBitSet(404) && Holds(lr.maRecentlyRemovedVehicles, { 404 })
                  && lr.maRecentlyRecoveredSlammedTraffic.GetLength() == 0,
              "Static: a hit (announced) parked car is released");
    }
    {
        // Zombie arm: the vehicle is already dead, nothing is released.
        Fixture& lr = Fresh();
        lr.maStaticTrafficParams[5].mxFlags = StaticTrafficParam::E_FLAG_ALIVE | StaticTrafficParam::E_FLAG_ZOMBIE;
        lr.mVehiclesAddedToCrashModule.SetBit(405);
        lr.StaticVehicles_KillParam(5);
        Check(gCalls.empty() && lr.mVehiclesAddedToCrashModule.IsBitSet(405) && lr.maRecentlyRemovedVehicles.GetLength() == 0,
              "Static: the zombie arm does not release anything");
    }

    Check(gAsserts == 0, "no assert fires on the valid cases");
    std::printf("FxTrafficCrashRelease: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures ? 1 : 0;
}
