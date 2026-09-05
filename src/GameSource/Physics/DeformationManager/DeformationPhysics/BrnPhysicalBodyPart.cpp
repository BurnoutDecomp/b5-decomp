#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnPhysicalBodyPart.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // gpDebugPrint (walls leg 4 gates)

#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnIKBodyPart.h"           // IKBodyPart, GetActiveJointSpec/Index, CheckSensorForcesForJointDetachment
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnDeformableObject.h"      // DeformableObject (GetVehicleBody / transform delta path)
#include "GameSource/Physics/VehicleManager/VehiclePhysics/VehiclePhysics.h"                   // VehiclePhysics::GetTransformDelta
#include "SharedClasses/Physics/Deformation/BrnIKBodyPartSpec.h"                               // IKBodyPartSpec
#include "SharedClasses/Physics/Deformation/BrnBodyPartBBoxSpec.h"                             // BodyPartBBoxSpec / BBoxPointSkinData (CalculateSkinnedPoint's argument + the ten control points)
#include "SharedClasses/Physics/Deformation/BrnDeformationJointSpec.h"                         // DeformationJointSpec::GetMaxStress/GetMaxAngle/...
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_SceneUpdate.h"                 // InSceneUpdateInterface remove/set producers
#include "GameShared/GameClasses/SceneManager/CgsEntityId.h"                                   // CgsSceneManager::EntityId
#include "GameShared/GameClasses/Core/CgsAssert.h"                                             // CGS_ASSERT
#include "rw/math/vpu/vector3_operation.h"                                                     // rw::math::vpu::IsValid (UpdateRW finiteness tripwires)
#include "GameShared/GameClasses/Physics/CgsPhysicsSimulationModuleIO.h"                       // InputBuffer::GetAddRigidBodyQueue (AddToSim)
#include "GameShared/GameClasses/Physics/CgsPhysicsSimulationIO_Events.h"                      // InAddRigidBody / NewRigidBody / OutUpdateRigidBody
#include "rw/physics/rigidbody.h"                                                              // rw::physics::ACTIVE_BODY / FROZEN_BODY, RigidBody::GetTransform
#include "rw/physics/inertia.h"                                                                // Inertia setters + ComputeFatBoxInertia
#include "GameSource/Physics/BrnPhysicsModuleIO.h"                                             // PhysicsModuleIO::OutputBuffer::GetDeformationOutputInterface
#include "GameSource/Physics/DeformationManager/SharedIO/BrnDeformationOutputInterface.h"          // DeformationOutputInterface::mDetachedPartNotificationQueue
#include "GameSource/Physics/DeformationManager/SharedIO/BrnDeformationEvents.h"                    // DetachedPartNotificationEvent

#include <cstring>   // memset (matching the X360 memset of the BBox scratch tail)
#include <cmath>     // std::sqrt / std::fabs (the vrsqrtefp magnitude refinements + the skin self-check)
#include <cstdint>   // uintptr_t (the console's 16-byte alignment assert on the skin record)
#include <cstdlib>   // getenv/atoi -- [DIAG] BRN_DEFORM_TRACE only, host-side

// ============================================================================
// GameSource/Physics/DeformationManager/DeformationPhysics/BrnPhysicalBodyPart.cpp
//
// BrnPhysics::Deformation::PhysicalBodyPart -- the PHYSICS side of one breakable car
// panel. Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX. This file owns the
// part lifecycle (Construct/Prepare), scene IO (RemoveFromScene/SetRigidBodyTransform/
// Update), the oriented-bounding-box build (CalcBoundingBox / CalculateAABBExtents /
// CalculateBoundingBoxExtents / UpdateBoundingBox / GetBoundingBox), the velocity caps
// (LimitVelocities), the per-frame contact accumulation (AddContact), the joint model
// (SetJoinedToVehicle / UpdateJoint / GetJointRotationProportion), and the joint-break
// test (TestJointForBreaking) -- the deformation-physics core of the wave.
//
//   Construct                    @ 0x825B4178
//   GetBoundingBox               @ 0x825E7D28
//   GetJointRotationProportion   @ 0x825C1B38
//   Prepare                      @ 0x82626700
//   RemoveFromScene              @ 0x825E7818
//   SetRigidBodyTransform        @ 0x825E7778
//   Update                       @ 0x825E78C8
//   SetJoinedToVehicle           @ 0x825BA4A8
//   LimitVelocities              @ 0x825BA5C0
//   AddContact                   @ 0x825E2FC8
//   CalcBoundingBox              @ 0x8260ABB0
//   CalculateAABBExtents         @ 0x825E2EA0
//   CalculateBoundingBoxExtents  @ 0x825E2B80
//   UpdateBoundingBox            @ 0x8260ACC8
//   UpdateJoint                  @ 0x8260B0F8
//   TestJointForBreaking         @ 0x8260C0F8
//
// MODELLED-vs-asm conventions (same as the committed sibling slices
// BrnStreamedDeformationSpec.cpp / BrnAbsorptionTable.cpp / BrnDeformationSensor.cpp):
//
//  * VMX128 vector math is modelled lane-by-lane in scalar f32. A `vspltw v,v,N` broadcast
//    is modelled by reading lane N and replicating it; a `vmsum3fp128` is the xyz dot
//    product; a `vmaddfp` is a fused multiply-add per lane; `vrlimi128 v,w,1,0` preserves
//    the w lane while replacing xyz (the Vector3Plus "Plus" lane). The packed Vector3Plus
//    members carry their scalar in the w lane exactly as the asm does.
//
//  * Asserts are NON-GATING tripwires: a failed CGS_ASSERT runs Begin/Fire/End and
//    execution CONTINUES past it, exactly as the X360 BeginAssert/FireAssert/EndAssert
//    triple does (no early-out). The assert message strings are the asm's FireAssert
//    strings (source paths/line numbers stripped per project rule).
//
//  * ⭐⭐ 2026-08-27 (detach wave): SIX of the "not in the exports" SIMD constants are RECOVERED --
//    kfJointForceMultiplier (0.4), kfJointPenetrationMultiplier (1.5), &unk_82FB96E0 min-bbox extent
//    (0.1), &unk_82FB9DD0 min-bbox HALF (0.05), &unk_82FB9B00 KV_BIG_VECTOR (100000) and (earlier)
//    &unk_82FB9E00 (0.3). They were never rodata CONSTANTS: each is a runtime-initialised VMX splat
//    written by its own tiny initialiser block in 0x82C5D700-0x82C5DE00, so an image dump of the
//    static reads zero and the value only exists after the boot initialisers run. THAT is why sweep
//    after sweep recorded them "unrecoverable" -- the method, not the data, was missing.
//  * FLAGGED-0 / placeholder rodata that IS still outstanding: &unk_82FB9AC0 / &unk_82FB95F0 /
//    &unk_82FB9D60 / &unk_82FB9E30 joint integration tuning, and the chebyshev/atan polynomial
//    tables &unk_82000BD0.. They are NOT in the per-function exports. Per project rule they are carried as correctly-shaped,
//    clearly-labelled placeholders (honest zeros / best-guess) -- NEVER fabricated numbers.
//    The control flow (which lane, which compare, which store, which call) is exact; the
//    numeric output of the gated tuning stays inert until the real rodata is recovered.
//
//  * Console member byte offsets the asm indexes (this+0..this+488) map onto the frozen
//    header members BY NAME as follows (cross-checked against the asm of every function in
//    this TU and the Construct/Prepare zero-init stores):
//       +0   mRwBody (ExternalPhysicsBody, embeds the +16 mTransform rows the asm reads as
//            the rigid-body transform; +64 linear vel, +80 angular vel, +208 mfMass=5.0)
//       +288 mBBoxOrientation (Matrix44Affine: rows +288/+304/+320/+336)
//       +352 mLocalJointPositionPlusRotation
//       +368 mLocalGraphicsPositionPlusJointVelocity
//       +384 mLocalInitialComPositionPlusMaxJointAngle
//       +400 mLocalInitialJointPositionPlusLimitStress
//       +416 mBoundingBoxHalfDimensions
//       +432 mWorldPenetrationPlusCollisionMagnitude
//       +448 mAverageCollisionPointPlusNumCollisions
//       +464 mRigidBodyId (8 bytes)   +472 mGlobalVehicleId   +476 mpIKPart
//       +480 mpDeformableObject       +484 mbJoinedToVehicle  +485 mbAddedToScene
//       +486 mbFrozen                 +487 mbNeedsWritingIntoRenderware
//       +488 mi8ActiveJointsTagPointIndex
//    Because every access here is BY MEMBER NAME, the 64-bit host recomputes the addresses
//    and semantic parity (which member, which lane) is exact without reproducing the
//    console offsets as padding.
// ============================================================================

namespace BrnPhysics
{
namespace Deformation
{
    // ----- maBoxInitialiseBuffer MOVED OUT 2026-08-27 (detached-part collision wave) --------------
    // The DWARF static (BrnPhysicalBodyPart.h:297, `extern char[96]`, X360 0x82FB7C30) used to sit
    // here, zero-filled and DEAD, with a banner calling it "transient working space ... its concrete
    // seed contents are not in the exports". Both halves of that were wrong: it is not scratch and
    // it has no seed contents. It is the memory block `rw::collision::BoxVolume` is placement-new'd
    // into by PhysicalBodyPart::AddToScene @0x8260A938 -- its ONLY user in the whole X360 export set
    // -- so it now lives in AddToScene's TU, BrnPhysicalBodyPart_Remove.cpp, where its size, its
    // alignment and the consumer's 128-byte over-read are all documented and measured.

    // ----- part rigid-body tuning (DWARF BrnPhysicalBodyPart.cpp:28-33) ---------------------------
    // ⭐⭐⭐ ALL SIX RECOVERED 2026-08-27 (detach-2 wave). The banner that stood here said they were
    // "not in the exports and are carried as honest zeros". They were never rodata-invisible at all:
    // they are a contiguous run of PLAIN .data floats at 0x82F2A370..0x82F2A384, and every one of
    // them reads its real value straight out of the image. What was missing was a reader that went
    // to the DATA section rather than to the pseudocode -- the same shape as the six VMX splats the
    // previous wave recovered. [[literal-scans-miss-real-stores]].
    //
    // Each address is the `lis rX,@ha ; lfs f0,@l(rX)` pair AddToSim @0x8260AD38 itself materialises,
    // decoded from the raw instruction words (not from the IDA label text):
    //     kfPartLinearDrag         0x82F2A370  @0x8260ADE4/EC  = 0.005
    //     kfPartAngularDrag        0x82F2A374  @0x8260ADDC/E0  = 0.005
    //     kfPartMaxLinearVelocity  0x82F2A378  @0x8260ADFC/AE04= 30.0
    //     kfPartMaxAngularVelocity 0x82F2A37C  @0x8260ADF0/F8  = 30.0
    //     kfPartMass               0x82F2A380  @0x8260AD6C/AE10= 100.0
    //     kfPartInertiaMultiplier  0x82F2A384  @0x8260AE80/84  = 1.2
    // CALIBRATION CONTROL, same decoder, same instruction shape, on a value already committed in
    // this tree: the pair at 0x8260AD68/0x8260AD84 resolves to 0x82001C98 = 1.0f, which
    // rw/physics/inertia.h:63 already carries as the byte-verified reciprocal numerator. And the
    // run is the same .data block that holds kbAllowRandomPartDetachment @0x82F2A344 (recovered
    // last wave), so the section reading is corroborated by an independent prior result.
    //
    // ⛔ AND THE CONFLATION THE OLD BANNER SHIPPED: "the Construct asm seeds mfMass = 5.0 ... which
    // is the only concrete value recovered" read the 5.0 as if it WERE kfPartMass. It is not.
    // Construct @0x825B4178 loads an UNNAMED rodata literal flt_8200426C (= 5.0) for
    // ExternalPhysicsBody::SetMass; kfPartMass is a DIFFERENT symbol at 0x82F2A380 holding 100.0,
    // and it is what AddToSim divides into 1.0 for the event's inverse mass. Two masses, both real:
    // the body carries 5.0, the sim is told 100.0. Neither is a stand-in for the other, and
    // BrnPhysicalBodyPart_Construct.cpp's own copy is renamed to say so.
    static const f32 KF_PART_LINEAR_DRAG          = 0.0049999999f; // RECOVERED 0x82F2A370
    static const f32 KF_PART_ANGULAR_DRAG         = 0.0049999999f; // RECOVERED 0x82F2A374
    static const f32 KF_PART_MAX_LINEAR_VELOCITY  = 30.0f;         // RECOVERED 0x82F2A378
    static const f32 KF_PART_MAX_ANGULAR_VELOCITY = 30.0f;         // RECOVERED 0x82F2A37C
    static const f32 KF_PART_MASS                 = 100.0f;        // RECOVERED 0x82F2A380 (NOT the 5.0)
    static const f32 KF_PART_INERTIA_MULTIPLIER   = 1.2f;          // RECOVERED 0x82F2A384

    // ----- AddToSim's own three numeric inputs ----------------------------------------------------
    // The fat-box rounding margin AddToSim hands ComputeFatBoxInertia: `lfs f4, flt_82001CC0`
    // @0x8260AE5C, byte-read = 0.0f. (A zero margin means the "fat" box degenerates to the plain
    // box -- the fat-box code path is still the one the console calls, and it is faithful to call it.)
    static const f32 KF_PART_FAT_BOX_MARGIN = 0.0f;                // RECOVERED flt_82001CC0

    // The additive inertia FLOOR of the vmaddfp at 0x8260AEE0 (`unk_82FB95B0`). ⭐ RECOVERED
    // 2026-08-27 by the same initialiser-scan the previous wave calibrated: it reads all-zero on
    // disk because it is a runtime VMX splat, and its block is at 0x82C5DBA8..0x82C5DBDC --
    //   lfs f0, flt_82004744 ; three stfs into -16(r1) ; stw 0 into the w lane ; lvx ; stvx 0x82FB95B0
    // with flt_82004744 = 0.2f. ⭐ CALIBRATION: flt_8200473C (= 0.4, kfJointForceMultiplier) and
    // flt_82004740 (= 0.30000001, KF_ROTATION_PROPORTION_GATE) sit in the SAME rodata run and both
    // match values this tree already carries; the very next initialiser block in the image
    // (0x82C5DBE0) materialises flt_820047C8 = 0.05, which FatBoxInertia.cpp already carries
    // byte-verified as KF_FivePct. Three independent controls on one reader.
    // ⛔ AND THE ZERO WAS NOT INERT. It is the ADDEND of `fatBox*mass*multiplier + floor`, and with
    // CalculateSkinnedPoint still a placeholder the fat-box term is tiny, so a zero floor makes the
    // inverse inertia enormous -- a shed panel would spin up violently on the first contact.
    // 0.2 is what keeps it bounded. FLAG on the NAME only (role-derived from the arithmetic); the
    // VALUE is measured.
    static const Vector4 KV_PART_INERTIA_FLOOR = { 0.2f, 0.2f, 0.2f, 0.0f };  // RECOVERED 0x82FB95B0

    // The "Bad inertia: " tripwire's threshold -- stru_8208F620 lane 0, byte-read = 1.1920929e-07
    // (FLT_EPSILON). The asm splats lane 0 and tests CR6 bit 2 (== none of the lanes is greater).
    static const f32 KF_INERTIA_DEGENERATE_EPSILON = 1.1920929e-07f;          // RECOVERED 0x8208F620

    namespace
    {
        // ----- min-bbox clamp vector + the two box magnitude bounds ---------------------------------
        // ⭐⭐⭐ ALL THREE RECOVERED 2026-08-27 (detach wave), by the same initialiser-scan method
        // that recovered the two joint multipliers above -- they are runtime-initialised VMX splats,
        // not rodata, which is why five sweeps recorded them "not in the exports". Each has the same
        // three-lane block shape (`lfs f0,<rodata>` ; three `stfs` into the -16(r1) scratch ; `stw 0`
        // into the w lane ; `lvx` ; `stvx <0x82FBxxxx>`), i.e. each is Vector3(v,v,v) with w == 0:
        //     0x82C5DC50  flt_82004014 = 0.1      -> 0x82FB96E0   KV_MIN_BBOX_SIZE
        //     0x82C5DBE0  flt_820047C8 = 0.05     -> 0x82FB9DD0   the CalcBoundingBox HALF-dim floor
        //     0x82C5DC18  flt_820080E8 = 100000.0 -> 0x82FB9B00   KV_BIG_VECTOR (seed + upper bound)
        // ⭐ THE READING SELF-CHECKS: CalcBoundingBox computes half = extent * 0.5 and floors it at
        // 0x82FB9DD0, while CalculateBoundingBoxExtents floors the FULL extent at 0x82FB96E0.
        // 0.05 is EXACTLY half of 0.1. Two independently-read statics that must be in a 2:1 ratio,
        // and are. (The 1.0e30 that stood in for KV_BIG_VECTOR.x was five orders too big.)
        //
        // ⛔ AND THE ZERO HERE WAS A LIVE DEFECT, not an inert default. [[placeholder-identity-element]]
        // again: this floor is applied with vmaxfp, and 0 IS NOT the identity of max for a quantity
        // that is itself 0. With CalculateSkinnedPoint still a documented placeholder returning the
        // origin, every corner reduces to 0, extent = 0, and `max(0, 0.0f)` left it 0 -- so the very
        // first part ever detached on this build fired
        //   "Magnitude(lBoundingBoxMax - lBoundingBoxMin) > 0.00001f"  (:693)
        // on the frame the detach path went live. With the real 0.1 floor the clamp does its job,
        // |extent| = sqrt(3*0.01) = 0.1732, and BOTH tripwires pass -- as they must on the console,
        // whose skinned points are real. The assert is NOT suppressed; it now measures something.
        const Vector4 KV_MIN_BBOX_SIZE = { 0.1f, 0.1f, 0.1f, 0.0f };        // RECOVERED 0x82FB96E0
        const Vector4 KV_MIN_BBOX_HALF_SIZE = { 0.05f, 0.05f, 0.05f, 0.0f };// RECOVERED 0x82FB9DD0

        // The asm asserts Magnitude(max-min) > 0.00001f and < KV_BIG_VECTOR.GetX().
        const f32 KF_MIN_BBOX_MAGNITUDE = 0.00001f;                 // recovered (v37[0])
        const f32 KF_BIG_VECTOR_X       = 100000.0f;                // RECOVERED 0x82FB9B00 <- flt_820080E8

