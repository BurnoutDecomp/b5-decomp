#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnDeformableObject.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"          // CGS_ASSERT
#include "GameSource/Physics/VehicleManager/VehiclePhysics/VehiclePhysics.h"   // VehiclePhysics::GetAllWheelsHaveTraction (the +0x135B byte, by name)
#include "rw/math/vpu/vector3_operation.h"                  // rw::math::vpu::{Abs, ...}

// ---- the world-contact-generation closure (2026-08-27, detach-3 wave) --------------------------
// Every one of these was listed by the retired banner as an "opaque forward-declared type" or an
// "unresolved external". All are real, committed headers today.
#include "GameShared/GameClasses/Geometric/Primitives/CgsBox.h"                               // CgsGeometric::Box (the part primitive)
#include "GameShared/GameClasses/Geometric/Primitives/CgsCylinder.h"                          // CgsGeometric::Cylinder (the wheel primitive)
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_TriangleCache.h"              // TriangleCacheInterface::GetCache / GetNumCachedTriangleBatches
#include "GameShared/GameClasses/SceneManager/Collision/Primitives/CgsTriangleList.h"         // TriangleList + CheckAlignment / ValidateTriangles
#include "GameShared/GameClasses/SceneManager/Collision/Primitives/CgsPrimitivePairListBuilder.h" // PrimitivePairListBuilder + Prepare / AddPrimitive
#include "GameShared/GameClasses/SceneManager/Collision/ContactGenerator/CgsCollisionGenerator.h"  // CollisionGenerator + the two collide entry points
#include "GameSource/Physics/BrnContactGenerationList.h"                                      // ContactGenList::AddEntry
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnDetachedPartManager.h"  // DetachedPartManager::GetPartFromIndex
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnDetachedWheelManager.h" // DetachedWheelManager::GetWheel (== the asm's sub_825E8308)
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnPhysicalBodyPart.h"     // PhysicalBodyPart accessors
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnPhysicalWheel.h"        // PhysicalWheel accessors + GetCylinder

#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // gpDebugPrint ([hinge-cache] witness)

#include <cmath>                                            // std::fabs
#include <cstdlib>                                          // getenv ([hinge-cache] latch)

// =====================================================================================================
// BrnPhysics::Deformation::DeformableObject -- CONTACT-GENERATION group.
//
// Four functions that feed the penetration / collision pipeline:
//
//   * GetVehicleWorldRestitution        @0x825E0C78  -- combined restitution for a vehicle-vs-world
//                                                       contact (consulted by ApplyCarWorldImpulse).
//   * AddContactsToPenetrationSolver    @0x82609F98  -- push every deformation sensor's stored contacts
//                                                       into the shared penetration solver.
//   * DoBodyPartWorldContactGeneration  @0x82609278  -- build world contacts for the detached / hinged
//                                                       body parts (primitive-vs-triangle collision).
//   * DoDetachedWheelWorldContactGeneration @0x82609878 -- the same, for detached wheels.
//
// Member-offset map recovered from the X360 ARTIST.XEX asm (BrnDeformableObject.cpp), pinned BY NAME
// onto the frozen header members:
//   this+6368  -> mpDeformationSpec      (spec+1618 == mu8NumDeformationSensors; GetNumSensors()-4)
//   this+6476  -> mVehicleBody's attached VehiclePhysics  (mVehicleBody.GetVehiclePhysics())
//   this+6480  -> maDeformationSensors[] (stride 432 == sizeof(DeformationSensor))
//   this+26180 -> maPartStates[50]       (per-part EPartState)
//   this+26236 -> mau8PhysicalBodyPartPoolIndex[50]
//   this+26286 -> mi16NumPhysicalParts   (GetNumPhysicalParts())
//   this+26288 -> mi16NumHingedParts     (GetNumHingedParts())
//   this+26384 -> mGlobalEntityId        (GetGlobalEntityId())
//
// ⭐⭐⭐ 2026-08-27 (detach-3 wave): THE TWO Do*WorldContactGeneration BODIES ARE REAL. The banner that
// stood here -- "almost every callee is an opaque forward-declared type with NO accessors, so the inner
// contact-emission cannot be reconstructed BY NAME without fabricating callee signatures" -- was TRUE
// when it was written (2026-07, wave 4) and had gone STALE in the helpful direction. Re-measured this
// wave, every single named blocker it lists now has a real, committed home:
//   PrimitivePairListBuilder + Prepare/AddPrimitive(Box*)/AddPrimitive(Cylinder*)  wave Q6, 2026-08-19
//     (sub_82814570 and sub_82814678 are those two AddPrimitive overloads, not unknowns)
//   BaseCollisionGenerator::CollidePrimitiveListAgainstTriangleList                wave Q6, 2026-08-19
//   BaseCollisionGenerator::AddPrimitiveListWithTriangleListToStream               wave Q6, 2026-08-19
//   TriangleList + CheckAlignment/ValidateTriangles                                CgsTriangleList.h
//   TriangleCacheInterface::GetCache / ::GetNumCachedTriangleBatches               CgsSceneManagerIO_TriangleCache.h
//   ContactGenList::AddEntry                                                       BrnContactGenerationList.h
//   DetachedWheelManager::GetWheel(EntityId, s32)                                  == the asm's sub_825E8308
//   PhysicalBodyPart::GetBoundingBox / PhysicalWheel::GetCylinder                  the two primitive sources
// ⚠️⚠️ WHAT WAS ACTUALLY BLOCKING was not opacity, it was TWO PHANTOM TYPES in the DECLARATION:
// `CgsPhysics::CollisionGenerator` and `InTriangleCacheInterface` had no definition anywhere and no
// possible definition, so these methods' mangled names could never be produced by any caller. Both are
// retyped to their real homes in BrnDeformableObject.h this wave; see the evidence block there.
// ⇒ THE LESSON, recorded because it cost this chain a month: a "cannot be reconstructed" note names a
// set of blockers, and that set expires silently. Ask WHEN it last ran before believing it.
//
// The bodies below are transcribed call-for-call from the X360 asm
// (.ida-exports/BURNOUT_X360_ARTIST.XEX/0x82609278.json, 384 insns, and 0x82609878.json, 156), with the
// register/argument mapping read out of the raw `assembly`, not the allocation-failed pseudocode.
// GetVehicleWorldRestitution and AddContactsToPenetrationSolver were already real.
// =====================================================================================================

namespace BrnPhysics
{
namespace Deformation
{
    namespace vpu = rw::math::vpu;

