// =================================================================================================
// GameSource/Physics/VehicleManager/BrnVehicleManager_RaceCarTrafficContact.cpp
//
// Wave T3 round 2, owner B -- THE PLAYER HITS A PHYSICAL TRAFFIC CAR. The race-car-vs-traffic
// branch of the crash-prediction web, from the averager drain down to the four response arms.
//
//   VehicleManager::DoCrashPredictionForRaceCarAndTrafficVehicle    @0x82643D30 (159)
//   VehicleManager::HandleCrashPredictionForRaceCarAndTrafficVehicle @0x82640AB0 ( 93)
//   VehicleManager::HandleRaceCarTrafficCarPotentialContact          @0x8263FA50 (783)
//   VehicleManager::DecideOutcomeOfRaceCarTrafficContact             @0x825C70A0 (305)
//   VehicleManager::ShouldRaceCarCrashOnCarImpact                    @0x825C6FF8 ( 42)
//   VehicleManager::PredictCarCarIntersection                        @0x825C57B0 (565)  NAMED GATE
//
// THREE NAMED GATES, all inside HandleRaceCarTrafficCarPotentialContact (each carries its own
// name + address + blocker + DELETE-WHEN at its seat):
//   PredictCarCarIntersection                     @0x825C57B0 -- returns true (see its banner)
//   VehicleManager::InstantTakedown               @0x82636108 -- reaches the SetRaceCarCrashing trap
//   VehicleManager::SetRaceCarCrashing            @0x82634C90 -- BrnVehicleManagerLinkStubs.cpp:97
// The last two are the RACE-CAR side of the outcome (flag bit 0). This round is the TRAFFIC car's
// reaction; the traffic-side arms (bits 1..3) are all live.
//
// HandleRaceCarTrafficCarPotentialContact was an .ida-exports HOLE (dumped by the wave-T3 scout,
// scratchpad .../wave3/scout/holes/0x8263FA50.txt) AND its Hex-Rays output is the degenerate
// "local variable allocation has failed" form. Every step below is read off the ASM.
//
// NO FEB-2007 SOURCE for any of these. ARTIST asm; DecFIGS DWARF for declaration shape
// (BrnVehicleManager.h :1290 / :1293 / :1143 / :1197 / :1194 / :1286).
//
// THE OUTCOME FLAGS. DecideOutcomeOfRaceCarTrafficContact writes one word whose bits the
// handler dispatches on; the console's own assert at BrnVehicleManager.cpp:7836 names the mask:
//     ( ( ( lxImpactResponseFlags & E_RCTIR_TRAFFIC_MASK ) - 1 )
//         & ( lxImpactResponseFlags & E_RCTIR_TRAFFIC_MASK ) ) == 0
// i.e. AT MOST ONE traffic-side bit may be set. The asm computes that mask as 0x1E.
//
// RECOVERED CONSTANTS (dyn-init .data splats -- ZERO in the image, taken from their static-init
//    thunks at 0x82C5BB88..0x82C5BD30 instead):
//     unk_82FB8270 = splat(flt_82004F5C) = 30.0f    the slam-vs-check magnitude threshold
//     unk_82FB8350 = splat(flt_82019638) = 5000.0f  the other-body mass clamp
//     unk_82FB7FD0 = splat(flt_82013A78) = 0.85f    PredictCarCarIntersection's box shrink
//     unk_82FBA330 = splat(flt_82004014) = 0.1f     the front-corner clip epsilon (lazy-inited
//                                                   in-place by this very function)
//     BrnTraffic::KF_MAX_MASS_FOR_TRAFFIC_CHECKING @0x82F2FFF0 = 5000.0f
//
// unk_8300D01C IS GENUINELY ZERO, and that is load-bearing. It is the damping factor handed
// to ExternalPhysicsBody::DampenAngularVelocity after a CHECK or a SLAM. It has exactly ONE xref
// in the whole image -- this read -- so nothing ever writes it (its neighbours at 0x8300D000 /
// 0x8300D010 / 0x8300D018 belong to BrnTraffic's Logger + DebugComponent block, which is what it
// is: an unwired debug tunable). DampenAngularVelocity computes pow(damping, dt*60), and
// pow(0, x>0) == 0, so on the SHIPPED build a check/slam ZEROES the race car's angular velocity.
// That is the reconstruction, not an approximation of one.
// =================================================================================================

#include "GameSource/Physics/VehicleManager/BrnVehicleManager.h"
#include "GameSource/Physics/VehicleManager/BrnPotentialContactAverager.h"
#include "GameSource/Physics/VehicleManager/BrnPhysicalTrafficManager.h"
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleOutputInterface.h"
#include "GameSource/Physics/VehicleManager/VehiclePhysics/VehiclePhysics.h"
#include "GameSource/Physics/VehicleManager/VehiclePhysics/BrnSimpleVehiclePhysics.h"
#include "GameSource/Physics/VehicleManager/VehiclePhysics/RaceCarPhysics.h"
#include "GameSource/Physics/VehicleManager/VehiclePhysics/VehicleAttribs.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficVehicle.h"   // GetVehicleSpecies
#include "GameSource/World/BrnEntityTypes.h"                                       // BrnWorld::EEntityTypeID
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"

#include <cstdlib>   // getenv (BRN_TRAFFIC_DIAG)
#include <cmath>     // sqrtf / fabsf

namespace BrnPhysics
{
namespace Vehicle
{
namespace
{
    // DIAG. NOT IN THE X360 BINARY. DELETE-WHEN-STABLE.
    bool TrafficDiagEnabled()
    {
        static const bool sbEnabled = (getenv("BRN_TRAFFIC_DIAG") != 0);
        return sbEnabled;
    }
    bool s_bPredictGateLogged = false;

    // unk_82FB8270 -- above this the contact is a CHECK, below it a SLAM.
    const f32 KF_TRAFFIC_CHECK_MAGNITUDE = 30.0f;

