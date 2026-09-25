// FX-LADDER item 1 regression (crash parity 2026-09-25): the car-vs-car impact ladder
// VehicleManager::CheckForAllTypesOfImpacts @0x82642E58 replayed on RECORDED contacts.
//
// run_fxladder_replay.py extracts the production ladder (the entry point and all eight rungs) and
// the production callees it reads through, and this fixture feeds it player-involved contacts the
// [td-replay] witness captured bit for bit in live runs (BRN_TD_DIAG=1 BRN_TD_REPLAY=1; layout "v1"
// is documented at the witness in BrnVehicleManager.cpp). Every record carries the console's
// decision for those inputs, derived gate by gate from the ARTIST asm; the gate that decides it and
// its address are on the record. The only doubles are the two commits the ladder calls
// (InstantTakedown, ApplyShunt): they record their arguments and change nothing.
//
// What the pin covers (item 1's finding: the ladder is console-faithful, so these are the console's
// own verdicts, "none" included):
//   * classified contacts -- slam, boost-slam, trading paint, shunt, boost-shunt, nudge;
//   * SlamAndTradingPaint's no-arm TRUE (0x8261A248 ble -> 0x8261A348 li r3,1): impact NONE, and the
//     ladder ends there (Stationary never runs);
//   * the 0.3 s cooldown (HasRaceCarHadRecentImpact @0x825B4EB8 at 0x8261A3C0/D8, 0x82619F9C/B4);
//   * the already-crashing gate (0x82642EC0..D4) and the pile-on obstacle age (+0xEF0.y > 1.0 at
//     0x8263DB38);
//   * geometry exhaustion (|n.At_A| + |n.At_B| < 1.9 at 0x8261A454 and dot(At_A, At_B) < 0.75 at
//     0x82619FDC).
// A change to any gate, threshold, the rung order or the aggressor pick turns a record RED.
#include "GameSource/Physics/VehicleManager/BrnVehicleManager.h"
#include "GameSource/Physics/VehicleManager/VehiclePhysics/RaceCarPhysics.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>

#include "restored_methods.inc"

namespace CgsDev { namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char* lpcMessage, const char*, int)
{
    std::fprintf(stderr, "Assertion: %s\n", lpcMessage); std::abort();
}
void* EndAssert() { return nullptr; }
} }

// The ladder's witness is compiled in and stays OFF: gpDebugPrint is null, so no stream call runs.
namespace CgsDev {
StrStreamBase::StrStreamBase() : mePrintMode(E_PRINTMODE_DECIMAL) {}
StrStreamBase& StrStreamBase::operator<<(s32) { return *this; }
StrStreamBase& StrStreamBase::operator<<(u32) { return *this; }
StrStreamBase& StrStreamBase::operator<<(u64) { return *this; }
StrStreamBase& StrStreamBase::operator<<(f32) { return *this; }
StrStreamBase& StrStreamBase::operator<<(void*) { return *this; }
StrStreamBase& StrStreamBase::operator<<(PrintMode) { return *this; }
void StrStreamBase::AppendFormat(const char*, ...) {}
namespace Log {
DebugPrint* gpDebugPrint = nullptr;
StrStreamBase& DebugPrint::operator<<(const char*) { return *this; }
} }

// The fixture constructs the real manager and its contained cars. These unrelated vtable entries
// must link but must never run in a ladder replay (the same set tests/RivalImpacts.cpp carries).
namespace CgsDev {
DebugComponent::DebugComponent() {}
void DebugComponent::Update() { std::abort(); }
void DebugComponent::RenderWorld(Debug3DImmediateRender*) { std::abort(); }
void DebugComponent::RenderHUD(Debug2DImmediateRender*) { std::abort(); }
const char* DebugComponent::GetPath() const { std::abort(); }
const char* DebugComponent::GetName() const { std::abort(); }
bool DebugComponent::IsSimple() const { std::abort(); }
void DebugComponent::OnActivate() { std::abort(); }
void DebugComponent::OnRegister() { std::abort(); }
namespace DebugUI {
Window::Window() {}
void Window::Update(f32, InputEvent) { std::abort(); }
void Window::Render(Debug2DImmediateRender*) { std::abort(); }
void Window::OnGetFocus() { std::abort(); }
void Window::OnLostFocus() { std::abort(); }
void Window::GetMenuPath(char*, s32) { std::abort(); }
void Window::GetSelectedItemString(char*, s32) const { std::abort(); }
} }

using namespace BrnPhysics::Vehicle;

namespace
{
    s32 giTakedowns = 0;
    s32 giShunts = 0;
    EntityId gLastVictim{ 0 }, gLastAggressor{ 0 };
    s32 giLastTakedownType = -2;
}

namespace BrnPhysics { namespace Vehicle {
VecFloat SimpleVehiclePhysics::GetSteeringAngle() const { std::abort(); }
void SimpleVehiclePhysics::ClearCrashing() { std::abort(); }
void SimpleVehiclePhysics::SetCrashing() { std::abort(); }
void VehiclePhysics::Update(VecFloat, VecFloat, const Matrix44Affine*, const BrnPlayerDriverControls*,
    bool, bool, bool, CgsNumeric::Random&) { std::abort(); }
void VehiclePhysics::UpdateSuspension(VecFloat) { std::abort(); }
void VehiclePhysics::SetCrashing() { std::abort(); }
void VehiclePhysics::ClearCrashing() { std::abort(); }
VecFloat VehiclePhysics::GetSteeringAngle() const { std::abort(); }
bool RaceCarPhysics::IsCrashingNormally() const { std::abort(); }
bool RaceCarPhysics::Prepare(Matrix44Affine, Vector3, Vector3, Vector3, Vector3,
    const CgsGeometric::AxisAlignedBox&, rw::physics::Inertia, VehicleAttribs*, const Vector3*, const f32*, u8)
{ std::abort(); }
void RaceCarPhysics::SetCrashing() { std::abort(); }
void RaceCarPhysics::Update(VecFloat, VecFloat, const Matrix44Affine*, const BrnPlayerDriverControls*,
    bool, bool, bool, CgsNumeric::Random&) { std::abort(); }
f32 RaceCarPhysics::GetShowtimePlayerCarStrength() const { std::abort(); }
bool RaceCarPhysics::IsPlayerVehicleInShowtime() const { std::abort(); }
void RaceCarPhysics::UpdateAftertouch(const BrnPlayerDriverControls*, const Matrix44Affine*, VecFloat, bool, bool)
{ std::abort(); }
void TrafficPhysics::Update(f32, f32, const Matrix44Affine*, const BrnPlayerDriverControls*, bool, bool, bool)
{ std::abort(); }
const char* PhysicalTrafficManagerDebugComponent::GetName() const { std::abort(); }
const char* VehicleManagerDebugComponent::GetName() const { std::abort(); }
const char* DebugComponent::GetName() const { std::abort(); }
const char* DebugComponent::GetPath() const { std::abort(); }
// [td-slamgate] witness lookup the ladder's (off) witness names.
const RaceCarPhysics::TdSlamGateSample* RaceCarPhysics::FindTdSlamGateSample(const RaceCarPhysics*) { return nullptr; }

// The two commits the ladder calls: observed-output doubles.
void VehicleManager::ApplyShunt(RaceCarResponseInfo*) { ++giShunts; }
void VehicleManager::InstantTakedown(EntityId lVictim, EntityId lAggressor, Vector3, Vector3, f32,
    VehicleOutputRequestInterface*, VehicleManagerOutputInterface*, VehicleOutputInterface*,
    BrnPhysics::Deformation::DeformationInputInterface*, BrnGameState::ETakedownType leType)
{
    ++giTakedowns; gLastVictim = lVictim; gLastAggressor = lAggressor; giLastTakedownType = static_cast<s32>(leType);
}
} }

namespace
{
    unsigned guChecks = 0, guFailures = 0;
    void Check(bool lbPass, const char* lpcLabel, const char* lpcWhat)
    {
        ++guChecks;
        if (!lbPass) { ++guFailures; std::printf("FAIL %s: %s\n", lpcLabel, lpcWhat); }
    }

    // One ladder evaluation, "v1" words (see the [td-replay] banner in BrnVehicleManager.cpp).
    const s32 KI_WORDS = 154;
    struct Record
    {
        const char* mpcLabel;
        const char* mpcGate;      // the deciding gate, with its address
        u32 mauWord[KI_WORDS];
        s32 miImpact;             // meImpactType after the ladder
        s32 miAggressor;          // meAggressorActiveRaceCarIndex
        s32 miVictim;             // meVictimActiveRaceCarIndex
        s32 miCrashA;             // mbCrashRaceCarA
        s32 miCrashB;             // mbCrashRaceCarB
        s32 miTakedowns;          // InstantTakedown calls
        s32 miShunts;             // ApplyShunt calls (HeadToHead's)
        s32 miTakedownVictim;     // entity index of the last InstantTakedown's victim (-1: none)
        s32 miTakedownAggressor;  // ... and aggressor
        s32 miTakedownType;       // ... and BrnGameState::ETakedownType
    };

    f32 BitsToF32(u32 luBits) { f32 lf; std::memcpy(&lf, &luBits, sizeof(lf)); return lf; }
    Vector3 Vec3At(const u32* lpu) { return Vector3{ BitsToF32(lpu[0]), BitsToF32(lpu[1]), BitsToF32(lpu[2]), BitsToF32(lpu[3]) }; }
    Vector4 Vec4At(const u32* lpu) { return Vector4{ BitsToF32(lpu[0]), BitsToF32(lpu[1]), BitsToF32(lpu[2]), BitsToF32(lpu[3]) }; }
    Matrix44Affine MatAt(const u32* lpu)
    {
        Matrix44Affine lm;
        lm.xAxis = Vec3At(lpu); lm.yAxis = Vec3At(lpu + 4); lm.zAxis = Vec3At(lpu + 8); lm.wAxis = Vec3At(lpu + 12);
        return lm;
    }

    VehicleManager gManager;
    BrnPhysics::ContactSpy::RaceCarContact gContact;
    VehicleAttribs gaAttribs[2];

    // The ladder tuning, as VehicleManager::Construct @0x8263B7C8 stores it (see FX-LADDER.md).
    void Tune()
    {
        gManager.mfMinSecondsBetweenImpacts     = 0.3f;    // +0x29E14
        gManager.mfTBoneTakedownMaxAngle        = 35.0f;   // +0x29E2C <- flt_8208FBA4
        gManager.mfTBoneTakedownSpeed           = 30.0f;   // +0x29E30 <- flt_82004F5C
        gManager.mfMinShuntSpeed                = 12.0f;   // +0x29E3C <- flt_8208FA28
        gManager.mfFatalShuntSpeed              = 140.0f;  // +0x29E40 <- flt_8208FA2C
        gManager.mfMinTradingPaintSpeed         = 0.8f;    // +0x29E60 <- flt_8208F9C8
        gManager.mfFatalSlamSpeed               = 140.0f;  // +0x29E64 <- flt_8208FA2C
        gManager.mfMaxHeadToHeadAngle           = 45.0f;   // +0x29E6C <- flt_82009B80
        gManager.mfMinHeadToHeadSpeed           = 40.0f;   // +0x29E70 <- flt_8208FBD0
        gManager.mfMinHeadToHeadIndividualSpeed = 40.0f;   // +0x29E74 <- flt_8208FBD0
    }