    // -------------------------------------------------------------------------------------------------
    // Unrecovered .rodata for GetVehicleWorldRestitution. The X360 build loads these from constant
    // pools with no resolvable symbol; per the no-fabrication rule they are honest zero placeholders.
    //   * unk_82FB7FF0 -- the normal-Y threshold the abs(normal.y) is compared against.
    //   * unk_82FB8260 -- the restitution value returned when abs(normal.y) >= that threshold (a broadcast vec4).
    // The compare/select SHAPE is exact; the numeric output stays inert until the rodata is recovered.
    // -------------------------------------------------------------------------------------------------
    // ⭐ RECOVERED 2026-08-03 from the static-init splats (they read zero in the image because they
    // are .data filled at init, not because they have no value). ⚠️ The zero threshold was NOT inert:
    // abs(normal.y) >= 0 is true for every contact, so the restitution branch was taken always --
    // and then returned 0 restitution. A 0.5 threshold is the usual "is this surface floor-like".
    static const f32     KF_WORLD_RESTITUTION_NORMAL_Y_THRESHOLD = 0.5f;   // unk_82FB7FF0 @82C5D3E0 <- flt_82001DA0
    static const VecFloat KVF_WORLD_RESTITUTION_VALUE = { 1.10000002f, 1.10000002f, 1.10000002f, 1.10000002f };  // unk_82FB8260 @82C5D3B4 <- flt_82004A1C

    // The global "spy world-contact mode" selector byte the two Do*WorldContactGeneration methods
    // branch on (asm: DoBodyPart `*(v77 - 23737)`, DoDetachedWheel `byte_82F2A347`): when set, the
    // generator records the contacts through AddPrimitiveListWithTriangleListT (the spy path);
    // when clear, through CollidePrimitiveListAgainstTriangles.
    // ⭐ RECOVERED 2026-08-24 (deform-land wave, P9; physics11 audit cluster C item 6): the
    // STATIC IMAGE VALUE of byte_82F2A347 is 0x01 -- the spy world-contact path IS the console
    // boot default, not a debug opt-in. The old FLAGGED-0 ran the other arm on every contact.
    static const u8 KU_USE_SPY_WORLD_CONTACT_PATH = 1;   // byte_82F2A347 image value == 0x01

    // ---------------------------------------------------------------------------------------------
    // The world-contact-generation literals, ALL of them baked into the two console bodies at
    // 0x82609278 / 0x82609878. Each is cited with the instruction that carries it so a later wave can
    // re-check one without re-reading both functions.
    // ---------------------------------------------------------------------------------------------
    // The scene-entity OWNER tags the contact-gen entries are keyed on (`li r4, 6` @0x8260932C with
    // the `li r4, 7` fall-through @0x82609344, selected by owner == 1).
    static const u32 KU_ENTITY_OWNER_RACECAR                  = 1u;
    static const u32 KU_ENTITY_OWNER_RACECAR_DEFORMABLE_PART  = 6u;
    static const u32 KU_ENTITY_OWNER_TRAFFIC_DEFORMABLE_PART  = 7u;

    // THE CONTACT PADDING BAND -- byte-verified image floats, see the DoBodyPart banner.
    static const f32 KF_HINGED_PART_CONTACT_PADDING    = 0.05f;  // flt_820047C8 (`fmr f1, f30` @0x82609504)
    static const f32 KF_FAT_BOX_HALF_EXTENT_THRESHOLD  = 0.15f;  // flt_82094574 -> unk_82FBA270 splat
    static const f32 KF_MAX_PART_CONTACT_PADDING       = 0.5f;   // flt_82001DA0 -> unk_82FBA260 splat

    // The two pool-bound tripwires (`cmplwi 0x32` @0x82609478/0x82609650, `cmplwi 0x70` in the wheel
    // twin). 50 == PhysicalBodyPartPool's slot count, 112 == the wheel pool's.
    static const u8 KU8_MAX_PART_POOL_INDEX  = 0x32u;
    static const u8 KU8_MAX_WHEEL_POOL_INDEX = 0x70u;

    // The generator call literals. 25 for a single detached part/wheel list, 200 for the car's whole
    // hinged list (`li r6, 0x19` / `li r6, 0xC8`); tag A 0 and tag B 1 on the per-part arms.
    static const u16 KU16_PART_MAX_RESULTS   = 25u;
    static const u16 KU16_HINGED_MAX_RESULTS = 200u;
    static const u32 KU_PART_QUEUE_TAG_A     = 0u;
    static const u16 KU16_PART_TAG_B         = 1u;

    // The wheel loop is a fixed four iterations (stride 224, bound 896) and only a wheel whose
    // Wheel::mu8State (+0xD7 of the 224-byte record, i.e. the asm's vehiclePhysics+519+224*i) reads
    // 2 participates.
    static const s32 KI_NUM_DETACHABLE_WHEELS  = 4;
    static const u8  KU8_WHEEL_STATE_DETACHED  = 2u;