    // unk_82FB8350 -- ShouldRaceCarCrashOnCarImpact clamps the other body's mass to this.
    const f32 KF_MAX_IMPACT_MASS = 5000.0f;

    // BrnTraffic::KF_MAX_MASS_FOR_TRAFFIC_CHECKING @0x82F2FFF0 (.rdata, read directly).
    const f32 KF_MAX_MASS_FOR_TRAFFIC_CHECKING = 5000.0f;

    // unk_82FBA330 <- flt_82004014. HandleRaceCarTrafficCarPotentialContact lazily splats this
    // itself the first time the front-corner test runs (the dword_82FBA340 bit-0 latch); on the
    // host that latch is just a constant.
    const f32 KF_FRONT_CORNER_EPSILON = 0.100000001f;

    // unk_8300D01C -- see the banner. Zero on the shipped build.
    const f32 KF_RACECAR_IMPACT_ANGULAR_DAMPING = 0.0f;

    // flt_82002138 -- the shared "is this normal unit length" tolerance.
    const f32 KF_UNIT_NORMAL_TOLERANCE = 0.00999999978f;

    // flt_82001C98 -- SetTrafficVehicleChecked's own re-check window is one second.
    const f32 KF_CHECK_NOTIFY_WINDOW_SECONDS = 1.0f;