    VehicleManager::RaceCarResponseInfo Load(const Record& lrRecord)
    {
        const u32* lpu = lrRecord.mauWord;
        const s32 laIdx[2] = { static_cast<s32>(lpu[0]), static_cast<s32>(lpu[1]) };
        const u32 luFlags = lpu[2];

        gContact = {};
        gContact.mEntityIdA = EntityId{ lpu[46] };
        gContact.mEntityIdB = EntityId{ lpu[47] };
        gContact.mNormal   = Vec3At(lpu + 48);
        gContact.mPointOnA = Vec3At(lpu + 52);
        gContact.mPointOnB = Vec3At(lpu + 56);

        gManager.mePlayerActiveRaceCarIndex = static_cast<EActiveRaceCarIndex>(static_cast<s32>(lpu[60]));
        gManager.mbIsOnlineGameMode = lpu[61] != 0;
        for (s32 liSide = 0; liSide < 2; ++liSide)
        {
            const s32 liCar = laIdx[liSide];
            const u32* lpm = lpu + 62 + 6 * liSide;
            gManager.maeRaceCarTypes[liCar]                    = static_cast<BrnWorld::ERaceCarType>(lpm[0]);
            gManager.mafNoImpactTimeSeconds[liCar]             = BitsToF32(lpm[1]);
            gManager.mafVulnerabilityFactor[liCar]             = BitsToF32(lpm[2]);
            gManager.mau8FramesSincePlayerGrindingOther[liCar] = static_cast<u8>(lpm[3]);
            gManager.mau8FramesSinceOtherGrindingPlayer[liCar] = static_cast<u8>(lpm[4]);
            gManager.maRaceCarDrivers[liCar].mControls.mbBoost = lpm[5] != 0;

            const u32* lpc = lpu + 74 + 40 * liSide;
            RaceCarPhysics& lrCar = gManager.maRaceCarVehicles[liCar];
            lrCar.mbCrashing                     = lpc[0] != 0;
            lrCar.mfSlamSteering                 = BitsToF32(lpc[1]);
            lrCar.mi8LastAttackersRaceCarIndex   = static_cast<s8>(static_cast<s32>(lpc[2]));
            lrCar.mSlamEffect.mfSlamLife         = BitsToF32(lpc[3]);
            lrCar.mShuntEffect.mDirectionPlusDesiredSpeed.w = BitsToF32(lpc[4]);
            lrCar.mShuntEffect.mv4_Life_SpeedIncreaseToQuit.x = BitsToF32(lpc[5]);
            lrCar.mi8LastContactedRaceCar        = static_cast<s8>(static_cast<s32>(lpc[6]));
            lrCar.mvPropSpeedMaintainAlongZ_PropSpeedMaintainAlongVel_TimeSinceLastRaceCarContact_SolvePenetrationWeightFactor.z = BitsToF32(lpc[7]);
            lrCar.mvTimeStandingStill_CoolDown_TimeWithoutTraction_TimeWithTraction.z = BitsToF32(lpc[8]);
            lrCar.mvSpeedOnLastCrashMPH_TimeCrashing_CounterSteerSideMag_Spare.y = BitsToF32(lpc[9]);
            lrCar.mfMass = VecFloat{ BitsToF32(lpc[10]), BitsToF32(lpc[10]), BitsToF32(lpc[10]), BitsToF32(lpc[10]) };
            gaAttribs[liSide].mCollisionAttribs.mvCrashSpeedMPS_CarAngularImpulseScale_Spare_Spare.x = BitsToF32(lpc[11]);
            lrCar.mpAttribs = &gaAttribs[liSide];
            lrCar.mLastLinearVelocity = Vec3At(lpc + 12);
            lrCar.mDeformableAABB.mMin = Vec4At(lpc + 16);
            lrCar.mDeformableAABB.mMax = Vec4At(lpc + 20);
            lrCar.mTransform = MatAt(lpc + 24);
        }

        // HandleRaceCarRaceCarContact's own initialisation of the info (0x826433D0..0x82643584).
        VehicleManager::RaceCarResponseInfo lInfo = {};
        lInfo.mpContact = &gContact;
        lInfo.mRaceCarAEntityID = EntityId{ lpu[3] };
        lInfo.mRaceCarBEntityID = EntityId{ lpu[4] };
        lInfo.meActiveRaceCarIndexA = static_cast<EActiveRaceCarIndex>(laIdx[0]);
        lInfo.meActiveRaceCarIndexB = static_cast<EActiveRaceCarIndex>(laIdx[1]);
        lInfo.mpRaceCarA = &gManager.maRaceCarVehicles[laIdx[0]];
        lInfo.mpRaceCarB = &gManager.maRaceCarVehicles[laIdx[1]];
        lInfo.mClosingVelocityAtoB = Vec3At(lpu + 10);
        lInfo.mbRaceCarAIsCrashing   = (luFlags & 1u) != 0;
        lInfo.mbRaceCarBIsCrashing   = (luFlags & 2u) != 0;
        lInfo.mbRaceCarAIsPlayer     = (luFlags & 4u) != 0;
        lInfo.mbRaceCarBIsPlayer     = (luFlags & 8u) != 0;
        lInfo.mbRaceCarAIsNetworkCar = (luFlags & 16u) != 0;
        lInfo.mbRaceCarBIsNetworkCar = (luFlags & 32u) != 0;
        lInfo.mbOtherCarIsAI         = (luFlags & 64u) != 0;
        lInfo.mfClosingSpeed   = BitsToF32(lpu[5]);
        lInfo.mfRaceCarASpeed  = BitsToF32(lpu[6]);
        lInfo.mfRaceCarBSpeed  = BitsToF32(lpu[7]);
        lInfo.mfNormalStressSq = BitsToF32(lpu[8]);
        lInfo.mfAngleBetweenCars = BitsToF32(lpu[9]);
        lInfo.mRaceCarATransform = MatAt(lpu + 14);
        lInfo.mRaceCarBTransform = MatAt(lpu + 30);
        lInfo.meImpactType = E_IMPACT_NONE;
        lInfo.meAggressorActiveRaceCarIndex = static_cast<EActiveRaceCarIndex>(-1);
        lInfo.meVictimActiveRaceCarIndex    = static_cast<EActiveRaceCarIndex>(-1);
        lInfo.meImpactSitutation            = E_IMPACT_SITUATION_INVALID;
        return lInfo;
    }