        // ----- LimitVelocities curve constants (recovered from the function's fsel/immediate soup) --
        // The asm computes time-scaled linear/angular velocity caps from a flag-guarded one-shot
        // static init (flt_82FB9FB8/.B4). These literals are the recovered immediates.
        const f32 KF_MAX_LIN_VEL_AT_60  = 120.0f;
        const f32 KF_MAX_ANG_VEL_AT_60  = 100.0f;
        const f32 KF_MAX_LIN_VEL_AT_INF = 120.0f;   // upper assert bound (lfMaxLinVel <= this)
        const f32 KF_MAX_ANG_VEL_AT_INF = 100.0f;   // upper assert bound (lfMaxAngVel <= this)
        const f32 KF_PROPORTION_OF_MAX_TO_CAP_TO = 0.80000001f;   // the v55/v51 = 0.8 store

        // ----- joint-break tuning (TestJointForBreaking / UpdateJoint) ------------------------------
        // ⭐⭐⭐ RECOVERED 2026-08-27 (detach wave). kfJointForceMultiplier / kfJointPenetrationMultiplier
        // are NOT rodata constants -- they are runtime-initialised VMX splats, which is why they read
        // as zero in a straight image dump and why five prior sweeps recorded them "not recovered".
        // Each has its own tiny initialiser block that materialises a rodata float, splats it, and
        // stores the splat to the 0x82FBxxxx VMX static the break predicate lvx's:
        //     0x82C5DDA0  lfs f0, flt_8200473C (0.4f) -> vspltw -> stvx 0x82FB96F0   <- FORCE
        //     0x82C5DD78  lfs f0, flt_820945DC (1.5f) -> vspltw -> stvx 0x82FB95D0   <- PENETRATION
        // CALIBRATION CONTROL, same method, on a value this file ALREADY had:
        //     0x82C5DDC8  lfs f0, flt_82004740 (0.30000001) -> stvx 0x82FB9E00 == KF_ROTATION_PROPORTION_GATE.
        // WHICH-IS-WHICH is proven at the two USE sites, not assumed: the force arm materialises
        // 0x82FB96F0 at 0x8260C3D8-E4 (right after the vmsum3fp128 of the assembled joint force at
        // 0x8260C3D4), and the penetration arm materialises 0x82FB95D0 at 0x8260C438-48 (right beside
        // `addi r10, r31, 400` -- the this+0x190 penetration lane).
        // ⛔ [[placeholder-identity-element]] -- WHY THIS MATTERED: the predicate is
        // `magnitude * multiplier > maxStress`, so a placeholder 0 is NOT an inert default; 0 is not
        // the identity of `*`. `x * 0 > maxStress` is false for EVERY finite stress, i.e. the honest
        // zero was a hard kill switch: no joint could break at any energy. The old banner below called
        // that "a documented INERT behaviour-shift" -- it was the single biggest reason no part had
        // ever come off a car on this build.
        const f32 KF_JOINT_FORCE_MULTIPLIER       = 0.40000001f;   // RECOVERED: 0x82FB96F0 @82C5DDA0 <- flt_8200473C
        const f32 KF_JOINT_PENETRATION_MULTIPLIER = 1.5f;          // RECOVERED: 0x82FB95D0 @82C5DD78 <- flt_820945DC
        // STILL FLAGGED (same block, not recovered): the joint integration tuning at &unk_82FB9AC0 /
        // &unk_82FB95F0 / &unk_82FB9750 (0.975 recovered) / &unk_82FB9D60 / &unk_82FB9E30.
        const f32 KF_JOINT_RELAX                  = 0.97500002f;   // recovered (v58[0])
        const f32 KF_ROTATION_PROPORTION_GATE     = 0.300000012f;   // unk_82FB9E00 @82C5DDC8 <- flt_82004740

        // The active-joint default-direction "always detaches when bent past -0.9" early gate the
        // TestJointForBreaking asm checks: GetActiveJointSpec()+52 (mfJointDetachThreshold reinterpreted
        // by the asm at +52 in the streamed record) <= -0.89999998 -> skip the break test.
        const f32 KF_JOINT_DETACH_DISABLED_THRESHOLD = -0.89999998f;   // recovered (the -0.9 compare)

        // The active-joint-spec word the asm reaches at +0x14 / +0x20 inside the streamed joint record
        // (the rotation-axis lane source for the joint force vector). Reached through the
        // DeformationJointSpec accessors below; no raw-offset access needed.

        // Broadcast a scalar into all four lanes (the vspltw idiom).
        inline Vector4 Splat(f32 lfValue) { return Vector4{ lfValue, lfValue, lfValue, lfValue }; }

        // The skin-blend agreement tolerance CalculateSkinnedPoint's opt-vs-unopt self-check uses:
        // `lfs f0, flt_82002138` @0x825E2A48, byte-read from the image == 0x3C23D70A == 0.01f.
        const f32 KF_SKIN_BLEND_TOLERANCE = 0.0099999998f;   // RECOVERED flt_82002138

        // ------------------------------------------------------------------------------------------
        // One skin influence of a bounding-box control point, resolved to its CURRENT-minus-REST
        // offset. The single flat byte index selects a TAG point when it is below the part's tag-point
        // count and a DRIVEN point above it (re-indexed by subtracting that count) -- the branch at
        // 0x825E25D4 / 0x825E278C / 0x825E2874 / 0x825E2958, four identical copies from the loop plus
        // the three unrolled steps.
        //
        // Both point types already carry `current - rest` as GetOffsetFromInitialPosition(), which is
        // exactly the `vsubfp v0, v0, v13` the asm performs after loading the record's own position
        // (+0x00) and its spec's rest position (TagPointSpec +0x20 / IKDrivenPointSpec +0x00).
        // The accessors' own bounds asserts are the console's -- see BrnIKBodyPart.h.
        // ------------------------------------------------------------------------------------------
        inline Vector3 ResolveSkinBoneOffset(const IKBodyPart* lpIKPart, u8 lu8BoneIndex)
        {
            const s32 liIndex        = static_cast<s32>(lu8BoneIndex);
            const s32 liNumTagPoints = lpIKPart->GetNumberOfTagPoints();

            if ( liIndex < liNumTagPoints )
            {
                return lpIKPart->GetTagPoint(liIndex)->GetOffsetFromInitialPosition();
            }
            return lpIKPart->GetDrivenPoint(liIndex - liNumTagPoints)->GetOffsetFromInitialPosition();
        }

        // [DIAG] NOT IN THE X360 BINARY. The same BRN_DEFORM_TRACE latch BrnDeformableObject_Detach.cpp
        // uses, duplicated here (file-local, no ODR surface) so the sim-entry witness is on the SAME
        // switch as the [detach-gate]/[detach-make]/[detach-part] lines it has to be read alongside.
        // DELETE-WHEN the detach question is closed and banked.
        inline bool DetachProbeOn()
        {
            static s32 siProbe = -1;
            if ( siProbe < 0 )
            {
                const char* lpcEnv = getenv("BRN_DEFORM_TRACE");
                siProbe = (lpcEnv != 0 && atoi(lpcEnv) > 0) ? 1 : 0;
            }
            return (siProbe == 1) && (CgsDev::Log::gpDebugPrint != 0);
        }
    }
    // ==========================================================================================
    // PhysicalBodyPart::Construct @0x825B4178 MOVED OUT on 2026-08-03 (task #116) to
    // BrnPhysicalBodyPart_Construct.cpp, verbatim. WHY: PhysicsModule::Construct @0x825AE308 was
    // a live empty stub; un-stubbing it reaches DetachedPartManager::Construct ->
    // PhysicalBodyPartPool::Construct -> this, 50 times.
    // ⛔ STALE BANNER, CORRECTED 2026-08-27: the text below ("THIS TU cannot be mounted") describes
    // the state on 2026-08-03. THIS TU IS MOUNTED (build_game_exe.bat: BrnPhysicalBodyPart.cpp) and
    // has been since the deformation-mount wave; the 16 unresolved externals were closed. The
    // re-merge note is kept for the history of WHY Construct lives next door.
    // (historic) a MEASURED trial link (task #116, M2) put it at 16 unresolved externals from TestJointForBreaking /
    // RemoveFromScene / SetJoinedToVehicle / UpdateJoint / UpdateRW / AddContact. Exactly ONE of
    // the 16 was referenced from Construct -- ExternalPhysicsBody::SetMass, which was declare-only
    // everywhere and is now bodied in its own home, ExternalPhysicsBody.cpp (already mounted).
    // TO RE-MERGE: close the other 15, mount this TU, move the body back.
    // ==========================================================================================


    // =========================================================================================
    // Prepare @ 0x82626700
    //
    // Bind the part to its vehicle + IK spec and build the local joint/graphics/COM frames.
    //   this+464 = lPartId (the 8-byte BurnoutBodyPartID); +472 = lGlobalVehicleId;
    //   this+488 = -1 (mi8ActiveJointsTagPointIndex none); +480 = lpDeformableObject;
    //   +476 = lpIKPart; +485 = 0 (mbAddedToScene); +484 = 0 (mbJoinedToVehicle).
    // Construct the embedded body; +487 = 0 (mbNeedsWritingIntoRenderware).
    //
    // ⭐⭐⭐ 2026-09-05 (hinge-geometry wave): THE SECOND MATRIX PARAMETER IS THE VEHICLE'S WORLD
    // TRANSFORM, NOT A BBOX ORIENTATION, AND THAT MISREADING IS WHY A SHED OR HINGED PANEL WAS
    // POSED AT THE WORLD ORIGIN. The body below used to spell `mBBoxOrientation = lBBoxOrientation`
    // and `mRwBody.SetTransform(lBBoxOrientation)`, which forced the ONE call site
    // (DeformableObject::DetachPart) to hand it IDENTITY -- because handing it the real vehicle
    // world transform then injected the car's world position into mBBoxOrientation, which
    // CalcBoundingBox uses as a LOCAL frame. Both halves are decoded now:
    //
    //   (a) mBBoxOrientation is built ENTIRELY from the IK spec, from no argument at all --
    //       0x8262675C..0x826267DC:
    //         lwz r11,0x1DC(this) ; lwz r11,8(r11) ; addi r11,r11,0x40   ; &spec->mBBoxSkinData
    //         lvx128 v12/v11/v13 <- rows +0x00/+0x10/+0x20 ; lvx128 v0 <- +0x30
    //         vsubfp128 v0, 0, v0                                        ; v0 = -translation
    //         six vmrgh/vmrglw            -> the 3x3 TRANSPOSE (columns become rows)
    //         three splats of -t + two vmaddfp -> wAxis = -t.x*X -t.y*Y -t.z*Z
    //       i.e. the AFFINE INVERSE of BodyPartBBoxSpec::mOrientation. The spec type only became
    //       reachable by name on 2026-09-05 (BrnBodyPartBBoxSpec.h -- row 3 used to be called
    //       "SIMD alignment padding"), which is exactly why this was modelled by an argument.
    //
    //   (b) the body pose (this+0x00..0x30 == mRwBody.mTransform) is
    //       lGraphicsTransform composed with lVehicleTransform -- 0x82626894..0x82626938, three
    //       rows of `splat(gfxRow.x)*veh0 + splat(gfxRow.y)*veh1 + splat(gfxRow.z)*veh2` and a
    //       translation of `splat(gfxPos.x)*veh0 + splat(gfxPos.y)*veh1 + splat(gfxPos.z)*veh2
    //       + veh3` -- and is then RE-WRITTEN after CalcBoundingBox (0x82626940..0x82626978) with
    //       the SAME three rotation rows but a translation of vehicleTransform applied to
    //       mLocalInitialComPositionPlusMaxJointAngle.xyz, the local COM CalcBoundingBox has just
    //       computed. That second store is the whole point of the call order: the physics body
    //       sits at the panel's CENTRE OF MASS, and GetEventRenderTransform adds
    //       (localGraphicsPos - localCom) back to draw the panel where the artist put it.
    // ⇒ PhysicalBodyPart::AddToSim's compensating `mRwBody.SetTransform(lVehicleTransform)` (which
    //   the previous wave added precisely because this compose was missing) is DELETED with this;
    //   the console's AddToSim only READS this pose. The two banners were one story, as flagged.
    //
    // The local graphics row (+368) is seeded from the passed graphics transform's translation
    // (w lane zeroed), the +352/+384/+400/+432/+448 zero stores clear the packed joint rows, and
    // CalcBoundingBox(lVehicleTransform) builds the oriented box.
    // =========================================================================================
    void PhysicalBodyPart::Prepare(BurnoutBodyPartID lPartId, EntityId lGlobalVehicleId,
                                   const DeformableObject* lpDeformableObject, const IKBodyPart* lpIKPart,
                                   Matrix44Affine lGraphicsTransform, Matrix44Affine lVehicleTransform)
    {
        mRigidBodyId                 = lPartId;            // +464
        mGlobalVehicleId             = lGlobalVehicleId;   // +472
        mi8ActiveJointsTagPointIndex = -1;                // +488
        mpDeformableObject           = lpDeformableObject; // +480
        mpIKPart                     = lpIKPart;           // +476
        mbAddedToScene               = false;             // +485
        mbJoinedToVehicle            = false;             // +484

        // BrnPhysics::ExternalPhysicsBody::Construct() ; *(this+487) = 0.
        mRwBody.Construct();
        mbNeedsWritingIntoRenderware = false;             // +487

        // Build mBBoxOrientation as the AFFINE INVERSE of the IK spec's bbox-skin orientation
        // (mpIKPart->GetSpec()->GetBBoxSpec().mOrientation): transpose the orthonormal 3x3, then
        // form the translation as -t.x*X - t.y*Y - t.z*Z through the transposed rows. Lane 3 of
        // every row is 0 (the merges pull it from the zero register).
        {
            const rw::math::vpu::Matrix44Affine& lrSpecOrientation =
                mpIKPart->GetSpec()->GetBBoxSpec().mOrientation;

            mBBoxOrientation.xAxis = { lrSpecOrientation.xAxis.x, lrSpecOrientation.yAxis.x,
                                       lrSpecOrientation.zAxis.x, 0.0f };
            mBBoxOrientation.yAxis = { lrSpecOrientation.xAxis.y, lrSpecOrientation.yAxis.y,
                                       lrSpecOrientation.zAxis.y, 0.0f };
            mBBoxOrientation.zAxis = { lrSpecOrientation.xAxis.z, lrSpecOrientation.yAxis.z,
                                       lrSpecOrientation.zAxis.z, 0.0f };

            const Vector3& lrTranslation = lrSpecOrientation.wAxis;
            mBBoxOrientation.wAxis = {
                -lrTranslation.x * mBBoxOrientation.xAxis.x - lrTranslation.y * mBBoxOrientation.yAxis.x
                    - lrTranslation.z * mBBoxOrientation.zAxis.x,
                -lrTranslation.x * mBBoxOrientation.xAxis.y - lrTranslation.y * mBBoxOrientation.yAxis.y
                    - lrTranslation.z * mBBoxOrientation.zAxis.y,
                -lrTranslation.x * mBBoxOrientation.xAxis.z - lrTranslation.y * mBBoxOrientation.yAxis.z
                    - lrTranslation.z * mBBoxOrientation.zAxis.z,
                0.0f
            };
        }

        // CgsNumeric::IsRightHanded(mBBoxOrientation).GetBool() tripwire: cross(x,y).z >= 0 (the asm's
        // vmsum3fp128 of (x cross y) . z >= 0). Non-gating.
        {
            const Vector3& lX = mBBoxOrientation.xAxis;
            const Vector3& lY = mBBoxOrientation.yAxis;
            const Vector3& lZ = mBBoxOrientation.zAxis;
            const f32 lfCrossX = lX.y * lY.z - lX.z * lY.y;
            const f32 lfCrossY = lX.z * lY.x - lX.x * lY.z;
            const f32 lfCrossZ = lX.x * lY.y - lX.y * lY.x;
            const f32 lfHandedness = lfCrossX * lZ.x + lfCrossY * lZ.y + lfCrossZ * lZ.z;
            CGS_ASSERT(lfHandedness >= 0.0f, "CgsNumeric::IsRightHanded( mBBoxOrientation ).GetBool()");
        }

        // Seed the local graphics row (+368) from the graphics transform's position relative to the
        // bbox orientation (the +368 store of v0 = compose(graphics, bboxOrient)), then clear the
        // packed joint/COM/initial-joint/collision rows (the +352/+400/+384/+432/+448 zero stores).
        mLocalGraphicsPositionPlusJointVelocity.SetVector3(lGraphicsTransform.Pos());
        mLocalGraphicsPositionPlusJointVelocity.SetPlus(0.0f);

        mLocalJointPositionPlusRotation.SetZero();             // +352
        mLocalInitialJointPositionPlusLimitStress.SetZero();   // +400
        mLocalInitialComPositionPlusMaxJointAngle.SetZero();   // +384
        mWorldPenetrationPlusCollisionMagnitude.SetZero();     // +432
        mAverageCollisionPointPlusNumCollisions.SetZero();     // +448

        // ---- the part's WORLD pose: lGraphicsTransform composed with lVehicleTransform ---------
        // 0x82626894..0x82626938. Row-vector convention throughout this codebase, so each output
        // row is `gfxRow.x*veh.xAxis + gfxRow.y*veh.yAxis + gfxRow.z*veh.zAxis` and the translation
        // additionally picks up veh.wAxis.
        const auto lTransformDirection = [&lVehicleTransform](const Vector3& lrLocal) -> Vector3
        {
            return Vector3{
                lVehicleTransform.xAxis.x * lrLocal.x + lVehicleTransform.yAxis.x * lrLocal.y
                    + lVehicleTransform.zAxis.x * lrLocal.z,
                lVehicleTransform.xAxis.y * lrLocal.x + lVehicleTransform.yAxis.y * lrLocal.y
                    + lVehicleTransform.zAxis.y * lrLocal.z,
                lVehicleTransform.xAxis.z * lrLocal.x + lVehicleTransform.yAxis.z * lrLocal.y
                    + lVehicleTransform.zAxis.z * lrLocal.z,
                0.0f
            };
        };
        const auto lTransformPoint = [&lVehicleTransform, &lTransformDirection](const Vector3& lrLocal) -> Vector3
        {
            const Vector3 lRotated = lTransformDirection(lrLocal);
            return Vector3{ lRotated.x + lVehicleTransform.wAxis.x,
                            lRotated.y + lVehicleTransform.wAxis.y,
                            lRotated.z + lVehicleTransform.wAxis.z, 0.0f };
        };

        Matrix44Affine lWorldTransform;
        lWorldTransform.xAxis = lTransformDirection(lGraphicsTransform.xAxis);   // stvx128 this+0x00
        lWorldTransform.yAxis = lTransformDirection(lGraphicsTransform.yAxis);   // stvx128 this+0x10
        lWorldTransform.zAxis = lTransformDirection(lGraphicsTransform.zAxis);   // stvx128 this+0x20
        lWorldTransform.wAxis = lTransformPoint(lGraphicsTransform.wAxis);       // stvx128 this+0x30
        mRwBody.SetTransform(lWorldTransform);

        // result = CalcBoundingBox(lVehicleTransform) -- the box is built in the vehicle's frame
        // because the part is still exactly where it was drawn on the car. This is also what
        // writes mLocalInitialComPositionPlusMaxJointAngle's xyz (the part's local COM).
        CalcBoundingBox(lVehicleTransform);

        // ---- re-pose the body ON its centre of mass (0x82626940..0x82626978) -------------------
        // Same three rotation rows (the asm re-stores v127/v126/v125 unchanged); the translation is
        // replaced by vehicleTransform applied to the local COM CalcBoundingBox just produced.
        lWorldTransform.wAxis = lTransformPoint(mLocalInitialComPositionPlusMaxJointAngle.GetVector3());
        mRwBody.SetTransform(lWorldTransform);
    }