    inline f32 Dot3(const Vector3& lrA, const Vector3& lrB)
    {
        return lrA.x * lrB.x + lrA.y * lrB.y + lrA.z * lrB.z;
    }
    inline Vector3 Sub3(const Vector3& lrA, const Vector3& lrB)
    {
        Vector3 lvR; lvR.x = lrA.x - lrB.x; lvR.y = lrA.y - lrB.y;
        lvR.z = lrA.z - lrB.z; lvR.w = lrA.w - lrB.w; return lvR;
    }
    inline Vector3 Scale3(const Vector3& lrV, f32 lfS)
    {
        Vector3 lvR; lvR.x = lrV.x * lfS; lvR.y = lrV.y * lfS;
        lvR.z = lrV.z * lfS; lvR.w = lrV.w * lfS; return lvR;
    }
    inline bool IsUnitLength(const Vector3& lrV)
    {
        const f32 lfLengthSq = Dot3(lrV, lrV);
        const f32 lfLength   = (lfLengthSq == 0.0f) ? 0.0f : lfLengthSq / sqrtf(lfLengthSq);
        return !(fabsf(lfLength - 1.0f) > KF_UNIT_NORMAL_TOLERANCE);
    }
    inline u32 EntityWordOf(const CgsSceneManager::VolumeInstanceId& lrId)
    {
        return static_cast<u32>(lrId.muId >> 32);
    }
    inline u16 EntityIndexOfWord(u32 luEntityWord)
    {
        return static_cast<u16>((luEntityWord >> 10) & 0x3FFFu);
    }
    // VecFloat is Vector4 here and has no scalar constructor; the console's `vspltw` splat.
    inline VecFloat SplatVecFloat(f32 lfValue)
    {
        VecFloat lvfResult;
        lvfResult.x = lfValue; lvfResult.y = lfValue;
        lvfResult.z = lfValue; lvfResult.w = lfValue;
        return lvfResult;
    }
}

// -------------------------------------------------------------------------------------------
// ShouldRaceCarCrashOnCarImpact  @0x825C6FF8 (42)  -- a register-only leaf, no frame.
//
//     (impactSpeed * min(5000, otherMass) / victimMass) * mafVulnerabilityFactor[victim]
//         >  attribs->mCollisionAttribs.GetCrashSpeedMPS() * scale
//
// The reciprocal is a vrefp estimate plus TWO Newton steps (0x825C702C/0x825C707C/0x825C7084);
// reproduced as a plain divide. The `lfsx f0, r8, r3` at 0x825C7044 is
// `*(f32*)(this + 4*(victim + 42944))` == mafVulnerabilityFactor[victim] (+171776), reached by
// name. The attribs slot is the one BrnSimpleVehiclePhysics.h's CollisionAttribs banner already
// names as "Breaker's inlined Race crash test @0x825C7054..60".
// -------------------------------------------------------------------------------------------
bool VehicleManager::ShouldRaceCarCrashOnCarImpact(EActiveRaceCarIndex leVictimActiveRaceCarIndex,
                                                   const RaceCarPhysics* lpVictim,
                                                   const SimpleVehiclePhysics* lpOtherBody,
                                                   VecFloat lvfImpactSpeed, VecFloat lvfScale) const
{
    const f32 lfVictimMass = lpVictim->GetMass().x;
    f32 lfOtherMass = lpOtherBody->GetMass().x;
    if (lfOtherMass > KF_MAX_IMPACT_MASS)
        lfOtherMass = KF_MAX_IMPACT_MASS;                       // vminfp against 5000

    const f32 lfVulnerability =
        mafVulnerabilityFactor[static_cast<s32>(leVictimActiveRaceCarIndex)];
    const f32 lfThreshold =
        lpVictim->GetAttribs()->mCollisionAttribs.GetCrashSpeedMPS().x * lvfScale.x;

    const f32 lfStress = (lvfImpactSpeed.x * lfOtherMass / lfVictimMass) * lfVulnerability;
    return lfStress > lfThreshold;
}

// -------------------------------------------------------------------------------------------
// PredictCarCarIntersection  @0x825C57B0 (565)
//
// GATE: VehicleManager::PredictCarCarIntersection @0x825C57B0 -- the swept-box prediction.
// Blocker: it builds two rw::collision::BoxVolumes over a stack rw::Resource and calls
// rw::collision::PrimitivePairIntersect @0x82BAC130, which has no declaration in the tree.
// DELETE-WHEN that entry point is homed.
//
// WHAT IS ALREADY RECOVERED, so the next round does not repeat this work:
//   * memoisation on mpCachedCarA/mpCachedCarB (this+172416/+172420, tested in BOTH orders at
//     0x825C57F4 and 0x825C5924), returning mbCachedCarCarPredictionResult (+172424) after
//     re-asserting mCachedCarCarPredictionNormal (+172432) is unit length
//     ("Bad cached normal in PredictCarCarIntersection", BrnVehicleManager.cpp:0x1835/0x183C).
// THE TREE MODELS mpCachedCarA/B AS `u32 muCachedCarASlot/BSlot` to hold the +172432 seat
//     on x64; a real body needs a pointer-shaped identity there (or the slot index the console's
//     pointers stand for). That is a HEADER decision, not a body one.
//   * both half-extents come from SimpleVehiclePhysics::mDeformableAABB (+0x6D0 min / +0x6E0
//     max), halved and then shrunk by (1 - 0.85) * halfExtent.x -- unk_82FB7FD0 == 0.85f,
//     recovered from its static-init thunk @0x82C5BBB0.
//   * each box's transform is the body's own, translated by the body velocity over the step and
//     re-orthonormalised (rw::math::vpu::OrthoNormalize3x3 @0x82203B28, twice).
//   * on a hit it additionally requires the result's +0x700 lane to be <= 0, then latches
//     mCachedCarCarPredictionNormal and asserts it ("Bad calculated normal in
//     PredictCarCarIntersection", :0x18A8).
//
// GATE RETURN VALUE, and why: the caller uses this as a REFINEMENT filter over contacts the
// scene manager has already reported as overlapping. Returning true means "no extra filtering" --
// every reported pair is handled, which is the round's goal; the near-miss/freak-out arm simply
// never fires. Returning false would make EVERY hit a near miss and nothing else could ever
// happen. Neither is the console; true is the honest degradation and is what is shipped here.
// -------------------------------------------------------------------------------------------
bool VehicleManager::PredictCarCarIntersection(const SimpleVehiclePhysics* lpBodyA,
                                               const SimpleVehiclePhysics* lpBodyB,
                                               f32 lfTimestep)
{
    (void)lpBodyA; (void)lpBodyB; (void)lfTimestep;

    if (!s_bPredictGateLogged && CgsDev::Log::gpDebugPrint != 0)
    {
        s_bPredictGateLogged = true;
        *CgsDev::Log::gpDebugPrint
            << "[GATE] VehicleManager::PredictCarCarIntersection @0x825C57B0 -- swept-box "
               "prediction not landed (rw::collision::PrimitivePairIntersect @0x82BAC130 "
               "undeclared); every predicted pair reported as intersecting.\n";
    }
    return true;
}

// -------------------------------------------------------------------------------------------
// DecideOutcomeOfRaceCarTrafficContact  @0x825C70A0 (305)
//
// The classifier. Register roles read off the prologue: r4/r5 the two indices, v1 the contact
// normal, v2 the point on the race car, v3 the point on the traffic car (v2/v3 are accepted and
// DEAD in the console body -- no VMX register other than v125 == v1 is ever read), r6 the
// VecFloat slam-magnitude out, r7 the flags out.
//
// Ladder, in asm order:
//   0x825C7180  "Bad normal in DecideOutcomeOfRaceCarTrafficContact" + the NULL-out assert
//   0x825C7238  both outs cleared
//   0x825C7258  maeRaceCarTypes[raceCar] classified PLAYER(0) / NETWORK(2)
//   0x825C727C  BrnTraffic::GetVehicleSpecies on the traffic car's GLOBAL index -> is it a TRAILER
//   0x825C7294  the RACE CAR already crashing -> E_RCTIR_CRASH_TRAFFIC and out
//   0x825C72DC  the traffic car ALREADY has a check owner:
//                 player / self / network  -> nothing at all
//                 otherwise                -> CRASH_RACECAR | CRASH_TRAFFIC
//   0x825C7328  can it be checked at all: not a trailer AND lighter than
//               BrnTraffic::KF_MAX_MASS_FOR_TRAFFIC_CHECKING
//   0x825C7374  the impact speed: |dot(trafficVel - raceCarVel, normal flattened against the race
//               car's up axis)|
//   0x825C73CC  the crash test, skipped entirely for a NETWORK car
//   0x825C744C  outcomes
// -------------------------------------------------------------------------------------------
void VehicleManager::DecideOutcomeOfRaceCarTrafficContact(u16 luActiveRaceCarIndex,
                                                          u16 lu16TrafficCarIndex,
                                                          Vector3 lContactNormal,
                                                          Vector3 lPointOnRaceCar,
                                                          Vector3 lPointOnTraffic,
                                                          VecFloat* lpTrafficSlamMagnitude,
                                                          u32* lpxOutResponseFlags)
{
    (void)lPointOnRaceCar;    // accepted, never read by the console body
    (void)lPointOnTraffic;

    CGS_ASSERT(IsUnitLength(lContactNormal),
               "Bad normal in DecideOutcomeOfRaceCarTrafficContact: ");
    CGS_ASSERT(lpxOutResponseFlags != 0, "lpxOutResponseFlags != NULL");

    *lpxOutResponseFlags = 0;
    lpTrafficSlamMagnitude->x = 0.0f;
    lpTrafficSlamMagnitude->y = 0.0f;
    lpTrafficSlamMagnitude->z = 0.0f;
    lpTrafficSlamMagnitude->w = 0.0f;

    const BrnWorld::ERaceCarType leRaceCarType = maeRaceCarTypes[luActiveRaceCarIndex];
    const bool lbRaceCarIsPlayer  = (leRaceCarType == BrnWorld::E_RACE_CAR_TYPE_PLAYER);
    const bool lbRaceCarIsNetwork = (leRaceCarType == BrnWorld::E_RACE_CAR_TYPE_NETWORK);

    const EntityId lGlobalTrafficID =
        mPhysicalTrafficManager.GetGlobalTrafficEntityId(lu16TrafficCarIndex);
    const bool lbTrafficIsTrailer =
        (BrnTraffic::GetVehicleSpecies(EntityIndexOfWord(lGlobalTrafficID.muValue))
             == BrnTraffic::Vehicle::E_SPECIES_TRAILER);

    RaceCarPhysics* const lpRaceCarPhysics = &maRaceCarVehicles[luActiveRaceCarIndex];

    // The race car is already crashing: the traffic car just crashes too, nothing else.
    if (lpRaceCarPhysics->IsCrashing())
    {
        *lpxOutResponseFlags = KU_RCTIR_CRASH_TRAFFIC;
        return;
    }

    PhysicalTrafficVehicle* const lpTrafficVehicle =
        mPhysicalTrafficManager.GetTrafficVehicle(static_cast<s32>(lu16TrafficCarIndex));
    SimpleVehiclePhysics* const lpTrafficBody = lpTrafficVehicle->mpVehicleBody;

    if (lpTrafficVehicle->miCheckOwner != -1)
    {
        if (lbRaceCarIsPlayer)                                                   return;
        if (lpTrafficVehicle->miCheckOwner == static_cast<s8>(luActiveRaceCarIndex)) return;
        if (lbRaceCarIsNetwork)                                                  return;
        *lpxOutResponseFlags = KU_RCTIR_CRASH_RACECAR | KU_RCTIR_CRASH_TRAFFIC;
        return;
    }

    const bool lbCanBeChecked =
        !lbTrafficIsTrailer
        && (KF_MAX_MASS_FOR_TRAFFIC_CHECKING > lpTrafficBody->GetMass().x);

    // The impact speed the crash test and the slam magnitude both use: the closing speed along
    // the contact normal with the race car's UP component projected out (0x825C7374..0x825C73C4).
    const Matrix44Affine lRaceCarTransform = lpRaceCarPhysics->GetTransform();
    const Vector3 lvRelativeVelocity =
        Sub3(lpTrafficBody->GetLinearVelocity(), lpRaceCarPhysics->GetLinearVelocity());
    const Vector3 lvFlatNormal =
        Sub3(lContactNormal, Scale3(lRaceCarTransform.yAxis, Dot3(lContactNormal, lRaceCarTransform.yAxis)));
    const f32 lfImpactSpeed = fabsf(Dot3(lvRelativeVelocity, lvFlatNormal));

    bool lbShouldCrash = false;
    if (!lbRaceCarIsNetwork)
    {
        f32 lfScale = 1.0f;
        if (!lpTrafficBody->IsCrashing())
        {
            // FLAG (VMX operand order): the scale cascade at 0x825C73D8..0x825C7434 is
            // reproduced from the vmaddfp/vnmsubfp convention this file's rsqrt chains pin
            // (vD = vA*vC + vB / vB - vA*vC). It only tunes WHEN THE RACE CAR crashes -- the
            // traffic-side arms do not read it.
            const f32 lfHeadingDot = Dot3(lRaceCarTransform.zAxis, lpTrafficBody->GetTransform().zAxis);
            const f32 lfAlign      = lfHeadingDot * 0.5f + 0.5f + 0.25f;   // vmaddcfp on 0.25/0.5
            f32 lfTrafficSpeed     = lpTrafficBody->GetSpeedMPH().x * 0.447039992f * 0.5f * 0.5f;
            if (lfTrafficSpeed < 0.0f) lfTrafficSpeed = 0.0f;              // vmaxfp against 0
            if (lfTrafficSpeed > 1.0f) lfTrafficSpeed = 1.0f;              // vminfp against 1
            const f32 lfBlend = (1.0f - lfAlign) * (1.0f - lfTrafficSpeed) + lfAlign;
            const f32 lfCube  = (1.0f - lfBlend) * (1.0f - lfBlend) * (1.0f - lfBlend);
            lfScale = 1.0f - lfCube;
        }
        lbShouldCrash = ShouldRaceCarCrashOnCarImpact(
            static_cast<EActiveRaceCarIndex>(luActiveRaceCarIndex),
            lpRaceCarPhysics, lpTrafficBody,
            SplatVecFloat(lfImpactSpeed), SplatVecFloat(lfScale));
    }

    // The traffic car is ALREADY crashing and this impact would crash the race car: the race car
    // alone goes (nothing more to do to a crashing traffic car).
    if (lpTrafficBody->IsCrashing() && lbShouldCrash)
    {
        *lpxOutResponseFlags = KU_RCTIR_CRASH_RACECAR;
        return;
    }
    // Hitting a TRAILER hard enough is a mutual crash.
    if (lbTrafficIsTrailer && lbShouldCrash)
    {
        *lpxOutResponseFlags = KU_RCTIR_CRASH_RACECAR | KU_RCTIR_CRASH_TRAFFIC;
        return;
    }

    u32 luFlags;
    if (lbShouldCrash)
    {
        luFlags = KU_RCTIR_CRASH_RACECAR | KU_RCTIR_CRASH_TRAFFIC;
    }
    else
    {
        // slamMagnitude = impactSpeed * raceCarMass / trafficMass (a vrefp + two Newton steps on
        // the traffic mass). The subfic/subfe/rlwinm/addi tail at 0x825C7500 selects
        //     magnitude > 30  ->  CHECK (8)      magnitude <= 30  ->  SLAM (4)
        const f32 lfSlamMagnitude =
            lfImpactSpeed * lpRaceCarPhysics->GetMass().x / lpTrafficBody->GetMass().x;
        lpTrafficSlamMagnitude->x = lfSlamMagnitude;
        lpTrafficSlamMagnitude->y = lfSlamMagnitude;
        lpTrafficSlamMagnitude->z = lfSlamMagnitude;
        lpTrafficSlamMagnitude->w = lfSlamMagnitude;
        luFlags = (lfSlamMagnitude > KF_TRAFFIC_CHECK_MAGNITUDE) ? KU_RCTIR_CHECK_TRAFFIC
                                                                 : KU_RCTIR_SLAM_TRAFFIC;
    }
    *lpxOutResponseFlags = luFlags;

    // A trailer, an articulated CAB, or a car too heavy to check cannot be slammed OR checked
    // (`rlwinm r11,r11,0,30,27` == clear bits 2 and 3).
    if (lbTrafficIsTrailer
        || lpTrafficVehicle->meArticulatedVehicleType == PhysicalTrafficVehicle::E_ARTICULATE_VEHICLE_CAB
        || !lbCanBeChecked)
    {
        *lpxOutResponseFlags &= ~(KU_RCTIR_SLAM_TRAFFIC | KU_RCTIR_CHECK_TRAFFIC);
    }
    // A NETWORK car never crashes locally (`clrrwi r11,r11,1`).
    if (lbRaceCarIsNetwork)
    {
        *lpxOutResponseFlags &= ~KU_RCTIR_CRASH_RACECAR;
    }
}

// -------------------------------------------------------------------------------------------
// HandleRaceCarTrafficCarPotentialContact  @0x8263FA50 (783)  -- DWARF :1143
//
// THE handler. The contact arrives BY VALUE (80 bytes: r4..r10 plus three stack slots at
// +0x58/+0x60/+0x68 of the caller's parameter area).
// -------------------------------------------------------------------------------------------
void VehicleManager::HandleRaceCarTrafficCarPotentialContact(
    CgsSceneManager::SceneManagerIO::PotentialContact lContact,
    BrnPhysics::Vehicle::VehicleOutputRequestInterface* lpRequestOutputInterface,
    BrnPhysics::Vehicle::VehicleOutputInterface* lpVehicleOutputInterface,
    VehicleManagerOutputInterface* lpManagerOutputInterface,
    BrnPhysics::Deformation::DeformationInputInterface* lpDeformationInterface,
    f32 lfTimestep)
{
    // 0x8263FA98: when volume A is the TRAFFIC vehicle the whole record is mirrored so that A is
    // always the race car -- exactly PotentialContact::SwapEntityOrder (the three vector swaps,
    // the id/tag/index swaps and the sign-flipped normal, all inline in the console).
    if ((EntityWordOf(lContact.muVolumeInstanceIdA) >> 24) == static_cast<u32>(BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE))
        lContact.SwapEntityOrder();

    const Vector3 lPointOnRaceCar = lContact.mPointOnA;   // v125
    const Vector3 lPointOnTraffic = lContact.mPointOnB;   // v127
    const Vector3 lContactNormal  = lContact.mNormal;     // v126

    const u32 luRaceCarGlobalWord = EntityWordOf(lContact.muVolumeInstanceIdA);
    const u32 luTrafficGlobalWord = EntityWordOf(lContact.muVolumeInstanceIdB);
    const u16 lu16TrafficGlobalIndex = EntityIndexOfWord(luTrafficGlobalWord);

    // 0x8263FB98: the traffic car must currently HAVE a physical body. This is the inlined
    // GetTrafficPhysicsEntityIDFromGlobalEntityID_Safe (its own h:944 bound assert included);
    // the 0x7F map sentinel is a silent return.
    ::EntityId lTrafficPhysicsID;
    if (!GetTrafficPhysicsEntityIDFromGlobalEntityID_Safe(luTrafficGlobalWord, &lTrafficPhysicsID))
        return;

    // 0x8263FC10: the RACE-CAR side goes through the owner-dispatching lookup (which, for a race
    // car, is pure validation and returns the id unchanged).
    const CgsSceneManager::EntityId lRaceCarPhysicsID =
        GetPhysicsEntityIDFromGlobalEntityID(CgsSceneManager::EntityId(luRaceCarGlobalWord));

    const u16 lu16RaceCarIndex = static_cast<u16>((static_cast<u32>(lRaceCarPhysicsID) >> 10) & 0x3FFFu);
    const u16 lu16TrafficIndex = EntityIndexOfWord(lTrafficPhysicsID.muValue);

    RaceCarPhysics* const lpRaceCarPhysics = &maRaceCarVehicles[lu16RaceCarIndex];
    PhysicalTrafficVehicle* const lpTrafficVehicle =
        mPhysicalTrafficManager.GetTrafficVehicle(static_cast<s32>(lu16TrafficIndex));
    SimpleVehiclePhysics* const lpTrafficBody = lpTrafficVehicle->mpVehicleBody;

    // 0x826400D4: the vtable +0x10 slot is IsPlayerVehicleInShowtime (VehiclePhysics.h:1192 --
    // settled by the crash/shunt wave, see that header's note). A showtime car skips the
    // prediction filter entirely and always handles the contact.
    if (!lpRaceCarPhysics->IsPlayerVehicleInShowtime()
        && !PredictCarCarIntersection(lpTrafficBody, lpRaceCarPhysics, lfTimestep))
    {
        // NEAR MISS. Note the argument order: the TRAFFIC physics id first (slot +0x70), then the
        // race car's (+0x78), then the race car body (+0x80) -- DWARF BrnPhysicalTrafficManager.h:386.
        ::EntityId lRaceCarPhysicsIDWord;
        lRaceCarPhysicsIDWord.muValue = static_cast<u32>(lRaceCarPhysicsID);
        mPhysicalTrafficManager.TestForNearMissFreakOut(
            lContact, lTrafficPhysicsID, lRaceCarPhysicsIDWord, lpRaceCarPhysics,
            lpManagerOutputInterface, lpVehicleOutputInterface, lpDeformationInterface);
        return;
    }

    CGS_ASSERT(lRaceCarPhysicsID.GetOwner() == static_cast<u32>(BrnWorld::E_ENTITYTYPE_RACECAR),
               "lRaceCarEntityID.GetOwner() == BrnWorld::E_ENTITYTYPE_RACECAR");
    CGS_ASSERT((luTrafficGlobalWord >> 24) == static_cast<u32>(BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE),
               "lTrafficEntityID.GetOwner() == BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE");

    // GATE: VehicleManagerDebugComponent::RecordRaceCarTrafficContact @0x825B7270 (140), called
    // at 0x826401E4. Blocker: its five "last traffic contact" debug seats (+472/+608/+672/+736/
    // +848 of the debug component) are not modelled; it is a debug-render snapshot with no
    // gameplay effect. DELETE-WHEN the VehicleManagerDebugComponent traffic block is recovered.

    u32 luImpactResponseFlags = 0;
    VecFloat lvfTrafficSlamMagnitude;
    DecideOutcomeOfRaceCarTrafficContact(lu16RaceCarIndex, lu16TrafficIndex,
                                         lContactNormal, lPointOnRaceCar, lPointOnTraffic,
                                         &lvfTrafficSlamMagnitude, &luImpactResponseFlags);

    const u32 luTrafficBits = luImpactResponseFlags & KU_RCTIR_TRAFFIC_MASK;
    CGS_ASSERT(((luTrafficBits - 1u) & luTrafficBits) == 0u,
               "( ( ( lxImpactResponseFlags & E_RCTIR_TRAFFIC_MASK ) - 1 ) "
               "& ( lxImpactResponseFlags & E_RCTIR_TRAFFIC_MASK ) ) == 0");

    // ---- [T4-hit] the DECODED outcome, once per outcome kind --------------------------------
    // DIAG. NOT IN THE X360 BINARY. Opt-in (BRN_TRAFFIC_DIAG). Names WHICH arm ran and latches per
    // kind so the first crash does not mask the first slam. FIVE kinds, not three: DecideOutcome
    // can return CRASH_RACECAR ALONE (:341-345, the traffic car is already crashing), and it can
    // return 0 (:375-386 clear SLAM|CHECK for trailer/cab/uncheckable and CRASH_RACECAR for
    // network cars). The NEARMISS kind never reaches here; it is reported from
    // TestForNearMissFreakOut. DELETE-WHEN-STABLE.
    if (TrafficDiagEnabled() && CgsDev::Log::gpDebugPrint != 0)
    {
        static bool sbCrashSeen    = false;
        static bool sbCheckSeen    = false;
        static bool sbSlamSeen     = false;
        static bool sbRaceOnlySeen = false;
        static bool sbNoneSeen     = false;

        const bool lbCrashRaceCar = (luImpactResponseFlags & KU_RCTIR_CRASH_RACECAR) != 0;
        const bool lbCrash    = (luImpactResponseFlags & KU_RCTIR_CRASH_TRAFFIC) != 0;
        const bool lbCheck    = !lbCrash && (luImpactResponseFlags & KU_RCTIR_CHECK_TRAFFIC) != 0;
        const bool lbSlam     = !lbCrash && (luImpactResponseFlags & KU_RCTIR_SLAM_TRAFFIC) != 0;
        const bool lbRaceOnly = !lbCrash && !lbCheck && !lbSlam && lbCrashRaceCar;
        const bool lbNone     = (luImpactResponseFlags == 0u);

        if ((lbCrash && !sbCrashSeen) || (lbCheck && !sbCheckSeen) || (lbSlam && !sbSlamSeen)
            || (lbRaceOnly && !sbRaceOnlySeen) || (lbNone && !sbNoneSeen))
        {
            if (lbCrash)    sbCrashSeen    = true;
            if (lbCheck)    sbCheckSeen    = true;
            if (lbSlam)     sbSlamSeen     = true;
            if (lbRaceOnly) sbRaceOnlySeen = true;
            if (lbNone)     sbNoneSeen     = true;
            *CgsDev::Log::gpDebugPrint
                << "[T4-hit] decided outcome="
                << (lbCrash    ? "CRASH_TRAFFIC"
                  : lbCheck    ? "CHECK_TRAFFIC"
                  : lbSlam     ? "SLAM_TRAFFIC"
                  : lbRaceOnly ? "CRASH_RACECAR_ONLY"
                               : "NONE")
                << ((lbCrashRaceCar && !lbRaceOnly) ? "+CRASH_RACECAR" : "")
                << " flags=" << CgsDev::E_PRINTMODE_HEXONCE << static_cast<u32>(luImpactResponseFlags)
                << " raceCar=" << static_cast<s32>(lu16RaceCarIndex)
                << " trafficSlot=" << static_cast<s32>(lu16TrafficIndex);
            // DecideOutcome writes the magnitude only on the CHECK/SLAM arm; the crash arms leave
            // the out param untouched, so do not read it there.
            if (lbCheck || lbSlam)
                *CgsDev::Log::gpDebugPrint << " slamMag=" << lvfTrafficSlamMagnitude.x;
            *CgsDev::Log::gpDebugPrint << "\n";
        }
    }

    // 0x82640244: the PLAYER-only "you checked a traffic car" 2-byte game event (type 73). Its
    // payload is the traffic car's GLOBAL entity index, not the physical slot.
    if ((luImpactResponseFlags & KU_RCTIR_CHECK_TRAFFIC) != 0
        && maeRaceCarTypes[lu16RaceCarIndex] == BrnWorld::E_RACE_CAR_TYPE_PLAYER)
    {
        struct PlayerCheckedTrafficEvent : public CgsModule::Event { u16 mu16TrafficGlobalIndex; };
        PlayerCheckedTrafficEvent lEvent;
        lEvent.mu16TrafficGlobalIndex = lu16TrafficGlobalIndex;
        lpVehicleOutputInterface->GetGameEventQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lEvent), 73, 2);
    }