    // =================================================================================================
    // GetVehicleWorldRestitution @0x825E0C78
    //
    // ⭐⭐⭐ READ THIS BEFORE BLAMING e == 0 FOR "THE CAR SLIDES ON ITS ROOF INSTEAD OF BARREL-ROLLING"
    // (rotation-preservation sweep, 2026-09-05). THE GATE POINTS THE OTHER WAY FROM WHAT THAT STORY
    // NEEDS, AND THIS IS NOT THE ONLY RESTITUTION IN THE CRASH.
    //
    // 1. THIS FUNCTION IS THE TUMBLE-*MAKER*, NOT THE TUMBLE-KILLER, AND IT IS SHOWTIME-ONLY.
    //    restitution = (|n.y| >= 0.5f) ? 1.1f : 0.0f, and only when the showtime predicate is TRUE.
    //    A FLAT ground / roof contact (|n.y| ~ 1) is precisely the case that selects 1.1 -- a
    //    SUPER-ELASTIC bounce, which is what a Hollywood barrel roll is made of. Our build never
    //    reaches it because it never enters showtime; a car that is merely CRASHING gets the zero
    //    arm at 0x825E0CB8. ⛔ So "is e == 0 right for a car-vs-world contact while inverted?" is
    //    answered YES for this function on this path, and the missing tumble is NOT hiding here.
    //
    // 2. THERE IS A SECOND, LARGER RESTITUTION, AND IT IS NOT IN THIS FILE. The car body's normal
    //    response against the world is an rw::physics SOLVER contact, and the potential-contact
    //    builder stamps its material at 0x825A9D68..0x825A9D74:
    //        stfs f31(0x8200473C = 0.4 ), 0x174(r1)   dynamic friction
    //        stfs f30(0x82001DA0 = 0.5 ), 0x170(r1)   static friction
    //        stfs f30(0x82001DA0 = 0.5 ), 0x178(r1)   mRestitution      <-- NOT zero
    //    with per-owner overrides at 0x825A9F54 (detached wheels/parts 9/10: 0.8 / 0.8 / 0.2) and
    //    0x825A9F80 (deformable parts 6/7: 0.4 / 0.6 / 0.05). BrnPhysicsModuleBridgeFunctions.cpp
    //    :918/:1015/:1021 already stamps exactly these. ⇒ THE GROUND IS ELASTIC (e = 0.5) AND
    //    FRICTIONAL (mu 0.4-0.5) AT THE SOLVER, even while the deformation-sensor impulse path runs
    //    at e == 0. The two are different mechanisms on the same collision; do not quote one as
    //    "the console's restitution rule" for the other.
    //
    // 3. AND THERE IS A TANGENTIAL TERM THAT CONVERTS SLIDE INTO SPIN, INSIDE ApplySensorImpulse
    //    @0x826078B0 -- the classic Coulomb form, applied DIRECTLY to the rigid body:
    //        0x82608070  vmsum3fp128 v12, v_rel, n          ; v_rel . n
    //        0x82608078  vsubfp128   v123, v_rel, n(v_rel.n) ; the tangent
    //        0x82608088  vcmpgefp    |v_t|^2 >= 1e-4         ; below eps -> NO impulse at all
    //        0x826081A4  saturate(|v_t| / 1.0)
    //        0x826081B8  * the MODE SCALE  ->  0x826081C0 GetImpulsesFromLocalImpulse
    //        0x8260821C  AddWorldSpaceImpulse
    //        0x82608230  AddWorldSpaceAngularImpulse        <-- the spin deposit
    //        0x826082C0  AddLocalForce (-v_t * physics+0xE0 * 0.9, WORLD contacts only)
    //    MODE SCALE, gated on VehiclePhysics::mbIsCrashing (+0x710, writers SetCrashing
    //    @0x825D990C / ClearCrashing @0x825B8EAC) and on contact+0xB8 (1 == world):
    //        not crashing            -> 0.0   (a world contact deposits NOTHING outside a crash)
    //        crashing, world         -> 5.0
    //        crashing, car-car       -> 20.0  (only when the other car is crashing too; else 0.0)
    //    ⚠️⚠️ THIS PATH DOES NOT GO THROUGH ImpulsePasser, so it is INVISIBLE to the
    //    [crash-response] arrive census, which watches RecievePassedOnImpulse only. Every "the
    //    contacts deposit X" number this campaign has published counts the passed-on chain and
    //    NOT this. The residual it would explain is already on the ledger: over the contact frames
    //    of mwK_h230_s60 the observed dW_roll runs ~+0.2 rad/s per frame ABOVE what the arriving
    //    deposits predict (f683 +0.046 predicted / +0.280 observed, f684 +0.060 / +0.291,
    //    f685 +0.132 / +0.338). Instrument AddWorldSpaceAngularImpulse before theorising further.
    //
    // 4. NO ANGULAR RESTITUTION EXISTS ANYWHERE ON THE CHAIN. Every angular deposit is a bare cross
    //    product: GetImpulsesFromLocalImpulse @0x825A1A80 is vpermwi/vmulfp/vnmsubfp/vpermwi == r x J
    //    and nothing else, and AddWorldSpaceAngularImpulse @0x825BEAA8 is a NaN assert plus
    //    `lvx128 this+0x110 ; vaddfp128` -- a plain accumulate, no scale, no clamp.
    //
    // ⛔ lbUseNormalScaledFriction IS DEAD IN *BOTH* PATHS, not just the world one: ApplySensorImpulse
    //    saves r3/r4/r5/r6/r7 across its `bl memcpy @0x8260790C` and NEVER saves r8, which memcpy then
    //    clobbers. So `li r8,1 @0x82625150` in the car-car sibling is equally dead. The real
    //    world/car-car discriminator is contact+0xB8.
    //
    // Combined restitution for a vehicle-vs-WORLD contact. The asm consults the virtual showtime-style
    // predicate on this car's physics body (mVehicleBody.GetVehiclePhysics(), vtable slot +16 -- the
    // same predicate ApplyCarCarImpulse gates its bounce shaping on). If the predicate is FALSE the
    // restitution is zero. If TRUE it takes |lContact.mNormal.y| (vandc sign-clear of the splatted
    // word-1 lane), and returns ZERO when that abs-Y is BELOW the rodata threshold, else the rodata
    // restitution vector (asm: vcmpgefp threshold then vnot+vsel -> select(value,0, absY < thresh)
    // = ZERO where absY < thresh).
    // Body hint (BrnDeformableObject.cpp:3371): VehicleRigidBody::GetVehiclePhysics, Abs<VectorAxisY>,
    // GetVecFloat_Zero, CompLessThan, Select. The VMX is de-SIMD'd to scalar per project convention.
    // =================================================================================================
    VecFloat DeformableObject::GetVehicleWorldRestitution(const StoredImpulseContact& lContact) const
    {
        // The showtime predicate on this car's physics body, through the race-car downcast guard.
        // ⭐ CORRECTED 2026-09-05: this called IsPlayerVehicleActuallyInShowtime(), which is vtable
        // slot +0x14 (@0x827E42B0, a bare `lbz r3,0x140C(r3)`). The console dispatches slot +0x10 --
        // read out of the image here, not inferred:
        //     0x825E0C90  lwz   r3, 0x194C(r4)     ; the RaceCarPhysics*
        //     0x825E0C98  lwz   r11, 0(r3)         ; its vtable
        //     0x825E0C9C  lwz   r11, 0x10(r11)     ; <-- SLOT +0x10
        //     0x825E0CA4  bctrl
        // and +0x10 on RaceCarPhysics' concrete table (0x820D1034) is IsPlayerVehicleInShowtime
        // @0x825D7B68, the THREE-term predicate (mbPlayerCarInShowtime && !mbDisableShowtime &&
        // mfTimeUntilPush <= 0), not the bare flag. The loose predicate let the super-elastic 1.1
        // arm fire in windows the console excludes. (BrnDeformableObject.cpp's two car-car sites
        // still spell the +0x14 name; they are NOT changed here because their own dispatch offsets
        // have not been read out of the image -- do that before touching them.)
        const Vehicle::RaceCarPhysics* lpRaceCarPhysics = AsRaceCarPhysics();
        const bool lbPredicate = (lpRaceCarPhysics != nullptr) &&
                                 lpRaceCarPhysics->IsPlayerVehicleInShowtime();

        if (!lbPredicate)
            return VecFloat{ 0.0f, 0.0f, 0.0f, 0.0f };

        // abs(normal.y) (asm: vspltw word-1 then vandc the sign bit) compared against the rodata
        // threshold. asm @0x825E0C78: vcmpgefp(absY, threshold) then vnot -> mask = (absY < threshold),
        // then vsel(VALUE, ZERO, mask) = mask ? ZERO : VALUE. So the routine returns ZERO when absY is
        // BELOW the threshold and the rodata restitution VALUE when absY >= threshold (Select).
        const f32 lfAbsNormalY = std::fabs(lContact.mNormal.y);
        if (lfAbsNormalY < KF_WORLD_RESTITUTION_NORMAL_Y_THRESHOLD)
            return VecFloat{ 0.0f, 0.0f, 0.0f, 0.0f };

        return KVF_WORLD_RESTITUTION_VALUE;
    }

