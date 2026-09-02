// =================================================================================================
// GameSource/Physics/VehicleManager/BrnVehicleManager_WorldCrashArm.cpp
//
// THE RACE-CAR-vs-WORLD CRASH ARM -- the two callees that kept HandleCrashPredictionForRaceCarAndWorld
// @0x82640C28 unmountable, and therefore kept "we can only crash on traffic, not on walls" true:
//
//   VehicleManager::HandleRaceCarWorldPotentialContact  @0x8263E3B8 (566 insns)  -- DWARF BrnVehicleManager.h:1200
//   VehicleManager::PredictCarWorldContactTime          @0x825B5300 ( 83 insns)  -- DWARF BrnVehicleManager.h:1215
//
// Slice TU (home BrnVehicleManager.cpp is still unmountable). Both bodies are read off the ARTIST
// asm. The Hex-Rays for the first is unusable (a 31-int prototype, "local variable allocation has
// failed"); for the second it DROPS the per-car index -- it prints this+1936 / this+1888, which is
// maRaceCarVehicles[0], but the asm at 0x825B53A0 / 0x825B53E4 does `extrwi r9, r9, 14,8 ;
// mulli r11, r9, 0x1460` first, so the reads are maRaceCarVehicles[idx].mLinearVelocity (+0x50)
// and .mTransform.yAxis (+0x20).
//
// ARGUMENT MAP (asm prologue 0x8263E3D4..0x8263E400): r3 = this; the 80-byte PotentialContact is
// passed BY VALUE -- r4..r10 carry bytes 0..55 (spilled to arg_20..arg_50), bytes 56..79 arrive on
// the stack at arg_58; f1 = lfTimestep (`fmr f31, f1`). The four interface pointers are stack args
// arg_74 / arg_7C / arg_84 / arg_8C and are only ever re-loaded for the SetRaceCarCrashing call
// (0x8263EC58..0x8263EC70), in the order request / vehicle-out / manager-out / deformation. The
// triangle-cache interface (arg_94) is NEVER read by this body: the console accepts it and
// ignores it, so the parameter is kept and unused.
//
// THE CONSTANTS -- three families, and two of them read ZERO in the image:
//   .rdata literals:   flt_82002138 = 0.01f (unit-normal tolerance), flt_82001CC0 = 0.0f,
//                      flt_82005450 = 0.9f, unk_82181510 = (0, 1, 0, 0) (world up).
//   function statics:  flt_8200D5FC = -0.70f, flt_8209C7F0 = -0.53f, flt_8201A1F0 = 200.0f,
//                      flt_82004740 = 0.30f -- four function-local `static const VecFloat` splats
//                      behind the dword_82FBA320 guard bits (0x8263E76C..0x8263E8E0 is MSVC's
//                      lazy init of each; unk_82FBA2E0/2F0/300/310 are the splat caches).
//   .data, dyn-init:   flt_82FB914C reads 0 in the image. It is WRITTEN by sub_82C5B8E0 as
//                      cosf(flt_82FB9CC8), and flt_82FB9CC8 is itself written by the EXPORT-HOLE
//                      thunk at 0x82C5B880..0x82C5B8DC as flt_82004A18 (80.0f) * flt_8208F5F4
//                      (0.017453292f) -- 80 degrees in radians. The two thunks are adjacent, in the
//                      same TU, in declaration order, so the cos sees the product:
//                      flt_82FB914C == cosf(80 deg) == 0.17364818f.
//                      unk_82FB82B0 reads 0 in the image. Written by the export-hole thunk at
//                      0x82C5BB88..0x82C5BBAC as splat(flt_82004014) == 0.10f.
//                      Neither thunk has a .ida-exports JSON; both were found by scanning the
//                      image for the `lis rX, 0x82FC ; stfs/addi rX, @l` pair (see the wave report).
//                      Reading either as its image value (0) would have turned the roll arm into
//                      "any tilt at all against any non-downward surface is a crash".
// =================================================================================================