    // =========================================================================================
    // GetJointRotationProportion @ 0x825C1B38
    //
    // The joint rotation angle (the w lane of mLocalJointPositionPlusRotation @+352) as a proportion
    // of the joint's max angle (the w lane of mLocalInitialComPositionPlusMaxJointAngle @+384).
    //   asm: v0 = reciprocal(maxAngle.w) refined (vrefp + two Newton steps) ; result = rotation.w * recip.
    // Tripwire: IsJoinedToVehicle().
    // =========================================================================================
    VecFloat PhysicalBodyPart::GetJointRotationProportion() const
    {
        CGS_ASSERT(mbJoinedToVehicle, "IsJoinedToVehicle()");

        const f32 lfMaxJointAngle = mLocalInitialComPositionPlusMaxJointAngle.GetPlus();   // +384 w lane
        const f32 lfRotation      = mLocalJointPositionPlusRotation.GetPlus();             // +352 w lane

        // vrefp + Newton-Raphson refine of 1/maxAngle, then * rotation. Modelled as the exact divide
        // the refined reciprocal converges to (the two vnmsubfp/vmaddfp steps are the refinement).
        const f32 lfProportion = (lfMaxJointAngle != 0.0f) ? (lfRotation / lfMaxJointAngle) : 0.0f;
        return Splat(lfProportion);
    }

    // =========================================================================================
    // GetJointVelocity (DWARF BrnPhysicalBodyPart.h:275; bodied 2026-08-24, deform-land wave).
    // The packed joint angular velocity -- the w lane of mLocalGraphicsPositionPlusJointVelocity
    // (part+0x170). The one landed consumer (DeformableObject::UpdateAndOutputJointStates
    // @0x82609AE8) reads it as `lvx128 part+0x170 ; vspltw lane 3` for the JointedPartStateEvent's
    // hinge-velocity field.
    // =========================================================================================
    VecFloat PhysicalBodyPart::GetJointVelocity() const
    {
        return Splat(mLocalGraphicsPositionPlusJointVelocity.GetPlus());
    }

    // =========================================================================================
    // GetPosition (DWARF BrnPhysicalBodyPart.h:215; bodied 2026-08-24, deform-land wave).
    // The part's world position -- the rigid-body transform's translation row (part+0x30, the
    // `lvx128 v13, r3, r25(0x30)` UpdateAndOutputJointStates' detached arm loads).
    // =========================================================================================
    Vector3 PhysicalBodyPart::GetPosition() const
    {
        return mRwBody.GetTransform().wAxis;
    }

    // =========================================================================================
    // GetGlobalEntityId (DWARF BrnPhysicalBodyPart.h:164; bodied 2026-08-24, deform-land wave).
    // The owning vehicle's global entity id (part+0x1D8 = mGlobalVehicleId -- the `lwz r10,
    // 0x1D8(r31)` both detached-part output events copy into their id field).
    // =========================================================================================
    EntityId PhysicalBodyPart::GetGlobalEntityId() const
    {
        return mGlobalVehicleId;
    }

    // =========================================================================================
    // RemoveFromScene @ 0x825E7818 -- ⭐ MOVED (deformation-mount wave) to the mounted slice TU
    // BrnPhysicalBodyPart_Remove.cpp (the authoritative body, with the four scene removals + the
    // tri-cache eviction). The stale duplicate that stood HERE was deleted 2026-08-14 (walls
    // leg 4) when this home TU first mounted -- the link's LNK2005 found it.
    // =========================================================================================

    // =========================================================================================
    // SetRigidBodyTransform @ 0x825E7778
    //
    // Force the embedded body's transform from the passed matrix (the four lvx128/stvx128 row copies
    // into mRwBody.mTransform @ +0/+16/+32/+48) and, if the part is in the scene (+485), republish the
    // transform to the scene via SetVolumeInstanceTransform keyed on mRigidBodyId.
    // =========================================================================================
    void PhysicalBodyPart::SetRigidBodyTransform(Matrix44Affine lTransform,
                                                 CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpSceneInterface)
    {
        // Copy the four rows into the embedded body's transform (this+0/+16/+32/+48).
        mRwBody.SetTransform(lTransform);

        // if ( mbAddedToScene ) republish; the asm reloads the just-stored rows and feeds them as the
        // new volume-instance transform.
        if ( mbAddedToScene )   // *(this+485)
        {
            // ⭐ walls leg 4: the real bodied SetVolumeInstanceTransform keys on the packed
            // VolumeInstanceId (the part handle IS the volume-instance id -- see the
            // GetContactVolumeInstanceId banner); the old EntityId spelling never compiled.
            lpSceneInterface->SetVolumeInstanceTransform(GetContactVolumeInstanceId(),
                                                         mRwBody.GetTransform());
        }
    }

    // =========================================================================================
    // Update @ 0x825E78C8
    //
    // Apply a post-physics rigid-body update event. First mirror the event's frozen flag into mbFrozen
    // (+486 = (lpUpdateEvent->flags & 2) != 0). Tripwire: !IsJoinedToVehicle() (a joined part must not
    // be physics-updated). When NOT frozen, copy the event's transform rows (+16..+96) into a local
    // matrix, push them through SetRigidBodyTransform (republishing to the scene), then read the body's
    // pose + properties back out of RenderWare (ReadFromRenderware / ReadPropertiesFromRenderware).
    // =========================================================================================
    // ⛔⛔ RE-SPELLED 2026-08-27 (detach-2 wave) -- THE RAW OFFSETS HERE WERE A LIVE HOST DEFECT,
    // and it was the SECOND of three silent breaks on the sim-echo path.
    // The body used to read the frozen flag as `*(u32*)((char*)event + 156)`, transcribed from the
    // console's `lwz r11, 0x9C(r31)`. On the X360 that word IS RigidBody::mState, because the console
    // packs mState into the mIsplt register's w lane. THIS TREE DOES NOT: rw/physics/rigidbody.h
    // promotes all ten packed scalars (mId/mRight/mLeft/mStasis/mInertia/mTag/mInvm/mState/mKine/
    // mCool) out of the w lanes into real members past the eleven vectors, precisely because five of
    // them are POINTERS that widen on x64. So event+156 on the host is mIsplt's unused PADDING lane,
    // and mbFrozen was being set from whatever happened to be in it. [[serialized-slots-stay-32-bit]]
    // -- a raw offset read off a big-endian asm dump, applied to a type that was deliberately widened.
    // No gate could see it: the read is well-formed, in-bounds, and produces a plausible bool.
    // The transform rows happened to survive (the eleven Vector4s still lead the record at the same
    // offsets) but they are spelled through GetTransform() now for the same reason.
    void PhysicalBodyPart::Update(const OutUpdateRigidBody* lpUpdateEvent,
                                  CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpSceneInterface)
    {
        // The event carries a whole rw::physics::RigidBody by value at +0x10.
        const rw::physics::RigidBody& lrRigidBody =
            reinterpret_cast<const CgsPhysics::PhysicsSimulationIO::OutUpdateRigidBody*>(lpUpdateEvent)
                ->mRigidBody;

        // *(this+486) = (mState & 2) != 0 -- the asm's `lwz 0x9C(event)` + `extrwi r11,r11,1,30`,
        // i.e. bit 1 of the body state == rw::physics::FROZEN_BODY.
        mbFrozen = (lrRigidBody.GetState() & rw::physics::FROZEN_BODY) != 0;

        // Tripwire: !IsJoinedToVehicle().
        CGS_ASSERT(!mbJoinedToVehicle, "!IsJoinedToVehicle()");

        if ( !mbFrozen )   // if ( !*(this+486) )
        {
            // The asm's four lvx128 from event+0x50/+0x60/+0x70/+0x20 into the by-value matrix it
            // hands SetRigidBodyTransform. Those are mRi / mUp / mAt / mCom -- i.e. exactly
            // RigidBody::GetTransform(), whose committed body reads the same four registers.
            SetRigidBodyTransform(lrRigidBody.GetTransform(), lpSceneInterface);

            // Read the body's pose + physical properties back out of the RW rigid body (r4 = the
            // event's embedded RigidBody in both calls).
            mRwBody.ReadFromRenderware(&lrRigidBody);
            mRwBody.ReadPropertiesFromRenderware(&lrRigidBody);
        }
    }

    // =========================================================================================
    // SetJoinedToVehicle @ 0x825BA4A8
    //
    // Join the part to the vehicle as an active joint. The DWARF signature is
    // `SetJoinedToVehicle(Vector3, Vector3, VecFloat, int32_t)`; the console body reads only v1
    // (the local joint position, `vmr128 v127,v1`) and v3 (the max-joint-angle splat, `vmr128
    // v126,v3`) plus the r4 tag index. v2 -- the part's local COM position -- IS PASSED AND NEVER
    // READ here; DeformableObject::DetachPart computes it for its own DetachedPartNotificationEvent
    // and hands it across anyway. Its ONLY calls are savegpr / BeginAssert / FireAssert / EndAssert:
    // it does NOT call GetActiveJointSpec() or GetMaxAngle() (the caller does, twice). The five
    // vrlimi128 lane writes (mask 1, shift 0 == replace the w lane, keep xyz) resolve to:
    //   +352 mLocalJointPositionPlusRotation        double-store -> {v1.xyz, 0}   (jointpos, rotation=0)
    //   +400 mLocalInitialJointPositionPlusLimitStress -> {v1.xyz, OLD w preserved} (limit stress NEVER
    //          written -- no argument carries it)
    //   +384 mLocalInitialComPositionPlusMaxJointAngle -> {OLD xyz preserved, w = -(v3.w)} (the max
    //          angle, negated via a `vslw -1,-1` sign mask + vxor)
    //   +368 mLocalGraphicsPositionPlusJointVelocity -> {OLD xyz preserved, w = 0} (joint velocity = 0)
    // Then mi8ActiveJointsTagPointIndex (+488) = arg, mbJoinedToVehicle (+484) = 1.
    // Tripwires: GetActiveJointIndex() != KU8_NO_ACTIVE_JOINT ; GetActiveJointSpec() != NULL.
    // ⚠️ +384's xyz is deliberately PRESERVED: CalcBoundingBox (run from Prepare, before this) owns
    // that lane -- it is the part's local COM, and GetEventRenderTransform subtracts it. Overwriting
    // it here with the passed COM would double-count.
    // =========================================================================================
    void PhysicalBodyPart::SetJoinedToVehicle(Vector3 lLocalJointPosition, Vector3 lLocalComPosition,
                                              VecFloat lvfMaxJointAngle, s32 liActiveJointsTagPointIndex)
    {
        // v2 is an ABI argument the console's body never reads (see the banner). Named, not dropped.
        (void)lLocalComPosition;

        // mpIKPart->GetActiveJointIndex() != KU8_NO_ACTIVE_JOINT (the *(v7+14)==255 test).
        CGS_ASSERT(mpIKPart->GetActiveJointIndex() != IKBodyPart::KU8_NO_ACTIVE_JOINT,
                   "mu8ActiveJointIndex != KU8_NO_ACTIVE_JOINT");
        // mpIKPart->GetActiveJointSpec() != NULL (asserted; the spec is NOT otherwise dereferenced here).
        CGS_ASSERT(mpIKPart->GetActiveJointSpec() != 0, "mpIKPart->GetActiveJointSpec() != NULL");

        // +400 mLocalInitialJointPositionPlusLimitStress: xyz = localJointPos; w (limit stress) is the
        // OLD lane PRESERVED -- the asm never writes it (no limit-stress arg exists).
        mLocalInitialJointPositionPlusLimitStress.SetVector3(lLocalJointPosition);

        // +384 mLocalInitialComPositionPlusMaxJointAngle: xyz is the OLD lane PRESERVED (the local COM
        // CalcBoundingBox wrote); w (max joint angle) = -(v3.w), i.e. the max-angle splat negated
        // (the vxor sign mask).
        mLocalInitialComPositionPlusMaxJointAngle.SetPlus(-lvfMaxJointAngle.w);

        // +368 mLocalGraphicsPositionPlusJointVelocity: xyz OLD preserved; w (joint velocity) = 0.
        mLocalGraphicsPositionPlusJointVelocity.SetPlus(0.0f);

        // +352 mLocalJointPositionPlusRotation: the double-store nets {v1.xyz, 0} -- xyz = localJointPos,
        // w (rotation) = 0.
        mLocalJointPositionPlusRotation.SetVector3(lLocalJointPosition);
        mLocalJointPositionPlusRotation.SetPlus(0.0f);

        // *(this+488) = arg ; *(this+484) = 1.
        mi8ActiveJointsTagPointIndex = static_cast<s8>(liActiveJointsTagPointIndex);
        mbJoinedToVehicle            = true;
    }

    // =========================================================================================
    // LimitVelocities @ 0x825BA5C0
    //
    // Clamp the part's linear + angular velocities to time-scaled caps. Early-out (return false) when
    // mbJoinedToVehicle (+484) is set (joined parts are joint-driven, not free). Otherwise build the
    // per-step max linear/angular velocities from a flag-guarded one-shot static init (the
    // flt_82FB9FB8/.B4 lazy seeds), assert them in range, and -- per axis -- if |velocity|^2 exceeds
    // the squared cap, renormalise the velocity to KF_PROPORTION_OF_MAX_TO_CAP_TO * cap and mark the
    // part dirty (return true).
    //
    // Returns true iff a clamp was applied (the v19 result).
    // =========================================================================================
    bool PhysicalBodyPart::LimitVelocities(VecFloat lvfTimeStep)
    {
        bool lbRequiresUpdate = false;   // v19 = 0

        if ( mbJoinedToVehicle )   // if ( *(this+484) ) result = 0
        {
            return false;
        }

        // Time-step-dependent velocity-cap curve (asm @0x825BA5C0 fsel/immediate cascade). The two
        // gradient immediates flt_82FB9FB8 = -3599.9998 and flt_82FB9FB4 = -5699.9995 are RECOVERED
        // literals (the lazy one-shot static init caches them in dword_82FB9FBC, but the VALUES are the
        // visible immediates -- NOT rodata). The cascade computes, with dt = lvfTimeStep:
        //   _FP8 = 60  - (dt*(-3599.9998) + 120)     // linear slope test
        //   _FP7 = 5   - (dt*(-5699.9995) + 100)     // angular slope test
        //   _FP12 = (_FP8 >= 0) ? 120 : (dt*(-3599.9998)+120)   // fsel f12,f8,f10,f12
        //   _FP11 = (_FP7 >= 0) ? 100 : (dt*(-5699.9995)+100)   // fsel f11,f7,f9,f11
        //   lfMaxLinVel = (120 - _FP12 >= 0) ? _FP12 : 120      // fsel f31,f10,f12,f0  (clamp to [.,120])
        //   lfMaxAngVel = (100 - _FP11 >= 0) ? _FP11 : 100      // fsel f29,f9,f11,f13  (clamp to [.,100])
        // i.e. the per-step cap is the time-scaled value (dt*grad + cap) clamped at the at-60 ceiling
        // (KF_MAX_LIN_VEL_AT_60 = 120 / KF_MAX_ANG_VEL_AT_60 = 100), and floored at the same ceiling when
        // the slope test goes non-negative (small dt -> ceiling). fsel(a,b,c) == (a >= 0) ? b : c.
        const f32 lfTimeStep = lvfTimeStep.x;
        auto lfFSel = [](f32 lfA, f32 lfB, f32 lfC) { return (lfA >= 0.0f) ? lfB : lfC; };

        const f32 lfLinSlope = lfTimeStep * -3599.9998f + KF_MAX_LIN_VEL_AT_60;   // dt*v22 + 120
        const f32 lfAngSlope = lfTimeStep * -5699.9995f + KF_MAX_ANG_VEL_AT_60;   // dt*v23 + 100

        const f32 lfLinTest  = 60.0f - lfLinSlope;                               // _FP8 = 60 - lfLinSlope
        const f32 lfAngTest  =  5.0f - lfAngSlope;                               // _FP7 = 5  - lfAngSlope

        const f32 lfLinSel   = lfFSel(lfLinTest, KF_MAX_LIN_VEL_AT_60, lfLinSlope);   // _FP12
        const f32 lfAngSel   = lfFSel(lfAngTest, KF_MAX_ANG_VEL_AT_60, lfAngSlope);   // _FP11

        f32 lfMaxLinVel = lfFSel(KF_MAX_LIN_VEL_AT_60 - lfLinSel, lfLinSel, KF_MAX_LIN_VEL_AT_60);  // _FP31
        f32 lfMaxAngVel = lfFSel(KF_MAX_ANG_VEL_AT_60 - lfAngSel, lfAngSel, KF_MAX_ANG_VEL_AT_60);  // _FP29

        // Range tripwires (non-gating).
        CGS_ASSERT(lfMaxLinVel >= 0.0f && lfMaxLinVel <= KF_MAX_LIN_VEL_AT_INF,
                   "lfMaxLinVel >= 0.0f && lfMaxLinVel <= kfMAX_LIN_VEL_AT_INF");
        CGS_ASSERT(lfMaxAngVel >= 0.0f && lfMaxAngVel <= KF_MAX_ANG_VEL_AT_INF,
                   "lfMaxAngVel >= 0.0f && lfMaxAngVel <= kfMAX_ANG_VEL_AT_INF");

        const f32 lfMaxLinVelSquared = lfMaxLinVel * lfMaxLinVel;   // v51 = _FP31 * _FP31
        const f32 lfMaxAngVelSquared = lfMaxAngVel * lfMaxAngVel;   // v55 = _FP29 * _FP29

        // --- linear velocity (mRwBody linear vel row, console this+64) ---
        {
            Vector3 lLinVel = mRwBody.GetLinearVelocity();
            const f32 lfMagSq = lLinVel.x * lLinVel.x + lLinVel.y * lLinVel.y + lLinVel.z * lLinVel.z;
            if ( lfMagSq > lfMaxLinVelSquared )   // vcmpgtfp.
            {
                // renormalise to KF_PROPORTION_OF_MAX_TO_CAP_TO * lfMaxLinVel (the vrsqrtefp refine
                // followed by * 0.8 * lfMaxLinVel). Modelled by the exact scale the refined recip
                // converges to.
                const f32 lfMag = (lfMagSq > 0.0f) ? std::sqrt(lfMagSq) : 0.0f;
                const f32 lfScale = (lfMag > 0.0f)
                    ? (KF_PROPORTION_OF_MAX_TO_CAP_TO * lfMaxLinVel / lfMag) : 0.0f;
                lLinVel.x *= lfScale;
                lLinVel.y *= lfScale;
                lLinVel.z *= lfScale;
                mRwBody.SetLinearVelocity(lLinVel);
                lbRequiresUpdate = true;   // v19 = 1
            }
        }

        // --- angular velocity (mRwBody angular vel row, console this+80) ---
        {
            Vector3 lAngVel = mRwBody.GetAngularVelocity();
            const f32 lfMagSq = lAngVel.x * lAngVel.x + lAngVel.y * lAngVel.y + lAngVel.z * lAngVel.z;
            if ( lfMagSq > lfMaxAngVelSquared )
            {
                const f32 lfMag = (lfMagSq > 0.0f) ? std::sqrt(lfMagSq) : 0.0f;
                const f32 lfScale = (lfMag > 0.0f)
                    ? (KF_PROPORTION_OF_MAX_TO_CAP_TO * lfMaxAngVel / lfMag) : 0.0f;
                lAngVel.x *= lfScale;
                lAngVel.y *= lfScale;
                lAngVel.z *= lfScale;
                mRwBody.SetAngularVelocity(lAngVel);
                lbRequiresUpdate = true;
            }
        }

        return lbRequiresUpdate;   // result = v19
    }

