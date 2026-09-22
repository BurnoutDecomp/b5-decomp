// FX-RUMBLE (crash parity 2026-09-22, G10-D1/D2/D3/D5/D7): the PRODUCTION RumbleManager bodies
// (Construct / Prepare / Update / UpdateImpacts / OnVehicleAggressorImpact / OnVehicleVictimImpact /
// PlayJolt, extracted from src/GameSource/GameState/RumbleManager/BrnRumbleManager.cpp by
// run_rumble_manager.py) driven through the REAL RCEntityActiveRaceCarOutputInterface /
// RaceCarState / ContactSpyInterface / ContactSpyData types, checked against numbers read off the
// ARTIST asm and image:
//   Construct      @0x82378A70  flags +0x398..+0x39F = 1,0,0,0,0,0,0,1; queues max 4 len 0
//   PlayJolt       @0x8236E7F8  {miPlayer 0, miPort -1, prio} + 0x30-byte memcpy, AddEventSafe (gated)
//   Update         @0x82386A98  crash latch (interface's OWN index, lbz 0x77A), pause gate +0x399,
//                               prio 0x3F2; air latch > flt_82004014 (0.1), landing prio 0x3EA with
//                               low sustain = flt_82CDBE40 (0.3) * TimePlayerInAir(), no pause gate;
//                               stb 0 +0x39D on both exits
//   UpdateImpacts  @0x82379370  gates -1 / +0x399 / +0x39B / meDriverType / mpData / run; max of
//                               |mNormalStress| (xyz only), x0.1 (0x8202ADF8) when entity B's owner
//                               byte is 0 (WORLD); high lanes 8/10/11 x Clamp(imp*0.001 0x8202ADF4),
//                               low lanes 2/4/5 x Clamp(imp*2e-5 0x8202ADF0); prio 0x3E8
//   OnVehicleAggressorImpact @0x823795C8  jump table 0x8237964C: 1/7/8 -> 0.25, 2 -> 0.5, 3..6 -> 1,
//                               else 0; assert on NONE (:751); low lanes 5/4/2; prio 0x3E9; no pause gate
// Every template lane that is NOT scaled is compared BIT-EXACT against the word x360rd reads at its
// address (0x82030F0C / 0x82CDBDE0 / 0x82CDBE10 / 0x82CDBE44); findinit finds no CRT writer.
#include "GameSource/GameState/RumbleManager/BrnRumbleManager.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include <cstdio>
#include <cstring>
#include <cmath>

static unsigned gAsserts = 0, gChecks = 0, gFailures = 0;

namespace CgsDev
{
namespace Assert
{
    int   BeginAssert() { return 0; }
    int   FireAssert(const char*, const char*, int) { ++gAsserts; return 0; }
    void* EndAssert() { return nullptr; }
}
namespace Log { DebugPrint* gpDebugPrint = nullptr; }   // the PlayJolt [DIAG] stays silent
StrStreamBase& StrStreamBase::operator<<(s32) { return *this; }
StrStreamBase& StrStreamBase::operator<<(f32) { return *this; }
}

// Harness-only: RaceCarState's default ctor calls Clear() (BrnVehicleEvents.cpp, not under test).
// Every field the rumble bodies read is set explicitly by each case below.
void BrnPhysics::Vehicle::RaceCarState::Clear() { std::memset(this, 0, sizeof(*this)); }

// The production bodies (and the production helpers they call through the real types:
// RCEntityActiveRaceCarOutputInterface::GetRaceCarState / TimePlayerInAir,
// ContactSpyInterface::Construct / GetRaceCarContactRunList), extracted verbatim.
#include "rumble_methods.inc"

using namespace BrnGameState;
using CgsInput::InputIO::JoltEffect;
using CgsInput::InputIO::PlayJoltEffectEvent;
namespace Vehicle = BrnPhysics::Vehicle;
namespace ContactSpy = BrnPhysics::ContactSpy;

static void Check(bool lbPass, const char* lpcName)
{
    ++gChecks;
    if (!lbPass)
    {
        ++gFailures;
        std::fprintf(stderr, "FAIL: %s\n", lpcName);
    }
}