#include "GameSource/Physics/VehicleManager/BrnVehicleManager.h"
#include "GameSource/Physics/VehicleManager/BrnVehicleManagerDebugComponent.h"
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleOutputInterface.h"
#include "GameSource/Physics/VehicleManager/VehiclePhysics/RaceCarPhysics.h"
#include "GameSource/Physics/VehicleManager/VehiclePhysics/BrnSimpleVehiclePhysics.h"
#include "GameShared/GameClasses/SceneManager/SharedIO/CgsPotentialContact.h"
#include "GameShared/GameClasses/Geometric/Primitives/CgsAxisAlignedBox.h"
#include "GameSource/World/BrnEntityTypes.h"                                       // BrnWorld::E_ENTITYTYPE_*
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // [world-crash] diag (opt-in)
#include <cstdlib>   // getenv (BRN_WORLD_CRASH_DIAG)
#include <cmath>     // sqrtf / fabsf

namespace BrnPhysics
{
namespace Vehicle
{
namespace
{
    // flt_82002138 -- the unit-normal tolerance shared with the traffic arm's IsUnitLength.
    const f32 KF_UNIT_NORMAL_TOLERANCE          = 0.00999999978f;

    // The four function-local statics (dword_82FBA320 bits 1/2/4/8).
    const f32 KF_HEAD_ON_COSINE_STOPPED         = -0.699999988f;   // flt_8200D5FC  (unk_82FBA310)
    const f32 KF_HEAD_ON_COSINE_AT_SPEED        = -0.529999971f;   // flt_8209C7F0  (unk_82FBA300)
    const f32 KF_HEAD_ON_SPEED_RANGE            = 200.0f;          // flt_8201A1F0  (unk_82FBA2F0)
    const f32 KF_FRONT_FACE_DEPTH               = 0.300000012f;    // flt_82004740  (unk_82FBA2E0)

    // The two dyn-init'd .data constants (see the banner for the thunks that write them).
    const f32 KF_ROLLED_CRASH_UP_COSINE         = 0.173648178f;    // flt_82FB914C == cosf(80 deg)
    const f32 KF_ROLLED_CRASH_MIN_NORMAL_Y      = 0.100000001f;    // unk_82FB82B0 == splat(flt_82004014)

    // vcsxwfp128 v123, v13(1), 1 == 1 * 2^-1: the corner-clip half-width the player test uses.
    const f32 KF_CORNER_CLIP_WIDTH              = 0.5f;

    // flt_82005450 -- the "this normal is the ground" cosine PredictCarWorldContactTime tests.
    const f32 KF_GROUND_NORMAL_UP_COSINE        = 0.899999976f;

    // Opt-in witness for the classification below (same shape as BRN_TRAFFIC_DIAG next door).
    bool WorldCrashDiagEnabled()
    {
        static const bool sbEnabled = (getenv("BRN_WORLD_CRASH_DIAG") != 0);
        return sbEnabled;
    }

    inline u32 EntityWordOf(const CgsSceneManager::VolumeInstanceId& lrId)
    {
        return static_cast<u32>(lrId.muId >> 32);
    }

    inline u16 EntityIndexOfWord(u32 luEntityWord)
    {
        return static_cast<u16>((luEntityWord >> 10) & 0x3FFFu);
    }

    // 0x8263E484..0x8263E51C: |n|^2 -> vrsqrtefp + two Newton steps -> |n| (vsel'd to 0 when
    // |n|^2 == 0) -> | |n| - 1 | > 0.01. The same shape the traffic arm reproduces.
    inline bool IsUnitLength(const Vector3& lrV)
    {
        const f32 lfLengthSq = rw::math::vpu::Dot(lrV, lrV);
        const f32 lfLength   = (lfLengthSq == 0.0f) ? 0.0f : lfLengthSq / sqrtf(lfLengthSq);
        return !(fabsf(lfLength - 1.0f) > KF_UNIT_NORMAL_TOLERANCE);
    }