    // =========================================================================================
    // AddContact @ 0x825E2FC8 (96 instructions)
    //
    // ⭐⭐⭐ RE-DECODED 2026-09-05 (hinge-geometry wave), INSTRUCTION BY INSTRUCTION, because
    // waking the hinge path made this the first function a hinged panel's contact ever reaches --
    // and what stood here read a DIFFERENT STRUCT through raw byte offsets. See the retired-fork
    // banner in BrnPhysicalBodyPartPool.h. Four separate faults, all invisible while the queues
    // were empty:
    //   (1) the owner tripwire read `*(u32*)(contact+48) >> 24`. The console reads `ld 0x30(r30)`
    //       then `srdi 32 ; srwi 24` -- the TOP byte of an EIGHT-byte field. On big-endian those
    //       coincide; on x64 a 4-byte load at +48 is the field's LOW half, so the byte tested was
    //       never the owner. (The classic width/endianness trap, and the reason 3 owner asserts
    //       fired in run jgeo_A1.)
    //   (2) the point was transformed by the PART'S OWN transform. The console transforms it by
    //       the VEHICLE'S PER-FRAME TRANSFORM DELTA: `lwz r10, 0x1E0(this)` (mpDeformableObject),
    //       `lwz r4, 0x194C(r10)` (its VehiclePhysics), `bl VehiclePhysics::GetTransformDelta`
    //       into the sret buffer var_70..var_40, whose four rows are then the vmaddfp cascade's
    //       basis. "How far the car moved this frame" is the whole point of the resolve.
    //   (3) the CONTACT NORMAL (record +0x20) was not read at all. The console projects the
    //       resolve onto it and CLAMPS THE PROJECTION AT ZERO:
    //           v13 = dot3(resolve, normal) ; vminfp v13, v13, 0 ; v13 = normal * v13
    //       so only a SEPARATING-into-penetrating component survives, and the stored vector is
    //       along the contact normal. Storing the raw resolve, as this body did, both changes the
    //       direction and drops the clamp -- and this member is arm (4a)'s gate input in
    //       TestJointForBreaking, i.e. it is directly part of the joint-break ladder.
    //   (4) the collision counter was `count + 1`. The asm's `vsel v0, v7, v8, v0` selects
    //       between splat(oldCount) and the 1.0f built by `vspltisw v8,1 ; vcfsx v8,v8,0` --
    //       it LATCHES 1, it does not accumulate. The lane is a has-any-contact flag.
    //
    // The two accumulator writes, exactly as the asm spells them (v127 == this+0x30, the part's
    // world position row; both stores go to their own member, w-lane preserved by vrlimi mask 1):
    //   +432 mWorldPenetrationPlusCollisionMagnitude.xyz
    //          = (|projected|^2 >= |old|^2) ? projected : old
    //   +448 mAverageCollisionPointPlusNumCollisions
    //          take = (|partPos - newPointOnA|^2 >= |partPos - storedPoint|^2) || count == 0
    //          .xyz = take ? newPointOnA : stored ;  .w = take ? 1.0f : count
    // Tripwire: the contact's volume-instance-A owner is a deformable part (6 or 7).
    // =========================================================================================
    void PhysicalBodyPart::AddContact(const PotentialContact& lContact)
    {
        // `ld r11, 0x30(contact) ; srdi 32 ; srwi 24` -- the id's top byte.
        const u32 luOwnerA = static_cast<u32>(lContact.muVolumeInstanceIdA.muId >> 56) & 0xFFu;
        CGS_ASSERT(luOwnerA == 6 || luOwnerA == 7,
                   "lContact.muVolumeInstanceIdA.GetEntityIDOwner() == BrnWorld::E_ENTITYTYPE_RACECAR_DEFORMABLE_PART || "
                   "lContact.muVolumeInstanceIdA.GetEntityIDOwner() == BrnWorld::E_ENTITYTYPE_TRAFFIC_DEFORMABLE_PART");

        // The owning vehicle's per-frame transform delta (`*(*(this+0x1E0)+0x194C)`).
        const Matrix44Affine lVehicleDeltaTransform =
            mpDeformableObject->GetVehiclePhysics()->GetTransformDelta();

        // v127 = *(this + 0x30) -- the part body's own world position row.
        const Vector3 lPartWorldPosition = mRwBody.GetTransform().wAxis;

        // lNewPointOnA = transformPoint(delta, contact.mPointOnA) -- the three vmaddfp rows.
        const Vector3& lrPointOnA = lContact.mPointOnA;
        const Vector3 lNewPointOnA = {
            lVehicleDeltaTransform.xAxis.x * lrPointOnA.x + lVehicleDeltaTransform.yAxis.x * lrPointOnA.y +
                lVehicleDeltaTransform.zAxis.x * lrPointOnA.z + lVehicleDeltaTransform.wAxis.x,
            lVehicleDeltaTransform.xAxis.y * lrPointOnA.x + lVehicleDeltaTransform.yAxis.y * lrPointOnA.y +
                lVehicleDeltaTransform.zAxis.y * lrPointOnA.z + lVehicleDeltaTransform.wAxis.y,
            lVehicleDeltaTransform.xAxis.z * lrPointOnA.x + lVehicleDeltaTransform.yAxis.z * lrPointOnA.y +
                lVehicleDeltaTransform.zAxis.z * lrPointOnA.z + lVehicleDeltaTransform.wAxis.z,
            0.0f
        };

        // lResolve = newPointOnA - mPointOnB (vsubfp v7, v0, v6).
        const Vector3& lrPointOnB = lContact.mPointOnB;
        const Vector3 lResolve = { lNewPointOnA.x - lrPointOnB.x,
                                   lNewPointOnA.y - lrPointOnB.y,
                                   lNewPointOnA.z - lrPointOnB.z, 0.0f };

        // The penetration along the contact normal, clamped at zero (vmsum3fp128 / vminfp / vmulfp128).
        const Vector3& lrNormal = lContact.mNormal;
        const f32 lfAlongNormal = lResolve.x * lrNormal.x + lResolve.y * lrNormal.y + lResolve.z * lrNormal.z;
        const f32 lfClamped     = (lfAlongNormal < 0.0f) ? lfAlongNormal : 0.0f;   // vminfp with 0
        const Vector3 lProjectedPenetration = { lrNormal.x * lfClamped,
                                                lrNormal.y * lfClamped,
                                                lrNormal.z * lfClamped, 0.0f };

        // Keep the deeper of (stored, projected) in +432; the w lane (the magnitude) is preserved.
        const Vector3 lStoredPenetration = mWorldPenetrationPlusCollisionMagnitude.GetVector3();
        const f32 lfStoredMagSq = lStoredPenetration.x * lStoredPenetration.x
                                + lStoredPenetration.y * lStoredPenetration.y
                                + lStoredPenetration.z * lStoredPenetration.z;
        const f32 lfProjectedMagSq = lProjectedPenetration.x * lProjectedPenetration.x
                                   + lProjectedPenetration.y * lProjectedPenetration.y
                                   + lProjectedPenetration.z * lProjectedPenetration.z;
        if ( lfProjectedMagSq >= lfStoredMagSq )   // vcmpgefp then vsel
        {
            mWorldPenetrationPlusCollisionMagnitude.SetVector3(lProjectedPenetration);
        }

        // +448: keep whichever candidate point is FURTHER from the part body's own position, and
        // latch the flag lane. First contact (count == 0) always takes the new point.
        const Vector3 lStoredPoint  = mAverageCollisionPointPlusNumCollisions.GetVector3();
        const f32 lfCount           = mAverageCollisionPointPlusNumCollisions.GetPlus();
        const bool lbFirstContact   = (lfCount == 0.0f);                     // vcmpeqfp against 0

        const f32 lfNewDx = lPartWorldPosition.x - lNewPointOnA.x;
        const f32 lfNewDy = lPartWorldPosition.y - lNewPointOnA.y;
        const f32 lfNewDz = lPartWorldPosition.z - lNewPointOnA.z;
        const f32 lfNewDistSq = lfNewDx * lfNewDx + lfNewDy * lfNewDy + lfNewDz * lfNewDz;

        const f32 lfOldDx = lPartWorldPosition.x - lStoredPoint.x;
        const f32 lfOldDy = lPartWorldPosition.y - lStoredPoint.y;
        const f32 lfOldDz = lPartWorldPosition.z - lStoredPoint.z;
        const f32 lfOldDistSq = lfOldDx * lfOldDx + lfOldDy * lfOldDy + lfOldDz * lfOldDz;

        const bool lbTakeNew = (lfNewDistSq >= lfOldDistSq) || lbFirstContact;   // vcmpgefp ; vor

        if ( lbTakeNew )
        {
            mAverageCollisionPointPlusNumCollisions.SetVector3(lNewPointOnA);
            mAverageCollisionPointPlusNumCollisions.SetPlus(1.0f);
        }
    }

    // =========================================================================================
    // CalculateSkinnedPoint @ 0x825E2560  (392 instructions) -- LANDED 2026-09-05, crash wave 2.
    //
    // Resolve ONE bounding-box control point into the panel's CURRENT deformed pose. The point is
    // skinned to THREE "bones" -- each bone being either a TagPoint or an IKDrivenPoint of the
    // owning IKBodyPart -- and the blend is a pure DELTA skin:
    //
    //     skinned = lrPoint.mVertex + SUM(i=0..2)  weight[i] * (bone[i].current - bone[i].rest)
    //
    // ⛔ THE PREVIOUS BANNER SAID "DELIBERATELY NOT ATTEMPTED ... a realism refinement, not a
    // behaviour", and that judgement was wrong for a reason it could not see from here: with this
    // function absent, every one of the ten control points was the ORIGIN, so the box built below
    // was DEGENERATE, floored at the 0.05 half-extent, and DoBodyPartWorldContactGeneration then
    // padded that "thin" box by the console's full 0.5 m anti-tunnelling ceiling (the inverse gate
    // at 0x8260962C). A flat door was collided as a ~0.55 m near-cube -- which is exactly why
    // detached panels came to rest STANDING ON EDGE in the owner's screenshot. A cube can balance
    // on a corner; a door cannot. The padding constants are the console's own and are untouched.
    //
    // ---- how the three bone indices are resolved (the branch at 0x825E25D4, four times over) ----
    // One BYTE index selects into a single flat numbering: [0 .. numTagPoints) are TAG points and
    // everything above is a DRIVEN point re-indexed by subtracting numTagPoints. The asm reads the
    // count once per path as `*(*(mpIKPart+8) + 0x1D8)` == mpSpec->GetNumberOfTagPoints(), and the
    // driven bound as `+0x1D0` == GetNumberOfDrivenPoints(). Both arrays are reached through the
    // IKBodyPart's own two window pointers (`lwz r10, 4(r30)` == maTagPoints, stride 32;
    // `lwz r10, 0(r30)` == maDrivenPoints, stride 48) -- i.e. the ordinary accessors, whose bounds
    // asserts this function bakes four times each (BrnIKBodyPart.h:243/244 and :264/265).
    //
    // ---- and what is actually differenced (the vsubfp at 0x825E26B0 and its three twins) ----
    //     TAG:     v0 = *(record + 0x00)        == TagPoint::mPos
    //              v13 = *( *(record + 0x10) + 0x20 )
    //                  == mpSpec->mInitialPositionAndDetachThreshold  (TagPointSpec + 0x20)
    //     DRIVEN:  v0 = *(record + 0x00)        == IKDrivenPoint::mPositionPlusDistanceToA
    //              v13 = *( *(record + 0x20) + 0x00 )
    //                  == mpSpec->mInitialPos                         (IKDrivenPointSpec + 0x00)
    // In both cases `current - rest` is exactly the accessor both types already carry,
    // GetOffsetFromInitialPosition() -- so the delta is spelled through it rather than by walking
    // the spec pointer by hand.
    //
    // ---- the two paths and the assert between them are the ORIGINAL SOURCE'S, not an artefact ----
    // The console computes the blend TWICE: once as a 3-iteration loop (the reference), once
    // unrolled with the weight vector loaded whole and lanes 0/1/2 splatted, and then asserts the
    // two agree to 0.01 -- the "Mismatch Opt: (%f, %f, %f), Unopt: (%f, %f, %f)" string pair at
    // BrnPhysicalBodyPart.cpp:257. That is a hand-vectorisation self-check the author wrote, so it
    // is transcribed rather than folded away. ⚠️ STATED PLAINLY: on the host both spellings lower
    // to the same scalar arithmetic, so this assert is FAITHFUL BUT NOT INDEPENDENTLY
    // DISCRIMINATING here -- it cannot fail on a PC build the way it could on VMX. It is kept
    // because deleting it would drop, without saying so, a side effect the binary has.
    //
    // The 16-byte alignment assert is real on the host and does discriminate: BBoxPointSkinData is
    // alignas(16) precisely because mafWeights sits at +0x10 and the console loads it with a single
    // vector load (`lvlx v0, r0, r21` @0x825E2780).
    //
    // Registers: r3 is the hidden sret buffer for the returned Vector3, r4 is `this`, r5 is the
    // point -- read from the prologue (`mr r23, r4` / `mr r20, r5` / `stw r3, arg_14`), not from
    // the pseudocode, which renders the whole function as a nullary `int`.
    // =========================================================================================
    Vector3 PhysicalBodyPart::CalculateSkinnedPoint(const BBoxPointSkinData& lrPoint)
    {
        // ---- the reference blend: the do/while(v11) over three influences ---------------------
        // Seeded from the point's own rest vertex (`lvx128 v123, r0, r20` -- record +0x00), then
        // one fused multiply-add per influence (`vmaddfp128 v123, v0, v13, v123`, classic raw
        // field order D,A,B,C == D = A*C + B, with v13 the splatted scalar weight).
        Vector3 lvUnoptimised = lrPoint.GetVertex();
        for ( s32 liInfluence = 0; liInfluence < BBoxPointSkinData::KI_NUM_SKIN_INFLUENCES; ++liInfluence )
        {
            const Vector3 lvBoneOffset = ResolveSkinBoneOffset(mpIKPart, lrPoint.GetBoneIndex(liInfluence));
            const f32     lfWeight     = lrPoint.GetWeight(liInfluence);

            lvUnoptimised.x += lvBoneOffset.x * lfWeight;
            lvUnoptimised.y += lvBoneOffset.y * lfWeight;
            lvUnoptimised.z += lvBoneOffset.z * lfWeight;
        }

        // ---- the hand-vectorised blend: weights fetched ONCE, the three lanes splatted ---------
        // `clrlwi r10, r20, 28` / `cmplwi r10, 0` @0x825E26EC-0x825E2720 -- the record base, hence
        // mafWeights at +0x10, must be 16-byte aligned for the single unaligned-vector weight load.
        CGS_ASSERT((reinterpret_cast<uintptr_t>(&lrPoint) & 0xFu) == 0u,
                   "Expected lPoint.mafWeights to be 16 byte aligned\n");

        const f32 lfWeightX = lrPoint.GetWeight(0);   // vspltw128 v127, v0, 0
        const f32 lfWeightY = lrPoint.GetWeight(1);   // vspltw128 v126, v0, 1
        const f32 lfWeightZ = lrPoint.GetWeight(2);   // vspltw128 v124, v0, 2

        const Vector3 lvBoneOffset0 = ResolveSkinBoneOffset(mpIKPart, lrPoint.GetBoneIndex(0));
        const Vector3 lvBoneOffset1 = ResolveSkinBoneOffset(mpIKPart, lrPoint.GetBoneIndex(1));
        const Vector3 lvBoneOffset2 = ResolveSkinBoneOffset(mpIKPart, lrPoint.GetBoneIndex(2));

        // vmaddcfp128 (0x825E2878) then two vmaddfp128 (0x825E295C, 0x825E2A50), accumulating into
        // the base vertex re-loaded unaligned at 0x825E26F0.
        Vector3 lvOptimised = lrPoint.GetVertex();
        lvOptimised.x += lvBoneOffset0.x * lfWeightX + lvBoneOffset1.x * lfWeightY + lvBoneOffset2.x * lfWeightZ;
        lvOptimised.y += lvBoneOffset0.y * lfWeightX + lvBoneOffset1.y * lfWeightY + lvBoneOffset2.y * lfWeightZ;
        lvOptimised.z += lvBoneOffset0.z * lfWeightX + lvBoneOffset1.z * lfWeightY + lvBoneOffset2.z * lfWeightZ;

        // The self-check: |opt - unopt| must be within 0.01 on every compared lane. The console
        // masks the w lane out of the comparison by overwriting it with the x lane
        // (`vrlimi128 v12, v0, 1, 1`), which is why w is not modelled or compared here.
        const f32 lfDiffX = std::fabs(lvUnoptimised.x - lvOptimised.x);
        const f32 lfDiffY = std::fabs(lvUnoptimised.y - lvOptimised.y);
        const f32 lfDiffZ = std::fabs(lvUnoptimised.z - lvOptimised.z);
        CGS_ASSERT(lfDiffX <= KF_SKIN_BLEND_TOLERANCE && lfDiffY <= KF_SKIN_BLEND_TOLERANCE &&
                       lfDiffZ <= KF_SKIN_BLEND_TOLERANCE,
                   "Mismatch Opt/Unopt skinned point");

        // `stvx128 v127, r0, r3` -- the OPTIMISED vector is the one returned.
        return lvOptimised;
    }

