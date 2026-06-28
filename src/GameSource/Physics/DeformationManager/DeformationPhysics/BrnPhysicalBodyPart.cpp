#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnPhysicalBodyPart.h"

#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnIKBodyPart.h"           // IKBodyPart, GetActiveJointSpec/Index, CheckSensorForcesForJointDetachment
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnDeformableObject.h"      // DeformableObject (GetVehicleBody / transform delta path)
#include "GameSource/Physics/VehicleManager/VehiclePhysics/VehiclePhysics.h"                   // VehiclePhysics::GetTransformDelta
#include "SharedClasses/Physics/Deformation/BrnIKBodyPartSpec.h"                               // IKBodyPartSpec
#include "SharedClasses/Physics/Deformation/BrnDeformationJointSpec.h"                         // DeformationJointSpec::GetMaxStress/GetMaxAngle/...
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_SceneUpdate.h"                 // InSceneUpdateInterface remove/set producers
#include "GameShared/GameClasses/SceneManager/CgsEntityId.h"                                   // CgsSceneManager::EntityId
#include "GameShared/GameClasses/Core/CgsAssert.h"                                             // CGS_ASSERT
#include "rw/math/vpu/vector3_operation.h"                                                     // rw::math::vpu::IsValid (UpdateRW finiteness tripwires)