static u32 Bits(f32 lfValue) { u32 lu; std::memcpy(&lu, &lfValue, 4); return lu; }
static f32 FromBits(u32 lu) { f32 lf; std::memcpy(&lf, &lu, 4); return lf; }
static const f32* Lanes(const JoltEffect& lr) { return &lr.mLowFreqJoltData.mfAttackTime; }

// The four templates, word for word as tools/re/x360rd.py reads them.
static const u32 KAU_CRASH_JOLT[12]   = { 0x00000000, 0x00000000, 0x3F000000, 0x3F800000, 0x3F800000, 0x3F800000,   // 0x82030F0C
                                          0x00000000, 0x00000000, 0x3F000000, 0x3F800000, 0x3F800000, 0x3F800000 };
static const u32 KAU_IMPACT_JOLT[12]  = { 0x00000000, 0x00000000, 0x3F000000, 0x3E4CCCCD, 0x3F19999A, 0x3F000000,   // 0x82CDBDE0
                                          0x3D4CCCCD, 0x00000000, 0x3DCCCCCD, 0x3DCCCCCD, 0x3F19999A, 0x3F000000 };
static const u32 KAU_VEHICLE_JOLT[12] = { 0x00000000, 0x00000000, 0x3F000000, 0x3E4CCCCD, 0x3DCCCCCD, 0x3E4CCCCD,   // 0x82CDBE10
                                          0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000 };
static const u32 KAU_LANDED_JOLT[12]  = { 0x00000000, 0x3E4CCCCD, 0x00000000, 0x3E99999A, 0x3F19999A, 0x3E99999A,   // 0x82CDBE44 (lane 2 runtime)
                                          0x00000000, 0x00000000, 0x3DCCCCCD, 0x3DCCCCCD, 0x3F800000, 0x3F333333 };
static const u32 KU_RUMBLE_TIME_FACTOR = 0x3E99999A;   // 0x82CDBE40
static const u32 KU_WORLD_SCALE        = 0x3DCCCCCD;   // 0x8202ADF8
static const u32 KU_SCALE_HIGH         = 0x3A83126F;   // 0x8202ADF4
static const u32 KU_SCALE_LOW          = 0x37A7C5AC;   // 0x8202ADF0

// Big fixtures live in static storage.
static BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface gInterface;
static ContactSpy::ContactSpyData gContactData;
static RumbleManager gManager;

static const PlayJoltEffectEvent& Jolt(s32 liIndex) { return gManager.mPlayJoltEffectEventQueue.GetEvent(liIndex); }
static s32 JoltCount() { return gManager.mPlayJoltEffectEventQueue.GetLength(); }

static void Fresh()
{
    std::memset(&gManager, 0xCD, sizeof(gManager));   // prove Construct seeds, not the harness
    gManager.Construct();
    gManager.Prepare();
    for (s32 li = 0; li < 8; ++li)
    {
        gInterface.maRaceCarStates[li].Clear();
        gInterface.maRaceCarStates[li].meDriverType = Vehicle::E_DRIVER_TYPE_PLAYER;
    }
    gInterface.mePlayerActiveRaceCarIndex = E_ACTIVE_RACE_CAR_INDEX_2;
    gContactData.mRaceCarContactQueue.Construct(BrnWorld::E_ENTITYTYPE_RACECAR);
    gContactData.mRaceCarContactRunList.Construct(BrnWorld::E_ENTITYTYPE_RACECAR);
}

static void Update(ContactSpy::ContactSpyInterface* lpSpy, EActiveRaceCarIndex leArgument = E_ACTIVE_RACE_CAR_INDEX_2)
{
    gManager.Update(&gInterface, nullptr, leArgument, lpSpy, 1.0f / 60.0f);
}

static bool LanesEqualBits(const JoltEffect& lr, const u32* lpauWords, u32 luSkipMask)
{
    const f32* lpf = Lanes(lr);
    for (s32 li = 0; li < 12; ++li)
    {
        if ((luSkipMask >> li) & 1u) continue;
        if (Bits(lpf[li]) != lpauWords[li]) return false;
    }
    return true;
}