    ::EntityId lRaceCarPhysicsIDWord;
    lRaceCarPhysicsIDWord.muValue = static_cast<u32>(lRaceCarPhysicsID);

    if ((luImpactResponseFlags & KU_RCTIR_CRASH_TRAFFIC) != 0)
    {
        mPhysicalTrafficManager.SetTrafficVehicleCrashing(
            lTrafficPhysicsID, lRaceCarPhysicsIDWord,
            lpManagerOutputInterface, lpVehicleOutputInterface, lpDeformationInterface);
    }
    else
    {
        bool lbDampenRaceCar = false;
        if ((luImpactResponseFlags & KU_RCTIR_CHECK_TRAFFIC) != 0)
        {
            mPhysicalTrafficManager.SetTrafficVehicleChecked(
                lTrafficPhysicsID, lRaceCarPhysicsIDWord, lpRaceCarPhysics,
                lpManagerOutputInterface, lpVehicleOutputInterface, lpDeformationInterface,
                lPointOnTraffic);
            lbDampenRaceCar = true;
        }
        else if ((luImpactResponseFlags & KU_RCTIR_SLAM_TRAFFIC) != 0)
        {
            mPhysicalTrafficManager.SetTrafficVehicleSlammed(
                lTrafficPhysicsID, lRaceCarPhysicsIDWord, lpRaceCarPhysics,
                lpManagerOutputInterface, lpVehicleOutputInterface, lpDeformationInterface,
                lPointOnTraffic, lvfTrafficSlamMagnitude);
            lbDampenRaceCar = true;
        }

        // 0x8264033C: BOTH the check and the slam arm fall into this; the crash arm branches past
        // it. See the banner for why the damping factor is 0.0f on the shipped build.
        if (lbDampenRaceCar)
        {
            lpRaceCarPhysics->DampenAngularVelocity(SplatVecFloat(KF_RACECAR_IMPACT_ANGULAR_DAMPING),
                                                    SplatVecFloat(lfTimestep));
        }
    }