    // ---- the pinned evaluations ---------------------------------------------------------------
    // fxladder_w3/20260925_134853 (exe f7751885ef91, sweeteners AI-pad pursuit, PASS 14/14): every
    // classified player-involved contact of the run, the one T-bone (rival 4 T-boned the player),
    // and two to four of each "none" bucket (two of the cooldown records have
    // rear-end shunt geometry, so a dropped ShuntAndNudge cooldown turns them RED). The console decision on each was re-derived by walking the
    // ARTIST gate list over the recorded bits (see FX-LADDER.md) and agrees with the recorded PC
    // decision on all 18.
    // Fields after the words: impact, aggressor, victim, crashA, crashB, takedowns, shunts,
    // takedown victim, takedown aggressor, takedown type.
    const Record kaRecords[] =
    {
        // fxladder_w3 #168, 0 vs 4
        { "fxladder_w3#168 T_BONE", "T-bone: |angle-pi/2| 0.5975 < 0.6109 (0x8263D4EC), lateral 4.90/28.54 > 13.411, B in the slab -> InstantTakedown(victim 0, T_BONE)",
          {
            0x00000000u, 0x00000004u, 0x00000044u, 0x01000000u, 0x01001000u, 0x40D97199u, 0x41F2C542u, 0x42118D30u,
            0x00000000u, 0x3F792873u, 0x403BA938u, 0xBF18A9F0u, 0x40C33A08u, 0x00000000u, 0x3E4C6D0Eu, 0x3B93F619u,
            0x3F7AD81Au, 0x00000000u, 0xBD4ABF9Bu, 0x3F7FAEAFu, 0x3BB3A53Eu, 0x00000000u, 0xBF7A86CCu, 0xBD4B25E4u,
            0x3E4C66B6u, 0x00000000u, 0x44FE9507u, 0xC0922DD0u, 0xC517FE8Au, 0x00000000u, 0xBF325C26u, 0xBC85EECCu,
            0x3F3797B0u, 0x00000000u, 0xBBE56A1Cu, 0x3F7FF605u, 0x3C8301E7u, 0x00000000u, 0xBF37A1A8u, 0x3BC8931Fu,
            0xBF325CB0u, 0x00000000u, 0x44FEC2B1u, 0xC0928A82u, 0xC517D147u, 0x00000000u, 0x01000000u, 0x01001000u,
            0xBE28868Fu, 0xBB27EF62u, 0xBF7C822Au, 0x80000000u, 0x44FEB3D8u, 0xC08A7C4Bu, 0xC517EA41u, 0x00000000u,
            0x44FEB3D7u, 0xC08A7C64u, 0xC517EA44u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u,
            0x3F800000u, 0x00000080u, 0x00000080u, 0x00000000u, 0x00000001u, 0x00000000u, 0x3F800000u, 0x00000080u,
            0x00000080u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000001u, 0xC0991120u, 0x00000000u, 0xBF800000u,
            0x00000004u, 0x3C888889u, 0x00000000u, 0x00000000u, 0x44C6A000u, 0x4217FE5Cu, 0xC1C71B1Au, 0xBF1413CDu,
            0xC18AD3C0u, 0x00000000u, 0xBF8629D7u, 0xBE965E32u, 0xC0189EFFu, 0x00000000u, 0x3F8469A6u, 0x3F7511D2u,
            0x40119D8Au, 0x00000000u, 0x3E4C6D0Eu, 0x3B93F619u, 0x3F7AD81Au, 0x00000000u, 0xBD4ABF9Bu, 0x3F7FAEAFu,
            0x3BB3A53Eu, 0x00000000u, 0xBF7A86CCu, 0xBD4B25E4u, 0x3E4C66B6u, 0x00000000u, 0x44FE9507u, 0xC0922DD0u,
            0xC517FE8Au, 0x00000000u, 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0xBF800000u,
            0x00000000u, 0x3C888889u, 0x00000000u, 0x00000000u, 0x44F22000u, 0x42061CACu, 0xC1DE9041u, 0x3C92C470u,
            0xC1BBA242u, 0x00000000u, 0xBF8A727Au, 0xBE7ECA40u, 0xC03BB80Eu, 0x00000000u, 0x3F8963E7u, 0x3F7782D4u,
            0x400AFB15u, 0x00000000u, 0xBF325C26u, 0xBC85EECCu, 0x3F3797B0u, 0x00000000u, 0xBBE56A1Cu, 0x3F7FF605u,
            0x3C8301E7u, 0x00000000u, 0xBF37A1A8u, 0x3BC8931Fu, 0xBF325CB0u, 0x00000000u, 0x44FEC2B1u, 0xC0928A82u,
            0xC517D147u, 0x00000000u,
          },
          0, -1, -1, 0, 0, 1, 0, 0, 4, 2 },
        // fxladder_w3 #50, 0 vs 3
        { "fxladder_w3#50 classified-boost-shunt", "shunt/nudge: align 1.9138 >= 1.9, aggressor 0 (behind), closing 10.601 > 5.364 (0x8261A554), boost -> 6",
          {
            0x00000000u, 0x00000003u, 0x00000044u, 0x01000000u, 0x01000C00u, 0x41299F7Au, 0x42257EC3u, 0x4226B876u,
            0x00000000u, 0x3EA2A265u, 0x4037D840u, 0xBF242F56u, 0x4122F464u, 0x00000000u, 0x3ECCCCDBu, 0x3CBC8DB0u,
            0x3F6A8DC9u, 0x00000000u, 0xBD071120u, 0x3F7FD89Au, 0xBC2F791Au, 0x00000000u, 0xBF6A79D6u, 0xBCD468B0u,
            0x3ECD10D0u, 0x00000000u, 0x451D6341u, 0xC09C24A9u, 0xC50F91B4u, 0x00000000u, 0x3DC3FD5Eu, 0xBA927DD4u,
            0x3F7ED32Du, 0x00000000u, 0x3B390401u, 0x3F7FFFB7u, 0x3A5F2F02u, 0x00000000u, 0xBF7ED2F5u, 0x3B32D3ABu,
            0x3DC3FECEu, 0x00000000u, 0x451D1254u, 0xC09AB4A0u, 0xC50F7879u, 0x00000000u, 0x01000000u, 0x01000C00u,
            0x3F60497Au, 0xBA9674A7u, 0xBEF6D479u, 0x80000000u, 0x451D48B0u, 0xC0938592u, 0xC50F868Fu, 0x00000000u,
            0x451D486Fu, 0xC0936552u, 0xC50F8676u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u,
            0x3F800000u, 0x00000080u, 0x00000080u, 0x00000001u, 0x00000001u, 0xB1800000u, 0x3F800000u, 0x00000080u,
            0x00000080u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000003u, 0xBFD5554Eu, 0x00000000u, 0xBF800000u,
            0x00000003u, 0x3FACCCC8u, 0x00000000u, 0x00000000u, 0x44C6A000u, 0x4217FE5Cu, 0xC21A1095u, 0xBEF5B123u,
            0x4171A0CAu, 0x00000000u, 0xBF888C3Au, 0xBE965E32u, 0xC0189EFFu, 0x00000000u, 0x3F86949Cu, 0x3F7511D2u,
            0x401E4276u, 0x00000000u, 0x3ECCCCDBu, 0x3CBC8DB0u, 0x3F6A8DC9u, 0x00000000u, 0xBD071120u, 0x3F7FD89Au,
            0xBC2F791Au, 0x00000000u, 0xBF6A79D6u, 0xBCD468B0u, 0x3ECD10D0u, 0x00000000u, 0x451D6341u, 0xC09C24A9u,
            0xC50F91B4u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0xBFBDE320u, 0x00000000u, 0xBF800000u,
            0x00000000u, 0x3C888889u, 0x00000000u, 0x00000000u, 0x44F22000u, 0x42061CACu, 0xC2258E19u, 0x3E255B12u,
            0x409D58CCu, 0x00000000u, 0xBF8AB402u, 0xBE7ECA40u, 0xC03B5B08u, 0x00000000u, 0x3F8567EEu, 0x3F7782D4u,
            0x400BE358u, 0x00000000u, 0x3DC3FD5Eu, 0xBA927DD4u, 0x3F7ED32Du, 0x00000000u, 0x3B390401u, 0x3F7FFFB7u,
            0x3A5F2F02u, 0x00000000u, 0xBF7ED2F5u, 0x3B32D3ABu, 0x3DC3FECEu, 0x00000000u, 0x451D1254u, 0xC09AB4A0u,
            0xC50F7879u, 0x00000000u,
          },
          6, 0, 3, 0, 0, 0, 0, -1, -1, -1 },
        // fxladder_w3 #392, 0 vs 4
        { "fxladder_w3#392 classified-boost-shunt", "shunt/nudge: align 1.9398 >= 1.9, aggressor 0 (behind), closing 5.410 > 5.364 (0x8261A554), boost -> 6",
          {
            0x00000000u, 0x00000004u, 0x00000044u, 0x01000000u, 0x01001000u, 0x40AD2242u, 0x42700493u, 0x426099AFu,
            0x00000000u, 0x3D96D6AEu, 0xC09C8040u, 0x3E2B6114u, 0xC013B418u, 0x00000000u, 0x3E96C110u, 0xBD36D375u,
            0x3F746268u, 0x00000000u, 0x3BFEBA1Fu, 0x3F7FBDA5u, 0x3D35803Du, 0x00000000u, 0xBF74A4AEu, 0xBBB85CC4u,
            0x3E96C777u, 0x00000000u, 0x44BE3625u, 0x3DDEB2ECu, 0xC50B4A04u, 0x00000000u, 0x3EB8DD75u, 0xB860A082u,
            0x3F6EBB00u, 0x00000000u, 0x3C710C67u, 0x3F7FF7DDu, 0xBBB8C747u, 0x00000000u, 0xBF6EB363u, 0x3C8112AFu,
            0x3EB8D7AEu, 0x00000000u, 0x44BDA224u, 0x3E86EA6Fu, 0xC50B2216u, 0x00000000u, 0x01000000u, 0x01001000u,
            0x3F568008u, 0x80000000u, 0xBF0BBC00u, 0x80000000u, 0x44BE15B6u, 0x3EC7A2DAu, 0xC50B3A65u, 0x00000000u,
            0x44BE147Eu, 0x3ECF373Bu, 0xC50B39FCu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0xBBB5DC7Au,
            0x3F800000u, 0x00000080u, 0x00000080u, 0x00000001u, 0x00000001u, 0x00000000u, 0x3F800000u, 0x00000080u,
            0x0000001Eu, 0x00000000u, 0x00000000u, 0x3D17B5F0u, 0xFFFFFFFFu, 0xC026665Fu, 0x00000000u, 0xBF800000u,
            0xFFFFFFFFu, 0x42C80000u, 0x00000000u, 0x00000000u, 0x44C6A000u, 0x4217FE5Cu, 0xC264E9C9u, 0x3F8384B6u,
            0x419016D2u, 0x00000000u, 0xBF888C3Au, 0xBE965E32u, 0xC0189EFFu, 0x00000000u, 0x3F86949Cu, 0x3F7511D2u,
            0x40128D74u, 0x00000000u, 0x3E96C110u, 0xBD36D375u, 0x3F746268u, 0x00000000u, 0x3BFEBA1Fu, 0x3F7FBDA5u,
            0x3D35803Du, 0x00000000u, 0xBF74A4AEu, 0xBBB85CC4u, 0x3E96C777u, 0x00000000u, 0x44BE3625u, 0x3DDEB2ECu,
            0xC50B4A04u, 0x00000000u, 0x00000000u, 0x33C2A88Au, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0xBF800000u,
            0xFFFFFFFFu, 0x42C80000u, 0x00000000u, 0x00000000u, 0x44F22000u, 0x42061CACu, 0xC25159C1u, 0x3F5C3127u,
            0x41A28D55u, 0x00000000u, 0xBF8AA2B8u, 0xBE7ECA40u, 0xC03B9551u, 0x00000000u, 0x3F87BBF3u, 0x3F7782D4u,
            0x400BE358u, 0x00000000u, 0x3EB8DD75u, 0xB860A082u, 0x3F6EBB00u, 0x00000000u, 0x3C710C67u, 0x3F7FF7DDu,
            0xBBB8C747u, 0x00000000u, 0xBF6EB363u, 0x3C8112AFu, 0x3EB8D7AEu, 0x00000000u, 0x44BDA224u, 0x3E86EA6Fu,
            0xC50B2216u, 0x00000000u,
          },
          6, 0, 4, 0, 0, 0, 0, -1, -1, -1 },
        // fxladder_w3 #54, 0 vs 1
        { "fxladder_w3#54 classified-boost-slam", "shunt: align 1.4291 < 1.9 (0x8261A454); slam: dotAt 0.9195 >= 0.75, steer 0.4295 vs threshold 0.1 -> boost-slam",
          {
            0x00000000u, 0x00000001u, 0x00000044u, 0x01000000u, 0x01000400u, 0x418ED1C1u, 0x424898BAu, 0x423928E6u,
            0x00000000u, 0x3ECED1F5u, 0xC002CC10u, 0xBED9E33Au, 0x418DD6EFu, 0x00000000u, 0x3E9D2EE2u, 0xBB617B62u,
            0x3F73A2DDu, 0x00000000u, 0xBCE0FDEDu, 0x3F7FE22Eu, 0x3C4C5BDEu, 0x00000000u, 0xBF73894Cu, 0xBCF57E9Cu,
            0x3E9D1031u, 0x00000000u, 0x4510EC60u, 0xC0AB9545u, 0xC50FF1C1u, 0x00000000u, 0xBDBA13EAu, 0x3A5B7231u,
            0x3F7EF0EAu, 0x00000000u, 0xB98C587Cu, 0x3F7FFFF9u, 0xBA62C27Eu, 0x00000000u, 0xBF7EF0EFu, 0xB9B4F89Bu,
            0xBDBA13C7u, 0x00000000u, 0x4510A0F3u, 0xC0AC575Bu, 0xC50FD40Cu, 0x00000000u, 0x01000000u, 0x01000400u,
            0x3F264D4Au, 0x00000000u, 0xBF42A086u, 0x80000000u, 0x4510D959u, 0xC0A4C0A0u, 0xC50FDE6Eu, 0x00000000u,
            0x4510D797u, 0xC0A3D751u, 0xC50FDC5Du, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u,
            0x3F800000u, 0x00000080u, 0x00000080u, 0x00000001u, 0x00000001u, 0x00000000u, 0x3F800000u, 0x00000080u,
            0x00000080u, 0x00000000u, 0x00000000u, 0xBEDBEC23u, 0x00000003u, 0xC01EEEE8u, 0x00000000u, 0xBF800000u,
            0x00000003u, 0x40BC8670u, 0x00000000u, 0x00000000u, 0x44C6A000u, 0x4217FE5Cu, 0xC240B4CDu, 0xBEE2932Bu,
            0x415EB758u, 0x00000000u, 0xBF888C3Au, 0xBE965E32u, 0xC0189EFFu, 0x00000000u, 0x3F86949Cu, 0x3F7511D2u,
            0x401E4276u, 0x00000000u, 0x3E9D2EE2u, 0xBB617B62u, 0x3F73A2DDu, 0x00000000u, 0xBCE0FDEDu, 0x3F7FE22Eu,
            0x3C4C5BDEu, 0x00000000u, 0xBF73894Cu, 0xBCF57E9Cu, 0x3E9D1031u, 0x00000000u, 0x4510EC60u, 0xC0AB9545u,
            0xC50FF1C1u, 0x00000000u, 0x00000000u, 0x3CFC3E36u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0xBF800000u,
            0xFFFFFFFFu, 0x42C80000u, 0x00000000u, 0x00000000u, 0x44F22000u, 0x42061CACu, 0xC238880Cu, 0xBC8AFF10u,
            0xC073DA19u, 0x00000000u, 0xBF88FFC2u, 0xBE7ECA40u, 0xC03BB80Eu, 0x00000000u, 0x3F89A570u, 0x3F7782D4u,
            0x400BE358u, 0x00000000u, 0xBDBA13EAu, 0x3A5B7231u, 0x3F7EF0EAu, 0x00000000u, 0xB98C587Cu, 0x3F7FFFF9u,
            0xBA62C27Eu, 0x00000000u, 0xBF7EF0EFu, 0xB9B4F89Bu, 0xBDBA13C7u, 0x00000000u, 0x4510A0F3u, 0xC0AC575Bu,
            0xC50FD40Cu, 0x00000000u,
          },
          5, 0, 1, 0, 0, 0, 0, -1, -1, -1 },
        // fxladder_w3 #374, 0 vs 1
        { "fxladder_w3#374 classified-boost-slam", "shunt: align 1.6974 < 1.9 (0x8261A454); slam: dotAt 0.9849 >= 0.75, steer 0.2814 vs threshold 0.1 -> boost-slam",
          {
            0x00000000u, 0x00000001u, 0x00000044u, 0x01000000u, 0x01000400u, 0x40DA0415u, 0x4232AB63u, 0x42289043u,
            0x00000000u, 0x3E3224E6u, 0xC0A04150u, 0xBF1DBB00u, 0xC0927E7Cu, 0x00000000u, 0x3EAC7C23u, 0xBBC3AF88u,
            0x3F7107EAu, 0x00000000u, 0x3C8C5258u, 0x3F7FF663u, 0x395F3C00u, 0x00000000u, 0xBF70FEF1u, 0x3C838772u,
            0x3EAC8310u, 0x00000000u, 0x44E56002u, 0xC091F0F7u, 0xC5140681u, 0x00000000u, 0x3EFC7169u, 0xBB888C1Fu,
            0x3F5EB7A0u, 0x00000000u, 0x3D1ED71Au, 0x3F7FC504u, 0xBC8CD666u, 0x00000000u, 0xBF5E7F9Du, 0x3D2CE8D4u,
            0x3EFC4C6Du, 0x00000000u, 0x44E4BE70u, 0xC08B0255u, 0xC513F7ABu, 0x00000000u, 0x01000000u, 0x01000400u,
            0x3F7E22B7u, 0x80000000u, 0x3DF6B82Au, 0x80000000u, 0x44E52902u, 0xC08849A1u, 0xC51408FFu, 0x00000000u,
            0x44E52661u, 0xC087A68Du, 0xC5140928u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0xBBB5DC7Au,
            0x3F800000u, 0x00000080u, 0x00000080u, 0x00000001u, 0x00000001u, 0xB1800000u, 0x3F800000u, 0x00000080u,
            0x00000080u, 0x00000000u, 0x00000000u, 0x3E900D81u, 0x00000002u, 0xC007264Eu, 0x00000000u, 0xBF800000u,
            0x00000002u, 0x4002221Du, 0x00000000u, 0x00000000u, 0x44C6A000u, 0x4217FE5Cu, 0xC22648D0u, 0x3F8E7094u,
            0x41826BF6u, 0x00000000u, 0xBF888C3Au, 0xBE965E32u, 0xC0189EFFu, 0x00000000u, 0x3F86949Cu, 0x3F7511D2u,
            0x40182200u, 0x00000000u, 0x3EAC7C23u, 0xBBC3AF88u, 0x3F7107EAu, 0x00000000u, 0x3C8C5258u, 0x3F7FF663u,
            0x395F3C00u, 0x00000000u, 0xBF70FEF1u, 0x3C838772u, 0x3EAC8310u, 0x00000000u, 0x44E56002u, 0xC091F0F7u,
            0xC5140681u, 0x00000000u, 0x00000000u, 0x3C6A5E66u, 0x00000000u, 0xBF222223u, 0x42264935u, 0x3F5DDDF0u,
            0x00000000u, 0x3F333333u, 0x00000000u, 0x00000000u, 0x44F22000u, 0x42061CACu, 0xC21240A6u, 0x3FDD4E14u,
            0x41A70B95u, 0x00000000u, 0xBF8AAADEu, 0xBE7ECA40u, 0xC03AE92Eu, 0x00000000u, 0x3F883B18u, 0x3F7782D4u,
            0x400BE358u, 0x00000000u, 0x3EFC7169u, 0xBB888C1Fu, 0x3F5EB7A0u, 0x00000000u, 0x3D1ED71Au, 0x3F7FC504u,
            0xBC8CD666u, 0x00000000u, 0xBF5E7F9Du, 0x3D2CE8D4u, 0x3EFC4C6Du, 0x00000000u, 0x44E4BE70u, 0xC08B0255u,
            0xC513F7ABu, 0x00000000u,
          },
          5, 0, 1, 0, 0, 0, 0, -1, -1, -1 },
        // fxladder_w3 #394, 0 vs 4
        { "fxladder_w3#394 classified-boost-slam", "shunt: align 1.1209 < 1.9 (0x8261A454); slam: dotAt 0.9577 >= 0.75, steer 0.1879 vs threshold 0.1 -> boost-slam",
          {
            0x00000000u, 0x00000004u, 0x00000044u, 0x01000000u, 0x01001000u, 0x416BFFE2u, 0x4273C1C6u, 0x427282F6u,
            0x00000000u, 0x3E956CC2u, 0x40B55030u, 0x3D9EECB4u, 0x4159E396u, 0x00000000u, 0x3F096822u, 0x3DA2F0A4u,
            0x3F570942u, 0x00000000u, 0xBD8AF8C5u, 0x3F7F136Au, 0xBD50F462u, 0x00000000u, 0xBF574C85u, 0xBCF2A040u,
            0x3F0A4AF5u, 0x00000000u, 0x44BABD6Bu, 0x3EC3FE26u, 0xC50A9312u, 0x00000000u, 0x3E8D6BDFu, 0xB7841800u,
            0x3F760A68u, 0x00000000u, 0xBC001A54u, 0x3F7FFDD4u, 0x3B14567Du, 0x00000000u, 0xBF760853u, 0xBC055CC6u,
            0x3E8D6AA8u, 0x00000000u, 0x44BA4793u, 0x3EE21209u, 0xC50A5EC9u, 0x00000000u, 0x01000000u, 0x01001000u,
            0x3E34495Bu, 0x00000000u, 0xBF7C0049u, 0x80000000u, 0x44BAAD0Au, 0x3F2E00F6u, 0xC50A7D38u, 0x00000000u,
            0x44BAACB7u, 0x3F318ECEu, 0xC50A7C54u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0xBBB5DC7Au,
            0x3F800000u, 0x00000080u, 0x00000080u, 0x00000001u, 0x00000001u, 0xB1800000u, 0x40C00000u, 0x00000080u,
            0x0000001Eu, 0x00000000u, 0x00000000u, 0xBE406A7Cu, 0xFFFFFFFFu, 0xC046665Du, 0x00000000u, 0xBF800000u,
            0xFFFFFFFFu, 0x42C80000u, 0x00000000u, 0x00000000u, 0x44C6A000u, 0x4217FE5Cu, 0xC25175EFu, 0xBEA387F8u,
            0x41F955D9u, 0x00000000u, 0xBF888C3Au, 0xBE965E32u, 0xC0189EFFu, 0x00000000u, 0x3F86949Cu, 0x3F7511D2u,
            0x40128D74u, 0x00000000u, 0x3F096822u, 0x3DA2F0A4u, 0x3F570942u, 0x00000000u, 0xBD8AF8C5u, 0x3F7F136Au,
            0xBD50F462u, 0x00000000u, 0xBF574C85u, 0xBCF2A040u, 0x3F0A4AF5u, 0x00000000u, 0x44BABD6Bu, 0x3EC3FE26u,
            0xC50A9312u, 0x00000000u, 0x00000000u, 0x3DE32826u, 0x00000000u, 0xBE99999Au, 0x426ABC19u, 0x3FC00004u,
            0x00000000u, 0x3F000003u, 0x00000000u, 0x00000000u, 0x44F22000u, 0x42061CACu, 0xC2681FF5u, 0xBECB4325u,
            0x418C640Eu, 0x00000000u, 0xBF8AA2B8u, 0xBE7ECA40u, 0xC03B9551u, 0x00000000u, 0x3F87BBF3u, 0x3F7782D4u,
            0x400BE358u, 0x00000000u, 0x3E8D6BDFu, 0xB7841800u, 0x3F760A68u, 0x00000000u, 0xBC001A54u, 0x3F7FFDD4u,
            0x3B14567Du, 0x00000000u, 0xBF760853u, 0xBC055CC6u, 0x3E8D6AA8u, 0x00000000u, 0x44BA4793u, 0x3EE21209u,
            0xC50A5EC9u, 0x00000000u,
          },
          5, 0, 4, 0, 0, 0, 0, -1, -1, -1 },
        // fxladder_w3 #320, 0 vs 1
        { "fxladder_w3#320 classified-nudge", "shunt/nudge: align 1.9895 >= 1.9, aggressor 0 (behind), closing 3.692 <= 5.364 (0x8261A554)",
          {
            0x00000000u, 0x00000001u, 0x00000044u, 0x01000000u, 0x01000400u, 0x406C45DFu, 0x4228BBC5u, 0x42204EF9u,
            0x00000000u, 0x3DD7181Du, 0x3D8DD800u, 0xBD9F93E5u, 0x406C2DC0u, 0x00000000u, 0x3F239A00u, 0x3D14B1DBu,
            0x3F44AED2u, 0x00000000u, 0xBD0C9D63u, 0x3F7FCDF4u, 0xBC98DAE5u, 0x00000000u, 0xBF44B4C4u, 0xBC6CC374u,
            0x3F23CBB1u, 0x00000000u, 0x44EA1E82u, 0xC097C968u, 0xC515D59Cu, 0x00000000u, 0x3F0E77C5u, 0xBC257C1Bu,
            0x3F54ADC0u, 0x00000000u, 0x3BA07719u, 0x3F7FFCA2u, 0x3C116FE1u, 0x00000000u, 0xBF54B0D3u, 0xBA648556u,
            0x3F0E7923u, 0x00000000u, 0x44E9A18Du, 0xC094FD44u, 0xC5159EBFu, 0x00000000u, 0x01000000u, 0x01000400u,
            0x3F3EBAA6u, 0x3C3FEB33u, 0xBF2ABB67u, 0x80000000u, 0x44EA014Bu, 0xC08E24FBu, 0xC515BF8Fu, 0x00000000u,
            0x44EA00E1u, 0xC08E00A8u, 0xC515BF64u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0xBBB5DC7Au,
            0x3F800000u, 0x00000080u, 0x00000080u, 0x00000001u, 0x00000001u, 0xB1800000u, 0x3F800000u, 0x00000080u,
            0x00000080u, 0x00000000u, 0x00000000u, 0xBE75A427u, 0x00000002u, 0xBF7A7725u, 0x00000000u, 0xBF800000u,
            0x00000002u, 0x3F666663u, 0x00000000u, 0x00000000u, 0x44C6A000u, 0x4217FE5Cu, 0xC2050B27u, 0xBD4B8A1Au,
            0x41CF8F88u, 0x00000000u, 0xBF888C3Au, 0xBE965E32u, 0xC0189EFFu, 0x00000000u, 0x3F86949Cu, 0x3F7511D2u,
            0x40182200u, 0x00000000u, 0x3F239A00u, 0x3D14B1DBu, 0x3F44AED2u, 0x00000000u, 0xBD0C9D63u, 0x3F7FCDF4u,
            0xBC98DAE5u, 0x00000000u, 0xBF44B4C4u, 0xBC6CC374u, 0x3F23CBB1u, 0x00000000u, 0x44EA1E82u, 0xC097C968u,
            0xC515D59Cu, 0x00000000u, 0x00000000u, 0xBD10A046u, 0x00000002u, 0xBF80B096u, 0x00000000u, 0xBF800000u,
            0x00000000u, 0x3F2AAAABu, 0x00000000u, 0x00000000u, 0x44F22000u, 0x42061CACu, 0xC2055213u, 0x3CE73B60u,
            0x41B209D0u, 0x00000000u, 0xBF8AAADEu, 0xBE7ECA40u, 0xC03B9D30u, 0x00000000u, 0x3F883B18u, 0x3F7782D4u,
            0x400BE358u, 0x00000000u, 0x3F0E77C5u, 0xBC257C1Bu, 0x3F54ADC0u, 0x00000000u, 0x3BA07719u, 0x3F7FFCA2u,
            0x3C116FE1u, 0x00000000u, 0xBF54B0D3u, 0xBA648556u, 0x3F0E7923u, 0x00000000u, 0x44E9A18Du, 0xC094FD44u,
            0xC5159EBFu, 0x00000000u,
          },
          2, 0, 1, 0, 0, 0, 0, -1, -1, -1 },
        // fxladder_w3 #356, 0 vs 1
        { "fxladder_w3#356 classified-nudge", "shunt/nudge: align 1.9854 >= 1.9, aggressor 0 (behind), closing 0.677 <= 5.364 (0x8261A554)",
          {
            0x00000000u, 0x00000001u, 0x00000044u, 0x01000000u, 0x01000400u, 0x3F2D5007u, 0x4226529Au, 0x4224F4F1u,
            0x00000000u, 0x3CE50EC4u, 0x3E102C00u, 0x3E0AFE59u, 0x3F25EC80u, 0x00000000u, 0x3F3142FCu, 0x3B2C3A49u,
            0x3F38B309u, 0x00000000u, 0xBCAD3219u, 0x3F7FE844u, 0x3C886490u, 0x00000000u, 0xBF389F0Bu, 0xBCDB665Au,
            0x3F313631u, 0x00000000u, 0x44E8FA0Au, 0xC09894EBu, 0xC5155133u, 0x00000000u, 0x3F2D7B6Eu, 0x3B9A7480u,
            0x3F3C404Au, 0x00000000u, 0xBC0F3D23u, 0x3F7FFD68u, 0x3AD7E0CCu, 0x00000000u, 0xBF3C3DDFu, 0xBBF73C7Du,
            0x3F2D7C60u, 0x00000000u, 0x44E8763Eu, 0xC0979FC7u, 0xC5151C17u, 0x00000000u, 0x01000000u, 0x01000400u,
            0x3F2426E1u, 0x3CDC4F51u, 0xBF4452BAu, 0x80000000u, 0x44E8C946u, 0xC0901F25u, 0xC5154447u, 0x00000000u,
            0x44E8C946u, 0xC0901F37u, 0xC5154447u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0xBBB5DC7Au,
            0x3F800000u, 0x00000080u, 0x00000080u, 0x00000001u, 0x00000001u, 0xB1800000u, 0x3F800000u, 0x00000080u,
            0x00000080u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000002u, 0xBFA3A1F7u, 0x00000000u, 0xBF800000u,
            0x00000002u, 0x3F999996u, 0x00000000u, 0x00000000u, 0x44C6A000u, 0x4217FE5Cu, 0xC1F0813Eu, 0xBDF45BC6u,
            0x41E5CD92u, 0x00000000u, 0xBF888C3Au, 0xBE965E32u, 0xC0189EFFu, 0x00000000u, 0x3F86949Cu, 0x3F7511D2u,
            0x40182200u, 0x00000000u, 0x3F3142FCu, 0x3B2C3A49u, 0x3F38B309u, 0x00000000u, 0xBCAD3219u, 0x3F7FE844u,
            0x3C886490u, 0x00000000u, 0xBF389F0Bu, 0xBCDB665Au, 0x3F313631u, 0x00000000u, 0x44E8FA0Au, 0xC09894EBu,
            0xC5155133u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0xBDCCCCCBu, 0x42264935u, 0x3FD9999Cu,
            0x00000000u, 0x3C888889u, 0x00000000u, 0x00000000u, 0x44F22000u, 0x42061CACu, 0xC1F1A196u, 0xBE82961Eu,
            0x41E09E2Eu, 0x00000000u, 0xBF8AAADEu, 0xBE7ECA40u, 0xC03B2105u, 0x00000000u, 0x3F883B18u, 0x3F7782D4u,
            0x400BE358u, 0x00000000u, 0x3F2D7B6Eu, 0x3B9A7480u, 0x3F3C404Au, 0x00000000u, 0xBC0F3D23u, 0x3F7FFD68u,
            0x3AD7E0CCu, 0x00000000u, 0xBF3C3DDFu, 0xBBF73C7Du, 0x3F2D7C60u, 0x00000000u, 0x44E8763Eu, 0xC0979FC7u,
            0xC5151C17u, 0x00000000u,
          },
          2, 0, 1, 0, 0, 0, 0, -1, -1, -1 },
        // fxladder_w3 #0, 0 vs 3
        { "fxladder_w3#0 classified-trading-paint", "shunt: align 1.3184 < 1.9 (0x8261A454); slam: dotAt 0.9799 >= 0.75, steer 0.0761 vs threshold 0.1 -> trading paint",
          {
            0x00000000u, 0x00000003u, 0x00000044u, 0x01000000u, 0x01000C00u, 0x41B8667Au, 0x41FC596Au, 0x42568DACu,
            0x00000000u, 0x3E4DAC0Fu, 0x41607E94u, 0x3EAE0534u, 0x419246A1u, 0x00000000u, 0xBF00E5C5u, 0x3B02A96Fu,
            0x3F5D2E69u, 0x00000000u, 0xBCAE9591u, 0x3F7FEA03u, 0xBC71478Du, 0x00000000u, 0xBF5D1D56u, 0xBCD394EFu,
            0xBF00D7EAu, 0x00000000u, 0x45213B18u, 0xC0AF0929u, 0xC50EE973u, 0x00000000u, 0xBF2A4B7Du, 0xBC0B1863u,
            0x3F3F2174u, 0x00000000u, 0xBC5CD05Eu, 0x3F7FFA0Au, 0xBA274908u, 0x00000000u, 0xBF3F1CA5u, 0xBC2BD0CDu,
            0xBF2A4F05u, 0x00000000u, 0x45215E1Du, 0xC0AD49B4u, 0xC50EABEAu, 0x00000000u, 0x01000000u, 0x01000C00u,
            0xBDC5886Du, 0x80000000u, 0xBF7ECE74u, 0x80000000u, 0x45215871u, 0xC0A527A0u, 0xC50EC519u, 0x00000000u,
            0x452158B8u, 0xC0A551C9u, 0xC50EC244u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u,
            0x3F800000u, 0x00000080u, 0x00000080u, 0x00000000u, 0x00000001u, 0x00000000u, 0x3F800000u, 0x00000080u,
            0x00000080u, 0x00000001u, 0x00000000u, 0xBD9BD7B2u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0xBF800000u,
            0xFFFFFFFFu, 0x42C80000u, 0x00000000u, 0x00000000u, 0x44C6A000u, 0x4217FE5Cu, 0xC1D8D52Au, 0xBE7E00D8u,
            0xC18112A9u, 0x00000000u, 0xBF888C3Au, 0xBE965E32u, 0xC0189EFFu, 0x00000000u, 0x3F86949Cu, 0x3F7511D2u,
            0x401E4276u, 0x00000000u, 0xBF00E5C5u, 0x3B02A96Fu, 0x3F5D2E69u, 0x00000000u, 0xBCAE9591u, 0x3F7FEA03u,
            0xBC71478Du, 0x00000000u, 0xBF5D1D56u, 0xBCD394EFu, 0xBF00D7EAu, 0x00000000u, 0x45213B18u, 0xC0AF0929u,
            0xC50EE973u, 0x00000000u, 0x00000000u, 0x3D38C592u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0xBF800000u,
            0xFFFFFFFFu, 0x42C80000u, 0x00000000u, 0x00000000u, 0x44F22000u, 0x42061CACu, 0xC2248A3Au, 0xBF1682D0u,
            0xC209ACA5u, 0x00000000u, 0xBF8AB402u, 0xBE7ECA40u, 0xC03BB80Eu, 0x00000000u, 0x3F8531CAu, 0x3F7782D4u,
            0x400BE358u, 0x00000000u, 0xBF2A4B7Du, 0xBC0B1863u, 0x3F3F2174u, 0x00000000u, 0xBC5CD05Eu, 0x3F7FFA0Au,
            0xBA274908u, 0x00000000u, 0xBF3F1CA5u, 0xBC2BD0CDu, 0xBF2A4F05u, 0x00000000u, 0x45215E1Du, 0xC0AD49B4u,
            0xC50EABEAu, 0x00000000u,
          },
          1, 0, 3, 0, 0, 0, 0, -1, -1, -1 },
        // fxladder_w3 #244, 0 vs 1
        { "fxladder_w3#244 classified-trading-paint", "shunt: align 0.2120 < 1.9 (0x8261A454); slam: dotAt 0.9770 >= 0.75, steer 0.0183 vs threshold 0.1 -> trading paint",
          {
            0x00000000u, 0x00000001u, 0x00000044u, 0x01000000u, 0x01000400u, 0x415B5B61u, 0x42088891u, 0x4236013Fu,
            0x00000000u, 0x3E5C1334u, 0x41061148u, 0xBD887070u, 0xC12D9D7Du, 0x00000000u, 0x3E53B656u, 0x3C715CAEu,
            0x3F7A70C0u, 0x00000000u, 0xBCD4D6F0u, 0x3F7FE6E3u, 0xBC1CA99Au, 0x00000000u, 0xBF7A6167u, 0xBCC005B6u,
            0x3E5405E4u, 0x00000000u, 0x44EEFD2Bu, 0xC09700CCu, 0xC516E7F6u, 0x00000000u, 0x3ECF04D5u, 0xBDDDF0CAu,
            0x3F687DBEu, 0x00000000u, 0x3D677DE6u, 0x3F7E7800u, 0x3DBF6377u, 0x00000000u, 0xBF69B16Fu, 0x3C5DDB0Au,
            0x3ED0EA99u, 0x00000000u, 0x44EEAF6Bu, 0xC092F3C5u, 0xC5170225u, 0x00000000u, 0x01000000u, 0x01000400u,
            0x3E96EEA2u, 0x3CAF1E48u, 0x3F749010u, 0x80000000u, 0x44EECA95u, 0xC0901655u, 0xC516F2D1u, 0x00000000u,
            0x44EEC963u, 0xC08F503Au, 0xC516F4B8u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0xBBB5DC7Au,
            0x3F800000u, 0x00000080u, 0x00000080u, 0x00000001u, 0x00000001u, 0xB1800000u, 0x3F800000u, 0x00000080u,
            0x00000080u, 0x00000001u, 0x00000000u, 0x3C95F557u, 0xFFFFFFFFu, 0xC08EEEF5u, 0x00000000u, 0xBF800000u,
            0xFFFFFFFFu, 0x42C80000u, 0x00000000u, 0x00000000u, 0x44C6A000u, 0x4217FE5Cu, 0xC2055E86u, 0x3DE1BB78u,
            0x40E9C3B2u, 0x00000000u, 0xBF888C3Au, 0xBE965E32u, 0xC0189EFFu, 0x00000000u, 0x3F86949Cu, 0x3F7511D2u,
            0x40182200u, 0x00000000u, 0x3E53B656u, 0x3C715CAEu, 0x3F7A70C0u, 0x00000000u, 0xBCD4D6F0u, 0x3F7FE6E3u,
            0xBC1CA99Au, 0x00000000u, 0xBF7A6167u, 0xBCC005B6u, 0x3E5405E4u, 0x00000000u, 0x44EEFD2Bu, 0xC09700CCu,
            0xC516E7F6u, 0x00000000u, 0x00000000u, 0x3CE1CF99u, 0x00000002u, 0x3D9FA12Au, 0x00000000u, 0xBF800000u,
            0xFFFFFFFFu, 0x42C80000u, 0x00000000u, 0x00000000u, 0x44F22000u, 0x42061CACu, 0xC226E2D8u, 0x3E3515F4u,
            0x41913FABu, 0x00000000u, 0xBF8A7FC4u, 0xBE7ECA40u, 0xC03BB80Eu, 0x00000000u, 0x3F8963E7u, 0x3F7782D4u,
            0x400BE358u, 0x00000000u, 0x3ECF04D5u, 0xBDDDF0CAu, 0x3F687DBEu, 0x00000000u, 0x3D677DE6u, 0x3F7E7800u,
            0x3DBF6377u, 0x00000000u, 0xBF69B16Fu, 0x3C5DDB0Au, 0x3ED0EA99u, 0x00000000u, 0x44EEAF6Bu, 0xC092F3C5u,
            0xC5170225u, 0x00000000u,
          },
          1, 0, 1, 0, 0, 0, 0, -1, -1, -1 },
        // fxladder_w3 #258, 0 vs 2
        { "fxladder_w3#258 classified-trading-paint", "shunt: align 1.7922 < 1.9 (0x8261A454); slam: dotAt 0.9560 >= 0.75, steer 0.0203 vs threshold 0.3 -> trading paint",
          {
            0x00000000u, 0x00000002u, 0x00000044u, 0x01000000u, 0x01000800u, 0x4117FE83u, 0x420A7B06u, 0x421BBCBBu,
            0x00000000u, 0x3E9860ACu, 0x3EFC6B80u, 0x3F08C2FCu, 0xC1178C6Bu, 0x00000000u, 0x3E7204B6u, 0xBC8BC22Cu,
            0x3F78B57Au, 0x00000000u, 0xBC8FF0BEu, 0x3F7FE644u, 0x3CB2D0F2u, 0x00000000u, 0xBF78B4E3u, 0xBCB61A76u,
            0x3E719DCEu, 0x00000000u, 0x44EEA600u, 0xC096BD55u, 0xC516D89Au, 0x00000000u, 0x3F02DC5Fu, 0xBDE3CBA4u,
            0x3F5A2CFCu, 0x00000000u, 0x3C8C6023u, 0x3F7E2268u, 0x3DF44A5Eu, 0x00000000u, 0xBF5BFB76u, 0xBD3DEED8u,
            0x3F026526u, 0x00000000u, 0x44EF1A69u, 0xC093D9CBu, 0xC51708CEu, 0x00000000u, 0x01000000u, 0x01000800u,
            0xBF2E41BBu, 0x80000000u, 0x3F3B89C8u, 0x80000000u, 0x44EEF878u, 0xC08E8925u, 0xC516F19Eu, 0x00000000u,
            0x44EEF9CFu, 0xC08EC74Fu, 0xC516F256u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0xBBB5DC7Au,
            0x3F800000u, 0x00000080u, 0x00000080u, 0x00000001u, 0x00000001u, 0x00000000u, 0x3F800000u, 0x00000080u,
            0x00000080u, 0x00000001u, 0x00000000u, 0x00000000u, 0x00000001u, 0xBDAAAAABu, 0x00000000u, 0xBF800000u,
            0x00000001u, 0x3C888889u, 0x00000000u, 0x00000000u, 0x44C6A000u, 0x4217FE5Cu, 0xC2043EC9u, 0x3DA8D15Cu,
            0x412456AFu, 0x00000000u, 0xBF888C3Au, 0xBE965E32u, 0xC0189EFFu, 0x00000000u, 0x3F86949Cu, 0x3F7511D2u,
            0x40182200u, 0x00000000u, 0x3E7204B6u, 0xBC8BC22Cu, 0x3F78B57Au, 0x00000000u, 0xBC8FF0BEu, 0x3F7FE644u,
            0x3CB2D0F2u, 0x00000000u, 0xBF78B4E3u, 0xBCB61A76u, 0x3E719DCEu, 0x00000000u, 0x44EEA600u, 0xC096BD55u,
            0xC516D89Au, 0x00000000u, 0x00000000u, 0xBCA5FB1Eu, 0x00000001u, 0xBEAAAAADu, 0x00000000u, 0xBF800000u,
            0xFFFFFFFFu, 0x42C80000u, 0x00000000u, 0x00000000u, 0x44DCA000u, 0x423BC1BDu, 0xC20637A0u, 0xBEE751A1u,
            0x419DF18Du, 0x00000000u, 0xBF887815u, 0xBE9DF3D4u, 0xC01C9643u, 0x00000000u, 0x3F898AECu, 0x3FA5DF88u,
            0x40108B16u, 0x00000000u, 0x3F02DC5Fu, 0xBDE3CBA4u, 0x3F5A2CFCu, 0x00000000u, 0x3C8C6023u, 0x3F7E2268u,
            0x3DF44A5Eu, 0x00000000u, 0xBF5BFB76u, 0xBD3DEED8u, 0x3F026526u, 0x00000000u, 0x44EF1A69u, 0xC093D9CBu,
            0xC51708CEu, 0x00000000u,
          },
          1, 2, 0, 0, 0, 0, 0, -1, -1, -1 },
        // fxladder_w3 #40, 0 vs 3
        { "fxladder_w3#40 slam-noarm", "shunt: align 0.1797 < 1.9 (0x8261A454); slam: steerA -0.0000 steerB 0.0000, no arm positive (0x8261A158/0x8261A248) -> TRUE with NONE (0x8261A348)",
          {
            0x00000000u, 0x00000003u, 0x00000044u, 0x01000000u, 0x01000C00u, 0x41847758u, 0x420CB09Fu, 0x424EDB27u,
            0x00000000u, 0x3E396B5Au, 0x4160BB16u, 0xBEEDE5A9u, 0x410C1B60u, 0x00000000u, 0xBEDA6BAAu, 0x3C9BF23Bu,
            0x3F677C33u, 0x00000000u, 0xB904FE80u, 0x3F7FF166u, 0xBCACE9BFu, 0x00000000u, 0xBF678953u, 0xBC1568CDu,
            0xBEDA5EE2u, 0x00000000u, 0x4520B248u, 0xC0AF1102u, 0xC50F4A88u, 0x00000000u, 0xBF14E250u, 0x3BD1F374u,
            0x3F503F42u, 0x00000000u, 0x3C3E077Au, 0x3F7FFB98u, 0x39DA4329u, 0x00000000u, 0xBF503B7Du, 0x3C1E8C85u,
            0xBF14E49Eu, 0x00000000u, 0x45208B8Au, 0xC0ACF428u, 0xC50F37BAu, 0x00000000u, 0x01000000u, 0x01000C00u,
            0x3EF8D1C6u, 0xBBA032F8u, 0xBF5FBBC7u, 0x80000000u, 0x45209507u, 0xC0A73AB7u, 0xC50F4551u, 0x00000000u,
            0x4520951Au, 0xC0A74269u, 0xC50F4563u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u,
            0x3F800000u, 0x00000080u, 0x00000080u, 0x00000001u, 0x00000001u, 0xB1800000u, 0x3F800000u, 0x00000080u,
            0x00000080u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000003u, 0xBE99999Bu, 0x00000000u, 0xBF800000u,
            0x00000003u, 0x3C888889u, 0x00000000u, 0x00000000u, 0x44C6A000u, 0x4217FE5Cu, 0xC1E769D3u, 0x3D5ED7B5u,
            0xC1A011D3u, 0x00000000u, 0xBF888C3Au, 0xBE965E32u, 0xC0189EFFu, 0x00000000u, 0x3F86949Cu, 0x3F7511D2u,
            0x401E4276u, 0x00000000u, 0xBEDA6BAAu, 0x3C9BF23Bu, 0x3F677C33u, 0x00000000u, 0xB904FE80u, 0x3F7FF166u,
            0xBCACE9BFu, 0x00000000u, 0xBF678953u, 0xBC1568CDu, 0xBEDA5EE2u, 0x00000000u, 0x4520B248u, 0xC0AF1102u,
            0xC50F4A88u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0xBDEF436Du, 0x00000000u, 0xBF800000u,
            0x00000000u, 0x3C888889u, 0x00000000u, 0x00000000u, 0x44F22000u, 0x42061CACu, 0xC22BE3AFu, 0x3F04E050u,
            0xC1E61F83u, 0x00000000u, 0xBF8AB402u, 0xBE7ECA40u, 0xC03BB80Eu, 0x00000000u, 0x3F8566DCu, 0x3F7782D4u,
            0x400BE358u, 0x00000000u, 0xBF14E250u, 0x3BD1F374u, 0x3F503F42u, 0x00000000u, 0x3C3E077Au, 0x3F7FFB98u,
            0x39DA4329u, 0x00000000u, 0xBF503B7Du, 0x3C1E8C85u, 0xBF14E49Eu, 0x00000000u, 0x45208B8Au, 0xC0ACF428u,
            0xC50F37BAu, 0x00000000u,
          },
          0, -1, -1, 0, 0, 0, 0, -1, -1, -1 },
        // fxladder_w3 #46, 0 vs 3
        { "fxladder_w3#46 slam-noarm", "shunt: align 1.8715 < 1.9 (0x8261A454); slam: steerA -0.9716 steerB -0.0361, no arm positive (0x8261A158/0x8261A248) -> TRUE with NONE (0x8261A348)",
          {
            0x00000000u, 0x00000003u, 0x00000044u, 0x01000000u, 0x01000C00u, 0x413C51A1u, 0x422D6302u, 0x4220962Cu,
            0x00000000u, 0x3E9CFB16u, 0xBEE27280u, 0xBF314E3Du, 0x413BDBFBu, 0x00000000u, 0x3EC82D59u, 0x3CCB1E0Eu,
            0x3F6B89A4u, 0x00000000u, 0xBD12A8DFu, 0x3F7FD17Cu, 0xBC3FEDC9u, 0x00000000u, 0xBF6B71DFu, 0xBCE85ACCu,
            0x3EC87D56u, 0x00000000u, 0x451D6D49u, 0xC09BEB47u, 0xC50F95B8u, 0x00000000u, 0x3DC711A5u, 0xBA8C6E69u,
            0x3F7EC9A2u, 0x00000000u, 0x3AECA6DBu, 0x3F7FFFDFu, 0x3A6BF943u, 0x00000000u, 0xBF7EC98Fu, 0x3AE01000u,
            0x3DC7128Eu, 0x00000000u, 0x451D1D92u, 0xC09ACE0Bu, 0xC50F79CBu, 0x00000000u, 0x01000000u, 0x01000C00u,
            0x3F568902u, 0x00000000u, 0xBF0BAE3Au, 0x80000000u, 0x451D5365u, 0xC093928Fu, 0xC50F89B1u, 0x00000000u,
            0x451D52BFu, 0xC0933A4Du, 0xC50F8945u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u,
            0x3F800000u, 0x00000080u, 0x00000080u, 0x00000001u, 0x00000001u, 0xB1800000u, 0x3F800000u, 0x00000080u,
            0x00000080u, 0x00000000u, 0x00000000u, 0xBF78BC2Cu, 0x00000003u, 0xBFD3332Cu, 0x00000000u, 0xBF800000u,
            0x00000003u, 0x3FAAAAA6u, 0x00000000u, 0x00000000u, 0x44C6A000u, 0x4217FE5Cu, 0xC2219041u, 0xBF11C035u,
            0x417B8E42u, 0x00000000u, 0xBF888C3Au, 0xBE965E32u, 0xC0189EFFu, 0x00000000u, 0x3F86949Cu, 0x3F7511D2u,
            0x401E4276u, 0x00000000u, 0x3EC82D59u, 0x3CCB1E0Eu, 0x3F6B89A4u, 0x00000000u, 0xBD12A8DFu, 0x3F7FD17Cu,
            0xBC3FEDC9u, 0x00000000u, 0xBF6B71DFu, 0xBCE85ACCu, 0x3EC87D56u, 0x00000000u, 0x451D6D49u, 0xC09BEB47u,
            0xC50F95B8u, 0x00000000u, 0x00000000u, 0xBD13ABB0u, 0x00000000u, 0xBFBBC0FEu, 0x00000000u, 0xBF800000u,
            0x00000000u, 0x3FAAAAA6u, 0x00000000u, 0x00000000u, 0x44F22000u, 0x42061CACu, 0xC21FCB5Cu, 0x3DFC7040u,
            0x407EC91Cu, 0x00000000u, 0xBF8AB402u, 0xBE7ECA40u, 0xC03B7E15u, 0x00000000u, 0x3F8567EEu, 0x3F7782D4u,
            0x400BE358u, 0x00000000u, 0x3DC711A5u, 0xBA8C6E69u, 0x3F7EC9A2u, 0x00000000u, 0x3AECA6DBu, 0x3F7FFFDFu,
            0x3A6BF943u, 0x00000000u, 0xBF7EC98Fu, 0x3AE01000u, 0x3DC7128Eu, 0x00000000u, 0x451D1D92u, 0xC09ACE0Bu,
            0xC50F79CBu, 0x00000000u,
          },
          0, -1, -1, 0, 0, 0, 0, -1, -1, -1 },
        // fxladder_w3 #1, 3 vs 0
        { "fxladder_w3#1 exhausted-cooldown", "shunt: cooldown A 0.3000 / B 0.0000 (0x8261A3C0/D8); slam: cooldown (0x82619F9C/B4); stationary: offline (+0x2A11B = 0) -> ladder exhausted",
          {
            0x00000003u, 0x00000000u, 0x00000048u, 0x01000C00u, 0x01000000u, 0x41B8667Au, 0x42568DACu, 0x41FC596Au,
            0x00000000u, 0x3E4DAC0Fu, 0xC1607E94u, 0xBEAE0534u, 0xC19246A1u, 0x00000000u, 0xBF2A4B7Du, 0xBC0B1863u,
            0x3F3F2174u, 0x00000000u, 0xBC5CD05Eu, 0x3F7FFA0Au, 0xBA274908u, 0x00000000u, 0xBF3F1CA5u, 0xBC2BD0CDu,
            0xBF2A4F05u, 0x00000000u, 0x45215E1Du, 0xC0AD49B4u, 0xC50EABEAu, 0x00000000u, 0xBF00E5C5u, 0x3B02A96Fu,
            0x3F5D2E69u, 0x00000000u, 0xBCAE9591u, 0x3F7FEA03u, 0xBC71478Du, 0x00000000u, 0xBF5D1D56u, 0xBCD394EFu,
            0xBF00D7EAu, 0x00000000u, 0x45213B18u, 0xC0AF0929u, 0xC50EE973u, 0x00000000u, 0x01000C00u, 0x01000000u,
            0x3DC5886Du, 0x00000000u, 0x3F7ECE74u, 0x00000000u, 0x452158B8u, 0xC0A551C9u, 0xC50EC244u, 0x00000000u,
            0x45215871u, 0xC0A527A0u, 0xC50EC519u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000001u, 0x3E99999Au,
            0x3F800000u, 0x00000080u, 0x00000080u, 0x00000001u, 0x00000000u, 0x00000000u, 0x3F800000u, 0x00000080u,
            0x00000080u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x3E3B917Cu, 0x00000000u, 0xBF800000u,
            0xFFFFFFFFu, 0x42C80000u, 0x00000000u, 0x00000000u, 0x44F22000u, 0x42061CACu, 0xC2248A3Au, 0xBF1682D0u,
            0xC209ACA5u, 0x00000000u, 0xBF8AB402u, 0xBE7ECA40u, 0xC03BB80Eu, 0x00000000u, 0x3F8531CAu, 0x3F7782D4u,
            0x400BE358u, 0x00000000u, 0xBF2A4B7Du, 0xBC0B1863u, 0x3F3F2174u, 0x00000000u, 0xBC5CD05Eu, 0x3F7FFA0Au,
            0xBA274908u, 0x00000000u, 0xBF3F1CA5u, 0xBC2BD0CDu, 0xBF2A4F05u, 0x00000000u, 0x45215E1Du, 0xC0AD49B4u,
            0xC50EABEAu, 0x00000000u, 0x00000000u, 0xBD9BD7B2u, 0x00000003u, 0x00000000u, 0x00000000u, 0xBF800000u,
            0xFFFFFFFFu, 0x42C80000u, 0x00000000u, 0x00000000u, 0x44C6A000u, 0x4217FE5Cu, 0xC1D8D52Au, 0xBE7E00D8u,
            0xC18112A9u, 0x00000000u, 0xBF888C3Au, 0xBE965E32u, 0xC0189EFFu, 0x00000000u, 0x3F86949Cu, 0x3F7511D2u,
            0x401E4276u, 0x00000000u, 0xBF00E5C5u, 0x3B02A96Fu, 0x3F5D2E69u, 0x00000000u, 0xBCAE9591u, 0x3F7FEA03u,
            0xBC71478Du, 0x00000000u, 0xBF5D1D56u, 0xBCD394EFu, 0xBF00D7EAu, 0x00000000u, 0x45213B18u, 0xC0AF0929u,
            0xC50EE973u, 0x00000000u,
          },
          0, -1, -1, 0, 0, 0, 0, -1, -1, -1 },
        // fxladder_w3 #2, 0 vs 3
        { "fxladder_w3#2 exhausted-cooldown", "shunt: cooldown A 0.0000 / B 0.2833 (0x8261A3C0/D8); slam: cooldown (0x82619F9C/B4); stationary: offline (+0x2A11B = 0) -> ladder exhausted",
          {
            0x00000000u, 0x00000003u, 0x00000044u, 0x01000000u, 0x01000C00u, 0x419575DBu, 0x4205C763u, 0x424F5D45u,
            0x00000000u, 0x3E4BB92Fu, 0x414A2FAEu, 0x3E80CBC5u, 0x415C21D0u, 0x00000000u, 0xBEFFD7E7u, 0x3AB302A8u,
            0x3F5DBF58u, 0x00000000u, 0xBCA9FC6Cu, 0x3F7FEBE0u, 0xBC5DF2A8u, 0x00000000u, 0xBF5DAF1Fu, 0xBCCAB1F0u,
            0xBEFFC013u, 0x00000000u, 0x452133DEu, 0xC0AF3640u, 0xC50EF004u, 0x00000000u, 0xBF29181Du, 0xBC027837u,
            0x3F4031F8u, 0x00000000u, 0xBC4D3AFFu, 0x3F7FFADCu, 0xB9D96482u, 0x00000000u, 0xBF402DE4u, 0xBC1E910Cu,
            0xBF291B40u, 0x00000000u, 0x45215334u, 0xC0AD8496u, 0xC50EB37Au, 0x00000000u, 0x01000000u, 0x01000C00u,
            0x3DC5E66Fu, 0x80000000u, 0xBF7ECD51u, 0x80000000u, 0x45214FDBu, 0xC0A578D0u, 0xC50ECB0Au, 0x00000000u,
            0x45214FD5u, 0xC0A57CF9u, 0xC50ECACAu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u,
            0x3F800000u, 0x00000080u, 0x00000080u, 0x00000000u, 0x00000001u, 0x3E911111u, 0x3F800000u, 0x00000080u,
            0x00000080u, 0x00000001u, 0x00000000u, 0x00000000u, 0x00000003u, 0xBC888889u, 0x00000000u, 0xBF800000u,
            0xFFFFFFFFu, 0x42C80000u, 0x00000000u, 0x00000000u, 0x44C6A000u, 0x4217FE5Cu, 0xC1DB6875u, 0xBE8A2CC7u,
            0xC1991C16u, 0x00000000u, 0xBF888C3Au, 0xBE965E32u, 0xC0189EFFu, 0x00000000u, 0x3F86949Cu, 0x3F7511D2u,
            0x401E4276u, 0x00000000u, 0xBEFFD7E7u, 0x3AB302A8u, 0x3F5DBF58u, 0x00000000u, 0xBCA9FC6Cu, 0x3F7FEBE0u,
            0xBC5DF2A8u, 0x00000000u, 0xBF5DAF1Fu, 0xBCCAB1F0u, 0xBEFFC013u, 0x00000000u, 0x452133DEu, 0xC0AF3640u,
            0xC50EF004u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x3E2A806Bu, 0x00000000u, 0xBF800000u,
            0xFFFFFFFFu, 0x42C80000u, 0x00000000u, 0x00000000u, 0x44F22000u, 0x42061CACu, 0xC2204026u, 0xBF057C46u,
            0xC203967Fu, 0x00000000u, 0xBF8AB402u, 0xBE7ECA40u, 0xC03BB80Eu, 0x00000000u, 0x3F8531CAu, 0x3F7782D4u,
            0x400BE358u, 0x00000000u, 0xBF29181Du, 0xBC027837u, 0x3F4031F8u, 0x00000000u, 0xBC4D3AFFu, 0x3F7FFADCu,
            0xB9D96482u, 0x00000000u, 0xBF402DE4u, 0xBC1E910Cu, 0xBF291B40u, 0x00000000u, 0x45215334u, 0xC0AD8496u,
            0xC50EB37Au, 0x00000000u,
          },
          0, -1, -1, 0, 0, 0, 0, -1, -1, -1 },
        // fxladder_w3 #52, 3 vs 0
        { "fxladder_w3#52 exhausted-cooldown", "shunt: cooldown A 0.3000 / B 0.0000 (0x8261A3C0/D8); slam: cooldown (0x82619F9C/B4); stationary: offline (+0x2A11B = 0) -> ladder exhausted",
          {
            0x00000003u, 0x00000000u, 0x00000048u, 0x01000C00u, 0x01000000u, 0x41299F7Au, 0x4226B876u, 0x42257EC3u,
            0x00000000u, 0x3EA2A265u, 0xC037D840u, 0x3F242F56u, 0xC122F464u, 0x00000000u, 0x3DC3FD5Eu, 0xBA927DD4u,
            0x3F7ED32Du, 0x00000000u, 0x3B390401u, 0x3F7FFFB7u, 0x3A5F2F02u, 0x00000000u, 0xBF7ED2F5u, 0x3B32D3ABu,
            0x3DC3FECEu, 0x00000000u, 0x451D1254u, 0xC09AB4A0u, 0xC50F7879u, 0x00000000u, 0x3ECCCCDBu, 0x3CBC8DB0u,
            0x3F6A8DC9u, 0x00000000u, 0xBD071120u, 0x3F7FD89Au, 0xBC2F791Au, 0x00000000u, 0xBF6A79D6u, 0xBCD468B0u,
            0x3ECD10D0u, 0x00000000u, 0x451D6341u, 0xC09C24A9u, 0xC50F91B4u, 0x00000000u, 0x01000C00u, 0x01000000u,
            0xBF60497Au, 0x3A9674A7u, 0x3EF6D479u, 0x00000000u, 0x451D486Fu, 0xC0936552u, 0xC50F8676u, 0x00000000u,
            0x451D48B0u, 0xC0938592u, 0xC50F868Fu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000001u, 0x3E99999Au,
            0x3F800000u, 0x00000080u, 0x00000080u, 0x00000000u, 0x00000000u, 0x00000000u, 0x3F800000u, 0x00000080u,
            0x00000080u, 0x00000001u, 0x00000000u, 0x00000000u, 0x00000000u, 0x3E4CCCCDu, 0x4223BF62u, 0x40000000u,
            0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x44F22000u, 0x42061CACu, 0xC2258E19u, 0x3E255B12u,
            0x409D58CCu, 0x00000000u, 0xBF8AB402u, 0xBE7ECA40u, 0xC03B5B08u, 0x00000000u, 0x3F8567EEu, 0x3F7782D4u,
            0x400BE358u, 0x00000000u, 0x3DC3FD5Eu, 0xBA927DD4u, 0x3F7ED32Du, 0x00000000u, 0x3B390401u, 0x3F7FFFB7u,
            0x3A5F2F02u, 0x00000000u, 0xBF7ED2F5u, 0x3B32D3ABu, 0x3DC3FECEu, 0x00000000u, 0x451D1254u, 0xC09AB4A0u,
            0xC50F7879u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000003u, 0xBFD5554Eu, 0x00000000u, 0xBF800000u,
            0x00000003u, 0x3FACCCC8u, 0x00000000u, 0x00000000u, 0x44C6A000u, 0x4217FE5Cu, 0xC21A1095u, 0xBEF5B123u,
            0x4171A0CAu, 0x00000000u, 0xBF888C3Au, 0xBE965E32u, 0xC0189EFFu, 0x00000000u, 0x3F86949Cu, 0x3F7511D2u,
            0x401E4276u, 0x00000000u, 0x3ECCCCDBu, 0x3CBC8DB0u, 0x3F6A8DC9u, 0x00000000u, 0xBD071120u, 0x3F7FD89Au,
            0xBC2F791Au, 0x00000000u, 0xBF6A79D6u, 0xBCD468B0u, 0x3ECD10D0u, 0x00000000u, 0x451D6341u, 0xC09C24A9u,
            0xC50F91B4u, 0x00000000u,
          },
          0, -1, -1, 0, 0, 0, 0, -1, -1, -1 },
        // fxladder_w3 #68, 0 vs 1
        { "fxladder_w3#68 exhausted-cooldown", "shunt: cooldown A 0.0000 / B 0.2000 (0x8261A3C0/D8); slam: cooldown (0x82619F9C/B4); stationary: offline (+0x2A11B = 0) -> ladder exhausted",
          {
            0x00000000u, 0x00000001u, 0x00000044u, 0x01000000u, 0x01000400u, 0x4175D76Eu, 0x422F89EDu, 0x423AFDAEu,
            0x00000000u, 0x3EC2EF47u, 0x4090ACD0u, 0xBF169A8Cu, 0x416AC500u, 0x00000000u, 0x3E9C37D7u, 0x3ABD9D2Bu,
            0x3F73CAE8u, 0x00000000u, 0xBCADB89Fu, 0x3F7FF05Bu, 0x3BACDEB0u, 0x00000000u, 0xBF73BB82u, 0xBCB29F9Du,
            0x3E9C3250u, 0x00000000u, 0x4510A8DDu, 0xC0AD6A2Cu, 0xC50FDF90u, 0x00000000u, 0xBD8F64D9u, 0x3A6BD6F6u,
            0x3F7F5F23u, 0x00000000u, 0xB9C512A4u, 0x3F7FFFF9u, 0xBA735608u, 0x00000000u, 0xBF7F5F29u, 0xB9E6AA07u,
            0xBD8F64A7u, 0x00000000u, 0x4510541Du, 0xC0AC936Bu, 0xC50FD6BBu, 0x00000000u, 0x01000000u, 0x01000400u,
            0x3F6EB965u, 0x3755DCE0u, 0xBEB8E5BEu, 0x80000000u, 0x45108EBEu, 0xC0A4B9E1u, 0xC50FD60Du, 0x00000000u,
            0x45108E9Eu, 0xC0A4AAD4u, 0xC50FD5FDu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u,
            0x3F800000u, 0x00000080u, 0x00000080u, 0x00000001u, 0x00000001u, 0x3E4CCCCCu, 0x40C00000u, 0x00000080u,
            0x00000080u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000001u, 0xBDCCCCCDu, 0x00000000u, 0xBF800000u,
            0x00000003u, 0x40BFB9A6u, 0x00000000u, 0x00000000u, 0x44C6A000u, 0x4217FE5Cu, 0xC228A32Bu, 0xBF27CF3Eu,
            0x4142AA73u, 0x00000000u, 0xBF888C3Au, 0xBE965E32u, 0xC0189EFFu, 0x00000000u, 0x3F86949Cu, 0x3F7511D2u,
            0x401E4276u, 0x00000000u, 0x3E9C37D7u, 0x3ABD9D2Bu, 0x3F73CAE8u, 0x00000000u, 0xBCADB89Fu, 0x3F7FF05Bu,
            0x3BACDEB0u, 0x00000000u, 0xBF73BB82u, 0xBCB29F9Du, 0x3E9C3250u, 0x00000000u, 0x4510A8DDu, 0xC0AD6A2Cu,
            0xC50FDF90u, 0x00000000u, 0x00000000u, 0x3B9568ACu, 0x00000000u, 0x3F00E658u, 0x00000000u, 0xBF800000u,
            0x00000000u, 0x3D088889u, 0x00000000u, 0x00000000u, 0x44F22000u, 0x42061CACu, 0xC23AB8C5u, 0xBD89A58Cu,
            0xC0206A34u, 0x00000000u, 0xBF88FFC2u, 0xBE7ECA40u, 0xC03AFC11u, 0x00000000u, 0x3F89A570u, 0x3F7782D4u,
            0x400BE358u, 0x00000000u, 0xBD8F64D9u, 0x3A6BD6F6u, 0x3F7F5F23u, 0x00000000u, 0xB9C512A4u, 0x3F7FFFF9u,
            0xBA735608u, 0x00000000u, 0xBF7F5F29u, 0xB9E6AA07u, 0xBD8F64A7u, 0x00000000u, 0x4510541Du, 0xC0AC936Bu,
            0xC50FD6BBu, 0x00000000u,
          },
          0, -1, -1, 0, 0, 0, 0, -1, -1, -1 },
        // fxladder_w3 #76, 0 vs 4
        { "fxladder_w3#76 exhausted-geometry", "shunt: align 0.7630 < 1.9 (0x8261A454); slam: dot(At_A,At_B) 0.6533 < 0.75 (0x82619FDC); stationary offline -> exhausted",
          {
            0x00000000u, 0x00000004u, 0x00000044u, 0x01000000u, 0x01001000u, 0x42224A2Bu, 0x42320E36u, 0x425A884Au,
            0x00000000u, 0x3F5BDBEAu, 0x405AC5D0u, 0xBE5D3ABDu, 0x4221B5DCu, 0x00000000u, 0x3E8BEE20u, 0xBAC20ABAu,
            0x3F7640E1u, 0x00000000u, 0x3C7DF233u, 0x3F7FF7DCu, 0xBB3BC120u, 0x00000000u, 0xBF7638C4u, 0x3C808D94u,
            0x3E8BECAEu, 0x00000000u, 0x451042A1u, 0xC0AEB1E0u, 0xC50FC57Fu, 0x00000000u, 0xBF0CB206u, 0xB62CBBA0u,
            0x3F55DEF9u, 0x00000000u, 0x3918AC9Du, 0x3F800000u, 0x38CF55D5u, 0x00000000u, 0xBF55DEF9u, 0x39388628u,
            0xBF0CB206u, 0x00000000u, 0x45107976u, 0xC0AC6C9Eu, 0xC50FA215u, 0x00000000u, 0x01000000u, 0x01001000u,
            0xBE834E0Du, 0xBBA40D91u, 0xBF776F62u, 0x80000000u, 0x4510702Au, 0xC0A5FE4Bu, 0xC50FBD1Fu, 0x00000000u,
            0x451070F0u, 0xC0A5775Eu, 0xC50FB967u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u,
            0x3F800000u, 0x00000080u, 0x00000080u, 0x00000001u, 0x00000001u, 0x00000000u, 0x3F800000u, 0x00000080u,
            0x00000080u, 0x00000001u, 0x00000000u, 0x3D2F651Bu, 0x00000001u, 0xBE800000u, 0x00000000u, 0xBF800000u,
            0x00000003u, 0x40C48677u, 0x00000000u, 0x00000000u, 0x44C6A000u, 0x4217FE5Cu, 0xC22BEB32u, 0xBE7DF3C8u,
            0x413954CFu, 0x00000000u, 0xBF888C3Au, 0xBE965E32u, 0xC0189EFFu, 0x00000000u, 0x3F86949Cu, 0x3F7511D2u,
            0x401E4276u, 0x00000000u, 0x3E8BEE20u, 0xBAC20ABAu, 0x3F7640E1u, 0x00000000u, 0x3C7DF233u, 0x3F7FF7DCu,
            0xBB3BC120u, 0x00000000u, 0xBF7638C4u, 0x3C808D94u, 0x3E8BECAEu, 0x00000000u, 0x451042A1u, 0xC0AEB1E0u,
            0xC50FC57Fu, 0x00000000u, 0x00000000u, 0x3E1CC15Au, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0xBF800000u,
            0xFFFFFFFFu, 0x42C80000u, 0x00000000u, 0x00000000u, 0x44F22000u, 0x42061CACu, 0xC239978Fu, 0xBD02E42Cu,
            0xC1E6C151u, 0x00000000u, 0xBF8A727Au, 0xBE7ECA40u, 0xC03BB80Eu, 0x00000000u, 0x3F8963E7u, 0x3F7782D4u,
            0x400ABFD7u, 0x00000000u, 0xBF0CB206u, 0xB62CBBA0u, 0x3F55DEF9u, 0x00000000u, 0x3918AC9Du, 0x3F800000u,
            0x38CF55D5u, 0x00000000u, 0xBF55DEF9u, 0x39388628u, 0xBF0CB206u, 0x00000000u, 0x45107976u, 0xC0AC6C9Eu,
            0xC50FA215u, 0x00000000u,
          },
          0, -1, -1, 0, 0, 0, 0, -1, -1, -1 },
        // fxladder_w3 #169, 4 vs 0
        { "fxladder_w3#169 already-crashing", "already crashing (0x82642EC0..D4): crashA=0 crashB=1, not an obstacle (+0xEF0.y 0.000/0.000 <= 1.0 at 0x8263DB38)",
          {
            0x00000004u, 0x00000000u, 0x0000004Au, 0x01001000u, 0x01000000u, 0x40D97199u, 0x42118D30u, 0x41F2C542u,
            0x00000000u, 0x3F792873u, 0xC03BA938u, 0x3F18A9F0u, 0xC0C33A08u, 0x00000000u, 0xBF325C26u, 0xBC85EECCu,
            0x3F3797B0u, 0x00000000u, 0xBBE56A1Cu, 0x3F7FF605u, 0x3C8301E7u, 0x00000000u, 0xBF37A1A8u, 0x3BC8931Fu,
            0xBF325CB0u, 0x00000000u, 0x44FEC2B1u, 0xC0928A82u, 0xC517D147u, 0x00000000u, 0x3E4C6D0Eu, 0x3B93F619u,
            0x3F7AD81Au, 0x00000000u, 0xBD4ABF9Bu, 0x3F7FAEAFu, 0x3BB3A53Eu, 0x00000000u, 0xBF7A86CCu, 0xBD4B25E4u,
            0x3E4C66B6u, 0x00000000u, 0x44FE9507u, 0xC0922DD0u, 0xC517FE8Au, 0x00000000u, 0x01001000u, 0x01000000u,
            0x3E28868Fu, 0x3B27EF62u, 0x3F7C822Au, 0x00000000u, 0x44FEB3D7u, 0xC08A7C64u, 0xC517EA44u, 0x00000000u,
            0x44FEB3D8u, 0xC08A7C4Bu, 0xC517EA41u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000001u, 0x00000000u,
            0x3F800000u, 0x00000080u, 0x00000080u, 0x00000000u, 0x00000000u, 0x3E99999Au, 0x3F800000u, 0x00000080u,
            0x00000080u, 0x00000000u, 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0xBF800000u,
            0x00000000u, 0x3C888889u, 0x00000000u, 0x00000000u, 0x44F22000u, 0x42061CACu, 0xC1DE9041u, 0x3C92C470u,
            0xC1BBA242u, 0x00000000u, 0xBF8A727Au, 0xBE7ECA40u, 0xC03BB80Eu, 0x00000000u, 0x3F8963E7u, 0x3F7782D4u,
            0x400AFB15u, 0x00000000u, 0xBF325C26u, 0xBC85EECCu, 0x3F3797B0u, 0x00000000u, 0xBBE56A1Cu, 0x3F7FF605u,
            0x3C8301E7u, 0x00000000u, 0xBF37A1A8u, 0x3BC8931Fu, 0xBF325CB0u, 0x00000000u, 0x44FEC2B1u, 0xC0928A82u,
            0xC517D147u, 0x00000000u, 0x00000001u, 0x00000000u, 0x00000001u, 0xC0991120u, 0x00000000u, 0xBF800000u,
            0x00000004u, 0x3C888889u, 0x00000000u, 0x00000000u, 0x44C6A000u, 0x4217FE5Cu, 0xC1C71B1Au, 0xBF1413CDu,
            0xC18AD3C0u, 0x00000000u, 0xBF888B77u, 0xBF327F4Eu, 0xC01DD30Eu, 0x00000000u, 0x3F86F440u, 0x3F0DC19Eu,
            0x40190E67u, 0x00000000u, 0x3E4C6D0Eu, 0x3B93F619u, 0x3F7AD81Au, 0x00000000u, 0xBD4ABF9Bu, 0x3F7FAEAFu,
            0x3BB3A53Eu, 0x00000000u, 0xBF7A86CCu, 0xBD4B25E4u, 0x3E4C66B6u, 0x00000000u, 0x44FE9507u, 0xC0922DD0u,
            0xC517FE8Au, 0x00000000u,
          },
          0, -1, -1, 0, 0, 0, 0, -1, -1, -1 },
        // fxladder_w3 #170, 0 vs 4
        { "fxladder_w3#170 already-crashing", "already crashing (0x82642EC0..D4): crashA=1 crashB=0, not an obstacle (+0xEF0.y 0.049/0.000 <= 1.0 at 0x8263DB38)",
          {
            0x00000000u, 0x00000004u, 0x00000045u, 0x01000000u, 0x01001000u, 0x40967C6Eu, 0x421614ACu, 0x4211CA9Cu,
            0x00000000u, 0x3F89CC99u, 0xC06A8690u, 0xBF1D299Eu, 0x40387E90u, 0x00000000u, 0x3EA3910Cu, 0x3DC5CDEDu,
            0x3F715258u, 0x00000000u, 0xBD7811D9u, 0x3F7EADC9u, 0xBDA6B7C2u, 0x00000000u, 0xBF7216CDu, 0xBCFEA646u,
            0x3EA5B7A9u, 0x00000000u, 0x44FE64C1u, 0xC08FD399u, 0xC5180FA1u, 0x00000000u, 0xBF2DBCDCu, 0xBD08943Fu,
            0x3F3BD346u, 0x00000000u, 0xBC542AAAu, 0x3F7FD5DAu, 0x3D08F84Fu, 0x00000000u, 0xBF3BFD6Cu, 0x3C582924u,
            0xBF2DBC8Eu, 0x00000000u, 0x44FE9859u, 0xC0927264u, 0xC517E458u, 0x00000000u, 0x01000000u, 0x01001000u,
            0xBE8B8850u, 0xBD5D8FF8u, 0xBF75EBABu, 0x80000000u, 0x44FE8843u, 0xC088F677u, 0xC517FC98u, 0x00000000u,
            0x44FE8842u, 0xC088F643u, 0xC517FC99u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x3E807C86u,
            0x3F800000u, 0x00000080u, 0x00000080u, 0x00000000u, 0x00000001u, 0x00000000u, 0x3F800000u, 0x00000080u,
            0x00000000u, 0x00000000u, 0x00000001u, 0x00000000u, 0x00000001u, 0xC0991120u, 0x00000000u, 0xBF800000u,
            0x00000004u, 0x3D869676u, 0x00000000u, 0x3D48E8A8u, 0x44C6A000u, 0x4217FE5Cu, 0xC1F5A0A9u, 0xBF11F3F7u,
            0xC1AC7653u, 0x00000000u, 0xBF86248Du, 0xBE965E32u, 0xC0189EFFu, 0x00000000u, 0x3F83F060u, 0x3F7511D2u,
            0x40119959u, 0x00000000u, 0x3EA3910Cu, 0x3DC5CDEDu, 0x3F715258u, 0x00000000u, 0xBD7811D9u, 0x3F7EADC9u,
            0xBDA6B7C2u, 0x00000000u, 0xBF7216CDu, 0xBCFEA646u, 0x3EA5B7A9u, 0x00000000u, 0x44FE64C1u, 0xC08FD399u,
            0xC5180FA1u, 0x00000000u, 0x00000000u, 0xBC7E686Eu, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0xBF800000u,
            0x00000000u, 0x3D869676u, 0x00000000u, 0x00000000u, 0x44F22000u, 0x42061CACu, 0xC1D84FD7u, 0x3D335A68u,
            0xC1C38625u, 0x00000000u, 0xBF8A727Au, 0xBE7ECA40u, 0xC03BB80Eu, 0x00000000u, 0x3F8963E7u, 0x3F7782D4u,
            0x400AFB15u, 0x00000000u, 0xBF2DBCDCu, 0xBD08943Fu, 0x3F3BD346u, 0x00000000u, 0xBC542AAAu, 0x3F7FD5DAu,
            0x3D08F84Fu, 0x00000000u, 0xBF3BFD6Cu, 0x3C582924u, 0xBF2DBC8Eu, 0x00000000u, 0x44FE9859u, 0xC0927264u,
            0xC517E458u, 0x00000000u,
          },
          0, -1, -1, 0, 0, 0, 0, -1, -1, -1 },
    };
}