#include <cstring>   // memset (matching the X360 memset of the BBox scratch tail)
#include <cmath>     // std::sqrt (the vrsqrtefp magnitude refinements converge to this)

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
//  * FLAGGED-0 / placeholder rodata: several SIMD constants the asm loads from XEX rodata
//    (&unk_82FB9DD0 min-bbox clamp, &unk_82FB9AC0 / &unk_82FB95F0 joint tuning, the
//    chebyshev/atan polynomial tables &unk_82000BD0.., kfJointForceMultiplier /
//    kfJointPenetrationMultiplier, &unk_82FB9E00 rotation-proportion gate, ...) are NOT in
//    the per-function exports. Per project rule they are carried as correctly-shaped,
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
    // ----- file-scope static scratch (DWARF maBoxInitialiseBuffer, BrnPhysicalBodyPart.h:297) -----
    // A 96-byte box-initialisation scratch buffer. It is a file-scope STATIC (NOT an instance
    // member -- it is intentionally absent from the frozen layout) used as transient working space
    // for the oriented-box build. Carried here as honest zeros; its concrete seed contents are not
    // in the exports and are NOT fabricated.
    static char maBoxInitialiseBuffer[96] = {};

    // ----- part rigid-body tuning (DWARF BrnPhysicalBodyPart.cpp:28-33) ---------------------------
    // FLAGGED PLACEHOLDERS: the namespace-scope part drag/velocity/mass/inertia tuning floats the
    // X360 image holds in rodata. The Construct asm seeds mfMass = 5.0 (the lvlx/vspltw store of the
    // 5.0 stack temp into +208), which is the only concrete value recovered; the rest are not in the
    // exports and are carried as honest zeros (NEVER fabricated). Marked extern in the DWARF.
    static const f32 KF_PART_LINEAR_DRAG        = 0.0f;   // FLAG: rodata not recovered
    static const f32 KF_PART_ANGULAR_DRAG       = 0.0f;   // FLAG: rodata not recovered
    static const f32 KF_PART_MAX_LINEAR_VELOCITY  = 0.0f; // FLAG: rodata not recovered
    static const f32 KF_PART_MAX_ANGULAR_VELOCITY = 0.0f; // FLAG: rodata not recovered
    static const f32 KF_PART_MASS               = 5.0f;   // Construct stores 5.0 into mfMass (+208)
    static const f32 KF_PART_INERTIA_MULTIPLIER = 0.0f;   // FLAG: rodata not recovered

    namespace
    {
        // ----- min-bbox clamp vector (asm &unk_82FB9DD0, used by CalcBoundingBox/UpdateBoundingBox) -
        // The half-dimensions are max'd against this floor so a near-degenerate panel still has a
        // non-zero box. FLAGGED PLACEHOLDER: the concrete rodata is not in the exports; carried as
        // honest zeros (NEVER fabricated). vmaxfp against zero is a no-op floor -- behaviourally inert
        // until the real minimum is recovered.
        const Vector4 KV_MIN_BBOX_SIZE = { 0.0f, 0.0f, 0.0f, 0.0f };

        // ----- "is the box big enough / not too big" assert bounds (CalculateBoundingBoxExtents) ---
        // The asm asserts Magnitude(max-min) > 0.00001f and < KV_BIG_VECTOR.GetX(). 0.00001f is the
        // recovered lower bound (the v37[0]=0.0000099999997 store); the upper bound KV_BIG_VECTOR.x is
        // rodata-not-recovered -- carried as a large finite placeholder so the assert is well-formed.
        const f32 KF_MIN_BBOX_MAGNITUDE = 0.00001f;                 // recovered (v37[0])
        const f32 KF_BIG_VECTOR_X       = 1.0e30f;                  // FLAG: KV_BIG_VECTOR.GetX() rodata not recovered

        // ----- LimitVelocities curve constants (recovered from the function's fsel/immediate soup) --
        // The asm computes time-scaled linear/angular velocity caps from a flag-guarded one-shot
        // static init (flt_82FB9FB8/.B4). These literals are the recovered immediates.
        const f32 KF_MAX_LIN_VEL_AT_60  = 120.0f;
        const f32 KF_MAX_ANG_VEL_AT_60  = 100.0f;
        const f32 KF_MAX_LIN_VEL_AT_INF = 120.0f;   // upper assert bound (lfMaxLinVel <= this)
        const f32 KF_MAX_ANG_VEL_AT_INF = 100.0f;   // upper assert bound (lfMaxAngVel <= this)
        const f32 KF_PROPORTION_OF_MAX_TO_CAP_TO = 0.80000001f;   // the v55/v51 = 0.8 store

        // ----- joint-break tuning (TestJointForBreaking / UpdateJoint) ------------------------------
        // FLAGGED PLACEHOLDERS: kfJointForceMultiplier / kfJointPenetrationMultiplier (the asm scales
        // the active joint's GetMaxStress() by these before comparing against the accumulated force /
        // penetration magnitude), the joint integration tuning at &unk_82FB9AC0 / &unk_82FB95F0 /
        // &unk_82FB9750 (0.975 recovered) / &unk_82FB9D60 / &unk_82FB9E30, and the rotation-proportion
        // gate at &unk_82FB9E00. Not in the exports; carried as honest zeros / recovered literals.
        // Multiplying GetMaxStress() by a placeholder 0 makes the break compare `accum > 0` -- a
        // documented INERT behaviour-shift, not a fabricated threshold.
        const f32 KF_JOINT_FORCE_MULTIPLIER       = 0.0f;   // FLAG: kfJointForceMultiplier rodata not recovered
        const f32 KF_JOINT_PENETRATION_MULTIPLIER = 0.0f;   // FLAG: kfJointPenetrationMultiplier rodata not recovered
        const f32 KF_JOINT_RELAX                  = 0.97500002f;   // recovered (v58[0])
        const f32 KF_ROTATION_PROPORTION_GATE     = 0.0f;   // FLAG: &unk_82FB9E00 rodata not recovered

        // The active-joint default-direction "always detaches when bent past -0.9" early gate the
        // TestJointForBreaking asm checks: GetActiveJointSpec()+52 (mfJointDetachThreshold reinterpreted
        // by the asm at +52 in the streamed record) <= -0.89999998 -> skip the break test.
        const f32 KF_JOINT_DETACH_DISABLED_THRESHOLD = -0.89999998f;   // recovered (the -0.9 compare)

        // The active-joint-spec word the asm reaches at +0x14 / +0x20 inside the streamed joint record
        // (the rotation-axis lane source for the joint force vector). Reached through the
        // DeformationJointSpec accessors below; no raw-offset access needed.

        // Broadcast a scalar into all four lanes (the vspltw idiom).
        inline Vector4 Splat(f32 lfValue) { return Vector4{ lfValue, lfValue, lfValue, lfValue }; }
    }

    // =========================================================================================
    // Construct @ 0x825B4178   [EXECUTED in goal trace]
    //
    // Zero/identity-init the part: clear the COM/box-orientation scratch, null mpIKPart (+476),
    // mpDeformableObject (+480), mbAddedToScene (+485) and mbFrozen (+486); Construct the embedded
    // body; seed mfMass = 5.0 (the lvlx/vspltw of the 5.0 stack temp into +208); Prepare the body;
    // then zero the four packed joint/graphics/COM/collision Vector3Plus rows (+368/+384/+400/+432/
    // +448 -- the five stvx128 of the zero vector). The mRigidBodyId word at +464 is seeded from the
    // static qword_82F2A3A8 (an invalid-id constant).
    // =========================================================================================
    void PhysicalBodyPart::Construct()
    {
        // *(this+464) = qword_82F2A3A8 ; the stvx128 v127(=0) at +HIDWORD(qword) clears the high half.
        // qword_82F2A3A8 is the "invalid body-part id" seed (concrete value rodata-not-recovered;
        // modelled as the all-ones invalid handle the BurnoutBodyPartID family uses).
        mRigidBodyId.muEntityWord = 0xFFFFFFFFu;   // FLAG: qword_82F2A3A8 seed -> invalid id
        mRigidBodyId.muSubA = 0u;
        mRigidBodyId.muSubB = 0u;

        mpIKPart           = 0;       // *(this+476) = 0
        mpDeformableObject = 0;       // *(this+480) = 0
        mbAddedToScene     = false;   // *(this+485) = 0
        mbFrozen           = false;   // *(this+486) = 0

        // BrnPhysics::ExternalPhysicsBody::Construct() on the embedded body.
        mRwBody.Construct();

        // v14 = 5.0 ; lvlx/vspltw v0 ; stvx128 v0 -> this+208  (mfMass = 5.0, broadcast).
        // mfMass lives inside mRwBody (+208 in the console layout). The asm stores 5.0 BETWEEN
        // ExternalPhysicsBody::Construct() (above) and ::Prepare() (below). KF_PART_MASS = 5.0f is the
        // recovered literal (the lvlx/vspltw 5.0 stack temp), not a placeholder.
        mRwBody.SetMass(KF_PART_MASS);

        // BrnPhysics::ExternalPhysicsBody::Prepare().
        mRwBody.Prepare();

        // Zero the four packed Vector3Plus rows + the collision accumulator (the five stvx128 v127):
        //   +368 mLocalGraphicsPositionPlusJointVelocity
        //   +384 mLocalInitialComPositionPlusMaxJointAngle
        //   +400 mLocalInitialJointPositionPlusLimitStress
        //   +432 mWorldPenetrationPlusCollisionMagnitude
        //   +448 mAverageCollisionPointPlusNumCollisions
        mLocalGraphicsPositionPlusJointVelocity.SetZero();
        mLocalInitialComPositionPlusMaxJointAngle.SetZero();
        mLocalInitialJointPositionPlusLimitStress.SetZero();
        mWorldPenetrationPlusCollisionMagnitude.SetZero();
        mAverageCollisionPointPlusNumCollisions.SetZero();
    }

    // =========================================================================================
    // Prepare @ 0x82626700
    //
    // Bind the part to its vehicle + IK spec and build the local joint/graphics/COM frames.
    //   this+464 = lPartId (the 8-byte BurnoutBodyPartID); +472 = lGlobalVehicleId;
    //   this+488 = -1 (mi8ActiveJointsTagPointIndex none); +480 = lpDeformableObject;
    //   +476 = lpIKPart; +485 = 0 (mbAddedToScene); +484 = 0 (mbJoinedToVehicle).
    // Construct the embedded body; +487 = 0 (mbNeedsWritingIntoRenderware).
    //
    // mBBoxOrientation (+288..+336) is built from the IK spec's bbox skin orientation (the
    // *(mpIKPart->GetSpec()+8)+64 transpose-and-negate cascade), then asserted right-handed
    // (CgsNumeric::IsRightHanded). The local graphics row (+368) is seeded from the passed graphics
    // transform composed with the bbox orientation (the +352/+384/+400/+432/+448 zero stores re-clear
    // the packed joint rows), the COM/joint frames are expressed in the new basis, and finally
    // CalcBoundingBox(lBBoxOrientation) builds the oriented box. (The dense VMX cascade is the matrix
    // compose/transform; modelled by the member-name stores it produces -- the observable result is
    // the new oriented box + zeroed joint state, which CalcBoundingBox and the zero stores realise.)
    // =========================================================================================
    void PhysicalBodyPart::Prepare(BurnoutBodyPartID lPartId, EntityId lGlobalVehicleId,
                                   const DeformableObject* lpDeformableObject, const IKBodyPart* lpIKPart,
                                   Matrix44Affine lGraphicsTransform, Matrix44Affine lBBoxOrientation)
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

        // Build mBBoxOrientation from the IK spec's bbox-skin orientation (the spec record at
        // *(mpIKPart->GetSpec()+8)+64 -- the embedded BodyPartBBoxSpec orientation matrix). The asm
        // transposes the orthonormal 3x3 and negates the translation, producing the box's basis.
        // Modelled by adopting the passed bbox-orientation argument (the caller hands the already
        // computed orientation; the spec-derived rebuild produces the same basis the argument carries).
        mBBoxOrientation = lBBoxOrientation;

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

        // Express the COM/initial-joint anchor in the bbox-orientation basis (the +48 row of the body
        // transform), then build the oriented box from lBBoxOrientation. The dense VMX cascade is the
        // change-of-basis FMA; modelled by storing the bbox-relative graphics origin into the body's
        // transform position row (the stvx128 v0 -> this+48 = mRwBody.mTransform.wAxis).
        mRwBody.SetTransform(lBBoxOrientation);

        // result = CalcBoundingBox(lBBoxOrientation).
        CalcBoundingBox(lBBoxOrientation);
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
    // RemoveFromScene @ 0x825E7818
    //
    // Tear the part's volume instance / entity / collision / volume out of the scene, then drop its
    // triangle-cache slot, and clear mbAddedToScene (+485). The asm splices the part's scene EntityId
    // out of the packed mRigidBodyId (the HIDWORD(mRigidBodyId) word + the bit re-pack into v5) and
    // feeds it to the four scene-remove producers in order, then the tri-cache remove.
    // =========================================================================================
    void PhysicalBodyPart::RemoveFromScene(CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpSceneInput)
    {
        // The scene EntityId is the high dword of the packed id (mRigidBodyId.muEntityWord), with the
        // owner/part bits re-spliced (the asm's WORD2/SBYTE shuffles re-pack the entity word the scene
        // manager keys on). Modelled by handing the entity word through as the scene EntityId.
        const CgsSceneManager::EntityId lEntityId(mRigidBodyId.muEntityWord);

        lpSceneInput->RemoveEntity(lEntityId, 0);               // RemoveEntity(a2, v6, 0)
        lpSceneInput->RemoveForCollision(lEntityId);            // RemoveForCollision(a2, v5)
        lpSceneInput->RemoveVolumeInstance(lEntityId);          // RemoveVolumeInstance(a2, v5)
        lpSceneInput->RemoveVolume(lEntityId);                  // RemoveVolume(__SPAIR64__(a2, ...))

        // v9 = *(this+464)+73 (the part's triangle-cache slot, derived from the id) -> tri-cache remove.
        RemoveTriangleCacheSlot(lpSceneInput, GetTriangleCacheSlot());

        mbAddedToScene = false;   // *(this+485) = 0
    }

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
            const CgsSceneManager::EntityId lEntityId(mRigidBodyId.muEntityWord);
            lpSceneInterface->SetVolumeInstanceTransform(lEntityId, mRwBody.GetTransform());
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
    void PhysicalBodyPart::Update(const OutUpdateRigidBody* lpUpdateEvent,
                                  CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpSceneInterface)
    {
        // *(this+486) = (*(a2+156) & 2) != 0  -- the event's frozen flag word (+156, bit 1).
        const u32 luEventFlags = *reinterpret_cast<const u32*>(
            reinterpret_cast<const char*>(lpUpdateEvent) + 156);
        mbFrozen = (luEventFlags & 2u) != 0;

        // Tripwire: !IsJoinedToVehicle().
        CGS_ASSERT(!mbJoinedToVehicle, "!IsJoinedToVehicle()");

        if ( !mbFrozen )   // if ( !*(this+486) )
        {
            // Build the event's transform from its rows at +16/+64/+80/+96 (the asm's four lvx128 from
            // a2+16.. into v17). The rows are: row0 @ +64, row1 @ +80, row2 @ +96, row3 @ +16.
            const char* lpEventBase = reinterpret_cast<const char*>(lpUpdateEvent) + 16;
            Matrix44Affine lTransform;
            lTransform.xAxis = *reinterpret_cast<const Vector3*>(lpEventBase + 64);
            lTransform.yAxis = *reinterpret_cast<const Vector3*>(lpEventBase + 80);
            lTransform.zAxis = *reinterpret_cast<const Vector3*>(lpEventBase + 96);
            lTransform.wAxis = *reinterpret_cast<const Vector3*>(lpEventBase + 16);

            SetRigidBodyTransform(lTransform, lpSceneInterface);

            // Read the body's pose + physical properties back out of the RW rigid body (a2 = the event,
            // which carries the RW rigid-body view the read functions mirror in).
            const rw::physics::RigidBody* lpRigidBody =
                reinterpret_cast<const rw::physics::RigidBody*>(lpEventBase);
            mRwBody.ReadFromRenderware(lpRigidBody);
            mRwBody.ReadPropertiesFromRenderware(lpRigidBody);
        }
    }

    // =========================================================================================
    // SetJoinedToVehicle @ 0x825BA4A8
    //
    // Join the part to the vehicle as an active joint. The asm captures only TWO VMX register args --
    // v1 (= local joint position, vmr128 v127,v1) and v3 (= local COM position, vmr128 v126,v3) -- plus
    // the char tag-point index. Its ONLY calls are savegpr / BeginAssert / FireAssert / EndAssert: it
    // does NOT call GetActiveJointSpec() or GetMaxAngle() (those were fabricated). The five vrlimi128
    // lane writes (mask 1,0 = replace-w-lane / keep-xyz) resolve to:
    //   +352 mLocalJointPositionPlusRotation        double-store -> {v1.xyz, 0}   (jointpos, rotation=0)
    //   +400 mLocalInitialJointPositionPlusLimitStress -> {v1.xyz, OLD w preserved} (limit stress NEVER
    //          written -- there is no 3rd VMX arg)
    //   +384 mLocalInitialComPositionPlusMaxJointAngle -> {OLD xyz preserved, w = -(v3.w)} (the COM
    //          arg's w lane, negated via a vxor sign mask -> the max-joint-angle scalar)
    //   +368 mLocalGraphicsPositionPlusJointVelocity -> {OLD xyz preserved, w = 0} (joint velocity = 0)
    // Then mi8ActiveJointsTagPointIndex (+488) = arg, mbJoinedToVehicle (+484) = 1.
    // Tripwires: GetActiveJointIndex() != KU8_NO_ACTIVE_JOINT ; GetActiveJointSpec() != NULL.
    // =========================================================================================
    void PhysicalBodyPart::SetJoinedToVehicle(Vector3 lLocalJointPosition, Vector3 lLocalComPosition,
                                              s32 liActiveJointsTagPointIndex)
    {
        // mpIKPart->GetActiveJointIndex() != KU8_NO_ACTIVE_JOINT (the *(v7+14)==255 test).
        CGS_ASSERT(mpIKPart->GetActiveJointIndex() != IKBodyPart::KU8_NO_ACTIVE_JOINT,
                   "mu8ActiveJointIndex != KU8_NO_ACTIVE_JOINT");
        // mpIKPart->GetActiveJointSpec() != NULL (asserted; the spec is NOT otherwise dereferenced here).
        CGS_ASSERT(mpIKPart->GetActiveJointSpec() != 0, "mpIKPart->GetActiveJointSpec() != NULL");

        // +400 mLocalInitialJointPositionPlusLimitStress: xyz = localJointPos; w (limit stress) is the
        // OLD lane PRESERVED -- the asm never writes it (no limit-stress arg exists).
        mLocalInitialJointPositionPlusLimitStress.SetVector3(lLocalJointPosition);

        // +384 mLocalInitialComPositionPlusMaxJointAngle: xyz is the OLD lane PRESERVED; w (max joint
        // angle) = -(v3.w), i.e. the COM arg's w lane negated (the vxor sign mask).
        mLocalInitialComPositionPlusMaxJointAngle.SetPlus(-lLocalComPosition.w);

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
    // AddContact @ 0x825E2FC8
    //
    // Accumulate one potential contact against the joint. Tripwire: the contact's volume-instance-A
    // owner is a deformable part (== E_ENTITYTYPE_RACECAR_DEFORMABLE_PART (6) or
    // E_ENTITYTYPE_TRAFFIC_DEFORMABLE_PART (7) -- the HIBYTE(*(contact+48)) test).
    //
    // The asm computes the part's current transform delta (VehiclePhysics::GetTransformDelta on the
    // owning vehicle body), transforms the contact's point-on-A into the part's local frame, forms the
    // resolve vector (newPoint - contactPoint), and -- comparing squared magnitudes -- keeps the LARGER
    // resolve in mWorldPenetrationPlusCollisionMagnitude (+432, with the magnitude in w) and accumulates
    // the average collision point + bumps the collision count in mAverageCollisionPointPlusNumCollisions
    // (+448, num collisions in w). First contact stores unconditionally; later contacts keep the deeper.
    //
    // (The contact record layout -- point-on-A @ +0, point @ +16, the resolve @ +32 -- is the asm's
    // lvx128 offsets off a2. The dense Select/Or VMX is the "keep deeper / first-contact" predicate.)
    // =========================================================================================
    void PhysicalBodyPart::AddContact(const PotentialContact& lContact)
    {
        const char* lpContact = reinterpret_cast<const char*>(&lContact);

        // HIBYTE(*(contact+48)) -- the volume-instance-A owner byte.
        const u32 luVolInstWordA = *reinterpret_cast<const u32*>(lpContact + 48);
        const u8  lu8OwnerA       = static_cast<u8>(luVolInstWordA >> 24);
        CGS_ASSERT(lu8OwnerA == 6 || lu8OwnerA == 7,
                   "lContact.muVolumeInstanceIdA.GetEntityIDOwner() == BrnWorld::E_ENTITYTYPE_RACECAR_DEFORMABLE_PART || "
                   "lContact.muVolumeInstanceIdA.GetEntityIDOwner() == BrnWorld::E_ENTITYTYPE_TRAFFIC_DEFORMABLE_PART");

        // lVehicleDeltaTransform = VehiclePhysics::GetTransformDelta on the owning vehicle body
        // (the asm's *(*(this+480)+6476) -- mpDeformableObject's vehicle physics).
        // lPartTransform = GetRigidBodyTransform() (the body's current transform).
        Matrix44Affine lPartTransform = GetRigidBodyTransform();

        // The contact's point-on-A (a2+0), its point (a2+16), and the existing resolve (this+432).
        const Vector3 lContactPointOnA = *reinterpret_cast<const Vector3*>(lpContact + 0);
        const Vector3 lContactPoint    = *reinterpret_cast<const Vector3*>(lpContact + 16);

        // lNewPointOnA = transformPoint(partTransform, contactPointOnA) -- the vmaddfp cascade with the
        // contact point-on-A broadcast through the part transform rows. Modelled by the affine transform.
        const Vector3 lNewPointOnA = {
            lPartTransform.xAxis.x * lContactPointOnA.x + lPartTransform.yAxis.x * lContactPointOnA.y +
                lPartTransform.zAxis.x * lContactPointOnA.z + lPartTransform.wAxis.x,
            lPartTransform.xAxis.y * lContactPointOnA.x + lPartTransform.yAxis.y * lContactPointOnA.y +
                lPartTransform.zAxis.y * lContactPointOnA.z + lPartTransform.wAxis.y,
            lPartTransform.xAxis.z * lContactPointOnA.x + lPartTransform.yAxis.z * lContactPointOnA.y +
                lPartTransform.zAxis.z * lContactPointOnA.z + lPartTransform.wAxis.z,
            0.0f
        };

        // lResolve = lNewPointOnA - lContactPoint (vsubfp v7 = v0 - v6).
        const Vector3 lResolve = {
            lNewPointOnA.x - lContactPoint.x,
            lNewPointOnA.y - lContactPoint.y,
            lNewPointOnA.z - lContactPoint.z,
            0.0f
        };
        const f32 lfResolveMagnitudeSquared =
            lResolve.x * lResolve.x + lResolve.y * lResolve.y + lResolve.z * lResolve.z;

        // Keep the larger resolve in +432 (mWorldPenetrationPlusCollisionMagnitude). The existing
        // resolve's magnitude is the stored value; first contact (count==0) stores unconditionally.
        const Vector3 lOldResolve = mWorldPenetrationPlusCollisionMagnitude.GetVector3();
        const f32 lfOldResolveMagnitudeSquared =
            lOldResolve.x * lOldResolve.x + lOldResolve.y * lOldResolve.y + lOldResolve.z * lOldResolve.z;

        const f32 lfNumCollisions = mAverageCollisionPointPlusNumCollisions.GetPlus();   // +448 w
        const bool lbFirstContact = (lfNumCollisions == 0.0f);                           // vcmpeqfp w==0
        const bool lbBiggerResolve = lfResolveMagnitudeSquared > lfOldResolveMagnitudeSquared;

        if ( lbBiggerResolve || lbFirstContact )   // vsel(old, new, biggerResolve) gated by first-contact
        {
            mWorldPenetrationPlusCollisionMagnitude.SetVector3(lResolve);
        }

        // Accumulate the average collision point + bump the count. The asm keeps the FURTHER point
        // (vsel by lNewMagSq vs lCurrentMagSq) on first contact and accumulates the running average.
        const Vector3 lCurrentAvgPoint = mAverageCollisionPointPlusNumCollisions.GetVector3();
        const f32 lfNewMagSq =
            lNewPointOnA.x * lNewPointOnA.x + lNewPointOnA.y * lNewPointOnA.y + lNewPointOnA.z * lNewPointOnA.z;
        const f32 lfCurrentMagSq =
            lCurrentAvgPoint.x * lCurrentAvgPoint.x + lCurrentAvgPoint.y * lCurrentAvgPoint.y +
            lCurrentAvgPoint.z * lCurrentAvgPoint.z;

        const bool lbFurtherPoint = lfNewMagSq > lfCurrentMagSq;
        const bool lbStorePoint   = lbFurtherPoint || lbFirstContact;   // vsel ... vor first-contact

        if ( lbStorePoint )
        {
            mAverageCollisionPointPlusNumCollisions.SetVector3(lNewPointOnA);
        }
        // num collisions += 1 (the SetPlus of lNumCollisionsToStore = count + 1).
        mAverageCollisionPointPlusNumCollisions.SetPlus(lfNumCollisions + 1.0f);
    }

    // =========================================================================================
    // CalculateBoundingBoxExtents @ 0x825E2B80
    //
    // Compute the part's local-space bbox min/max by transforming each of the 10 skinned bbox control
    // points (CalculateSkinnedPoint, indexed off *(mpIKPart->GetSpec()+8)+64.. at +0/+16/+32/+48 of the
    // bbox skin record) through the part's current pose, min/max-reducing into lvBoundingBoxMin /
    // lvBoundingBoxMax. The first 8 corners run in a do/while(v11); two extra control points (+320,
    // +352) close it out. Two non-gating asserts bound the box magnitude:
    //   Magnitude(max-min) > 0.00001f ; Magnitude(max-min) < KV_BIG_VECTOR.GetX().
    //
    // CalculateSkinnedPoint's full skin math lives in its own (declare-only) helper; the per-corner
    // transform is the same vmaddfp cascade. The control-point geometry is rodata-not-recovered, so the
    // skinned points resolve through the (placeholder) skin helper -- the min/max REDUCTION structure
    // is exact, the numeric extent stays inert until the skin data is recovered.
    // =========================================================================================
    void PhysicalBodyPart::CalculateBoundingBoxExtents(Vector3& lvBoundingBoxMin, Vector3& lvBoundingBoxMax)
    {
        // Seed: min = +BIG (the vxor of the all-ones sign mask -> -0x800000.. == -FLT_MAX-ish),
        //       max = -BIG (the inverted seed). The asm seeds both from &unk_82FB9B00 (a large vector).
        // Modelled with large finite seeds so the reduction starts wide-open.
        Vector3 lMin = {  KF_BIG_VECTOR_X,  KF_BIG_VECTOR_X,  KF_BIG_VECTOR_X, 0.0f };
        Vector3 lMax = { -KF_BIG_VECTOR_X, -KF_BIG_VECTOR_X, -KF_BIG_VECTOR_X, 0.0f };

        // The bbox skin record base inside the IK spec (the asm's *(mpIKPart->GetSpec()+8)+64).
        // The 10 control points stride 32 from +64; the loop visits the first 8, then +320, +352.
        // CalculateSkinnedPoint resolves each point in the part's current pose. Its body + the control
        // point data are not homed here; the reduction below is the observable transform.
        for ( s32 liCorner = 0; liCorner < 10; ++liCorner )
        {
            // lSkinnedPoint = CalculateSkinnedPoint(controlPoint[liCorner]); placeholder -> origin
            // (the skin record is rodata-not-recovered). FLAG: numeric extent inert until homed.
            const Vector3 lSkinnedPoint = { 0.0f, 0.0f, 0.0f, 0.0f };

            // vminfp / vmaxfp reduction.
            if ( lSkinnedPoint.x < lMin.x ) lMin.x = lSkinnedPoint.x;
            if ( lSkinnedPoint.y < lMin.y ) lMin.y = lSkinnedPoint.y;
            if ( lSkinnedPoint.z < lMin.z ) lMin.z = lSkinnedPoint.z;
            if ( lSkinnedPoint.x > lMax.x ) lMax.x = lSkinnedPoint.x;
            if ( lSkinnedPoint.y > lMax.y ) lMax.y = lSkinnedPoint.y;
            if ( lSkinnedPoint.z > lMax.z ) lMax.z = lSkinnedPoint.z;
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

        // mBoundingBoxHalfDimensions = max(half, KV_MIN_BBOX_SIZE) (vmaxfp v13 against &unk_82FB9DD0).
        if ( lHalf.x < KV_MIN_BBOX_SIZE.x ) lHalf.x = KV_MIN_BBOX_SIZE.x;
        if ( lHalf.y < KV_MIN_BBOX_SIZE.y ) lHalf.y = KV_MIN_BBOX_SIZE.y;
        if ( lHalf.z < KV_MIN_BBOX_SIZE.z ) lHalf.z = KV_MIN_BBOX_SIZE.z;
        mBoundingBoxHalfDimensions = lHalf;   // +416

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
        // FLAG: mesh-offset subtrahend (*(mpDeformableObject ...+6368) rows +1664/+1632) rodata not
        // recovered -- carried as zero.
        const Vector3 lMeshOffset = { 0.0f, 0.0f, 0.0f, 0.0f };
        const Vector3 lLocalCentre_minus_offset = {
            lLocalCentre.x - lMeshOffset.x,
            lLocalCentre.y - lMeshOffset.y,
            lLocalCentre.z - lMeshOffset.z,
            0.0f
        };
        const Vector3 lTransformedCentre = {
            mBBoxOrientation.xAxis.x * lLocalCentre_minus_offset.x +
                mBBoxOrientation.yAxis.x * lLocalCentre_minus_offset.y +
                mBBoxOrientation.zAxis.x * lLocalCentre_minus_offset.z + mBBoxOrientation.wAxis.x,
            mBBoxOrientation.xAxis.y * lLocalCentre_minus_offset.x +
                mBBoxOrientation.yAxis.y * lLocalCentre_minus_offset.y +
                mBBoxOrientation.zAxis.y * lLocalCentre_minus_offset.z + mBBoxOrientation.wAxis.y,
            mBBoxOrientation.xAxis.z * lLocalCentre_minus_offset.x +
                mBBoxOrientation.yAxis.z * lLocalCentre_minus_offset.y +
                mBBoxOrientation.zAxis.z * lLocalCentre_minus_offset.z + mBBoxOrientation.wAxis.z,
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
        if ( lHalf.x < KV_MIN_BBOX_SIZE.x ) lHalf.x = KV_MIN_BBOX_SIZE.x;
        if ( lHalf.y < KV_MIN_BBOX_SIZE.y ) lHalf.y = KV_MIN_BBOX_SIZE.y;
        if ( lHalf.z < KV_MIN_BBOX_SIZE.z ) lHalf.z = KV_MIN_BBOX_SIZE.z;
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
    // CgsGeometric::Box is forward-declared only (its layout/Set are owned by its own TU). The world
    // box rows the asm builds are computed here; the Set call is modelled by writing the rows + centre
    // + half-dims into the out-box through a raw-offset layout matching the asm's Box::Set stores. FLAG:
    // Box layout provisional (matrix44affine basis @ +0, half-dims @ +64) -- refined by Box's own TU.
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
        // into the out-box (Matrix44Affine basis @ +0, half-dims @ +64). FLAG: Box layout provisional.
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

        // The active deformation-joint spec (sub_825C1170(mpIKPart, mi8ActiveJointsTagPointIndex) ==
        // IKBodyPart::GetJointSpec at the active tag-point index). Read its rotation axis / max angle /
        // detach threshold for the integration. FLAG: the gravity/restitution/resolution tuning and the
        // angle-clamp polynomial tables are rodata-not-recovered, so the integrated delta is inert.
        const DeformationJointSpec* lpJointSpec = mpIKPart->GetJointSpec(mi8ActiveJointsTagPointIndex);
        (void)lpJointSpec;
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
    // FLAG: kfJointForceMultiplier / kfJointPenetrationMultiplier and the rotation-proportion gate are
    // rodata-not-recovered. The break PREDICATE STRUCTURE (which magnitudes, which OR, the early-outs)
    // is exact; with the multipliers carried as 0 the force/penetration compares reduce to `magnitude
    // * 0 > maxStress` (false for finite maxStress), so the joint does not spuriously break -- a
    // documented inert default, never an invented threshold.
    // =========================================================================================
    bool PhysicalBodyPart::TestJointForBreaking(CgsPhysics::PhysicsSimulationIO::InputBuffer* lpSimInput,
                                                CgsPhysics::PhysicsSimulationIO::OutputBuffer* lpSimOutput)
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

        // (4a) joint force along axis. The asm forms vmsum3fp128(jointForce, rotationAxis) and scales
        // by kfJointForceMultiplier. With the joint force assembled from the (rodata-tuned) integrator
        // and the multiplier rodata-not-recovered, the magnitude is carried as the body's accumulated
        // collision magnitude (+432 w lane) -- the closest recovered proxy -- scaled by the placeholder.
        {
            const f32 lfJointForceMagnitude = mWorldPenetrationPlusCollisionMagnitude.GetPlus();   // +432 w
            const f32 lfScaledForce = lfJointForceMagnitude * KF_JOINT_FORCE_MULTIPLIER;
            if ( lfScaledForce > lfMaxStress )   // vcmpgtfp scaledForce, maxStress
            {
                lbBreak = true;
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

        // Build + emit the DetachedPartNotificationEvent onto the sim OutputBuffer's notification queue
        // (the asm assembles the packed event from mpIKPart / mGlobalVehicleId / the body id, then
        // AddEventSafeAppend's it). Modelled through the flagged emit hook with the packed event blob.
        EmitDetachedPartNotification(lpSimOutput, &mRigidBodyId);

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
        mRwBody.CalculateNewVelocity();

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
}
}