    // =========================================================================================
    // CalculateBoundingBoxExtents @ 0x825E2B80
    //
    // Compute the part's local-space bbox min/max by resolving each of the 10 skinned bbox control
    // points (CalculateSkinnedPoint over the IK spec's embedded BodyPartBBoxSpec) and transforming
    // each through that spec's own ORIENTATION matrix, min/max-reducing into lvBoundingBoxMin /
    // lvBoundingBoxMax. The first 8 corners run in a do/while(r27); the centre (+0x140) and joint
    // (+0x160) control points close it out. Two non-gating asserts bound the box magnitude:
    //   Magnitude(max-min) > 0.00001f ; Magnitude(max-min) < KV_BIG_VECTOR.GetX().
    //
    // ⭐⭐ 2026-09-05: THE PER-POINT TRANSFORM WAS MISSING ENTIRELY, and it was invisible because
    // the points were all the origin (0 transformed by anything but a translation is still 0, and
    // the translation row had been modelled as PADDING -- see BrnBodyPartBBoxSpec.h). The asm
    // cascade at 0x825E2C28..0x825E2C3C is an ordinary affine point transform:
    //     out = orientation.xAxis * p.x + orientation.yAxis * p.y + orientation.zAxis * p.z
    //           + orientation.wAxis
    // taken in raw field order (`vmaddfp vD,vA,vB,vC` == vA*vC + vB), with the three lanes of the
    // skinned point splatted into vC by the three vspltw at 0x825E2C08/0C14/0C1C.
    // =========================================================================================
    void PhysicalBodyPart::CalculateBoundingBoxExtents(Vector3& lvBoundingBoxMin, Vector3& lvBoundingBoxMax)
    {
        // Seed: min = +BIG (the vxor of the all-ones sign mask -> -0x800000.. == -FLT_MAX-ish),
        //       max = -BIG (the inverted seed). The asm seeds both from &unk_82FB9B00 (a large vector).
        // Modelled with large finite seeds so the reduction starts wide-open.
        Vector3 lMin = {  KF_BIG_VECTOR_X,  KF_BIG_VECTOR_X,  KF_BIG_VECTOR_X, 0.0f };
        Vector3 lMax = { -KF_BIG_VECTOR_X, -KF_BIG_VECTOR_X, -KF_BIG_VECTOR_X, 0.0f };

        // The bbox skin record inside the IK spec: `lwz r10, 0x1DC(this)` (mpIKPart), `lwz r11, 8(r10)`
        // (its spec), `addi r29, r11, 0x40` (the embedded BodyPartBBoxSpec).
        const BodyPartBBoxSpec& lrBBoxSpec = mpIKPart->GetSpec()->GetBBoxSpec();
        const Matrix44Affine&   lrOrientation = lrBBoxSpec.mOrientation;

        for ( s32 liPoint = 0; liPoint < BodyPartBBoxSpec::KI_NUM_BBOX_POINTS; ++liPoint )
        {
            const Vector3 lSkinnedPoint = CalculateSkinnedPoint(lrBBoxSpec.GetSkinPoint(liPoint));

            // The vmaddfp cascade: transform the skinned point through the spec's orientation.
            const Vector3 lTransformed = {
                lrOrientation.xAxis.x * lSkinnedPoint.x + lrOrientation.yAxis.x * lSkinnedPoint.y +
                    lrOrientation.zAxis.x * lSkinnedPoint.z + lrOrientation.wAxis.x,
                lrOrientation.xAxis.y * lSkinnedPoint.x + lrOrientation.yAxis.y * lSkinnedPoint.y +
                    lrOrientation.zAxis.y * lSkinnedPoint.z + lrOrientation.wAxis.y,
                lrOrientation.xAxis.z * lSkinnedPoint.x + lrOrientation.yAxis.z * lSkinnedPoint.y +
                    lrOrientation.zAxis.z * lSkinnedPoint.z + lrOrientation.wAxis.z,
                0.0f
            };

            // vminfp / vmaxfp reduction.
            if ( lTransformed.x < lMin.x ) lMin.x = lTransformed.x;
            if ( lTransformed.y < lMin.y ) lMin.y = lTransformed.y;
            if ( lTransformed.z < lMin.z ) lMin.z = lTransformed.z;
            if ( lTransformed.x > lMax.x ) lMax.x = lTransformed.x;
            if ( lTransformed.y > lMax.y ) lMax.y = lTransformed.y;
            if ( lTransformed.z > lMax.z ) lMax.z = lTransformed.z;
        }

        // The asm clamps (max-min) to a minimum vector (vmaxfp v0,v0,v12 against &unk_82FB96E0) before
        // re-centring max = min + (max-min). Modelled by max'ing the extent against the min-bbox floor.
        Vector3 lExtent = { lMax.x - lMin.x, lMax.y - lMin.y, lMax.z - lMin.z, 0.0f };
        if ( lExtent.x < KV_MIN_BBOX_SIZE.x ) lExtent.x = KV_MIN_BBOX_SIZE.x;
        if ( lExtent.y < KV_MIN_BBOX_SIZE.y ) lExtent.y = KV_MIN_BBOX_SIZE.y;
        if ( lExtent.z < KV_MIN_BBOX_SIZE.z ) lExtent.z = KV_MIN_BBOX_SIZE.z;
        lMax.x = lMin.x + lExtent.x;
        lMax.y = lMin.y + lExtent.y;
        lMax.z = lMin.z + lExtent.z;

        // Two non-gating magnitude tripwires on Magnitude(max-min).
        const f32 lfMagSq =
            lExtent.x * lExtent.x + lExtent.y * lExtent.y + lExtent.z * lExtent.z;
        const f32 lfMagnitude = (lfMagSq > 0.0f) ? std::sqrt(lfMagSq) : 0.0f;
        CGS_ASSERT(lfMagnitude > KF_MIN_BBOX_MAGNITUDE, "Magnitude(lBoundingBoxMax - lBoundingBoxMin) > 0.00001f");
        CGS_ASSERT(lfMagnitude < KF_BIG_VECTOR_X, "Magnitude(lBoundingBoxMax - lBoundingBoxMin) < KV_BIG_VECTOR.GetX()");

        lvBoundingBoxMin = lMin;
        lvBoundingBoxMax = lMax;
    }

    // =========================================================================================
    // CalcBoundingBox @ 0x8260ABB0
    //
    // (Re)build the oriented bounding box from a transform. Calls CalculateBoundingBoxExtents to get
    // local min/max, sets mBoundingBoxHalfDimensions (+416) = (max-min) * 0.5 max'd against the
    // min-bbox floor (&unk_82FB9DD0), and stores the box centre into mLocalInitialComPositionPlusMax-
    // JointAngle (+384, w lane preserved by vrlimi128 mask 1,0). The centre = (max+min)*0.5 is
    // transformed through the part's OWN mBBoxOrientation rows (this+288/+304/+320, translation +336),
    // NOT the passed lTransform arg (they coincide for the Prepare call but differ for UpdateJoint,
    // which passes GetRigidBodyTransform()), then offset by the spec mesh offset
    // *(mpDeformableObject->...+6368) (v8 = lvx[r8+1664] - lvx[r8+1632]).
    // =========================================================================================
    void PhysicalBodyPart::CalcBoundingBox(Matrix44Affine lTransform)
    {
        Vector3 lvBoundingBoxMin, lvBoundingBoxMax;
        CalculateBoundingBoxExtents(lvBoundingBoxMin, lvBoundingBoxMax);   // v18=min, v19=max

        // half = (max - min) * 0.5  (vsubfp v11 ; vmulfp by 0.5 (vcfsx v0,1,1 == 0.5)).
        Vector3 lHalf = {
            (lvBoundingBoxMax.x - lvBoundingBoxMin.x) * 0.5f,
            (lvBoundingBoxMax.y - lvBoundingBoxMin.y) * 0.5f,
            (lvBoundingBoxMax.z - lvBoundingBoxMin.z) * 0.5f,
            0.0f
        };

        // mBoundingBoxHalfDimensions = max(half, KV_MIN_BBOX_HALF_SIZE) (vmaxfp v13 against
        // &unk_82FB9DD0). ⭐ 2026-08-27: this is the HALF floor (0.05), a DIFFERENT static from the
        // full-extent floor (0.1) at &unk_82FB96E0 -- both were zero placeholders, so the two were
        // indistinguishable and this site used the wrong one.
        const Vector3 lRawHalf = lHalf;
        if ( lHalf.x < KV_MIN_BBOX_HALF_SIZE.x ) lHalf.x = KV_MIN_BBOX_HALF_SIZE.x;
        if ( lHalf.y < KV_MIN_BBOX_HALF_SIZE.y ) lHalf.y = KV_MIN_BBOX_HALF_SIZE.y;
        if ( lHalf.z < KV_MIN_BBOX_HALF_SIZE.z ) lHalf.z = KV_MIN_BBOX_HALF_SIZE.z;
        mBoundingBoxHalfDimensions = lHalf;   // +416

        // [DIAG] NOT IN THE X360 BINARY. The before/after witness for CalculateSkinnedPoint: the box
        // the ten skinned control points actually produce, whether it still lands on the 0.05 half
        // floor, and -- computed with the CONSOLE'S OWN rule from DoBodyPartWorldContactGeneration
        // @0x826095E0..0x826096D4 -- the contact padding that box earns. A degenerate box is "thin"
        // (min half <= 0.15) and takes the full 0.5 m pad, i.e. a ~0.55 m near-cube collider for a
        // flat panel. DELETE-WHEN the part-box question is closed and banked.
        if ( DetachProbeOn() )
        {
            static s32 siBoxLines = 0;
            if ( siBoxLines < 400 )
            {
                ++siBoxLines;
                f32 lfMinHalf = lHalf.x;
                if ( lHalf.y < lfMinHalf ) lfMinHalf = lHalf.y;
                if ( lHalf.z < lfMinHalf ) lfMinHalf = lHalf.z;
                const bool lbFatBox = (lfMinHalf > 0.15f);
                const f32  lfPad = lbFatBox ? ((lfMinHalf < 0.5f) ? lfMinHalf : 0.5f) : 0.5f;
                *CgsDev::Log::gpDebugPrint
                    << "[part-box] id " << CgsDev::E_PRINTMODE_HEXONCE
                    << mRigidBodyId.GetBaseRigidBodyID()
                    << " type " << static_cast<s32>(mpIKPart->GetPartType())
                    << " raw (" << lRawHalf.x << ", " << lRawHalf.y << ", " << lRawHalf.z << ")"
                    << " half (" << lHalf.x << ", " << lHalf.y << ", " << lHalf.z << ")"
                    << " minHalf " << lfMinHalf
                    << (lbFatBox ? " FAT" : " THIN")
                    << " pad " << lfPad
                    << " effective " << (lfMinHalf + lfPad)
                    << " ext (" << (lvBoundingBoxMax.x - lvBoundingBoxMin.x) << ", "
                    << (lvBoundingBoxMax.y - lvBoundingBoxMin.y) << ", "
                    << (lvBoundingBoxMax.z - lvBoundingBoxMin.z) << ")\n";
            }
        }

        // centre = (max + min) * 0.5, transformed through the part's OWN mBBoxOrientation rows (the asm
        // reads this+288/+304/+320 and adds the translation row this+336 -- NOT the passed lTransform;
        // they coincide for the Prepare call but UpdateJoint passes a different transform), then offset
        // by the mesh offset (the *(mpDeformableObject ... +6368) rows at +1664/+1632 the asm subtracts).
        // The dense vmaddfp cascade is that affine transform. The mesh-offset subtrahend is rodata/
        // spec-derived and not recovered -- carried as zero so the centre lands at the transformed box
        // centre. lTransform is NOT the centre's frame here (see header note); it remains the caller's
        // pose for the parallel UpdateJoint store path and is intentionally unused in this store.
        (void)lTransform;
        const Vector3 lLocalCentre = {
            (lvBoundingBoxMax.x + lvBoundingBoxMin.x) * 0.5f,
            (lvBoundingBoxMax.y + lvBoundingBoxMin.y) * 0.5f,
            (lvBoundingBoxMax.z + lvBoundingBoxMin.z) * 0.5f,
            0.0f
        };

        // ⭐⭐⭐ RE-DECODED 2026-09-02 (deform close-out wave). What stood here was ONE affine
        // transform and a subtrahend "carried as zero" behind a FLAG that called it unrecovered
        // rodata. Both were wrong, and together they are why a shed panel is DRAWN in mid-air:
        // this member is the COM term GetEventRenderTransform subtracts, so an error in it is an
        // error in the drawn position of every detached part, and nothing else.
        // MEASURED (run wrst_A2, [detach-pose], 16 parts / 2356 samples): the published render
        // position and the physics body NEVER coincide -- separation min 0.873 m, median 2.101 m,
        // max 2.517 m, and still 1.13-2.52 m once every part is at rest. A constant floor that
        // high is the signature of a missing constant term, not of physics noise.
        //
        // The asm (0x8260AC48..0x8260ACAC), decoded with the VMX128 raw-field rule
        // `vmaddfp D,A,B,C  =>  D = A*C + B`:
        //   lwz    r3, 0x1E0(this)          ; mpDeformableObject
        //   lwz    r4, 0x1DC(this)          ; mpIKPart
        //   lwz    r8, 0x18E0(r3)           ; mpDeformableObject->mpDeformationSpec
        //   lwz    r10, 8(r4)               ; mpIKPart->mpSpec   (IKBodyPart +0x08)
        //   lvx128 v7, r8, 0x680  / v8, r8, 0x660 ; vsubfp v8, v7, v8
        //                                  ; == spec.mRigidBodyOffset (+1664)
        //                                  ;  - spec.mCurrentCOMOffset (+1632)
        //   (transform 1) rows this+0x120/0x130/0x140 + translation this+0x150  == mBBoxOrientation
        //   (transform 2) rows r10+0x00/0x10/0x20 + translation r10+0x30
        //                                  ; == IKBodyPartSpec::mGraphicsTransform, which sits at
        //                                  ;    OFFSET 0 of the spec -- the panel's rest graphics
        //                                  ;    transform, already exposed as GetPartGraphicsTransform
        //   vsubfp v0, v0, v8              ; the offset is subtracted AFTER both transforms
        //   vrlimi128 v0, v9, 1, 0 ; stvx128 v0, this+0x180   ; w lane preserved
        //
        // So there were THREE faults in one expression: the second transform was absent; the
        // subtrahend was called unrecovered when the asm reads it from two named spec members;
        // and the old code applied its (zero) offset BEFORE the transform instead of after.
        // With the subtrahend at zero the sign of the third fault was invisible.

        // ---- transform 1: the part's own bbox orientation (this+0x120..0x150) ----------------
        const Vector3 lBBoxSpaceCentre = {
            mBBoxOrientation.xAxis.x * lLocalCentre.x +
                mBBoxOrientation.yAxis.x * lLocalCentre.y +
                mBBoxOrientation.zAxis.x * lLocalCentre.z + mBBoxOrientation.wAxis.x,
            mBBoxOrientation.xAxis.y * lLocalCentre.x +
                mBBoxOrientation.yAxis.y * lLocalCentre.y +
                mBBoxOrientation.zAxis.y * lLocalCentre.z + mBBoxOrientation.wAxis.y,
            mBBoxOrientation.xAxis.z * lLocalCentre.x +
                mBBoxOrientation.yAxis.z * lLocalCentre.y +
                mBBoxOrientation.zAxis.z * lLocalCentre.z + mBBoxOrientation.wAxis.z,
            0.0f
        };

        // ---- transform 2: the panel's rest graphics transform (mpIKPart->mpSpec, rows @ +0) ---
        const rw::math::vpu::Matrix44Affine& lrPartGraphics =
            mpIKPart->GetSpec()->GetPartGraphicsTransform();
        const Vector3 lGraphicsSpaceCentre = {
            lrPartGraphics.xAxis.x * lBBoxSpaceCentre.x +
                lrPartGraphics.yAxis.x * lBBoxSpaceCentre.y +
                lrPartGraphics.zAxis.x * lBBoxSpaceCentre.z + lrPartGraphics.wAxis.x,
            lrPartGraphics.xAxis.y * lBBoxSpaceCentre.x +
                lrPartGraphics.yAxis.y * lBBoxSpaceCentre.y +
                lrPartGraphics.zAxis.y * lBBoxSpaceCentre.z + lrPartGraphics.wAxis.y,
            lrPartGraphics.xAxis.z * lBBoxSpaceCentre.x +
                lrPartGraphics.yAxis.z * lBBoxSpaceCentre.y +
                lrPartGraphics.zAxis.z * lBBoxSpaceCentre.z + lrPartGraphics.wAxis.z,
            0.0f
        };

        // ---- the COM-space -> rigid-body-space shift, subtracted AFTER both transforms --------
        const StreamedDeformationSpec* lpVehicleSpec = mpDeformableObject->GetDeformationSpec();
        const Vector3 lComToRigidBody = {
            lpVehicleSpec->mRigidBodyOffset.x - lpVehicleSpec->mCurrentCOMOffset.x,
            lpVehicleSpec->mRigidBodyOffset.y - lpVehicleSpec->mCurrentCOMOffset.y,
            lpVehicleSpec->mRigidBodyOffset.z - lpVehicleSpec->mCurrentCOMOffset.z,
            0.0f
        };

        const Vector3 lTransformedCentre = {
            lGraphicsSpaceCentre.x - lComToRigidBody.x,
            lGraphicsSpaceCentre.y - lComToRigidBody.y,
            lGraphicsSpaceCentre.z - lComToRigidBody.z,
            0.0f
        };

        // Store the box centre into mLocalInitialComPositionPlusMaxJointAngle (+384), xyz only --
        // vrlimi128 v0, v9, 1, 0 preserves the existing w lane (the max-joint-angle scalar).
        mLocalInitialComPositionPlusMaxJointAngle.SetVector3(lTransformedCentre);
    }