    // 0x8263E70C..0x8263E75C: a world point expressed in the car's frame -- (p - pos) dotted
    // onto the three rotation rows, assembled with vrlimi128 8/4/2 into x/y/z.
    inline Vector3 ToCarFrame(const Matrix44Affine& lrTransform, const Vector3& lrWorldPoint)
    {
        const Vector3 lvDelta = lrWorldPoint - lrTransform.wAxis;
        return Vector3{ rw::math::vpu::Dot(lvDelta, lrTransform.xAxis),
                        rw::math::vpu::Dot(lvDelta, lrTransform.yAxis),
                        rw::math::vpu::Dot(lvDelta, lrTransform.zAxis),
                        0.0f };
    }
}

// -------------------------------------------------------------------------------------------
// HandleRaceCarWorldPotentialContact  @0x8263E3B8 (566)  -- DWARF BrnVehicleManager.h:1200
//
// Called by HandleCrashPredictionForRaceCarAndWorld for every impact-time-ordered, validated
// race-car-vs-world potential contact. Decides whether THIS contact is a crash -- a head-on into
// the front face, a hard side/other impact above the axle line, or landing on the world while
// rolled past 80 degrees -- and, if so, commits it through the universal sink SetRaceCarCrashing
// and latches the player's slow-motion inhibit for a corner clip.
// -------------------------------------------------------------------------------------------
void VehicleManager::HandleRaceCarWorldPotentialContact(
    CgsSceneManager::SceneManagerIO::PotentialContact lContact,
    BrnPhysics::Vehicle::VehicleOutputRequestInterface* lpRequestOutputInterface,
    BrnPhysics::Vehicle::VehicleOutputInterface* lpVehicleOutputInterface,
    VehicleManagerOutputInterface* lpManagerOutputInterface,
    BrnPhysics::Deformation::DeformationInputInterface* lpDeformationInterface,
    const CgsSceneManager::SceneManagerIO::TriangleCacheInterface* lpTriangleCacheInterface,
    f32 lfTimestep)
{
    // arg_94 is never loaded by the console body (see the banner).
    (void)lpTriangleCacheInterface;

    // 0x8263E3F0 / 0x8263E438 -- the two owner bytes of the packed ids (BrnVehicleManager.cpp:5963/:5964).
    const u32 luRaceCarEntityWord = EntityWordOf(lContact.muVolumeInstanceIdA);   // r25
    const u32 luWorldEntityWord   = EntityWordOf(lContact.muVolumeInstanceIdB);   // r24

    CGS_ASSERT((luRaceCarEntityWord >> 24) == static_cast<u32>(BrnWorld::E_ENTITYTYPE_RACECAR),
               "lContact.muVolumeInstanceIdA.GetEntityId().GetOwner() == BrnWorld::E_ENTITYTYPE_RACECAR");  // :5963
    CGS_ASSERT((luWorldEntityWord >> 24) == static_cast<u32>(BrnWorld::E_ENTITYTYPE_WORLD),
               "lContact.muVolumeInstanceIdB.GetEntityId().GetOwner() == BrnWorld::E_ENTITYTYPE_WORLD");    // :5964

    // 0x8263E460..0x8263E5A0 (:5965). The console streams the offending normal after the prefix;
    // message-buffer streaming is lowered to the static prefix per the standing rule.
    const Vector3 lContactNormal = lContact.mNormal;   // v125 (spilled to var_120 around the asserts)
    CGS_ASSERT(IsUnitLength(lContactNormal), "Bad normal in HandleRaceCarWorldPotentialContact: ");

    // 0x8263E5AC / 0x8263E5DC -- the same two owner tests again on the extracted words (:5976/:5977).
    CGS_ASSERT((luRaceCarEntityWord >> 24) == static_cast<u32>(BrnWorld::E_ENTITYTYPE_RACECAR),
               "lRaceCarEntityID.GetOwner() == BrnWorld::E_ENTITYTYPE_RACECAR");   // :5976
    CGS_ASSERT((luWorldEntityWord >> 24) == static_cast<u32>(BrnWorld::E_ENTITYTYPE_WORLD),
               "lWorldEntityID.GetOwner() == BrnWorld::E_ENTITYTYPE_WORLD");       // :5977

    // 0x8263E60C..0x8263E620: `extrwi r4, r25, 14,8 ; lwzx maeRaceCarTypes[r4] ; cmpwi 2`.
    // A NETWORK car's world contacts are never crash-predicted locally.
    const u16 lu16RaceCarIndex = EntityIndexOfWord(luRaceCarEntityWord);   // r4
    if (maeRaceCarTypes[lu16RaceCarIndex] == BrnWorld::E_RACE_CAR_TYPE_NETWORK)
        return;

    // r5 = this + 5216 * idx ; the record itself sits at r5 + 0x740 == maRaceCarVehicles[idx].
    const RaceCarPhysics& lrRaceCar     = maRaceCarVehicles[lu16RaceCarIndex];
    const Matrix44Affine  lTransform    = lrRaceCar.GetTransform();          // rec +0x10..+0x40
    const Vector3         lLinearVelocity  = lrRaceCar.GetLinearVelocity();  // rec +0x50
    const Vector3         lAngularVelocity = lrRaceCar.GetAngularVelocity(); // rec +0x60

    // ---- (1) 0x8263E624..0x8263E6E0: will the race-car point still be on the world side of the
    //      plane one timestep from now?  vP = w x (pA - pos) + v ; (pA + vP*dt - pB) . n > 0 => it
    //      is moving APART, and there is nothing to do.
    const Vector3 lvRadius        = lContact.mPointOnA - lTransform.wAxis;                     // v13
    const Vector3 lvPointVelocity = rw::math::vpu::Cross(lAngularVelocity, lvRadius) + lLinearVelocity;   // v7
    const Vector3 lvPredictedPoint = lvPointVelocity * lfTimestep + lContact.mPointOnA;        // vmaddfp v0,v7,v10,v12
    if (rw::math::vpu::Dot(lvPredictedPoint - lContact.mPointOnB, lContactNormal) > 0.0f)     // flt_82001CC0
        return;

    // ---- 0x8263E6E4..0x8263E760: both contact points in the car's frame.
    //      v9 (-> var_110) = the WORLD point in car space; v122 = the RACE-CAR point in car space.
    const Vector3 lvWorldPointLocal   = ToCarFrame(lTransform, lContact.mPointOnB);   // v9
    const Vector3 lvRaceCarPointLocal = ToCarFrame(lTransform, lContact.mPointOnA);   // v122

    // ---- 0x8263E81C..0x8263E870: the head-on cone widens with forward speed.
    //      threshold = -0.70 + (-0.53 - -0.70) * clamp(v.forward, 0, 200) / 200 ;
    //      lbHeadOn  = threshold > n . forward   (the normal faces OUT of the world, so a wall
    //      square in front of the car gives n . forward == -1).
    f32 lfForwardSpeed = rw::math::vpu::Dot(lLinearVelocity, lTransform.zAxis);   // v0 = vel . zAxis
    if (lfForwardSpeed < 0.0f)                    lfForwardSpeed = 0.0f;           // vmaxfp128 v0, 0, v0
    if (lfForwardSpeed > KF_HEAD_ON_SPEED_RANGE)  lfForwardSpeed = KF_HEAD_ON_SPEED_RANGE;   // vminfp v13, 200, v0
    const f32 lfHeadOnCosine =
        (lfForwardSpeed / KF_HEAD_ON_SPEED_RANGE) * (KF_HEAD_ON_COSINE_AT_SPEED - KF_HEAD_ON_COSINE_STOPPED)
        + KF_HEAD_ON_COSINE_STOPPED;                                                // vmaddfp v0, v0, v11, v10
    const bool lbHeadOn = lfHeadOnCosine > rw::math::vpu::Dot(lContactNormal, lTransform.zAxis);   // r8

    // ---- 0x8263E878..0x8263E8A8: the deformable box, copied to the stack (rec +0x6D0, 32 bytes).
    const CgsGeometric::AxisAlignedBox& lrBox = lrRaceCar.GetDeformableAABB();

    // ---- 0x8263E8E4..0x8263E970: is the WORLD point on the car's front face?  Within the box's
    //      width, and within 0.3 of its front plane.
    const bool lbOnFrontFace =
           lvWorldPointLocal.x >= lrBox.mMin.x                          // vcmpgefp. v13(Bx), v10(min.x)
        && lrBox.mMax.x >= lvWorldPointLocal.x                          // vcmpgefp. v6(max.x), v13
        && lvWorldPointLocal.z >= lrBox.mMax.z - KF_FRONT_FACE_DEPTH;   // vcmpgefp. v5(Bz), max.z - 0.3

    // 0x8263E974..0x8263E990: `lfsx f0, 4*(idx + 0xA7C0), this` == mafVulnerabilityFactor[idx].
    const f32 lfVulnerabilityFactor = mafVulnerabilityFactor[lu16RaceCarIndex];

    // ---- 0x8263E99C..0x8263EA1C: HEAD-ON. Front face, inside the cone, and the closing speed
    //      along the normal (scaled by the car's vulnerability) at or above the head-on threshold.
    bool lbHeadOnCrash = false;                                                     // r8 (re-used)
    if (lbOnFrontFace && lbHeadOn)
    {
        const f32 lfClosingSpeed = rw::math::vpu::Dot(rw::math::vpu::Negate(lContactNormal), lLinearVelocity);
        lbHeadOnCrash = (lfClosingSpeed * lfVulnerabilityFactor) >= mfHeadOnWorldCrashThreshold;   // +172212
    }

    // ---- 0x8263EA20..0x8263EAE0: SIDE-ON. Only when the world point is NOT on the front face:
    //      the POINT closing speed (scaled) at or above the side threshold, and the world point
    //      above the car's origin (localB.y > 0) -- a kerb scrape below the axle line is not a crash.
    bool lbSideOnCrash = false;                                                     // r9
    if (!lbOnFrontFace)
    {
        const f32 lfPointClosingSpeed =
            rw::math::vpu::Dot(rw::math::vpu::Negate(lContactNormal), lvPointVelocity);   // vmsum3fp128 v7, v6, v7
        if ((lfPointClosingSpeed * lfVulnerabilityFactor) >= mfSideOnWorldCrashThreshold)  // +172216
        {
            lbSideOnCrash = lvWorldPointLocal.y > 0.0f;                             // flt_82001CC0 (f13)
        }
    }

    // ---- 0x8263EAE4..0x8263EB64: ROLLED. The car's up axis has tipped past 80 degrees from world
    //      up, and the surface it is touching faces upward (n.y > 0.1) -- it is landing on the world.
    bool lbRolledOntoWorld = false;                                                 // r10
    {
        const Vector3 lvWorldUp = Vector3{ 0.0f, 1.0f, 0.0f, 0.0f };               // unk_82181510
        if (KF_ROLLED_CRASH_UP_COSINE > rw::math::vpu::Dot(lTransform.yAxis, lvWorldUp))   // flt_82FB914C > up.y
        {
            lbRolledOntoWorld = lContactNormal.y > KF_ROLLED_CRASH_MIN_NORMAL_Y;    // vspltw128 v9, v125, 1 ; > unk_82FB82B0
        }
    }

    // ---- [world-crash] the DECISION, both sides of every compare ------------------------------
    // DIAG. NOT IN THE X360 BINARY. Opt-in (BRN_WORLD_CRASH_DIAG=1). Prints every classified
    // contact that got past the separating test -- crash or not -- so a run can show WHY a wall
    // hit did or did not commit: the head-on cosine vs the normal's forward component, the two
    // scaled closing speeds vs their thresholds, the world point in the car's frame, the roll
    // cosine. Budget: one line per validated contact per frame; a wall hit is a handful of
    // lines, a scrape along a barrier can be tens. DELETE-WHEN-STABLE.
    if (WorldCrashDiagEnabled() && CgsDev::Log::gpDebugPrint != 0)
    {
        const f32 lfNormalForward = rw::math::vpu::Dot(lContactNormal, lTransform.zAxis);
        const f32 lfUpY           = rw::math::vpu::Dot(lTransform.yAxis, Vector3{ 0.0f, 1.0f, 0.0f, 0.0f });
        const f32 lfClosingLin    = rw::math::vpu::Dot(rw::math::vpu::Negate(lContactNormal), lLinearVelocity);
        const f32 lfClosingPt     = rw::math::vpu::Dot(rw::math::vpu::Negate(lContactNormal), lvPointVelocity);
        *CgsDev::Log::gpDebugPrint
            << "[world-crash] car=" << static_cast<s32>(lu16RaceCarIndex)
            << " decision=" << (lbHeadOnCrash ? "HEAD_ON" : lbSideOnCrash ? "SIDE_ON" : lbRolledOntoWorld ? "ROLLED" : "NONE")
            << " front=" << (lbOnFrontFace ? 1 : 0) << " headOn=" << (lbHeadOn ? 1 : 0)
            << " nDotFwd=" << lfNormalForward << " vs cone=" << lfHeadOnCosine
            << " fwdSpeed=" << lfForwardSpeed
            << " closingLin*vf=" << (lfClosingLin * lfVulnerabilityFactor) << " vs headOnThr=" << mfHeadOnWorldCrashThreshold
            << " closingPt*vf=" << (lfClosingPt * lfVulnerabilityFactor) << " vs sideOnThr=" << mfSideOnWorldCrashThreshold
            << " vf=" << lfVulnerabilityFactor
            << " localB=(" << lvWorldPointLocal.x << "," << lvWorldPointLocal.y << "," << lvWorldPointLocal.z << ")"
            << " box.x=[" << lrBox.mMin.x << "," << lrBox.mMax.x << "] box.maxZ=" << lrBox.mMax.z
            << " upY=" << lfUpY << " vs rollCos=" << KF_ROLLED_CRASH_UP_COSINE
            << " n.y=" << lContactNormal.y << " vs " << KF_ROLLED_CRASH_MIN_NORMAL_Y
            << " crashing=" << (lrRaceCar.IsCrashing() ? 1 : 0)
            << " speedMPH=" << (lfForwardSpeed * 2.2369363f)
            << "\n";
    }

    // 0x8263EB68..0x8263EB88: none of the three fired -> not a crash.
    if (!lbHeadOnCrash && !lbSideOnCrash && !lbRolledOntoWorld)
        return;

    // ---- 0x8263EB8C..0x8263EBE8: PLAYER CORNER CLIP. A head-on whose world point lies within 0.5
    //      of either side of the box, on the player's car.
    bool lbPlayerCornerClip = false;                                                // r11
    if (lbHeadOnCrash)
    {
        const f32 lfFromRightSide = lrBox.mMax.x - lvWorldPointLocal.x;             // vsubfp v0, max.x, Bx
        const f32 lfFromLeftSide  = lvWorldPointLocal.x - lrBox.mMin.x;             // vsubfp v12, Bx, min.x
        const f32 lfFromNearestSide = (lfFromRightSide < lfFromLeftSide) ? lfFromRightSide : lfFromLeftSide;   // vminfp
        if (KF_CORNER_CLIP_WIDTH > lfFromNearestSide                                // vcmpgtfp128. v123(0.5), v0
            && static_cast<u32>(mePlayerActiveRaceCarIndex) == lu16RaceCarIndex)   // lwzx this+0x2A0AC ; cmpw r4
        {
            lbPlayerCornerClip = true;
        }
    }

    // ---- 0x8263EBEC..0x8263EC2C: the slow-motion inhibit this crash will leave behind. A fresh
    //      corner clip inhibits; a re-hit while already crashing keeps whatever was latched, and
    //      only if this contact is a corner clip too.
    bool lbForceNoSlowMo = false;                                                   // r29
    if (!lrRaceCar.IsCrashing() && lbPlayerCornerClip)                              // lbz rec+0x710
    {
        lbForceNoSlowMo = true;
    }
    if (lrRaceCar.IsCrashing())
    {
        lbForceNoSlowMo = lbPlayerCornerClip ? mbForceNoSlowMo : false;            // lbzx this+0x2A11D
    }

    // 0x8263EC30: the debug snapshot (this + 161968 == mDebugComponent, &arg_20 == the by-value copy).
    mDebugComponent.RecordCrashContact(&lContact);

    // 0x8263EC40..0x8263EC74: THE COMMIT. Victim = the race car's entity word, aggressor = the world
    // entity word, normal SIGN-FLIPPED towards the victim (`vslw v13,v124,v124 ; vxor v1, v0, v13`),
    // point = v122 == the race-car contact point IN THE CAR'S FRAME (not the world point -- the
    // console passes the local vector it built at 0x8263E75C), takedown type -1 (NONE).
    ::EntityId lVictimID;
    lVictimID.muValue = luRaceCarEntityWord;
    ::EntityId lAggressorID;
    lAggressorID.muValue = luWorldEntityWord;

    SetRaceCarCrashing(lVictimID, lAggressorID,
                       rw::math::vpu::Negate(lContactNormal), lvRaceCarPointLocal,
                       lpRequestOutputInterface,
                       lpManagerOutputInterface,
                       lpVehicleOutputInterface,
                       lpDeformationInterface,
                       BrnGameState::E_TAKEDOWN_NONE);

    mbForceNoSlowMo = lbForceNoSlowMo;   // stbx r29, this, 0x2A11D
}

// -------------------------------------------------------------------------------------------
// PredictCarWorldContactTime  @0x825B5300 (83)  -- DWARF BrnVehicleManager.h:1215
//
// The impact-time key HandleCrashPredictionForRaceCarAndWorld orders each car's world contacts by:
//     t = (pA - pB) . n  /  v . (-n)
// i.e. the separation along the normal over the closing speed along it, plus a whole second when
// the normal is within ~26 degrees of the car's own up axis (n . up > 0.9 -- a ground-like contact
// is pushed to the back of the queue). The division is the console's vrefp + two Newton steps,
// de-optimised to an exact divide; the result is splatted to all four lanes, and the driver reads
// lane .x.
// -------------------------------------------------------------------------------------------
VecFloat VehicleManager::PredictCarWorldContactTime(const CgsSceneManager::SceneManagerIO::PotentialContact& lContact)
{
    const u32 luRaceCarEntityWord = EntityWordOf(lContact.muVolumeInstanceIdA);
    const u32 luWorldEntityWord   = EntityWordOf(lContact.muVolumeInstanceIdB);

    CGS_ASSERT((luRaceCarEntityWord >> 24) == static_cast<u32>(BrnWorld::E_ENTITYTYPE_RACECAR),
               "lContact.muVolumeInstanceIdA.GetEntityId().GetOwner() == BrnWorld::E_ENTITYTYPE_RACECAR");  // :2839
    CGS_ASSERT((luWorldEntityWord >> 24) == static_cast<u32>(BrnWorld::E_ENTITYTYPE_WORLD),
               "lContact.muVolumeInstanceIdB.GetEntityId().GetOwner() == BrnWorld::E_ENTITYTYPE_WORLD");    // :2840

    // 0x825B53A0 / 0x825B53E4: `extrwi r9, r9, 14,8 ; mulli r11, r9, 0x1460 ; add r11, r11, this`.
    const u16 lu16RaceCarIndex = EntityIndexOfWord(luRaceCarEntityWord);
    const RaceCarPhysics& lrRaceCar = maRaceCarVehicles[lu16RaceCarIndex];

    const Vector3 lLinearVelocity = lrRaceCar.GetLinearVelocity();        // lvx128 v9, r11, 0x790 (rec +0x50)
    const Vector3 lUpAxis         = lrRaceCar.GetTransform().yAxis;       // lvx128 v9, r11, 0x760 (rec +0x20)

    const f32 lfSeparation   = rw::math::vpu::Dot(lContact.mPointOnA - lContact.mPointOnB, lContact.mNormal);   // v11
    const f32 lfClosingSpeed = rw::math::vpu::Dot(lLinearVelocity, rw::math::vpu::Negate(lContact.mNormal));   // v13
    const f32 lfNormalUp     = rw::math::vpu::Dot(lContact.mNormal, lUpAxis);                                   // v0

    f32 lfImpactTime = lfSeparation / lfClosingSpeed;                     // vrefp + 2x NR, then vmulfp128
    if (lfNormalUp > KF_GROUND_NORMAL_UP_COSINE)                          // vcmpgtfp. v0, v0, splat(0.9)
    {
        lfImpactTime += 1.0f;                                             // vaddfp v0, v0, v12 (vcfsx 1)
    }

    return VecFloat{ lfImpactTime, lfImpactTime, lfImpactTime, lfImpactTime };
}

}   // namespace Vehicle
}   // namespace BrnPhysics