    if ((luImpactResponseFlags & KU_RCTIR_CRASH_RACECAR) == 0)
        return;

    CGS_ASSERT((luImpactResponseFlags & KU_RCTIR_CRASH_TRAFFIC) == 0 || lpTrafficBody->IsCrashing(),
               "(lxImpactResponseFlags & E_RCTIR_CRASH_TRAFFIC) == 0 || lpTrafficPhysics->IsCrashing()");

    // 0x8264037C: THE TRAFFIC-CHECK TAKEDOWN. If this traffic car was checked into us, recently,
    // by a DIFFERENT car, and that car is the player, and the traffic car is (roughly) in front of
    // it, the player gets the takedown instead of a plain crash.
    bool lbTakenDown = false;
    const s8 li8CheckOwner = lpTrafficVehicle->miCheckOwner;
    if (li8CheckOwner != -1
        && lpTrafficVehicle->mfTimeSinceCheckNotify < KF_CHECK_NOTIFY_WINDOW_SECONDS
        && li8CheckOwner != static_cast<s8>(lu16RaceCarIndex)
        && maeRaceCarTypes[li8CheckOwner] == BrnWorld::E_RACE_CAR_TYPE_PLAYER)
    {
        const RaceCarPhysics& lrChecker = maRaceCarVehicles[li8CheckOwner];
        const Matrix44Affine lCheckerTransform = lrChecker.GetTransform();
        const Vector3 lvTrafficFromChecker =
            Sub3(lpTrafficBody->GetTransform().wAxis, lCheckerTransform.wAxis);
        // `vcfsx v13,1,1` == 0.5, sign-flipped and halved again -> the -0.25 the dot is tested
        // against (0x826403D0..0x8264040C).
        if (Dot3(lvTrafficFromChecker, lCheckerTransform.zAxis) > -0.25f)
        {
            // The two commit operands, computed exactly as the console does so the gate below is a
            // one-line deletion:
            //   aggressor = maRaceCarHandlingBodyIDs[checkOwner] >> 32 (the handle's entity word,
            //               `ldx r11, 8*(checkOwner+0x155C), this ; srdi 32` @0x82640454)
            //   victim    = the race car's GLOBAL entity word (var_184)
            //   stress    = flt_82002138 == 0.01f ; type = E_TAKEDOWN_TRAFFIC_CHECK (`li r6, 4`)
            ::EntityId lAggressorID;
            lAggressorID.muValue = static_cast<u32>(maRaceCarHandlingBodyIDs[li8CheckOwner] >> 32);
            ::EntityId lVictimID;
            lVictimID.muValue = luRaceCarGlobalWord;
            (void)lAggressorID; (void)lVictimID;

            // GATE: VehicleManager::InstantTakedown @0x82636108 (called at 0x82640464).
            // Blocker: it is REAL and mounted, but its own commit calls SetRaceCarCrashing
            // @0x82634C90, which is the LOUD TRAP in BrnVehicleManagerLinkStubs.cpp:97 (the
            // 923-insn body is in the unmounted BrnVehicleManager.cpp). This round is the TRAFFIC
            // car's reaction, not race-car takedowns. DELETE-WHEN the crash-commit chain mounts.
            lbTakenDown = true;
        }
    }