    // =========================================================================================
    // UpdateBoundingBox @ 0x8260ACC8  (the no-publish, recompute-only overload)
    //
    // Recompute mBoundingBoxHalfDimensions (+416) from CalculateBoundingBoxExtents:
    //   half = (max - min) * 0.5, max'd against the min-bbox floor (&unk_82FB9DD0). No scene publish.
    // =========================================================================================
    void PhysicalBodyPart::UpdateBoundingBox()
    {
        Vector3 lvBoundingBoxMin, lvBoundingBoxMax;
        CalculateBoundingBoxExtents(lvBoundingBoxMin, lvBoundingBoxMax);   // v8=min, v9=max

        Vector3 lHalf = {
            (lvBoundingBoxMax.x - lvBoundingBoxMin.x) * 0.5f,
            (lvBoundingBoxMax.y - lvBoundingBoxMin.y) * 0.5f,
            (lvBoundingBoxMax.z - lvBoundingBoxMin.z) * 0.5f,
            0.0f
        };
        if ( lHalf.x < KV_MIN_BBOX_HALF_SIZE.x ) lHalf.x = KV_MIN_BBOX_HALF_SIZE.x;   // &unk_82FB9DD0
        if ( lHalf.y < KV_MIN_BBOX_HALF_SIZE.y ) lHalf.y = KV_MIN_BBOX_HALF_SIZE.y;
        if ( lHalf.z < KV_MIN_BBOX_HALF_SIZE.z ) lHalf.z = KV_MIN_BBOX_HALF_SIZE.z;
        mBoundingBoxHalfDimensions = lHalf;   // +416
    }

    // =========================================================================================
    // CalculateAABBExtents @ 0x825E2EA0
    //
    // Compute the world axis-aligned bbox half-extents from mBoundingBoxHalfDimensions (+416) projected
    // through the part's bbox orientation rows (the *(mpIKPart->GetSpec()+8)+64.. matrix). The asm forms
    // the 8 signed combinations of the three oriented half-axes (half.x*row0 +/- half.y*row1 +/-
    // half.z*row2), takes the per-lane abs (vandc against the sign mask), and max-reduces them into the
    // AABB half-extent vector, storing it to the r3 out register (the function returns it as a Vector3).
    // =========================================================================================
    Vector3 PhysicalBodyPart::CalculateAABBExtents()
    {
        // The three oriented half-axes: half-dimension scalar * each orientation row. The asm reads the
        // orientation rows from *(*(mpIKPart->GetSpec()+8)+8)+64.. (the bbox-skin orientation matrix);
        // modelled via mBBoxOrientation's basis rows (the part's stored oriented-box basis).
        const Vector3& lHalf = mBoundingBoxHalfDimensions;   // +416 (broadcast per lane in the asm)

        const Vector3 lAxisX = { mBBoxOrientation.xAxis.x * lHalf.x, mBBoxOrientation.xAxis.y * lHalf.x,
                                 mBBoxOrientation.xAxis.z * lHalf.x, 0.0f };
        const Vector3 lAxisY = { mBBoxOrientation.yAxis.x * lHalf.y, mBBoxOrientation.yAxis.y * lHalf.y,
                                 mBBoxOrientation.yAxis.z * lHalf.y, 0.0f };
        const Vector3 lAxisZ = { mBBoxOrientation.zAxis.x * lHalf.z, mBBoxOrientation.zAxis.y * lHalf.z,
                                 mBBoxOrientation.zAxis.z * lHalf.z, 0.0f };

        // The AABB half-extent is |axisX| + |axisY| + |axisZ| per lane (the max-reduction over the 8
        // signed corners collapses to the sum of the per-lane absolute axis contributions).
        auto lfAbs = [](f32 lf) { return lf < 0.0f ? -lf : lf; };
        Vector3 lAABBDimensions = {
            lfAbs(lAxisX.x) + lfAbs(lAxisY.x) + lfAbs(lAxisZ.x),
            lfAbs(lAxisX.y) + lfAbs(lAxisY.y) + lfAbs(lAxisZ.y),
            lfAbs(lAxisX.z) + lfAbs(lAxisY.z) + lfAbs(lAxisZ.z),
            0.0f
        };
        return lAABBDimensions;
    }

    // =========================================================================================
    // GetBoundingBox @ 0x825E7D28
    //
    // Write the part's world oriented bounding box out. The asm transforms the bbox-orientation basis
    // rows (this+288/+304/+320 -- mBBoxOrientation x/y/z axes, broadcast per-lane) through the body's
    // current transform (this+0/+16/+32/+48 -- mRwBody.mTransform rows), producing the world-space box
    // orientation (v26/v27/v28) and centre (v29), then hands them to CgsGeometric::Box::Set with the
    // half-dimensions (this+416 -- mBoundingBoxHalfDimensions).
    //
    // CgsGeometric::Box is forward-declared only here. The world box rows the asm builds are computed
    // here; the Set call is modelled by writing the rows + centre + half-dims into the out-box through
    // a raw-offset layout matching the asm's Box::Set stores.
    // ✅ FLAG CLEARED 2026-08-19 (wave Q6, cluster `addprim`): the layout is no longer provisional and
    // the guess was RIGHT. Box is homed at GameShared/GameClasses/Geometric/Primitives/CgsBox.h --
    // the console's own home, proven by the file string Box::Set @0x825E6918 passes -- with
    // DWARF-authoritative members +0x00 Matrix44Affine mTransform and +0x40 Vector3Plus
    // mDimensionsAndFatness. FOLLOW-UP, deliberately NOT done here: the raw-offset writes below can
    // now become a by-name Box::Set call. That is a separate de-duplication, not a comment fix.
    // =========================================================================================
    void PhysicalBodyPart::GetBoundingBox(CgsGeometric::Box* lpBoxOut) const
    {
        // Body transform rows (mRwBody.mTransform): row0 @ +0, row1 @ +16, row2 @ +32, pos @ +48.
        const Matrix44Affine lBodyTransform = mRwBody.GetTransform();

        // Transform each oriented-basis row + the box centre through the body transform (the vmaddfp
        // cascade with each mBBoxOrientation row's lanes broadcast through the body rows).
        auto lTransformDir = [&](const Vector3& lDir) -> Vector3 {
            return Vector3{
                lBodyTransform.xAxis.x * lDir.x + lBodyTransform.yAxis.x * lDir.y + lBodyTransform.zAxis.x * lDir.z,
                lBodyTransform.xAxis.y * lDir.x + lBodyTransform.yAxis.y * lDir.y + lBodyTransform.zAxis.y * lDir.z,
                lBodyTransform.xAxis.z * lDir.x + lBodyTransform.yAxis.z * lDir.y + lBodyTransform.zAxis.z * lDir.z,
                0.0f
            };
        };
        const Vector3 lWorldRight  = lTransformDir(mBBoxOrientation.xAxis);   // v26
        const Vector3 lWorldUp     = lTransformDir(mBBoxOrientation.yAxis);   // v27
        const Vector3 lWorldAt     = lTransformDir(mBBoxOrientation.zAxis);   // v28
        const Vector3 lWorldCentre = {                                        // v29 (full affine of centre)
            lBodyTransform.xAxis.x * mBBoxOrientation.wAxis.x + lBodyTransform.yAxis.x * mBBoxOrientation.wAxis.y +
                lBodyTransform.zAxis.x * mBBoxOrientation.wAxis.z + lBodyTransform.wAxis.x,
            lBodyTransform.xAxis.y * mBBoxOrientation.wAxis.x + lBodyTransform.yAxis.y * mBBoxOrientation.wAxis.y +
                lBodyTransform.zAxis.y * mBBoxOrientation.wAxis.z + lBodyTransform.wAxis.y,
            lBodyTransform.xAxis.z * mBBoxOrientation.wAxis.x + lBodyTransform.yAxis.z * mBBoxOrientation.wAxis.y +
                lBodyTransform.zAxis.z * mBBoxOrientation.wAxis.z + lBodyTransform.wAxis.z,
            0.0f
        };

        // CgsGeometric::Box::Set(box, {right,up,at,centre}, halfDims). Modelled as a raw-offset write
        // into the out-box (Matrix44Affine basis @ +0, half-dims @ +64). ✅ CONFIRMED layout, not
        // provisional -- CgsGeometric::Box is homed at CgsBox.h; see the banner above.
        char* lpBox = reinterpret_cast<char*>(lpBoxOut);
        *reinterpret_cast<Vector3*>(lpBox +  0) = lWorldRight;
        *reinterpret_cast<Vector3*>(lpBox + 16) = lWorldUp;
        *reinterpret_cast<Vector3*>(lpBox + 32) = lWorldAt;
        *reinterpret_cast<Vector3*>(lpBox + 48) = lWorldCentre;
        *reinterpret_cast<Vector3*>(lpBox + 64) = mBoundingBoxHalfDimensions;
    }

    // =========================================================================================
    // UpdateJoint @ 0x8260B0F8
    //
    // Integrate the joint one step (gravity + restitution + penetration resolve), updating the packed
    // joint rotation (+352 w) and joint velocity (+368 w) and the body's linear/angular velocities.
    //
    // The X360 body is an extremely dense VMX128 cascade implementing:
    //   * tripwires: GetActiveJointIndex() != KU8_NO_ACTIVE_JOINT ; IsJoinedToVehicle() ;
    //     mi8ActiveJointsTagPointIndex != -1 ;
    //   * pull the active-joint spec (GetActiveJointSpec via sub_825C1170 == IKBodyPart::GetJointSpec);
    //   * rebuild the oriented box for the current pose (CalcBoundingBox);
    //   * compute the joint anchor/axis in world space, apply the hinged-part gravity
    //     (KVF_HINGED_PART_GRAVITY), joint restitution (KVF_JOINT_RESTITUTION) and penetration
    //     resolution (KVF_JOINT_PENETRATION_RESOLUTION) against the accumulated collision, and integrate
    //     the joint angle/velocity with the chebyshev/atan polynomial tables (&unk_82000BD0..C20) for
    //     the angle clamp;
    //   * store the new joint rotation (+352 w), joint velocity (+368 w) and the body's velocity rows.
    //
    // The polynomial coefficient tables, the gravity/restitution/resolution tuning vectors, and the
    // joint-relax constant (0.975 recovered) are XEX rodata that is NOT in the per-function exports.
    // Per project rule they are FLAGGED placeholders. The control flow -- the tripwires, the
    // CalcBoundingBox rebuild, the joint-state stores, and the call order -- is reproduced exactly; the
    // numeric integration stays inert (no joint motion) until the tuning rodata is recovered, rather
    // than fabricating an integrator from invented coefficients.
    // =========================================================================================
    void PhysicalBodyPart::UpdateJoint(VecFloat lvfTimeStep)
    {
        (void)lvfTimeStep;

        // mpIKPart->GetActiveJointIndex() != KU8_NO_ACTIVE_JOINT.
        CGS_ASSERT(mpIKPart->GetActiveJointIndex() != IKBodyPart::KU8_NO_ACTIVE_JOINT,
                   "mu8ActiveJointIndex != KU8_NO_ACTIVE_JOINT");
        // IsJoinedToVehicle().
        CGS_ASSERT(mbJoinedToVehicle, "IsJoinedToVehicle()");

        // Rebuild the oriented box for the current pose (the asm's CalcBoundingBox(this, v60), where v60
        // is the inverse-mass-weighted body transform it just assembled).
        CalcBoundingBox(GetRigidBodyTransform());

        // mi8ActiveJointsTagPointIndex != -1.
        CGS_ASSERT(mi8ActiveJointsTagPointIndex != -1, "mi8ActiveJointsTagPointIndex != -1");

        // ⛔⛔ 2026-09-05 (hinge-geometry wave): sub_825C1170 IS `IKBodyPart::GetTagPoint`, NOT
        // `GetJointSpec`. This line read `mpIKPart->GetJointSpec(mi8ActiveJointsTagPointIndex)` --
        // a TAG-POINT index used to subscript the JOINT-SPEC array, which is a different, SHORTER
        // array with no bound check on it (`&mpaJointSpecs[liIndex]`, BrnIKBodyPartSpec.h). On
        // PUSMC01 a jointed panel has ~4 tag points and 1-2 joints, so tag index 3 addresses
        // 0xC0 bytes past a one-element array. It never faulted only because the result was
        // `(void)`-cast while the integrator is inert -- an OOB the moment that lands.
        // The asm is unambiguous (0x8260B254..0x8260B264):
        //     lbz  r11, 0x1E8(this)   ; mi8ActiveJointsTagPointIndex
        //     lwz  r3,  0x1DC(this)   ; mpIKPart
        //     extsb r4, r11 ; bl sub_825C1170
        //     lwz  r6,  0x10(r3)      ; the returned record's +0x10 -- a POINTER (TagPointSpec*)
        // and sub_825C1170 itself asserts "liIndex >= 0" and "liIndex < GetNumberOfTagPoints()"
        // (its own strings), then returns `*(this+4) + liIndex*32` -- the 32-byte TAG POINT
        // records off IKBodyPart+4. GetJointSpec would stride sizeof(DeformationJointSpec) off
        // the SPEC. Two bound asserts the console carries were being skipped as well.
        // FLAG: the gravity/restitution/resolution tuning and the angle-clamp polynomial tables
        // are rodata-not-recovered, so the integrated delta is still inert.
        const TagPoint* lpActiveTagPoint = mpIKPart->GetTagPoint(mi8ActiveJointsTagPointIndex);
        (void)lpActiveTagPoint;
        (void)KF_JOINT_RELAX;   // recovered 0.975 relax factor applied by the (inert) integrator

        // INERT integration step: with the tuning rodata unrecovered, the joint rotation/velocity and
        // the body velocity rows are left at their current values (the asm's vmaddfp cascade collapses
        // to a no-op delta when the gravity/restitution/resolution vectors are zero). The packed lanes
        // are re-stored unchanged, matching the asm's store structure without inventing a delta.
        // (When KVF_HINGED_PART_GRAVITY / KVF_JOINT_RESTITUTION / KVF_JOINT_PENETRATION_RESOLUTION and
        // the &unk_82000BD0.. polynomial tables are recovered, the integrator delta is applied here.)
    }