    // =================================================================================================
    // AddContactsToPenetrationSolver @0x82609F98
    //
    // Push every deformation sensor's stored contacts into the shared penetration solver. The asm:
    //   v11 = *(*(this+6476) + 4955);              // +0x135B == VehiclePhysics::mbAllWheelsHaveTraction
    //                                              // (NOT a "world" flag -- see the body's note)
    //   if ( *(*(this+6368) + 1618) )              // mpDeformationSpec->mu8NumDeformationSensors > 0
    //     do
    //       DeformationSensor::AddContactsToPenetrationSolver(sensor, lpSolver, lpDefObjBase,
    //           liWorldObjectIndex, liObjectIndex, v11);
    //       sensor += 432; ++i;
    //     while ( i < *(*(this+6368) + 1618) );
    // The loop count is the spec's BARE sensor count `*(mpDeformationSpec + 1618)` ==
    // mu8NumDeformationSensors (asm @0x82609F98 bounds the do/while on this exact field, NO +4). The
    // asm-attested identity GetNumSensors() == *(spec+1618) + 4 (dossier `return *(*(a1+6368)+1618)+4`)
    // means the bare count is GetNumSensors() - 4; mu8NumDeformationSensors itself is private on
    // StreamedDeformationSpec with no public accessor, so the bare count is spelled GetNumSensors() - 4
    // here -- matching the committed siblings (Update.cpp UpdateSensorDisplacements etc.). const.
    // =================================================================================================
    void DeformableObject::AddContactsToPenetrationSolver(PenetrationSolver* lpSolver,
                                                          DeformableObject* lpDefObjBase,
                                                          s32 liWorldObjectIndex, s32 liObjectIndex) const
    {
        // The byte the asm reads at physics-body +4955 is +0x135B == VehiclePhysics::
        // mbAllWheelsHaveTraction (VehiclePhysics.h:1584, accessor :1624) -- NOT a "world" flag: the
        // sensor uses it as one half of the vehicle-arm normal-flattening gate, and the DWARF spells
        // the parameter it becomes `lbVehicleWheelsAllHaveTraction` (BrnPhysicsUnity2.cpp:6563).
        // The parameter names here are the DWARF's too (BrnDeformableObject.cpp:1877).
        const bool lbVehicleWheelsAllHaveTraction =
            mVehicleBody.GetVehiclePhysics()->GetAllWheelsHaveTraction();

        // The bare deformation-sensor count (asm: *(mpDeformationSpec + 1618)). == GetNumSensors() - 4.
        // FLAG: the frozen header declares GetNumSensors() non-const, so it is reached through a const_cast
        // here (this is a pure read in the asm); drop the cast if GetNumSensors() is later made const.
        const s32 liNumSensors = const_cast<DeformableObject*>(this)->GetNumSensors() - 4;
        if (liNumSensors <= 0)
            return;

        const DeformationSensor* lpSensor = &maDeformationSensors[0];
        s32 liIndex = 0;
        do
        {
            lpSensor->AddContactsToPenetrationSolver(lpSolver, lpDefObjBase, liWorldObjectIndex,
                                                     liObjectIndex, lbVehicleWheelsAllHaveTraction);
            ++liIndex;
            ++lpSensor;
        }
        while (liIndex < liNumSensors);
    }