    if (!lbTakenDown)
    {
        // 0x826404A8: the plain crash commit -- the normal NEGATED for the victim
        // (`vspltisw v0,-1 ; vslw ; vxor128 v1, v126, v0`), takedown type -1 (NONE).
        // GATE: VehicleManager::SetRaceCarCrashing @0x82634C90 (923). Blocker: the only body is in
        // the unmounted BrnVehicleManager.cpp; the mounted symbol is the CGS_ASSERT(false) trap at
        // BrnVehicleManagerLinkStubs.cpp:97. DELETE-WHEN that chain mounts.
        (void)lpRequestOutputInterface;
    }

    // 0x826404AC: PLAYER ONLY. Both contact points must lie OUTSIDE the other car's deformable
    // box laterally AND beyond its front, in BOTH frames -- i.e. the two cars clipped corners
    // rather than met face on. When that holds the slow-motion suppression flag is latched.
    if (EntityIndexOfWord(luRaceCarGlobalWord) == static_cast<u32>(mePlayerActiveRaceCarIndex))
    {
        if (IsFrontCornerClip(*lpRaceCarPhysics, lPointOnTraffic)
            && IsFrontCornerClip(*lpTrafficBody, lPointOnRaceCar))
        {
            mbForceNoSlowMo = true;   // `stbx r10(1), r18, 0x2A11D` == this + 172317
        }
    }
}