    // =========================================================================================
    // TestJointForBreaking @ 0x8260C0F8
    //
    // Decide whether the joint breaks this frame; if so, detach the part onto the sim/output buffers.
    // Returns true iff it broke.
    //
    // The X360 control flow (reproduced exactly):
    //   1. tripwire GetActiveJointIndex() != KU8_NO_ACTIVE_JOINT.
    //   2. EARLY "no break": if the active joint's detach threshold (spec+52) <= -0.89999998 -> goto
    //      LABEL_20 (return false) -- this joint never breaks.
    //   3. EARLY "no break" if ANY of:
    //        a. rotation proportion (GetJointRotationProportion) <= the rotation gate (&unk_82FB9E00); OR
    //        b. the IK spec's part-type word (*(spec+8)+476) == 3 (a non-breaking part class); OR
    //        c. !IKBodyPart::CheckSensorForcesForJointDetachment().
    //   4. Otherwise compute the joint force / penetration magnitude and break if EITHER:
    //        a. force-along-axis * kfJointForceMultiplier > GetActiveJointSpec()->GetMaxStress(); OR
    //        b. penetration.w * kfJointPenetrationMultiplier > GetActiveJointSpec()->GetMaxStress().
    //      (v34 is the OR of the two break predicates.)
    //   5. On break: AddToSim(this, transform, &velocity) (hand the part to the sim as a free body),
    //      clear mbJoinedToVehicle (+484 = 0), build the DetachedPartNotificationEvent and
    //      AddEventSafeAppend it onto the sim OutputBuffer's notification queue, return true.
    //
    // Tripwires inside the break path: IsJoinedToVehicle() (:949), mpIKPart != NULL (:950),
    // GetActiveJointSpec() != NULL (:951).
    //
    // ⭐ 2026-08-27: kfJointForceMultiplier (0.4) / kfJointPenetrationMultiplier (1.5) and the
    // rotation-proportion gate (0.3) are ALL RECOVERED now -- see the constant block at the top of this
    // file for the initialiser addresses and the calibration control. Arm (4b) is therefore FAITHFUL
    // end to end (its magnitude was already the asm's own +400 w lane).
    // ⭐⭐⭐ 2026-08-27 (detach-2 wave): ARM (4a) IS NOW FAITHFUL TOO. The vperm/vaddfp chain at
    // 0x8260C390..0x8260C3CC IS DECODED -- it is a cross product, and the whole arm was carrying an
    // invented magnitude AND was missing a gate the console has.
    //
    // THE CHAIN, instruction by instruction. `vpermwi128 x, 0x63` is the word-permute-immediate with
    // selector fields [01,10,00,11] == lanes (y, z, x, w) -- the yzx rotation. With A = the vehicle
    // body's angular velocity and D = (part position - vehicle position):
    //     0x8260C3A8  vsubfp128 v13, v126, v0        D   = partPos - bodyPos
    //     0x8260C3B0  vpermwi128 v11, v0, 0x63       perm(A)
    //     0x8260C3BC  vpermwi128 v13, v13, 0x63      perm(D)
    //     0x8260C3C0  vmulfp128  v0, v0, v13         A * perm(D)
    //     0x8260C3C4  vnmsubfp   v0, v11, v0, v10    (A*perm(D)) - perm(A)*D
    //     0x8260C3C8  vpermwi128 v0, v0, 0x63        -> cross(A, D), lanes rotated back
    //     0x8260C3CC  vaddfp128  v124, v0, v12       + the body's LINEAR velocity
    // i.e. v124 = omega x r + v -- THE WORLD VELOCITY OF THE PART'S ORIGIN under the vehicle's rigid
    // motion. (`vnmsubfp vD,vA,vC,vB` is vB - vA*vC, and IDA prints the raw field order (vD,vA,vB,vC);
    // the operand order is the same one calibrated against HandleContactWithLeanProp in AddToSim's
    // banner.) The lane pattern is the textbook SIMD cross product and it closes exactly.
    //
    // ⭐ AND THE POINTER CHAIN CORROBORATES A FINDING THIS TREE ALREADY PAID FOR. The asm does
    //     lwz r10, 0x194C(mpDeformableObject)   ; the attached vehicle physics
    //     addi r11, r10, 0x10                   ; <-- and THEN indexes 0x30/0x40/0x50 off r11
    // The +0x10 is the vptr adjustment: SimpleVehiclePhysics introduces the vtable, so the
    // non-polymorphic ExternalPhysicsBody base subobject sits 16 bytes into the derived object. That
    // is the SAME +16 the walls-leg-4 wave discovered the hard way (BrnDeformableObject.h's
    // GetVehicleBody banner: a reinterpret_cast that missed it read every transform row one row low
    // and stomped the vptr). Two unrelated functions, one adjustment. So r11+0x30/+0x40/+0x50 are
    // ExternallySimulatedBody's mTransform.wAxis / mLinearVelocity / mAngularVelocity, by name.
    //
    // ⛔ AND ARM (4a) IS GATED, WHICH THE PREVIOUS SPELLING DID NOT HAVE AT ALL. Before the chain
    // above, 0x8260C364..0x8260C388 computes `axis * dot(axis, mWorldPenetrationPlusCollisionMagnitude)`
    // , takes its abs, and compares it against a splat of stru_8208F620 lane 0 (== 1.1920929e-07,
    // FLT_EPSILON, byte-read). The branch tests CR6 bit 26 -- the "NONE TRUE" bit -- and jumps PAST
    // arm (4a) when it is set. So arm (4a) only runs when the joint axis has a non-negligible
    // projection of the accumulated world penetration. Running it unconditionally, as this file did,
    // is a strictly LOOSER predicate than the console's.
    // ⇒ the arm's old magnitude (`mWorldPenetrationPlusCollisionMagnitude.GetPlus()`, the +432 w lane)
    // is retired: that scalar's VECTOR half turns out to be the gate's input, not the magnitude.
    // =========================================================================================
    bool PhysicalBodyPart::TestJointForBreaking(CgsPhysics::PhysicsSimulationIO::InputBuffer* lpSimInput,
                                                BrnPhysics::PhysicsModuleIO::OutputBuffer* lpOutput)
    {
        // (1) tripwire.
        CGS_ASSERT(mpIKPart->GetActiveJointIndex() != IKBodyPart::KU8_NO_ACTIVE_JOINT,
                   "mu8ActiveJointIndex != KU8_NO_ACTIVE_JOINT");

        const DeformationJointSpec* lpActiveJoint = mpIKPart->GetActiveJointSpec();

        // (2) early "never breaks": detach threshold (asm reads spec+52 == mfJointDetachThreshold)
        // <= -0.9 -> return false. The asm dereferences spec+52 UNCONDITIONALLY (no null guard), so the
        // 'lpActiveJoint &&' guard is removed to match.
        if ( lpActiveJoint->GetMaxStress() <= KF_JOINT_DETACH_DISABLED_THRESHOLD )
        {
            return false;   // LABEL_20: _restvmx_121(0)
        }

        // (3) early "no break" gates.
        //   a. rotation proportion < gate (STRICT). The asm is vcmpgtfp128(gate, proportion) -> the
        //      early-out fires when gate > proportion, i.e. proportion < gate.
        const VecFloat lJointRotationProportion = GetJointRotationProportion();
        if ( lJointRotationProportion.x < KF_ROTATION_PROPORTION_GATE )   // vcmpgtfp gate,proportion
        {
            return false;
        }
        //   b. part-type word == 3 (the *(*(mpIKPart->GetSpec())+476) == 3 test). GetPartType() reads
        //      the same word.
        if ( mpIKPart->GetPartType() == 3 )
        {
            return false;
        }
        //   c. sensor-force detachment gate. FLAG: the bool arg (false) is a GUESS -- the X360 Hex-Rays
        //   dropped the arg list for CheckSensorForcesForJointDetachment, so the argument value is not
        //   recovered from the asm.
        if ( !mpIKPart->CheckSensorForcesForJointDetachment(false) )
        {
            return false;
        }

        // (4) break predicate. Compute the joint force vector (the asm builds it from the body's velocity
        // /transform and the joint's rotation axis -- GetActiveJointSpec()'s rotation-axis lane). The
        // force-along-axis magnitude and the accumulated penetration magnitude (+400 w lane,
        // mLocalInitialJointPositionPlusLimitStress packing, reached as the joint penetration) are each
        // scaled by their multiplier and compared against GetMaxStress().
        const f32 lfMaxStress = lpActiveJoint ? lpActiveJoint->GetMaxStress() : 0.0f;

        bool lbBreak = false;   // v34

        // (4a) joint force along axis -- FAITHFUL as of 2026-08-27; see the banner for the decode.
        {
            // The joint's rotation axis, rotated out of the part's own frame by its rw-body basis
            // (0x8260C354..0x8260C360: xAxis*s.x + yAxis*s.y + zAxis*s.z, each lane splatted from
            // the spec's mJointAxis at spec+0x10).
            const Vector3 lLocalAxis = lpActiveJoint->GetRotationAxis();
            const Matrix44Affine lPartTransform = mRwBody.GetTransform();
            const Vector3 lWorldAxis = {
                lPartTransform.xAxis.x * lLocalAxis.x + lPartTransform.yAxis.x * lLocalAxis.y
                    + lPartTransform.zAxis.x * lLocalAxis.z,
                lPartTransform.xAxis.y * lLocalAxis.x + lPartTransform.yAxis.y * lLocalAxis.y
                    + lPartTransform.zAxis.y * lLocalAxis.z,
                lPartTransform.xAxis.z * lLocalAxis.x + lPartTransform.yAxis.z * lLocalAxis.y
                    + lPartTransform.zAxis.z * lLocalAxis.z,
                0.0f
            };

            // THE GATE (0x8260C364..0x8260C388): |axis * dot(axis, worldPenetration)| must exceed
            // FLT_EPSILON in at least one lane. CR6 bit 26 is "none true", and the branch skips this
            // whole arm when it is set.
            const Vector3 lWorldPenetration = mWorldPenetrationPlusCollisionMagnitude.GetVector3();
            const f32 lfPenetrationAlongAxis = lWorldAxis.x * lWorldPenetration.x
                                             + lWorldAxis.y * lWorldPenetration.y
                                             + lWorldAxis.z * lWorldPenetration.z;
            auto lfAbs = [](f32 lf) { return lf < 0.0f ? -lf : lf; };
            const bool lbAxisEngaged =
                   lfAbs(lWorldAxis.x * lfPenetrationAlongAxis) > KF_INERTIA_DEGENERATE_EPSILON
                || lfAbs(lWorldAxis.y * lfPenetrationAlongAxis) > KF_INERTIA_DEGENERATE_EPSILON
                || lfAbs(lWorldAxis.z * lfPenetrationAlongAxis) > KF_INERTIA_DEGENERATE_EPSILON;

            if ( lbAxisEngaged )
            {
                // v124 = omega x (partPos - bodyPos) + v  -- the world velocity of the part's origin
                // under the vehicle's rigid motion (the decoded cross-product chain).
                const ExternalPhysicsBody& lrVehicleBody = mpDeformableObject->GetVehicleBody();
                const Vector3 lBodyPos = lrVehicleBody.GetTransform().wAxis;        // body +0x30
                const Vector3 lBodyLinVel = lrVehicleBody.GetLinearVelocity();      // body +0x40
                const Vector3 lBodyAngVel = lrVehicleBody.GetAngularVelocity();     // body +0x50
                const Vector3 lLever = { lPartTransform.wAxis.x - lBodyPos.x,
                                         lPartTransform.wAxis.y - lBodyPos.y,
                                         lPartTransform.wAxis.z - lBodyPos.z, 0.0f };

                const Vector3 lPointVelocity = {
                    lBodyAngVel.y * lLever.z - lBodyAngVel.z * lLever.y + lBodyLinVel.x,
                    lBodyAngVel.z * lLever.x - lBodyAngVel.x * lLever.z + lBodyLinVel.y,
                    lBodyAngVel.x * lLever.y - lBodyAngVel.y * lLever.x + lBodyLinVel.z,
                    0.0f
                };

                // vmsum3fp128 @0x8260C3D4, vandc @0x8260C410 (abs), vmulfp128 @0x8260C414
                // (* kfJointForceMultiplier), vcmpgtfp. @0x8260C418 (> maxStress).
                const f32 lfJointForceMagnitude = lPointVelocity.x * lWorldAxis.x
                                                + lPointVelocity.y * lWorldAxis.y
                                                + lPointVelocity.z * lWorldAxis.z;
                const f32 lfScaledForce = lfAbs(lfJointForceMagnitude) * KF_JOINT_FORCE_MULTIPLIER;
                if ( lfScaledForce > lfMaxStress )
                {
                    lbBreak = true;
                }
            }
        }

        // (4b) joint penetration. The asm reads the joint penetration scalar (this+400 w lane) and
        // scales by kfJointPenetrationMultiplier.
        {
            const f32 lfPenetration = mLocalInitialJointPositionPlusLimitStress.GetPlus();   // +400 w
            const f32 lfScaledPenetration = lfPenetration * KF_JOINT_PENETRATION_MULTIPLIER;
            if ( lfScaledPenetration > lfMaxStress )
            {
                lbBreak = true;   // v34 = 1
            }
        }

        if ( !lbBreak )
        {
            return false;   // LABEL_20: _restvmx_121(0)
        }

        // (5) break path tripwires.
        CGS_ASSERT(mbJoinedToVehicle, "IsJoinedToVehicle()");
        CGS_ASSERT(mpIKPart != 0, "mpIKPart != NULL");
        CGS_ASSERT(mpIKPart->GetActiveJointSpec() != 0, "mpIKPart->GetActiveJointSpec() != NULL");

        // Hand the part to the sim as a free body: AddToSim(this, transform, velocity). The asm passes
        // v4 (the body transform) and &v67 (the assembled detach velocity). Use the body's current
        // transform + linear/angular velocity (the recovered detach seed).
        AddToSim(lpSimInput, GetRigidBodyTransform(), GetLinearVelocity(), mRwBody.GetAngularVelocity());

        // *(this+484) = 0 (no longer joined).
        mbJoinedToVehicle = false;

        // Build + emit the DetachedPartNotificationEvent. FAITHFUL as of 2026-08-27 (detach-2 wave);
        // the `EmitDetachedPartNotification(buffer, const void* blob)` hook this used to go through is
        // DELETED. That hook was a fabricated API in two ways at once: its second parameter was an
        // untyped blob (the event is a named 32-byte record with three named fields), and the pointer
        // handed to it here -- &mRigidBodyId -- is not one of the three fields the console writes.
        // The asm at 0x8260C4B8..0x8260C514 assembles the record on the stack and appends it:
        //     stvx128 v126 -> event+0x00   mPointOnA  == v126 == *(this+0x30), the part's WORLD
        //                                  position (the same register the arm-4a lever arm uses)
        //     lwz 0x1D8(this) -> +0x10     mVehicleId == mGlobalVehicleId
        //     lwz 8(mpIKPart) ; lwz 0x1DC(spec) -> +0x14   meType == the IK spec's part type
        //     bl OutputBuffer::GetDeformationOutputInterface ; addi r3,r3,0x3A0
        //     bl BaseEventQueue<DetachedPartNotificationEvent>::AddEventSafe
        // +0x3A0 is mDetachedPartNotificationQueue, exactly where BrnDeformationOutputInterface.h:77
        // already puts it, so the accessor + offset agree with a layout this tree had committed.
        // ⚠️ HONEST ABOUT THE PAYOFF: NOTHING IN THIS TREE READS THAT QUEUE YET (only Construct /
        // Clear / Append touch it). The PS3 consumers are BrnSound -- this is the hook crash audio
        // will hang off. Landing it retires a stub and an invented signature; it does not yet make a
        // sound. And this particular call site is on the hinge path, which has never run (nHinged 0).
        {
            Deformation::DetachedPartNotificationEvent lNotification;
            lNotification.mPointOnA  = mRwBody.GetTransform().wAxis;
            lNotification.mVehicleId = mGlobalVehicleId;
            lNotification.meType     = static_cast<EBodyParts>(mpIKPart->GetPartType());

            lpOutput->GetDeformationOutputInterface()
                    ->mDetachedPartNotificationQueue.AddEventSafe(lNotification);
        }

        return true;   // _restvmx_121(1)
    }

    // =========================================================================================
    // UpdateRW @ 0x825E7998   [not executed in goal trace]
    //
    // Push this part's pose into the RenderWare physics sim for the current timestep. Skipped
    // entirely (no event emitted) unless the part is dirty -- the asm short-circuits on
    //   if ( mbNeedsWritingIntoRenderware || LimitVelocities(lvfTimeStep) )
    // (the `lbz r11,0x1E7(r30)` / `bne` is the +487 dirty flag; only if it is clear does the
    // `bl LimitVelocities` run, and the body is entered when EITHER is true -- a logical OR with
    // LimitVelocities short-circuited away when already dirty).
    //
    // When entered:
    //   1) mRwBody.CalculateNewVelocity() -- integrate the accumulated forces into the body's
    //      linear/angular velocity (the base integrate checkpoint).
    //   2) Assemble the InUpdateExternalBody event blob from the body's id + freshly-integrated
    //      pose: { mRigidBodyId (qword), mRwBody.GetTransform() (4 rows), mRwBody linear vel,
    //      mRwBody angular vel } -- the asm's `ld r7,0x1D0` (id) + the six `lvx128`/`stvx128`
    //      copies of this+0/+16/+32/+48 (transform), this+64 (linear vel), this+80 (angular vel)
    //      into the stacked event slots starting at &v110.
    //   3) Three non-gating finiteness tripwires on the event (the `vspltw`+`vcmpeqfp.` per-lane
    //      self-equality NaN checks, ANDed across lanes/rows): IsValid(angularVel), IsValid(vel),
    //      IsValid(transform) -- in that asm order.
    //   4) Emit the event onto the sim InputBuffer's InUpdateExternalBody queue
    //      (`CgsPhysi`(InputBuffer) -> channel; channel->AddEvent(&event)), modelled through the
    //      flagged EmitUpdateExternalBodyEvent hook.
    //   5) Clear mbNeedsWritingIntoRenderware (+487 = 0).
    //
    // The packed event layout (16-byte-aligned stack slots the asm builds at &v110.., handed to
    // AddEvent) is reproduced as a POD blob here; the concrete InUpdateExternalBody event type +
    // the queue's AddEvent are owned by the CgsPhysics sim-IO TU (not homed in this family), so
    // the emit goes through the provisional hook exactly as the detach-notification emit does.
    // =========================================================================================
    void PhysicalBodyPart::UpdateRW(CgsPhysics::PhysicsSimulationIO::InputBuffer* lpSimInput,
                                    VecFloat lvfTimeStep)
    {
        // Dirty gate: enter only if already flagged for RW write OR a velocity clamp was applied
        // this step. LimitVelocities is short-circuited away when the part is already dirty
        // (matching the asm's `bne` past the `bl LimitVelocities`).
        if ( !mbNeedsWritingIntoRenderware && !LimitVelocities(lvfTimeStep) )   // +487 == 0 && no clamp
        {
            return;
        }

        // Integrate the accumulated forces/torques/impulses into the body velocity (the asm's
        // `mr r3,r30 ; bl CalculateNewVelocity` -- this==&mRwBody since mRwBody is the first member).
        // ⚠️ The TIMESTEP was being dropped here. The X360 stashes this function's own v1 into
        // v127 on entry (0x825E79B0) and replays `vmr128 v1,v127` in the instruction immediately
        // before the branch (0x825E79D4 / 0x825E79DC), so the same dt this function received is
        // what the integrator receives. CalculateNewVelocity's signature has been corrected to
        // take it; without it the body's forces integrated over a zero step.
        mRwBody.CalculateNewVelocity(lvfTimeStep);

        // Assemble the InUpdateExternalBody event blob (the stacked &v110.. slots). 16-byte-aligned
        // POD matching the asm's stvx128 store layout: id qword, then the 4 transform rows, then the
        // linear + angular velocity rows.
        struct UpdateExternalBodyEvent
        {
            BurnoutBodyPartID mBodyId;        // event+0  (the `ld r7,0x1D0` qword, 16-byte slot)
            Matrix44Affine    mTransform;     // event+16 (rows from this+0/+16/+32/+48)
            Vector3           mVel;           // event+80 (this+64 linear velocity)
            Vector3           mAngularVel;    // event+96 (this+80 angular velocity)
        };

        UpdateExternalBodyEvent lEvent;
        lEvent.mBodyId      = mRigidBodyId;                 // event id == this+464
        lEvent.mTransform   = mRwBody.GetTransform();       // 4 rows, this+0/+16/+32/+48
        lEvent.mVel         = mRwBody.GetLinearVelocity();  // this+64
        lEvent.mAngularVel  = mRwBody.GetAngularVelocity(); // this+80

        // Non-gating finiteness tripwires (the per-lane vcmpeqfp self-equality NaN checks), in the
        // asm's order: angular velocity, then velocity, then the full transform.
        CGS_ASSERT(rw::math::vpu::IsValid(lEvent.mAngularVel),
                   "rw::math::IsValid( lUpdateEvent.mAngularVel )");
        CGS_ASSERT(rw::math::vpu::IsValid(lEvent.mVel),
                   "rw::math::IsValid( lUpdateEvent.mVel )");
        CGS_ASSERT(rw::math::vpu::IsValid(lEvent.mTransform.xAxis)
                       && rw::math::vpu::IsValid(lEvent.mTransform.yAxis)
                       && rw::math::vpu::IsValid(lEvent.mTransform.zAxis)
                       && rw::math::vpu::IsValid(lEvent.mTransform.wAxis),
                   "rw::math::IsValid( lUpdateEvent.mTransform )");

        // Emit the event onto the sim InputBuffer's InUpdateExternalBody queue (the asm's
        // `bl CgsPhysi`(InputBuffer) -> channel ; channel->AddEvent(&event)). Modelled through the
        // flagged emit hook with the packed event blob.
        EmitUpdateExternalBodyEvent(lpSimInput, &lEvent);

        // *(this+487) = 0 -- the part is no longer dirty for RW.
        mbNeedsWritingIntoRenderware = false;
    }