int main()
{
    Tune();
    for (const Record& lrRecord : kaRecords)
    {
        giTakedowns = 0; giShunts = 0; giLastTakedownType = -2;
        gLastVictim = EntityId{ 0xFFFFFFFFu }; gLastAggressor = EntityId{ 0xFFFFFFFFu };
        VehicleManager::RaceCarResponseInfo lInfo = Load(lrRecord);
        gManager.CheckForAllTypesOfImpacts(&lInfo);
        Check(static_cast<s32>(lInfo.meImpactType) == lrRecord.miImpact, lrRecord.mpcLabel, "impact type");
        Check(static_cast<s32>(lInfo.meAggressorActiveRaceCarIndex) == lrRecord.miAggressor, lrRecord.mpcLabel, "aggressor");
        Check(static_cast<s32>(lInfo.meVictimActiveRaceCarIndex) == lrRecord.miVictim, lrRecord.mpcLabel, "victim");
        Check((lInfo.mbCrashRaceCarA ? 1 : 0) == lrRecord.miCrashA, lrRecord.mpcLabel, "crash flag A");
        Check((lInfo.mbCrashRaceCarB ? 1 : 0) == lrRecord.miCrashB, lrRecord.mpcLabel, "crash flag B");
        Check(giTakedowns == lrRecord.miTakedowns, lrRecord.mpcLabel, "InstantTakedown calls");
        Check(giShunts == lrRecord.miShunts, lrRecord.mpcLabel, "ApplyShunt calls");
        if (lrRecord.miTakedowns > 0)
        {
            Check(static_cast<s32>((gLastVictim.muValue >> 10) & 0x3FFFu) == lrRecord.miTakedownVictim,
                  lrRecord.mpcLabel, "takedown victim");
            Check(static_cast<s32>((gLastAggressor.muValue >> 10) & 0x3FFFu) == lrRecord.miTakedownAggressor,
                  lrRecord.mpcLabel, "takedown aggressor");
            Check(giLastTakedownType == lrRecord.miTakedownType, lrRecord.mpcLabel, "takedown type");
        }
        std::printf("%-44s impact=%d aggr=%d victim=%d crash=%d%d takedowns=%d shunts=%d | %s\n", lrRecord.mpcLabel,
                    static_cast<s32>(lInfo.meImpactType), static_cast<s32>(lInfo.meAggressorActiveRaceCarIndex),
                    static_cast<s32>(lInfo.meVictimActiveRaceCarIndex), lInfo.mbCrashRaceCarA ? 1 : 0,
                    lInfo.mbCrashRaceCarB ? 1 : 0, giTakedowns, giShunts, lrRecord.mpcGate);
    }
    std::printf("%s: %u/%u checks passed\n", guFailures ? "FAIL" : "PASS", guChecks - guFailures, guChecks);
    return guFailures ? 1 : 0;
}