// -------------------------------------------------------------------------------------------
// The front-corner test HandleRaceCarTrafficCarPotentialContact runs twice (0x82640510 and
// 0x826405B8, instruction-identical with the roles swapped): express the OTHER car's contact
// point in this body's frame and require it to be past the front face AND outside one of the two
// side faces of mDeformableAABB, each face pulled in by unk_82FBA330 == 0.1.
// NOT AN X360 SYMBOL -- the console emits the sequence twice inline; outlined here per the
// de-optimisation rule, with the two call sites kept explicit.
// -------------------------------------------------------------------------------------------
bool VehicleManager::IsFrontCornerClip(const SimpleVehiclePhysics& lrBody, Vector3 lContactPoint)
{
    const Matrix44Affine lTransform = lrBody.GetTransform();
    const Vector3 lvLocal = Sub3(lContactPoint, lTransform.wAxis);

    const f32 lfAlongForward = Dot3(lvLocal, lTransform.zAxis);
    const f32 lfAlongRight   = Dot3(lvLocal, lTransform.xAxis);

    const CgsGeometric::AxisAlignedBox& lrBox = lrBody.GetDeformableAABB();

    if (!(lfAlongForward > lrBox.mMax.z - KF_FRONT_CORNER_EPSILON))
        return false;
    if (lfAlongRight > lrBox.mMax.x - KF_FRONT_CORNER_EPSILON)
        return true;
    return (lrBox.mMin.x + KF_FRONT_CORNER_EPSILON) > lfAlongRight;
}