    // =============================================================================================
    // LOG-ONCE GATES 2026-08-14 (walls leg 4). Declared methods/hooks the newly-mounted family
    // links against whose real bodies are NOT reconstructed yet.
    // ⛔ "ALL are dead on the junkyard path (0 physical parts / 0 hinged joints)" -- STALE as of
    // 2026-08-27. Parts detach now (mi16NumPhysicalParts 0 -> 7 on the player car in the
    // deterministic junkyard crash), so AddToSim, AddContactSpy, PostVehicleUpdate and
    // EmitDetachedPartNotification are ALL ON THE LIVE PATH. Hinged joints are still 0 -- nothing
    // has taken the hinge arm yet -- so the joint-side gates remain untested rather than dead.
    // Reconstruct and DELETE each gate; AddToSim @0x8260AD38 is the one that matters most (it is
    // why a shed panel never moves). See its own banner below.
    // =============================================================================================
    void PhysicalBodyPart::AddContactSpy(ContactSpyData* /*lpContactSpyData*/)
    {
        static bool sbLoggedACS = false;
        if ( !sbLoggedACS )
        {
            sbLoggedACS = true;
            if ( CgsDev::Message::gxMessageFilterFlags & 1 )
                *CgsDev::Log::gpDebugPrint << "conductor gate: PhysicalBodyPart::AddContactSpy reached but not "
                                              "reconstructed [FLAG PC boot gate]\n";
        }
        
    }

    // =============================================================================================
    // AddToSim @0x8260AD38 (310) -- RECONSTRUCTED 2026-08-27 (detach-2 wave). THE GATE IS GONE.
    //
    // WHAT THE GATE THAT STOOD HERE SAID, AND WHY IT WAS THE HEADLINE BLOCKER: "NONE OF THAT IS
    // RECONSTRUCTED. The InAddRigidBody event layout, the fat-box inertia and the sim-side consumer
    // are all absent, so a detached part is NEVER SIMULATED on this build: it does not fall, tumble
    // or bounce." Two of those three claims were STALE and the third was a name search failing:
    //   * the event layout is fully gated in CgsPhysicsSimulationIO_Events.h (InAddRigidBody stride
    //     192 + NewRigidBody's six offsets, X360-attested off ProcessAddRigidBodyQueue @0x828A2708);
    //   * the sim-side consumer is BODIED AND LIVE -- ProcessAddRigidBodyQueue drains it and
    //     PhysicsSimulationModule::AddActiveBodiesToOutputQueue @0x828A6CC8 echoes every ACTIVE
    //     body back as an OutUpdateRigidBody;
    //   * the fat-box inertia was COMMITTED THE WHOLE TIME at vendor/renderware/physics/
    //     FatBoxInertia.cpp -- it simply had no declaration in any header and was not in the bat,
    //     so no caller could reach it. [[unnamed-sub-bodies-and-env-faults]].
    // Nothing here needed reconstructing from scratch; it needed a producer written against parts
    // that already existed.
    //
    // ---- THE RECORD, OFFSET BY OFFSET (record base == var_130 == the pointer handed to AddEvent) -
    // Every console offset below is matched to the member a committed static_assert already pins,
    // so none of this is an offset cast. (NewRigidBody sits at InAddRigidBody+0x10 and Inertia at
    // NewRigidBody+0x60, so Inertia+0x18 == record+0x88 and so on.)
    //   +0x00 mID                              `ld 0x1D0(this)` @0x8260AD80 -> `std` @0x8260ADA4
    //   +0x10..+0x40 mRigidBody.mTransform     lvx128 this+0/+0x10/+0x20/+0x30 -> four stvx128
    //   +0x50 mRigidBody.mVelocity             `stvx128 v127` (v1 == lInitialLinearVelocity)
    //   +0x60 mRigidBody.mAngularVelocity      `stvx128 v126` (v2 == lInitialAngularVelocity)
    //   +0x70 mInertia.mInvTens                `stvx128 v11`  @0x8260AF04
    //   +0x80 mInertia.mInvMass                `stfs` @0x8260AE18   = 1.0f / kfPartMass
    //   +0x84 mInertia.mSpherical              `stfs` (the 1/min(x,y,z) SetInverseInertia tail)
    //   +0x88 mInertia.mMaxVelocity            `stfs` @0x8260AE0C   = kfPartMaxLinearVelocity
    //   +0x8C mInertia.mMaxOmega               `stfs` @0x8260AE00   = kfPartMaxAngularVelocity
    //   +0x90 mInertia.mLinearDrag             `stfs` @0x8260ADF4   = kfPartLinearDrag
    //   +0x94 mInertia.mAngularDrag            `stfs` @0x8260ADE8   = kfPartAngularDrag
    //   +0xA0 mRigidBody.mbSpy                 `li r10,1 ; stb`     -- TRUE (props store 0 here)
    //   +0xB0 meState                          `li r10,4 ; stw`     == rw::physics::ACTIVE_BODY
    //
    // THE MAX-VELOCITY PAIR IS **UNCROSSED** HERE, and that is worth recording because the
    // PropManager pair is not. PropManager_wQ2_04.cpp carries a measured, deliberately-unresolved
    // cross-wiring (KF_PROP_MAX_ANGULAR_VEL -> +0x18, the LINEAR clamp). This site stores
    // kfPartMaxLinearVelocity into +0x88 (mMaxVelocity, the linear clamp) and
    // kfPartMaxAngularVelocity into +0x8C (mMaxOmega). IT DOES NOT SETTLE THE PROP QUESTION and
    // is not used to: both part constants are 30.0, so this site cannot discriminate the two VALUES.
    // What it does corroborate is the OFFSET -> MEMBER map (+0x18 linear, +0x1C angular), which is
    // the half of the prop puzzle that was already twice-attested. Left as a datum, not a verdict.
    //
    // ---- THE TRANSFORM IS **READ**, NOT WRITTEN, BY THE CONSOLE ------------------------------
    // MEASURED AND FLAGGED. The console loads the four transform rows from `this+0..0x30` -- the
    // embedded body's own pose -- and never stores a transform; the only stores it makes back into
    // the part are `stvx128 v127, this+0x40` (mLinearVelocity) and `stvx128 v126, this+0x50`
    // (mAngularVelocity). That is because the console's Prepare @0x82626700 DOES pose the body: it
    // writes all four rows of this+0..0x30 twice (0x82626924..0x82626938 and again at
    // 0x82626964..0x82626978, around CalcBoundingBox), from the graphics-transform x vehicle-frame
    // compose. THE PREVIOUS WAVE'S BANNER SAID "PhysicalBodyPart::Prepare never gives the
    // embedded body a pose" -- that is true of THIS TREE'S Prepare, which models the compose as
    // `mRwBody.SetTransform(lBBoxOrientation)` with the caller now passing identity, and it is FALSE
    // of the console. Corrected there too.
    // ✅ RETIRED 2026-09-05 (hinge-geometry wave): Prepare's compose IS reconstructed, so the
    // compensating `mRwBody.SetTransform(lVehicleTransform)` this body used to open with is DELETED
    // and the pose is READ exactly as the console reads it. The panel now enters the sim at its own
    // centre of mass on the bodywork instead of at the car's origin, which also puts its collision
    // box where the panel is. lVehicleTransform is retained as the console's argument (it is on the
    // ABI, and TestJointForBreaking/RemoveJointAndAddToSim pass the part's own pose through it) but
    // this body does not consume it -- exactly as the asm does not.
    //
    // ---- THE INERTIA CHAIN -------------------------------------------------------------------
    //   extents = CalculateAABBExtents()                       (bodied, this file)
    //   ComputeFatBoxInertia(extents.x/.y/.z, margin=flt_82001CC0=0.0, &lInertia)
    //   lInertia = lInertia * kfPartMass * splat(kfPartInertiaMultiplier) + KV_PART_INERTIA_FLOOR
    //   -- THE vmaddfp OPERAND ORDER IS DECODED, NOT ASSUMED. IDA prints the raw field order
    //      (vD, vA, vB, vC) and the operation is vD = vA*vC + vB; the word at 0x8260AEE0 is
    //      0x100C6AAE -> vD=v0 vA=v12(fatBox*mass) vB=v13(the floor) vC=v10(splat(multiplier)).
    //      CALIBRATION CONTROL: the same decoder on 0x8260FCB0 (0x118A62EE) inside
    //      HandleContactWithLeanProp yields vA=v10 vB=v12 vC=v11 == `v*m + F*dt`, which is exactly
    //      what this tree's committed ExternalPhysicsBody::GetLinearMomentum banner says that site
    //      computes. Method calibrated against a result the tree already holds.
    //   inverseInertia = per-axis 1.0f/lInertia  ; Inertia::SetInverseInertia(inverseInertia)
    //   mRwBody.SetInverseInertia(diag(inverseInertia))        (the three this+0x70/0x80/0x90 rows)
    // The "Bad inertia: " / " Bounding box half dimensions: " assert (BrnPhysicalBodyPart.cpp:548)
    // is a NON-GATING tripwire and fires when NO lane of |inertia| exceeds stru_8208F620 lane 0
    // (== 1.1920929e-07, FLT_EPSILON, byte-read from the image) -- i.e. it catches a DEGENERATE
    // inertia, not a large one. CR6 bit 2 ("none true") is the bit the asm tests.
    // =============================================================================================
    void PhysicalBodyPart::AddToSim(CgsPhysics::PhysicsSimulationIO::InputBuffer* lpSimInput,
                                    const Matrix44Affine& lVehicleTransform,
                                    Vector3 lInitialLinearVelocity, Vector3 lInitialAngularVelocity)
    {
        // The console never stores a transform here -- Prepare (and thereafter UpdateJoint / the sim)
        // owns the pose. The argument stays on the signature because the console's does.
        (void)lVehicleTransform;

        CgsPhysics::PhysicsSimulationIO::InAddRigidBody lAddBodyEvent;

        // +0x00. `ld r10, 0x1D0(r30)` -- the part's packed BurnoutBodyPartID IS its RigidBodyId.
        lAddBodyEvent.mID = mRigidBodyId.GetBaseRigidBodyID();

        // +0x10..+0x40 -- the four rows the console lvx128's from this+0/0x10/0x20/0x30.
        lAddBodyEvent.mRigidBody.mTransform = mRwBody.GetTransform();

        // +0x50 / +0x60 -- the two velocity arguments, straight through (v127 = v1, v126 = v2).
        lAddBodyEvent.mRigidBody.mVelocity        = lInitialLinearVelocity;
        lAddBodyEvent.mRigidBody.mAngularVelocity = lInitialAngularVelocity;

        // +0x80/+0x88/+0x8C/+0x90/+0x94 -- the five scalar tuning fields.
        lAddBodyEvent.mRigidBody.mInertia.SetInverseMass(1.0f / KF_PART_MASS);
        lAddBodyEvent.mRigidBody.mInertia.SetMaxLinearVelocity(KF_PART_MAX_LINEAR_VELOCITY);
        lAddBodyEvent.mRigidBody.mInertia.SetMaxAngularVelocity(KF_PART_MAX_ANGULAR_VELOCITY);
        lAddBodyEvent.mRigidBody.mInertia.SetLinearDrag(KF_PART_LINEAR_DRAG);
        lAddBodyEvent.mRigidBody.mInertia.SetAngularDrag(KF_PART_ANGULAR_DRAG);

        // +0xA0 `stb 1` -- TRUE here (the prop producers store 0). The sim's spy flag.
        lAddBodyEvent.mRigidBody.mbSpy = true;
        // +0xB0 `stw 4`.
        lAddBodyEvent.meState = rw::physics::ACTIVE_BODY;

        // ---- the fat-box inertia --------------------------------------------------------------
        // The console seeds the out-vector with (1,1,1,0) before the call (three `stfs f31` of the
        // 1.0 it already holds plus a `stw 0`); ComputeFatBoxInertia overwrites all four lanes, so
        // the seed is dead. Reproduced because it is a real store, not because it is load-bearing.
        Vector4 lInertia = { 1.0f, 1.0f, 1.0f, 0.0f };

        const Vector3 lAABBHalfExtents = CalculateAABBExtents();
        rw::physics::ComputeFatBoxInertia(lAABBHalfExtents.x, lAABBHalfExtents.y,
                                          lAABBHalfExtents.z, KF_PART_FAT_BOX_MARGIN, &lInertia);

        // lInertia = (fatBox * mass) * splat(multiplier) + the floor vector. Lane 3 is not read.
        lInertia.x = lInertia.x * KF_PART_MASS * KF_PART_INERTIA_MULTIPLIER + KV_PART_INERTIA_FLOOR.x;
        lInertia.y = lInertia.y * KF_PART_MASS * KF_PART_INERTIA_MULTIPLIER + KV_PART_INERTIA_FLOOR.y;
        lInertia.z = lInertia.z * KF_PART_MASS * KF_PART_INERTIA_MULTIPLIER + KV_PART_INERTIA_FLOOR.z;

        // NON-GATING tripwire, exactly as the asm spells it: the abs of the three lanes (the w lane
        // is replaced by a copy of x by the vrlimi128 so it cannot poison the reduction) compared
        // against a splat of stru_8208F620 lane 0, testing CR6 bit 2 == "none of the lanes is
        // greater". Fires on a DEGENERATE inertia and falls straight through.
        {
            const f32 lfAbsX = (lInertia.x < 0.0f) ? -lInertia.x : lInertia.x;
            const f32 lfAbsY = (lInertia.y < 0.0f) ? -lInertia.y : lInertia.y;
            const f32 lfAbsZ = (lInertia.z < 0.0f) ? -lInertia.z : lInertia.z;
            CGS_ASSERT(lfAbsX > KF_INERTIA_DEGENERATE_EPSILON
                       || lfAbsY > KF_INERTIA_DEGENERATE_EPSILON
                       || lfAbsZ > KF_INERTIA_DEGENERATE_EPSILON,
                       "Bad inertia: ");
        }

        // The three `fdivs f31(1.0), lane` reciprocals, reassembled by the vperm/vrlimi128 pair.
        const Vector3 lInverseInertia = { 1.0f / lInertia.x, 1.0f / lInertia.y,
                                          1.0f / lInertia.z, 0.0f };

        // +0x70 mInvTens and +0x84 mSpherical == 1/min(x,y,z) -- Inertia::SetInverseInertia exactly.
        lAddBodyEvent.mRigidBody.mInertia.SetInverseInertia(lInverseInertia);

        // this+0x70/+0x80/+0x90 -- the body's own LOCAL inverse-inertia tensor, built as the
        // diagonal of the same reciprocal vector (gIVector/(0,1,0,0)/(0,0,1,0) each vrlimi128'd
        // with one lane of v11). mWorldInverseInertia (+0xA0) is deliberately NOT rebuilt --
        // the asm makes no store there. See ExternalPhysicsBody::SetInverseInertia's banner.
        {
            Matrix33 lLocalInverseInertia;
            lLocalInverseInertia.xAxis = { lInverseInertia.x, 0.0f, 0.0f, 0.0f };
            lLocalInverseInertia.yAxis = { 0.0f, lInverseInertia.y, 0.0f, 0.0f };
            lLocalInverseInertia.zAxis = { 0.0f, 0.0f, lInverseInertia.z, 0.0f };
            mRwBody.SetInverseInertia(&lLocalInverseInertia);
        }

        // this+0x40 / this+0x50 -- the two velocity stores the asm makes back into the part.
        mRwBody.SetLinearVelocity(lInitialLinearVelocity);
        mRwBody.SetAngularVelocity(lInitialAngularVelocity);

        // 0x8260AF3C `bl CgsPhysics::PhysicsSimulationIO::InputBuffer::GetAddRigidBodyQueue` then
        // `bl BaseEventQueue<InAddRigidBody>::AddEvent @0x825A3000`.
        lpSimInput->GetAddRigidBodyQueue()->AddEvent(lAddBodyEvent);

        // [DIAG] NOT IN THE X360 BINARY. The win-condition witness for this wave, at the site and
        // after the event is built, printing the values that decide whether the part can move at
        // all: a non-ACTIVE state or a zero/NaN inverse mass means it is in the sim and inert.
        if ( DetachProbeOn() )
        {
            const Vector3 lPos = lAddBodyEvent.mRigidBody.mTransform.wAxis;
            *CgsDev::Log::gpDebugPrint
                << "[detach-sim] ENTERED SIM id " << CgsDev::E_PRINTMODE_HEXONCE
                << static_cast<u64>(lAddBodyEvent.mID)
                << " state " << (lAddBodyEvent.meState == rw::physics::ACTIVE_BODY ? "ACTIVE" : "NOT-ACTIVE")
                << " invMass " << lAddBodyEvent.mRigidBody.mInertia.GetInverseMass()
                << " invI (" << lInverseInertia.x << ", " << lInverseInertia.y << ", "
                << lInverseInertia.z << ")"
                << " pos (" << lPos.x << ", " << lPos.y << ", " << lPos.z << ")"
                << " vel (" << lInitialLinearVelocity.x << ", " << lInitialLinearVelocity.y << ", "
                << lInitialLinearVelocity.z << ")\n";
        }
    }

    void PhysicalBodyPart::PostVehicleUpdate()
    {
        static bool sbLoggedPVU = false;
        if ( !sbLoggedPVU )
        {
            sbLoggedPVU = true;
            if ( CgsDev::Message::gxMessageFilterFlags & 1 )
                *CgsDev::Log::gpDebugPrint << "conductor gate: PhysicalBodyPart::PostVehicleUpdate reached but not "
                                              "reconstructed [FLAG PC boot gate]\n";
        }
        
    }

    void RemoveTriangleCacheSlot(CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* /*lpSceneInput*/, u16 /*lu16TriangleCacheSlot*/)
    {
        static bool sbLoggedRTCS = false;
        if ( !sbLoggedRTCS )
        {
            sbLoggedRTCS = true;
            if ( CgsDev::Message::gxMessageFilterFlags & 1 )
                *CgsDev::Log::gpDebugPrint << "conductor gate: RemoveTriangleCacheSlot hook reached but not "
                                              "reconstructed [FLAG PC boot gate]\n";
        }
        
    }

    // ⛔ EmitDetachedPartNotification's GATE IS DELETED (2026-08-27, detach-2 wave), and so is the
    // free function itself -- see the two call sites (this file's TestJointForBreaking and
    // BrnDeformableObject_Detach.cpp's DetachPart), which now build the real
    // Deformation::DetachedPartNotificationEvent and AddEventSafe it onto the deformation output
    // interface's +0x3A0 queue, exactly as the console does. The hook took a `const void*` blob,
    // which was never a real parameter of anything.

    void EmitUpdateExternalBodyEvent(CgsPhysics::PhysicsSimulationIO::InputBuffer* /*lpSimInput*/, const void* /*lpEventBlob*/)
    {
        static bool sbLoggedEUEB = false;
        if ( !sbLoggedEUEB )
        {
            sbLoggedEUEB = true;
            if ( CgsDev::Message::gxMessageFilterFlags & 1 )
                *CgsDev::Log::gpDebugPrint << "conductor gate: EmitUpdateExternalBodyEvent hook reached but not "
                                              "reconstructed [FLAG PC boot gate]\n";
        }
        
    }

}
}