static void AddContact(u32 luEntityA, u32 luEntityB, f32 lfX, f32 lfY, f32 lfZ)
{
    ContactSpy::RaceCarContact lContact;
    std::memset(&lContact, 0, sizeof(lContact));
    lContact.mEntityIdA.muValue = luEntityA;
    lContact.mEntityIdB.muValue = luEntityB;
    lContact.mNormalStress = Vector3{ lfX, lfY, lfZ, 1.0e30f };   // w must NOT count (vmsum3fp)
    lContact.mFrictionStress = Vector3{ 1.0e9f, 1.0e9f, 1.0e9f, 0.0f };   // the +0x10 neighbour must not count
    gContactData.mRaceCarContactQueue.AddEventSafe(lContact);
}

static void AddRun(u32 luEntity, s32 liStart, s32 liLength)
{
    ContactSpy::ContactSpyRunData lRun;
    std::memset(&lRun, 0, sizeof(lRun));
    lRun.mEntityId.muValue = luEntity;
    lRun.miStartIndex = liStart;
    lRun.miRunLength  = liLength;
    gContactData.mRaceCarContactRunList.AddEventSafe(lRun);
}

int main()
{
    ContactSpy::ContactSpyInterface lUnbound;
    lUnbound.Construct();
    ContactSpy::ContactSpyInterface lSpy;
    lSpy.Construct();
    lSpy.SetData(&gContactData);

    const u32 KU_PLAYER_ENTITY = 0x01000005u;   // owner 1 (race car), index 5
    const f32 KF_SCALE_HIGH = FromBits(KU_SCALE_HIGH), KF_SCALE_LOW = FromBits(KU_SCALE_LOW);

    // ---- the constants are the image's words ---------------------------------------------
    Check(Bits(KF_RUMBLE_IMPACT_WORLD_SCALE) == KU_WORLD_SCALE, "KF_RUMBLE_IMPACT_WORLD_SCALE == .rdata 0x8202ADF8");
    Check(Bits(KF_RUMBLE_IMPACT_SCALE_HIGH) == KU_SCALE_HIGH, "KF_RUMBLE_IMPACT_SCALE_HIGH == .rdata 0x8202ADF4");
    Check(Bits(KF_RUMBLE_IMPACT_SCALE_LOW) == KU_SCALE_LOW, "KF_RUMBLE_IMPACT_SCALE_LOW == .rdata 0x8202ADF0");
    Check(KN_RUMBLE_IMPACT_PRIORITY == 0x3E8 && KN_RUMBLE_VEHICLE_IMPACT_PRIORITY == 0x3E9 &&
          KN_LANDING_RUMBLE_PRIORITY == 0x3EA && KN_CRASH_RUMBLE_PRIORITY == 0x3F2, "KN_* priorities == the li immediates");

    // ---- Construct @0x82378A70 --------------------------------------------------------------
    Fresh();
    Check(gManager.mbRumbleEnabled && !gManager.mbRumblePaused && !gManager.mbGameWasPaused &&
          !gManager.mbInPictureParadise && !gManager.mbPlayerIsCrashing &&
          !gManager.mbPlayerHasJustCheckedTraffic && !gManager.mbPlayerIsInAir && gManager.mbWheelForceFeedback,
          "Construct seeds +0x398..+0x39F = 1,0,0,0,0,0,0,1");
    Check(gManager.mPlayJoltEffectEventQueue.GetMaxLength() == 4 && gManager.mPlayJoltEffectEventQueue.GetLength() == 0 &&
          gManager.mPlayJoltEffectEventQueue.mpEvents == &gManager.mPlayJoltEffectEventQueue.maEvents[0],
          "Construct binds the jolt queue (0x823736F0: inline storage, 4, 0)");
    Check(gManager.mPlayRumbleEffectEventQueue.GetMaxLength() == 4 && gManager.mPlayRumbleEffectEventQueue.mpEvents != nullptr &&
          gManager.mChangeVolumeRumbleEffectEventQueue.GetMaxLength() == 4 && gManager.mChangeVolumeRumbleEffectEventQueue.mpEvents != nullptr &&
          gManager.mStopRumbleEffectEventQueue.GetMaxLength() == 4 && gManager.mStopRumbleEffectEventQueue.mpEvents != nullptr,
          "Construct binds the play-rumble / change-volume / stop queues");
    Check(gManager.mau8SurfaceID[3] == 0xFF && gManager.manRumbleID[0] == -1, "Prepare seeds the per-wheel slots");

    // ---- PlayJolt @0x8236E7F8 -----------------------------------------------------------------
    {
        Fresh();
        JoltEffect lJolt;
        f32* lpf = &lJolt.mLowFreqJoltData.mfAttackTime;
        for (s32 li = 0; li < 12; ++li) lpf[li] = 1.5f + li;
        gManager.PlayJolt(1234, lJolt);
        Check(JoltCount() == 1 && Jolt(0).miPlayer == 0 && Jolt(0).miPort == -1 && Jolt(0).miRumblePriority == 1234,
              "PlayJolt record {0, -1, prio}");
        Check(std::memcmp(&Jolt(0).mJoltEffect, &lJolt, sizeof(JoltEffect)) == 0, "PlayJolt copies the 0x30-byte JoltEffect");
        const unsigned luAsserts = gAsserts;
        for (s32 li = 0; li < 4; ++li) gManager.PlayJolt(1, lJolt);
        Check(JoltCount() == 4 && gAsserts == luAsserts, "a fifth jolt is DROPPED by AddEventSafe, no assert");
    }

    // ---- Update: the crash latch -------------------------------------------------------------
    {
        Fresh();
        gInterface.maRaceCarStates[2].mbCrashing = true;
        Update(&lUnbound);
        Check(JoltCount() == 1 && Jolt(0).miRumblePriority == 1010, "crash start -> one prio-1010 jolt");
        Check(JoltCount() == 1 && LanesEqualBits(Jolt(0).mJoltEffect, KAU_CRASH_JOLT, 0), "crash jolt == .rdata 0x82030F0C bit-exact");
        Check(gManager.mbPlayerIsCrashing, "crash latch +0x39C set");
        Update(&lUnbound);
        Check(JoltCount() == 1, "held crash does not re-jolt");
        gInterface.maRaceCarStates[2].mbCrashing = false;
        Update(&lUnbound);
        Check(!gManager.mbPlayerIsCrashing && JoltCount() == 1, "crash end clears the latch, no jolt");

        Fresh();
        gManager.mbRumblePaused = true;
        gInterface.maRaceCarStates[2].mbCrashing = true;
        Update(&lUnbound);
        Check(gManager.mbPlayerIsCrashing && JoltCount() == 0, "paused: latch set, crash jolt suppressed (lbz +0x399)");

        // lwz 0x2858 -- the INTERFACE's player index decides, not the r6 argument.
        Fresh();
        gInterface.maRaceCarStates[2].mbCrashing = true;
        Update(&lUnbound, E_ACTIVE_RACE_CAR_INDEX_INVALID);
        Check(JoltCount() == 1 && Jolt(0).miRumblePriority == 1010, "crash test reads the interface's own index (argument -1)");
        Fresh();
        gInterface.mePlayerActiveRaceCarIndex = E_ACTIVE_RACE_CAR_INDEX_INVALID;
        gInterface.maRaceCarStates[2].mbCrashing = true;
        Update(&lUnbound);
        Check(JoltCount() == 0 && !gManager.mbPlayerIsCrashing, "interface index -1 -> not crashing");

        Fresh();
        gManager.mbPlayerHasJustCheckedTraffic = true;
        Update(&lUnbound);
        Check(!gManager.mbPlayerHasJustCheckedTraffic, "+0x39D cleared on the grounded exit");
        gInterface.maRaceCarStates[2].mfTimeInAir = 1.0f;
        gManager.mbPlayerHasJustCheckedTraffic = true;
        Update(&lUnbound);
        Check(!gManager.mbPlayerHasJustCheckedTraffic, "+0x39D cleared on the in-air exit");
    }

    // ---- Update: the air latch and the landing jolt ------------------------------------------
    {
        Fresh();
        gInterface.maRaceCarStates[2].mfTimeInAir = 0.5f;
        Update(&lUnbound);
        Check(gManager.mbPlayerIsInAir && JoltCount() == 0, "air time 0.5 > 0.1 latches +0x39E, no jolt");
        const f32 lfLandingAir = 0.05f;
        gInterface.maRaceCarStates[2].mfTimeInAir = lfLandingAir;
        Update(&lUnbound);
        Check(JoltCount() == 1 && Jolt(0).miRumblePriority == 1002, "landing -> one prio-1002 jolt");
        const f32 lfExpectedSustain = FromBits(KU_RUMBLE_TIME_FACTOR) * lfLandingAir;
        Check(JoltCount() == 1 && Bits(Jolt(0).mJoltEffect.mLowFreqJoltData.mfSustainTime) == Bits(lfExpectedSustain),
              "landing low sustain = 0.3 (0x82CDBE40) * TimePlayerInAir() on the landing frame");
        Check(JoltCount() == 1 && LanesEqualBits(Jolt(0).mJoltEffect, KAU_LANDED_JOLT, 1u << 2),
              "landing jolt's other lanes == .data 0x82CDBE44 bit-exact");
        Check(!gManager.mbPlayerIsInAir, "landing clears +0x39E");

        Fresh();
        gInterface.maRaceCarStates[2].mfTimeInAir = 0.5f;
        Update(&lUnbound);
        gManager.mbRumblePaused = true;
        gInterface.maRaceCarStates[2].mfTimeInAir = 0.0f;
        Update(&lUnbound);
        Check(JoltCount() == 1 && Jolt(0).miRumblePriority == 1002, "landing jolt has NO pause gate");

        Fresh();
        gInterface.maRaceCarStates[2].mfTimeInAir = 0.5f;
        Update(&lUnbound);
        gInterface.maRaceCarStates[2].mfTimeInAir = 0.1f;   // == flt_82004014: bgt is strict
        Update(&lUnbound);
        Check(JoltCount() == 1 && Bits(Jolt(0).mJoltEffect.mLowFreqJoltData.mfSustainTime) ==
              Bits(FromBits(KU_RUMBLE_TIME_FACTOR) * 0.1f), "exactly 0.1 in air is landed (strict >)");

        Fresh();
        gInterface.maRaceCarStates[2].mfTimeInAir = 0.5f;
        Update(&lUnbound);
        gInterface.mePlayerActiveRaceCarIndex = E_ACTIVE_RACE_CAR_INDEX_INVALID;
        Update(&lUnbound);
        Check(JoltCount() == 1 && Jolt(0).mJoltEffect.mLowFreqJoltData.mfSustainTime == 0.0f,
              "index -1 lands with sustain 0 (flt_82001CC0)");
    }

    // ---- UpdateImpacts @0x82379370 (via Update) ----------------------------------------------
    {
        // The player's run is contacts [1..3]; [0] and [4] belong to another car and must not count.
        Fresh();
        gInterface.maRaceCarStates[2].mEntityId.muValue = KU_PLAYER_ENTITY;
        AddContact(0x01000009u, 0x01000005u, 1.0e6f, 0.0f, 0.0f);     // [0] other car's run
        AddContact(KU_PLAYER_ENTITY, 0x00000000u, 3000.0f, 4000.0f, 0.0f); // [1] WORLD: 5000 * 0.1 = 500
        AddContact(KU_PLAYER_ENTITY, 0x01000002u, 0.0f, 0.0f, 800.0f);    // [2] race car: 800   <- hardest
        AddContact(KU_PLAYER_ENTITY, 0x02000007u, 600.0f, 0.0f, 0.0f);    // [3] traffic: 600
        AddContact(0x01000009u, 0x00000000u, 9.0e6f, 0.0f, 0.0f);     // [4] other car's run
        AddRun(0x01000009u, 0, 1);
        AddRun(KU_PLAYER_ENTITY, 1, 3);
        Update(&lSpy);
        Check(JoltCount() == 1 && Jolt(0).miRumblePriority == 1000, "hardest contact -> one prio-1000 jolt");
        if (JoltCount() == 1)
        {
            const f32 lfHard = 800.0f;
            const f32 lfHigh = lfHard * KF_SCALE_HIGH;   // 0.8 (inside [0,1])
            const f32 lfLow  = lfHard * KF_SCALE_LOW;    // 0.016
            const JoltEffect& lr = Jolt(0).mJoltEffect;
            const f32* lpfTemplate = nullptr;
            f32 lafTemplate[12];
            for (s32 li = 0; li < 12; ++li) lafTemplate[li] = FromBits(KAU_IMPACT_JOLT[li]);
            lpfTemplate = lafTemplate;
            Check(Bits(lr.mHighFreqJoltData.mfSustainTime) == Bits(lpfTemplate[8] * lfHigh) &&
                  Bits(lr.mHighFreqJoltData.mfPeakSpeedValue) == Bits(lpfTemplate[10] * lfHigh) &&
                  Bits(lr.mHighFreqJoltData.mfSustainSpeedValue) == Bits(lpfTemplate[11] * lfHigh),
                  "high lanes 8/10/11 x Clamp(800 * 0.001)");
            Check(Bits(lr.mLowFreqJoltData.mfSustainTime) == Bits(lpfTemplate[2] * lfLow) &&
                  Bits(lr.mLowFreqJoltData.mfPeakSpeedValue) == Bits(lpfTemplate[4] * lfLow) &&
                  Bits(lr.mLowFreqJoltData.mfSustainSpeedValue) == Bits(lpfTemplate[5] * lfLow),
                  "low lanes 2/4/5 x Clamp(800 * 2e-5)");
            Check(LanesEqualBits(lr, KAU_IMPACT_JOLT, (1u << 2) | (1u << 4) | (1u << 5) | (1u << 8) | (1u << 10) | (1u << 11)),
                  "impact jolt's unscaled lanes == .data 0x82CDBDE0 bit-exact");
        }

        // A world contact alone: 5000 x 0.1 = 500 -> high 0.5.
        Fresh();
        gInterface.maRaceCarStates[2].mEntityId.muValue = KU_PLAYER_ENTITY;
        AddContact(KU_PLAYER_ENTITY, 0x000000FFu, 3000.0f, 4000.0f, 0.0f);   // owner byte 0, index bits set
        AddRun(KU_PLAYER_ENTITY, 0, 1);
        Update(&lSpy);
        Check(JoltCount() == 1 && Bits(Jolt(0).mJoltEffect.mHighFreqJoltData.mfPeakSpeedValue) ==
              Bits(FromBits(KAU_IMPACT_JOLT[10]) * ((5000.0f * FromBits(KU_WORLD_SCALE)) * KF_SCALE_HIGH)),
              "entity B owner byte 0 (WORLD) scales the magnitude by 0.1");

        // Huge impact: both volumes clamp to 1 -> the scaled lanes equal the template.
        Fresh();
        gInterface.maRaceCarStates[2].mEntityId.muValue = KU_PLAYER_ENTITY;
        AddContact(KU_PLAYER_ENTITY, 0x01000001u, 100000.0f, 0.0f, 0.0f);
        AddRun(KU_PLAYER_ENTITY, 0, 1);
        Update(&lSpy);
        Check(JoltCount() == 1 && LanesEqualBits(Jolt(0).mJoltEffect, KAU_IMPACT_JOLT, 0), "both volumes clamp at 1.0 (fsel Min)");

        // Zero-magnitude contact: hardest stays 0.0 -> no jolt (fcmpu f30, 0.0 / ble).
        Fresh();
        gInterface.maRaceCarStates[2].mEntityId.muValue = KU_PLAYER_ENTITY;
        AddContact(KU_PLAYER_ENTITY, 0x01000001u, 0.0f, 0.0f, 0.0f);
        AddRun(KU_PLAYER_ENTITY, 0, 1);
        Update(&lSpy);
        Check(JoltCount() == 0, "zero impact -> no jolt");

        // The gates, each alone.
        const char* lapcGate[6] = { "paused (+0x399)", "picture paradise (+0x39B)", "meDriverType != PLAYER (+0x458)",
                                    "unbound spy (mpData == NULL)", "no run for the player's entity", "player index argument -1" };
        for (s32 liGate = 0; liGate < 6; ++liGate)
        {
            Fresh();
            gInterface.maRaceCarStates[2].mEntityId.muValue = KU_PLAYER_ENTITY;
            AddContact(KU_PLAYER_ENTITY, 0x01000001u, 0.0f, 900.0f, 0.0f);
            AddRun(liGate == 4 ? 0x01000009u : KU_PLAYER_ENTITY, 0, 1);
            if (liGate == 0) gManager.mbRumblePaused = true;
            if (liGate == 1) gManager.mbInPictureParadise = true;
            if (liGate == 2) gInterface.maRaceCarStates[2].meDriverType = Vehicle::E_DRIVER_TYPE_AI;
            Update(liGate == 3 ? &lUnbound : &lSpy, liGate == 5 ? E_ACTIVE_RACE_CAR_INDEX_INVALID : E_ACTIVE_RACE_CAR_INDEX_2);
            char lacName[96];
            std::snprintf(lacName, sizeof(lacName), "UpdateImpacts gate: %s -> no jolt", lapcGate[liGate]);
            Check(JoltCount() == 0, lacName);
        }

        // GetRaceCarState(r5): the ARGUMENT index picks the state whose mEntityId is looked up.
        Fresh();
        gInterface.maRaceCarStates[3].mEntityId.muValue = KU_PLAYER_ENTITY;
        gInterface.maRaceCarStates[2].mEntityId.muValue = 0x01000009u;
        AddContact(KU_PLAYER_ENTITY, 0x01000001u, 0.0f, 900.0f, 0.0f);
        AddRun(KU_PLAYER_ENTITY, 0, 1);
        Update(&lSpy, E_ACTIVE_RACE_CAR_INDEX_3);
        Check(JoltCount() == 1 && Jolt(0).miRumblePriority == 1000, "UpdateImpacts reads the state at the argument index");
        Check(gAsserts == 0, "no assert on any valid UpdateImpacts fixture");
    }

    // ---- OnVehicleAggressorImpact @0x823795C8 / OnVehicleVictimImpact --------------------------
    {
        const f32 lafVolume[10] = { 0.0f, 0.25f, 0.5f, 1.0f, 1.0f, 1.0f, 1.0f, 0.25f, 0.25f, 0.0f };
        for (s32 liVictim = 0; liVictim < 2; ++liVictim)
        {
            for (s32 liType = 0; liType < 10; ++liType)
            {
                Fresh();
                const unsigned luAsserts = gAsserts;
                const Vehicle::EImpactType leType = static_cast<Vehicle::EImpactType>(liType);
                if (liVictim) gManager.OnVehicleVictimImpact(leType); else gManager.OnVehicleAggressorImpact(leType);
                char lacName[112];
                const f32 lfVolume = lafVolume[liType];
                if (lfVolume > 0.0f)
                {
                    bool lbLanes = JoltCount() == 1;
                    if (lbLanes)
                    {
                        const JoltEffect& lr = Jolt(0).mJoltEffect;
                        lbLanes =
                            Jolt(0).miRumblePriority == 1001 && Jolt(0).miPlayer == 0 && Jolt(0).miPort == -1 &&
                            Bits(lr.mLowFreqJoltData.mfSustainTime) == Bits(FromBits(KAU_VEHICLE_JOLT[2]) * lfVolume) &&
                            Bits(lr.mLowFreqJoltData.mfPeakSpeedValue) == Bits(FromBits(KAU_VEHICLE_JOLT[4]) * lfVolume) &&
                            Bits(lr.mLowFreqJoltData.mfSustainSpeedValue) == Bits(FromBits(KAU_VEHICLE_JOLT[5]) * lfVolume) &&
                            LanesEqualBits(lr, KAU_VEHICLE_JOLT, (1u << 2) | (1u << 4) | (1u << 5));
                    }
                    std::snprintf(lacName, sizeof(lacName), "%s type %d -> prio 1001, low lanes x %.2f, rest == 0x82CDBE10",
                                  liVictim ? "victim" : "aggressor", liType, lfVolume);
                    Check(lbLanes, lacName);
                }
                else
                {
                    std::snprintf(lacName, sizeof(lacName), "%s type %d -> no jolt", liVictim ? "victim" : "aggressor", liType);
                    Check(JoltCount() == 0, lacName);
                }
                std::snprintf(lacName, sizeof(lacName), "%s type %d -> %s", liVictim ? "victim" : "aggressor", liType,
                              liType == 0 ? "the E_IMPACT_NONE assert fires (:751)" : "no assert");
                Check((gAsserts - luAsserts) == (liType == 0 ? 1u : 0u), lacName);
            }
        }
        Fresh();
        gManager.mbRumblePaused = true;
        gManager.OnVehicleAggressorImpact(Vehicle::E_IMPACT_SLAM);
        Check(JoltCount() == 1, "vehicle-impact jolt has NO pause gate");
        Fresh();
        gManager.OnVehicleAggressorImpact(static_cast<Vehicle::EImpactType>(-1));
        Check(JoltCount() == 0, "negative type takes the default (cmplwi is unsigned)");
    }

    std::printf("RumbleManager: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures ? 1 : 0;
}