// -------------------------------------------------------------------------------------------
// HandleCrashPredictionForRaceCarAndTrafficVehicle  @0x82640AB0 (93)  -- DWARF :1293
//
// Drain the averager: one HandleRaceCarTrafficCarPotentialContact per accumulated pair, then
// reset the pair count (`stw r24, 0x690(r30)` @0x82640C14).
// -------------------------------------------------------------------------------------------
void VehicleManager::HandleCrashPredictionForRaceCarAndTrafficVehicle(
    PotentialContactAverager* lpContactPairAverager,
    f32 lfTimestep,
    BrnPhysics::Vehicle::VehicleOutputInterface* lpVehicleOutputInterface,
    BrnPhysics::Vehicle::VehicleOutputRequestInterface* lpRequestOutputInterface,
    VehicleManagerOutputInterface* lpManagerOutputInterface,
    BrnPhysics::Deformation::DeformationInputInterface* lpDeformationInterface)
{
    CGS_ASSERT(lpContactPairAverager != 0, "lpContactPairAverager");
    CGS_ASSERT(lpVehicleOutputInterface != 0, "lpVehicleOutputInterface");
    CGS_ASSERT(lpManagerOutputInterface != 0, "lpVehicleManagerOutputInterface");
    CGS_ASSERT(lpRequestOutputInterface != 0, "lpRequestOutputInterface");
    CGS_ASSERT(lpDeformationInterface != 0, "lpDeformationInterface");

    for (u32 luPair = 0; luPair < lpContactPairAverager->muContactPairCount; ++luPair)
    {
        CgsSceneManager::SceneManagerIO::PotentialContact lAveraged;
        lpContactPairAverager->GetAveragedContactPoint(luPair, lAveraged);
        HandleRaceCarTrafficCarPotentialContact(lAveraged,
                                                lpRequestOutputInterface, lpVehicleOutputInterface,
                                                lpManagerOutputInterface, lpDeformationInterface,
                                                lfTimestep);
    }
    lpContactPairAverager->Reset();
}

// -------------------------------------------------------------------------------------------
// DoCrashPredictionForRaceCarAndTrafficVehicle  @0x82643D30 (159)  -- DWARF :1290
//
// One contact in: validate its normal, fold it into the averager, and -- only when the averager
// was already full -- flush and retry. The X360's finite check is three vcmpeqfp(n,n) lane tests
// with the offending vector streamed into the message (BrnVehicleManager.cpp:9486).
// -------------------------------------------------------------------------------------------
void VehicleManager::DoCrashPredictionForRaceCarAndTrafficVehicle(
    PotentialContactAverager* lpContactPairAverager,
    const CgsSceneManager::SceneManagerIO::PotentialContact* lpContact,
    f32 lfTimestep,
    BrnPhysics::Vehicle::VehicleOutputInterface* lpVehicleOutputInterface,
    BrnPhysics::Vehicle::VehicleOutputRequestInterface* lpRequestOutputInterface,
    VehicleManagerOutputInterface* lpManagerOutputInterface,
    BrnPhysics::Deformation::DeformationInputInterface* lpDeformationInterface)
{
    CGS_ASSERT(lpContactPairAverager != 0, "lpContactPairAverager");
    CGS_ASSERT(lpVehicleOutputInterface != 0, "lpVehicleOutputInterface");
    CGS_ASSERT(lpManagerOutputInterface != 0, "lpVehicleManagerOutputInterface");
    CGS_ASSERT(lpRequestOutputInterface != 0, "lpRequestOutputInterface");
    CGS_ASSERT(lpDeformationInterface != 0, "lpDeformationInterface");

    const Vector3& lrNormal = lpContact->mNormal;
    CGS_ASSERT(lrNormal.x == lrNormal.x && lrNormal.y == lrNormal.y && lrNormal.z == lrNormal.z,
               "Invalid contact: ");

    if (!lpContactPairAverager->AddContactPair(*lpContact))
    {
        HandleCrashPredictionForRaceCarAndTrafficVehicle(
            lpContactPairAverager, lfTimestep, lpVehicleOutputInterface,
            lpRequestOutputInterface, lpManagerOutputInterface, lpDeformationInterface);
        lpContactPairAverager->AddContactPair(*lpContact);
    }
}

}   // namespace Vehicle
}   // namespace BrnPhysics