    // =================================================================================================
    // DoBodyPartWorldContactGeneration @0x82609278 (384 insns) -- TRANSCRIBED CALL-FOR-CALL 2026-08-27.
    //
    // Generate the world contacts for this car's DETACHED and HINGED body parts against the cached
    // collision triangles, and register one ContactGenList entry per emitted primitive list so
    // VehicleManager::EndPartContactGeneration can harvest the results.
    //
    // Argument mapping read from the raw asm prologue (0x82609298..0x826092D8), NOT the pseudocode
    // (which is flagged "local variable allocation has failed" and invents 26 int parameters):
    //     r3  this                r4 -> r29     lpTriCache      r5 -> arg_24  lpGenList
    //     r6  -> arg_2C  lpGen    r7 -> arg_34  lpProducer      r9 -> arg_44  luResultListTag
    //     r8  -> arg_3C  lpPartMgr (the pool fetch at 0x82609408 loads it back)
    //     r10 -> arg_4C  lpAlloc
    //
    // TWO CONSTANTS AND A BRANCH THAT MATTER MORE THAN THEY LOOK -- the CONTACT PADDING BAND.
    // The console does NOT collide the part's bare bounding box. It measures the box's SMALLEST
    // half-extent and pads the primitive by an amount that is LARGER when the box is SMALLER:
    //     0x826095E0..0x826095FC  v127 = min(box.mDimensionsAndFatness.x, .y, .z)   (three vspltw +
    //                             two vminfp -- the three half-extent lanes, reduced)
    //     0x8260962C              vcmpgtfp128. v127 > splat(0.15)  ->  r29, the "fat box" flag
    //     0x826096B8..0x826096D4  fat  -> padding = min(smallest half-extent, 0.5)
    //                             thin -> padding = 0.5                (fmr f1, f31)
    // ⭐⭐ CONSEQUENCE, stated because the next wave would otherwise re-derive it the expensive
    // way: a DEGENERATE part box -- and every part's box is degenerate today, sitting on the 0.05 half
    // floor because PhysicalBodyPart::CalculateSkinnedPoint is still a placeholder -- is handed to the
    // collider with a 0.5 m pad, i.e. an effective ~0.55 m test region, not 0.087 m. The console's own
    // code carries the guard against a small primitive tunnelling, and it is inversely gated on the box
    // size. So the placeholder skin data does NOT gate this chain, and a part that still falls after
    // this commit is NOT explained by its box being small. That is what makes the measurement
    // discriminating, and it is why this wave landed contact generation before the skinning.
    // (The three floats are byte-verified image constants: flt_820047C8 == 0.05 -- the HINGED pad,
    //  flt_82094574 == 0.15 -- the fat threshold, flt_82001DA0 == 0.5 -- the pad ceiling. The last is
    //  the same literal this file's own KF_WORLD_RESTITUTION_NORMAL_Y_THRESHOLD already recovered.)
    //
    // The two static splat vectors the compare uses (unk_82FBA260 == 0.5, unk_82FBA270 == 0.15) are
    // lazily built on first use behind the bit flags in dword_82FBA254. That is a pure compiler
    // materialisation of two constants, not behaviour, so it is spelled as the constants.
    // =================================================================================================
    void DeformableObject::DoBodyPartWorldContactGeneration(const InTriangleCacheInterface* lpTriCache,
                                                            ContactGenList* lpGenList,
                                                            CgsSceneManager::CgsCollision::CollisionGenerator* lpGen,
                                                            CgsMemory::SimpleDataStreamProducer* lpProducer,
                                                            const DetachedPartManager* lpPartMgr,
                                                            u32 luResultListTag,
                                                            CgsMemory::LinearMalloc* lpAlloc) const
    {
        typedef CgsSceneManager::CgsCollision::PrimitivePairListBuilder PrimitivePairListBuilder;
        typedef CgsSceneManager::CgsCollision::TriangleList             TriangleList;

        // asm 0x826092D0/0x826092F8: the whole routine is gated on mi16NumPhysicalParts != 0.
        const s16 li16NumPhysicalParts = GetNumPhysicalParts();
        const s16 li16NumHingedParts   = GetNumHingedParts();
        if (li16NumPhysicalParts == 0)
        {
            return;
        }

        // ---- the two scene ids the ContactGenList entries are keyed on ----------------------------
        // asm 0x826092FC..0x82609358. The first EntityId::Set(0,0,0) is the WORLD side and is stashed
        // as var_140; the second re-Sets the same stack id to this car's DEFORMABLE-PART owner and is
        // stashed as var_138. Both are widened into a VolumeInstanceId by shifting the 32-bit entity
        // word into the HIGH dword (`extldi r10,r10,64,32`), which is the packing
        // CgsVolumeInstanceId.h documents (KU_ENTITY_ID_START_INDEX == 32).
        // NOTE: the packing/unpacking type is CgsSceneManager::EntityId (Set/GetOwner/
        // GetEntityIndex live there); the tree's unqualified ::EntityId is the bare 32-bit storage
        // word the ContactGenList and the model members carry. They convert both ways by value.
        CgsSceneManager::EntityId lWorldEntityId;
        lWorldEntityId.Set(0u, 0u, 0u);
        CgsSceneManager::VolumeInstanceId lWorldVolumeInstanceId;
        lWorldVolumeInstanceId.muId = static_cast<u64>(static_cast<u32>(lWorldEntityId)) << 32;

        // ⭐⭐⭐ SOURCE CORRECTED 2026-09-05 (detach wave) -- THIS IS mHandlingBodyID, NOT
        // mGlobalEntityId, AND THE DIFFERENCE CRASHES THE GAME.
        // Every entity word this function derives comes from console +0x6710 == 26384 ==
        // mHandlingBodyID, read as an 8-byte `ld` whose HIGH dword is the entity word:
        //     0x82609314  ld     r11, 0x6710(r20)    ; the 8-byte handling-body id
        //     0x82609320  srdi   r11, r11, 32        ; -> the entity word
        //     0x82609334  srwi   r10, r11, 24        ; -> GetOwner()          (the 1 -> 6 / else 7 map)
        //     0x82609338  extrwi r5,  r11, 14,8      ; -> GetEntityIndex()
        //   ...and again, verbatim, for the hinged block's TRIANGLE-CACHE SLOT:
        //     0x8260977C  ld     r11, 0x6710(r20)
        //     0x82609784  srdi   r11, r11, 32
        //     0x82609788  extrwi r31, r11, 14,8      ; -> the slot handed to GetNumCachedTriangleBatches
        // +26392 (mGlobalEntityId) is never loaded anywhere in this function's 384 instructions.
        // ⛔ WHY IT MATTERS, MEASURED: the two ids are NOT interchangeable. mHandlingBodyID's entity
        // index is the PHYSICS-BODY index -- race cars 0..7, traffic 8.., which is exactly the
        // triangle-cache slot numbering VehicleManager::AddRaceCarTractionLineTests (slot == liCar)
        // and PhysicalTrafficManager::AddTrafficTractionLineTests (slot == i + 8) already use.
        // mGlobalEntityId's index is the WORLD entity index: the traffic cars in run hinge_A1
        // carried global ids 0x02064000 / 0x02070000 / 0x02090000, whose (>>10)&0x3FFF indices are
        // 400 / 448 / 576 -- and TriangleCacheManager::Prepare allocates exactly
        // KU_MAX_CACHED_OBJECTS == 298 CacheSlots. So those three reads land 102, 150 and 278 slots
        // PAST the end of mpaCachedObjectSlots; miIndexIntoTriangleCache came back as heap garbage and
        // TriangleList::ValidateTriangles walked a wild Triangle4* -- measured, a hard
        // EXCEPTION_ACCESS_VIOLATION reading (Triangle4*)0x2CC6F8BF50 + 0x90 (&mValidMasks) in
        // CgsGeometric::Triangle4::AssertIsValid, from exactly this call site.
        // ⚠️ IT WAS LATENT, NOT NEW: the hinged block is gated on the builder having primitives, and
        // until DeformableObject::CheckForDetachment started passing the console's own lbHinge
        // (same wave, BrnDeformableObject_Detach.cpp) NO PART HAD EVER BEEN HINGED in this build,
        // so this code had never run. The two ContactGenList keys above it were wrong all along --
        // silently, because a mis-keyed entry only loses a harvest.
        const CgsSceneManager::EntityId lHandlingEntityId = mHandlingBodyID.GetEntityId();
        // asm: `srwi r10, r11, 24` then `cmplwi 1` -- owner 1 (RACECAR) maps to the RACECAR
        // deformable-part owner 6, anything else (traffic) to 7.
        const u32 luDeformablePartOwner =
            (lHandlingEntityId.GetOwner() == KU_ENTITY_OWNER_RACECAR)
                ? KU_ENTITY_OWNER_RACECAR_DEFORMABLE_PART
                : KU_ENTITY_OWNER_TRAFFIC_DEFORMABLE_PART;
        CgsSceneManager::EntityId lDeformablePartEntityId;
        lDeformablePartEntityId.Set(luDeformablePartOwner, lHandlingEntityId.GetEntityIndex(), 0u);
        CgsSceneManager::VolumeInstanceId lDeformablePartVolumeInstanceId;
        lDeformablePartVolumeInstanceId.muId = static_cast<u64>(static_cast<u32>(lDeformablePartEntityId)) << 32;

        // ---- the HINGED parts' shared builder -----------------------------------------------------
        // asm 0x8260935C/0x82609378: Construct always, Prepare only when there is at least one hinged
        // part, sized by mi16NumHingedParts.
        PrimitivePairListBuilder lHingedPartBuilder;
        lHingedPartBuilder.Construct();
        if (li16NumHingedParts > 0)
        {
            lHingedPartBuilder.Prepare(lpAlloc, static_cast<u16>(li16NumHingedParts));
        }

        // ---- per physical part (asm 0x82609404..0x82609764) ---------------------------------------
        for (s32 liPart = 0; liPart < li16NumPhysicalParts; ++liPart)
        {
            // `lbz r4, 0x667C(this + i)` == mau8PhysicalBodyPartPoolIndex[i], fetched through the
            // manager (its one member mPartPool sits at +0, which is why the console passes the
            // MANAGER address as the pool's `this` -- the GetPartFromIndex inline-forward the
            // DetachedPartManager header documents).
            const PhysicalBodyPart* lpCurrentPart =
                lpPartMgr->GetPartFromIndex(mau8PhysicalBodyPartPoolIndex[liPart]);

            // WARNING: asm 0x8260941C reads `lbz r11, 0x1E6(part)` and SKIPS when it is set. +0x1E6 is
            // mbFrozen, NOT "already in the gen list" as the banner that stood here claimed -- the
            // same wrong-member-named-in-a-comment shape the previous wave found in
            // DetachedPartManager::UpdateTriangleCache (+0x1E4 mbJoinedToVehicle read as IsFrozen).
            // Read the offset, not the name. A settled part generates no world contacts.
            if (lpCurrentPart->IsFrozen())
            {
                continue;
            }

            // The part's oriented bounding box, in world space (asm 0x8260942C).
            CgsGeometric::Box lPartBox;
            lpCurrentPart->GetBoundingBox(&lPartBox);

            // `ld 0x1D0(part) ; srdi 32 ; clrlwi 22` == mRigidBodyId.muEntityWord & 0x3FF ==
            // GetRigidBodyId().GetEntityId().GetPartIndex(), the index into maPartStates.
            const s32 liIKPartIndex = lpCurrentPart->GetIKPartIndex();

            if (GetPartState(liIKPartIndex) == E_PART_STATE_HINGED)
            {
                // asm 0x8260944C..0x82609510. A hinged part is still bolted on: it contributes its
                // box to the car's SHARED hinged list at the fixed 0.05 pad, and is collided once
                // for the whole car in the hinged block below.
                CGS_ASSERT(lpCurrentPart->IsJoinedToVehicle(),
                           "lpCurrentPart->IsJoinedToVehicle()");                              // :3572
                CGS_ASSERT(lpCurrentPart->GetPoolIndex() < KU8_MAX_PART_POOL_INDEX,
                           "Bad Part pool index: ");                                           // :3573
                lHingedPartBuilder.AddPrimitive(&lPartBox, KF_HINGED_PART_CONTACT_PADDING,
                                                lpCurrentPart->GetPoolIndex());
                continue;
            }

            // ---- DETACHED part (asm 0x82609514..0x82609744) ---------------------------------------
            CGS_ASSERT(GetPartState(liIKPartIndex) == E_PART_STATE_DETATCHED,
                       "maPartStates[lpCurrentPart->GetRigidBodyId().GetEntityId().GetPartIndex()] == E_PART_STATE_DETATCHED"); // :3586
            CGS_ASSERT(!lpCurrentPart->IsJoinedToVehicle(),
                       "!lpCurrentPart->IsJoinedToVehicle()");                                 // :3587

            // `ld 0x1D0 ; clrlwi 24 ; addi 0x49` == (pool index) + 73 == GetTriangleCacheSlot().
            const s32 liCacheSlot = static_cast<s32>(lpCurrentPart->GetTriangleCacheSlot());
            const s32 liNumCachedBatches = lpTriCache->GetNumCachedTriangleBatches(liCacheSlot);
            if (liNumCachedBatches <= 0)
            {
                continue;   // nothing cached under this part -- no collision this frame
            }

            TriangleList lTriangleList;
            lTriangleList.mpTriangles =
                const_cast<CgsGeometric::Triangle4*>(lpTriCache->GetCache(liCacheSlot));
            lTriangleList.miNumTriangles = liNumCachedBatches;
            lTriangleList.CheckAlignment();
            lTriangleList.ValidateTriangles();

            // THE PADDING BAND (see the banner). vminfp reduction of the three half-extent lanes,
            // compared against 0.15; the pad is 0.5 for a thin box and min(extent, 0.5) for a fat one.
            const Vector3 lBoxHalfDimensions = lPartBox.GetDimensions();
            f32 lfSmallestHalfExtent = lBoxHalfDimensions.x;
            if (lBoxHalfDimensions.y < lfSmallestHalfExtent) { lfSmallestHalfExtent = lBoxHalfDimensions.y; }
            if (lBoxHalfDimensions.z < lfSmallestHalfExtent) { lfSmallestHalfExtent = lBoxHalfDimensions.z; }

            const bool lbBoxIsFat = (lfSmallestHalfExtent > KF_FAT_BOX_HALF_EXTENT_THRESHOLD);
            const f32  lfContactPadding =
                lbBoxIsFat
                    ? ((lfSmallestHalfExtent < KF_MAX_PART_CONTACT_PADDING) ? lfSmallestHalfExtent
                                                                            : KF_MAX_PART_CONTACT_PADDING)
                    : KF_MAX_PART_CONTACT_PADDING;

            // One fresh single-primitive list per detached part. Prepare fully re-initialises the
            // builder (pointer / used / count / capacity), which is why the console Constructs only
            // the shared hinged one.
            PrimitivePairListBuilder lPartBuilder;
            lPartBuilder.Prepare(lpAlloc, 1u);

            CGS_ASSERT(lpCurrentPart->GetPoolIndex() < KU8_MAX_PART_POOL_INDEX,
                       "Bad Part pool index: ");                                               // :3612

            lPartBuilder.AddPrimitive(&lPartBox, lfContactPadding, lpCurrentPart->GetPoolIndex());

            // The generator entry point is picked by the global spy-mode byte (byte_82F2A347, image
            // value 1). The two calls carry the SAME six values in the two families' own argument
            // orders -- read register-for-register off 0x826096DC..0x82609720, not inferred.
            if (KU_USE_SPY_WORLD_CONTACT_PATH)
            {
                lpGen->AddPrimitiveListWithTriangleListToStream(&lPartBuilder, &lTriangleList,
                                                                KU16_PART_MAX_RESULTS,
                                                                lbBoxIsFat,
                                                                KU_PART_QUEUE_TAG_A, KU16_PART_TAG_B,
                                                                lpProducer);
            }
            else
            {
                lpGen->CollidePrimitiveListAgainstTriangleList(&lPartBuilder, &lTriangleList,
                                                               KU16_PART_MAX_RESULTS,
                                                               KU_PART_QUEUE_TAG_A, KU16_PART_TAG_B,
                                                               lbBoxIsFat);
            }

            // asm 0x82609724..0x82609744: the entry is keyed (PART, WORLD) -- `ld 0x1D0(part)` whole
            // into r4 and the stashed world id into r5. The WHOLE 8-byte handle, the same full-width
            // read the previous wave had to restore in PhysicalWheel::RemoveFromScene.
            CgsSceneManager::VolumeInstanceId lPartVolumeInstanceId;
            lPartVolumeInstanceId.muId = lpCurrentPart->GetRigidBodyId().GetBaseRigidBodyID();
            lpGenList->AddEntry(lPartVolumeInstanceId, lWorldVolumeInstanceId, 0u, 0u);
        }

        // ---- the HINGED block: one collide for the whole car (asm 0x82609770..0x82609858) ----------
        // ⚠️ THE GATE IS THE BUILDER'S TEST COUNT, NOT mi16NumHingedParts. asm 0x82609770 reads
        // `lhz r11, var_172` -- var_178 is the builder and +6 is its mu16NumTests -- and exits when
        // it is zero. Those differ: a hinged part that was skipped as FROZEN adds no primitive, so a
        // car with hinged parts can still reach here with an empty list. Gating on the count instead
        // would hand the generator a zero-primitive list, which its own
        // "lpPrimitiveList->GetNumTests() > 0" assert (CgsCollisionGenerator.cpp:1942) refuses.
        if (lHingedPartBuilder.GetNumTests() != 0)
        {
            // The car's own triangle-cache slot is the HANDLING BODY's entity index (no +73 base) --
            // 0x8260977C..0x82609788, the same `ld 0x6710 / srdi 32 / extrwi 14,8` triple as the ids
            // above. See the correction banner there for why it is not mGlobalEntityId.
            const s32 liCacheSlot = static_cast<s32>(lHandlingEntityId.GetEntityIndex());
            const s32 liNumCachedBatches = lpTriCache->GetNumCachedTriangleBatches(liCacheSlot);

            // ---- [hinge-cache] READ-ONLY WITNESS. NOT IN THE X360 BINARY. Opt-in via
            //      BRN_DEFORM_TRACE. DELETE-WHEN the hinged block is banked.
            // It prints BOTH candidate slot sources side by side so the correction above is a
            // measurement rather than a claim: `slot` is the console's (handling body) and
            // `globalSlot` is the one this file used to use. On the player they can agree; on
            // traffic they must not.
            {
                static s32 siHingeProbe = -1;
                if ( siHingeProbe < 0 )
                {
                    const char* lpcEnv = getenv("BRN_DEFORM_TRACE");
                    siHingeProbe = ( lpcEnv != 0 && atoi(lpcEnv) > 0 ) ? 1 : 0;
                }
                static u32 suHingeLines = 0u;
                if ( siHingeProbe == 1 && CgsDev::Log::gpDebugPrint != 0 && suHingeLines < 400u )
                {
                    ++suHingeLines;
                    const CgsSceneManager::EntityId lGlobal(
                        const_cast<DeformableObject*>(this)->GetGlobalEntityId().muValue);
                    *CgsDev::Log::gpDebugPrint
                        << "[hinge-cache] ent " << static_cast<s32>(lGlobal.GetEntityIndex() & 0xFFFFu)
                        << " owner " << static_cast<s32>(lHandlingEntityId.GetOwner())
                        << " slot " << liCacheSlot
                        << " globalSlot " << static_cast<s32>(lGlobal.GetEntityIndex())
                        << " batches " << liNumCachedBatches
                        << " tests " << static_cast<s32>(lHingedPartBuilder.GetNumTests())
                        << "\n";
                }
            }
            // ---- end [hinge-cache] ---------------------------------------------------------
            if (liNumCachedBatches > 0)
            {
                TriangleList lTriangleList;
                lTriangleList.mpTriangles =
                    const_cast<CgsGeometric::Triangle4*>(lpTriCache->GetCache(liCacheSlot));
                lTriangleList.miNumTriangles = liNumCachedBatches;
                lTriangleList.CheckAlignment();
                lTriangleList.ValidateTriangles();

                // asm 0x826097C4..0x826097D8: `lwz 0x194C(this)` is the attached VehiclePhysics and
                // `lwz 0x10D4(vp)` is mPreviousControls.meDriverType; the cntlzw/extrwi/xori triple
                // is the compiler's (value != 0). It rides the generator's optimised-box-test flag.
                const bool lbDriverTypeSet =
                    (GetVehiclePhysics()->GetPreviousControls()->GetType()
                        != BrnPhysics::Vehicle::E_DRIVER_TYPE_PLAYER);   // E_DRIVER_TYPE_PLAYER == 0

                CGS_ASSERT(luResultListTag > 0, "luResultListTag > 0");                        // :3675

                if (KU_USE_SPY_WORLD_CONTACT_PATH)
                {
                    lpGen->AddPrimitiveListWithTriangleListToStream(&lHingedPartBuilder, &lTriangleList,
                                                                    KU16_HINGED_MAX_RESULTS,
                                                                    lbDriverTypeSet,
                                                                    luResultListTag, KU16_PART_TAG_B,
                                                                    lpProducer);
                }
                else
                {
                    lpGen->CollidePrimitiveListAgainstTriangleList(&lHingedPartBuilder, &lTriangleList,
                                                                   KU16_HINGED_MAX_RESULTS,
                                                                   luResultListTag, KU16_PART_TAG_B,
                                                                   lbDriverTypeSet);
                }

                lpGenList->AddEntry(lDeformablePartVolumeInstanceId, lWorldVolumeInstanceId, 0u, 0u);
            }
        }
    }

    // =================================================================================================
    // DoDetachedWheelWorldContactGeneration @0x82609878 (156 insns) -- TRANSCRIBED 2026-08-27.
    //
    // The detached-WHEEL twin. Four fixed iterations (asm: stride 224 over the VehiclePhysics wheel
    // array, bound 896 == 4 * 224); a wheel participates only when its physics-body state byte reads
    // 2 (DETACHED). Unlike the body-part path there is no padding band and no fat/thin branch: a
    // detached wheel is always collided as a CYLINDER at the flat 0.5 pad with the generator flag
    // hard-wired true (`li r7, 1` on the stream arm, `li r9, 1` on the synchronous arm).
    // =================================================================================================
    void DeformableObject::DoDetachedWheelWorldContactGeneration(const InTriangleCacheInterface* lpTriCache,
                                                                 ContactGenList* lpGenList,
                                                                 CgsSceneManager::CgsCollision::CollisionGenerator* lpGen,
                                                                 CgsMemory::SimpleDataStreamProducer* lpProducer,
                                                                 const DetachedWheelManager* lpWheelMgr,
                                                                 u32 /*luResultListTag*/,
                                                                 CgsMemory::LinearMalloc* lpAlloc) const
    {
        typedef CgsSceneManager::CgsCollision::PrimitivePairListBuilder PrimitivePairListBuilder;
        typedef CgsSceneManager::CgsCollision::TriangleList             TriangleList;

        // asm 0x82609888: the world side of every entry this routine appends -- EntityId::Set(0,0,0)
        // widened into a VolumeInstanceId's high dword. (There is no second Set here; the wheel path
        // keys its entries on the wheel's own handle, not on a deformable-part entity.)
        CgsSceneManager::EntityId lWorldEntityId;
        lWorldEntityId.Set(0u, 0u, 0u);
        CgsSceneManager::VolumeInstanceId lWorldVolumeInstanceId;
        lWorldVolumeInstanceId.muId = static_cast<u64>(static_cast<u32>(lWorldEntityId)) << 32;

        PrimitivePairListBuilder lWheelBuilder;
        lWheelBuilder.Construct();

        for (s32 liWheel = 0; liWheel < KI_NUM_DETACHABLE_WHEELS; ++liWheel)
        {
            // asm: `*(vehiclePhysics + 224 * wheel + 519) == 2` -- only a DETACHED wheel generates.
            if (GetVehiclePhysics()->GetWheel(
                    static_cast<BrnPhysics::Vehicle::EVehicleDrivenWheel>(liWheel)).mu8State
                != KU8_WHEEL_STATE_DETACHED)
            {
                continue;
            }

            // sub_825E8308 == DetachedWheelManager::GetWheel(EntityId, s32).
            // ⛔⛔ 2026-09-05 (hinge-geometry wave): THE KEY WAS mGlobalEntityId AND THE CONSOLE'S IS
            // mHandlingBodyID -- the SAME field confusion that overran the triangle cache by 106
            // slots in DoBodyPartWorldContactGeneration, found by sweeping for it rather than by
            // tripping over it (tools/re/idfield_sweep.py). The asm, 0x82609920..0x82609934:
            //     ld     r11, 0x6710(r18)   ; mHandlingBodyID, the 8-byte handle
            //     srdi   r11, r11, 32       ; its entity word
            //     clrlwi r4, r11, 0         ; all 32 bits -> GetWheel's EntityId argument
            //     bl     sub_825E8308
            // +0x6718 (mGlobalEntityId) is NOT LOADED ONCE in this function's 156 instructions.
            // ⚠️ AND THIS ONE IS SILENT, WHICH IS WHY NOTHING CAUGHT IT: GetWheel is a LOOKUP, not a
            // subscript -- it walks its used-bitmask and matches the owner byte (`r14 >> 24`) then
            // the entity index (`r14 >> 10`), so a wrong key returns null and the loop `continue`s.
            // The observable cost is a DETACHED WHEEL THAT GENERATES NO WORLD CONTACTS: it falls
            // through the road instead of bouncing on it. DeformableObject::OutputWheelData
            // @0x82608E28 already keyed the same manager correctly (`mHandlingBodyID >> 32`,
            // BrnDeformableObject_GlassState.cpp:3442) -- two callers of one lookup, one of them
            // wrong, which is exactly how a silent key error survives.
            EntityId lWheelLookupEntityId;
            lWheelLookupEntityId.muValue = static_cast<u32>(static_cast<u64>(mHandlingBodyID) >> 32);
            const PhysicalWheel* lpWheel = lpWheelMgr->GetWheel(lWheelLookupEntityId, liWheel);
            if (lpWheel == nullptr)
            {
                continue;
            }

            // `lbz 128(wheel)` == mbFrozen, the same settled-body skip the body-part loop does at
            // +0x1E6. (Both are byte reads the pseudocode renders as an anonymous offset.)
            if (lpWheel->IsFrozen())
            {
                continue;
            }

            // The wheel's cache slot base is 123, not the parts' 73 (GetTriangleCacheSlot()).
            const s32 liCacheSlot = static_cast<s32>(lpWheel->GetTriangleCacheSlot());
            const s32 liNumCachedBatches = lpTriCache->GetNumCachedTriangleBatches(liCacheSlot);
            if (liNumCachedBatches <= 0)
            {
                continue;
            }

            TriangleList lTriangleList;
            lTriangleList.mpTriangles =
                const_cast<CgsGeometric::Triangle4*>(lpTriCache->GetCache(liCacheSlot));
            lTriangleList.miNumTriangles = liNumCachedBatches;
            lTriangleList.CheckAlignment();
            lTriangleList.ValidateTriangles();

            lWheelBuilder.Prepare(lpAlloc, 1u);

            // The console INLINES PhysicalWheel::GetCylinder here (four lvx128/stvx128 row moves with
            // one negated row, plus the two scalars) -- de-inlined back to the named helper per the
            // project's inlining-reversal rule. See its body in BrnPhysicalWheel.cpp.
            CgsGeometric::Cylinder lWheelCylinder;
            lpWheel->GetCylinder(lWheelCylinder);

            CGS_ASSERT(lpWheel->GetPoolIndex() < KU8_MAX_WHEEL_POOL_INDEX,
                       "Bad wheel pool index: ");                                              // :3767

            lWheelBuilder.AddPrimitive(&lWheelCylinder, KF_MAX_PART_CONTACT_PADDING,
                                       lpWheel->GetPoolIndex());

            if (KU_USE_SPY_WORLD_CONTACT_PATH)
            {
                lpGen->AddPrimitiveListWithTriangleListToStream(&lWheelBuilder, &lTriangleList,
                                                                KU16_PART_MAX_RESULTS,
                                                                true,
                                                                KU_PART_QUEUE_TAG_A, KU16_PART_TAG_B,
                                                                lpProducer);
            }
            else
            {
                lpGen->CollidePrimitiveListAgainstTriangleList(&lWheelBuilder, &lTriangleList,
                                                               KU16_PART_MAX_RESULTS,
                                                               KU_PART_QUEUE_TAG_A, KU16_PART_TAG_B,
                                                               true);
            }

            // Keyed (WHEEL, WORLD) -- `ld 112(wheel)`, the whole 8-byte mWheelBodyId.
            lpGenList->AddEntry(lpWheel->GetVolumeInstanceId(), lWorldVolumeInstanceId, 0u, 0u);
        }
    }
}
}
