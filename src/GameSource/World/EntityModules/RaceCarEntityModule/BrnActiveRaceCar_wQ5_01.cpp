// ============================================================================
// BrnWorld::ActiveRaceCar -- THE SCENE ADD CHAIN (wave Q5, 2026-08-18).
//
// The exact inverse of the Detach chain that already lives in BrnActiveRaceCar.cpp, and
// the missing half of car-vs-prop collision: without these five bodies a race car has no
// scene entity, no collision volume, no volume instance and no collision registration, so
// no broad-phase pair between a car and a smash gate can exist even once the scene
// manager's own volume legs land.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX, RAW ASSEMBLY (the Hex-Rays pseudocode for
// AddToScene is not usable -- it renders the whole VMX half as inline asm, drops the
// AddEntity centre vector, and mis-pairs the AddDynamicVolume id):
//   ActiveRaceCar::OnHandlingModelAdded      @ 0x822F7EF8   (41 insns)
//   ActiveRaceCar::AddToScene                @ 0x822EB768  (332 insns)
//   ActiveRaceCar::AddToCollision            @ 0x822D41F0  (138 insns)
//   ActiveRaceCar::SendSceneUpdatesPostPhysics @ 0x822EB6B0 (46 insns)
//   ActiveRaceCar::UpdateCullingGroup        @ 0x822BF5B8   (44 insns)
//   ActiveRaceCar::DetermineCullingGroup     @ 0x822BF4D8   (56 insns)
//
// A PARTFILE and not an addition to BrnActiveRaceCar.cpp: that file is 1,557 lines and is
// live in other sessions' checkouts this wave; the same ledger TU
// (class:BrnWorld::ActiveRaceCar) owns both. MOUNT LINE REQUIRED -- see the owner record
// scratchpad/waveQ5/caradd.owner.md §8.
//
// ---------------------------------------------------------------------------
// TWO LEGS OF AddToScene ARE FLAGGED, NOT PARAPHRASED. Both are blocked on files this
// owner does not own; both are named exactly, in place, where they belong:
//
//   [FLAG BLOCKED 1] the AddDynamicVolume POST needs the DWARF's 64-bit form
//       CgsSceneManagerIO_SceneUpdate.h:455  void AddDynamicVolume(VolumeId,
//                                                 const VolRef::Volume*, VolumeTypeFlags)
//     The tree only declares the fitted 32-bit `AddDynamicVolume(EntityId, const void*, u8)`
//     (CgsSceneManagerIO_SceneUpdate.h:125). A race car's handle CANNOT go through it: the
//     console passes the WHOLE 8-byte mHandlingBodyVolumeId (`ld r11,0xD0(r30) ; std ;
//     ld r4,0(r10)` @0x822EBBFC..0x822EBC04) and for a race car the entity word lives in the
//     HIGH dword while the LOW dword is IDENTICALLY ZERO (Attach seeds `muId = 0` then only
//     SetEntityIDOwner/SetEntityIDEntityIndex, both of which write the high dword --
//     BrnActiveRaceCar.cpp:290-292). Narrowing to u32 would post volume key 0 for all eight
//     cars. This is the same width trap wave Q4 recorded for AddVolumeInstance /
//     AddForCollision, which is why those two DO have 64-bit forms and this one does not yet.
//     (The compiler cannot silently do it either: VolumeId->u64->u32->EntityId needs two
//     user-defined conversions, so the call would not compile. The park is loud by
//     construction.)
//
//   [FLAG BLOCKED 2] the volume's own TRANSFORM row. The console re-reads the freshly built
//     box at lpVolume+0x30 -- rw::collision::Volume's Matrix44Affine translation row -- and
//     pulls its Y down by the same half-delta it just added to the Y half-extent
//     (0x822EBBD4..0x822EBBF8: `lvx v0,[vol+0x30] ; vsubfp v0,v0,v124 ; vrlimi v13,v0,4,0 ;
//     stvx v13,[vol+0x30]`), so the box grows DOWNWARD only and its top face stays on the
//     authored handling-body top. The tree's rw::collision::BoxVolume
//     (vendor/renderware/collision/CollisionVolume.hpp:56) is an explicitly-flagged 128-byte
//     PLACEHOLDER with `{u32 muVolumeType; f32 mafParams[3]; u8 maPad[112];}` and no
//     transform at all, and rw::collision::BoxVolume::GetBBox @0x82BA9FC8 has NO body
//     anywhere in the tree. Reported to the rwcollision owner rather than forked here
//     (AGENTS.md: never fork a type that has a home).
//
// Everything else in all six bodies is real and lands: the entity, its centre and its
// bounding radius, the box half-extents (including the deformation-spec read the old
// OnResourcesLoaded banner said was unreachable -- it is reachable through the handle), the
// two "Invalid handling body" tripwires, the volume instance, the collision registration,
// the culling group and its per-frame republish.
// ============================================================================

#include "GameSource/World/EntityModules/RaceCarEntityModule/BrnActiveRaceCar.h"
#include "GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCar.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"                              // CGS_ASSERT
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_SceneUpdate.h"  // InSceneUpdateInterface
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                      // gpDebugPrint / gxMessageFilterFlags
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnStreamedDeformationSpec.h" // StreamedDeformationSpec::mHandlingBodyDimensions
#include "vendor/renderware/collision/CollisionVolume.hpp"                      // rw::collision::BoxVolume
#include "rw/physics/rigidbody.h"                                              // rw::physics::ACTIVE_BODY

#include <cmath>   // sqrtf (the console's vrsqrtefp + two Newton refinements)
#include <new>     // placement new (the volume image, exactly as BrnTriggerEntityModule does)

namespace BrnWorld
{
namespace
{
    // ------------------------------------------------------------------------
    // KF_HANDLING_BODY_Y_PAD -- MEASURED, value 0.05f.
    //
    // The console reads it as the .data VecFloat `unk_82FAD550`, which is ZERO in the
    // shipped image and is splat-filled at run time by an unnamed C++ dynamic initialiser
    // at 0x82C4C098..0x82C4C0BC (`lfs flt_8201497C ; stfs -0x10(r1) ; lvlx ; vspltw v0,v0,0
    // ; stvx128 -> unk_82FAD550`) from the rodata literal flt_8201497C == 0.05f. A
    // function-export-only scan finds no writer, which is exactly the "zero-initialised
    // unrecoverable global" trap; the value comes from a headless-IDA dump of the database
    // (scratchpad/waveQ5/caradd/q5b_out.txt, q5c_out.txt).
    //
    // Modelled as a scalar because all four splat lanes are identical and only the Y lane
    // is ever consumed (the two `vrlimi128 ...,4,0` inserts).
    //
    // Its sibling `unk_82FAD5E0` -- the Vector3 AddForCollision hands the scene as the
    // collision PADDING -- is initialised the same way at 0x82C4B030..0x82C4B064 from
    // flt_82001CC0 == 0.0f in x/y/z with a literal 0 in w, i.e. it is the ZERO vector.
    // Spelled inline at the call site as Vector3{0,0,0,0} with that attestation.
    const f32 KF_HANDLING_BODY_Y_PAD = 0.05f;

    // The "is this handling body sane" ceiling both AddToScene tripwires compare against
    // (`lfs f31, flt_82009E10` == 1000.0f, splatted and fed to `vcmpgtfp. v0, v11, v0`).
    const f32 KF_MAX_VALID_HANDLING_BODY_EXTENT = 1000.0f;

    // The entity-type flag word AddToScene stamps on the car's scene entity (`li r5, 0x484`
    // @0x822EB904 == 1156). The scene's entity-type FLAG space is a different enum from
    // BrnWorld::EEntityTypeID -- BrnTriggerEntityModule.cpp:47 already records that for
    // triggers (flag 32, owner 4) -- and that enum has no home in this tree, so the console
    // immediate is named here rather than composed out of bits that cannot be checked.
    const u32 KU_RACECAR_SCENE_ENTITY_TYPE_FLAG = 0x484u;

    // The volume-type flag AddDynamicVolume tags the car's box with (`li r6, 1`
    // @0x822EBBDC). Same flag space as the prop module's KU8_PROP_VOLUME_TYPE_FLAG == 16
    // and the trigger module's 1/2/4 -- unhomed, so the immediate is named, not decomposed.
    const u8 KU8_RACECAR_VOLUME_TYPE_FLAG = 1u;

    // DetermineCullingGroup's three results (`li r3, 6` @0x822BF55C; the AI/other pair is
    // the branchless `subfic/subfe/rlwinm 0,29,29/addi 1` tail at 0x822BF5A0..0x822BF5AC,
    // which is 5 when IsAIDriven() and 1 otherwise).
    const u32 KU_CULLING_GROUP_PLAYER_CAR = 6u;
    const u32 KU_CULLING_GROUP_AI_CAR     = 5u;
    const u32 KU_CULLING_GROUP_OTHER_CAR  = 1u;

    // Resolve the resident deformation spec from a ResourceHandle.
    //
    // WHY THIS EXISTS: the console reads the handling-body dimensions through
    // `BrnPhysics::Def...(this + 0x1C90)` @0x822C7708 -- which is
    // CgsResource::ResourcePtr<StreamedDeformationSpec>::operator-> (its baked assert is
    // "Can not instance resource pointer - it has no main memory resource" at
    // CgsResourcePtr.h:544, matching the tree's own accessor exactly) applied to
    // ActiveRaceCar::mDeformationModelResourcePtr. That member is still the opaque
    // `maPad1C90a[20]` span in BrnActiveRaceCar.h because homing the ResourcePtr wrapper
    // would give ActiveRaceCar a non-trivial ctor/dtor its 8-slot array has never had.
    //
    // What IS named is the ResourceHandle the wrapper stores at its own +0x14 --
    // mDeformationModelHandle -- and it addresses the SAME resource: OnRaceCarResourcesLoaded
    // fills it from RaceCarStreamer::GetPhysicsResourceHandle, i.e. from the very
    // ResourcePtr the console dereferences. The double deref below is the tree's established
    // handle->resource idiom, used verbatim by BrnDeformationManager.cpp:128
    // (ResolveDeformationSpec) and by CgsResource::SafeResourceHandle::operator->
    // (CgsResourceHandle.h:401); CgsBaseResourcePtr.cpp:60-90 records the runtime witness
    // that the +0x14 pair resolves through it and the +0x04 pair does not.
    //
    // Returns 0 when the car's physics resource is not resident, which is the case the
    // console's operator-> asserts on.
    const BrnPhysics::Deformation::StreamedDeformationSpec*
    ResolveDeformationSpec(const CgsResource::ResourceHandle& lrHandle)
    {
        if (lrHandle.mpResourceMemory == 0)
        {
            return 0;
        }
        return *reinterpret_cast<const BrnPhysics::Deformation::StreamedDeformationSpec* const*>(
                   lrHandle.mpResourceMemory);
    }

    // dot3 + sqrt, the console's `vmsum3fp128` followed by `vrsqrtefp` and two
    // Newton-Raphson refinements (0x822EB864..0x822EB8A4). The refined reciprocal square
    // root times the input is the square root; `vsel ... vcmpeqfp128 v9, v127, v0` returns
    // 0 for a zero input, which sqrtf(0) also does.
    f32 Length3(const Vector3& lrV)
    {
        return sqrtf(lrV.x * lrV.x + lrV.y * lrV.y + lrV.z * lrV.z);
    }
}

// ============================================================================
// OnHandlingModelAdded @ 0x822F7EF8   (41 insns)
//
// The console's only caller of AddToScene. Two tripwires, then the tail call; r4/r5/f1 are
// forwarded unchanged (0x822F7F7C..0x822F7F8C).
//
//   0x822F7F18  IsAttached()   assert BrnActiveRaceCar.cpp:0x39D == :925
//   0x822F7F50  IsActive()     assert BrnActiveRaceCar.cpp:0x39E == :926
//   0x822F7F8C  bl AddToScene(this, lpSceneInterface, lpVehicleInterface, lfTimeStep)
//
// ⚠️ ITS OWN CALLER IS NOT LANDED. RaceCarEntityModule::ProcessCreateVehicleEvents
// @0x822FF620 (182 insns) is still replaced by the stand-in
// PublishNewVehicleToDirectorWithoutPhysicsBringUp (BrnRaceCarEntityModule.cpp) -- the
// sibling owner's file. Until that lands, nothing in the build calls this, and therefore
// nothing calls AddToScene. Reported, not worked around.
// ============================================================================
void ActiveRaceCar::OnHandlingModelAdded(
        CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpSceneInterface,
        BrnPhysics::Vehicle::VehicleInputInterface* lpVehicleInterface,
        f32 lfTimeStep )
{
    CGS_ASSERT(IsAttached(), "IsAttached()");   // X360 :925
    CGS_ASSERT(IsActive(),   "IsActive()");     // X360 :926

    AddToScene(lpSceneInterface, lpVehicleInterface, lfTimeStep);
}

// ============================================================================
// AddToScene @ 0x822EB768   (332 insns)  -- THE CAR'S SCENE REGISTRATION.
//
// Asm order, leg by leg (every offset below is read off the listing in
// scratchpad/waveQ5/caradd/asm_0x822EB768.txt):
//
//   0x822EB78C  IsActive()                 assert :0x4D8 == :1240
//   0x822EB7C0  lpSceneInterface != NULL   assert :0x4D9 == :1241
//   0x822EB7E4  r3 = this + 0x1C90 -> BrnPhysics::Def... (ResourcePtr<StreamedDeformationSpec>
//               ::operator->)  ->  lpSpec
//   0x822EB80C  lvx128 v0, [this + 0xC0]        == mCentreOfMassTransform.wAxis (row 3)
//   0x822EB814  vspltw v10, v0, 1              == splat(COM.y)
//   0x822EB81C  lvx128 v0, [lpSpec + 0x40]     == StreamedDeformationSpec::mHandlingBodyDimensions
//                                                 (the header's own static_assert pins +64)
//   0x822EB834  vandc v12, v10, <sign mask>    == fabs(COM.y)
//   0x822EB83C  vsubfp / vmaxfp / vaddfp       == max(fabs(COM.y) - dims.y, 0) + 0.05f
//   0x822EB850  vmulfp128 v124, v13, 0.5       == the HALF-DELTA, kept in a callee-saved
//                                                 vector register for the whole body
//   0x822EB858  v0 = dims + halfDelta ; vrlimi v9, v0, 4, 0  -- ONLY the Y lane is taken
//   0x822EB860  stvx -> var_240                == the box half-extents
//   0x822EB864  vmsum3fp128 + rsqrt refine -> var_280 == Length3(halfExtents)
//   0x822EB8AC  IsAttached()                   assert BrnActiveRaceCar.h:0x441 == :1089
//   0x822EB8E4  ld [this+0xD0] ; srdi 32       == the EMBEDDED 32-BIT ENTITY WORD
//   0x822EB8F8  RaceCar::GetPosition(mpRaceCar)
//   0x822EB914  AddEntity(entityWord, 0x484, position, Length3(halfExtents))
//   0x822EB948  BoxVolume::Initialize(<128-byte image>, halfExtents)  -> lpVolume
//   0x822EB95C  BoxVolume::GetBBox(lpVolume, NULL, 1, &var_230)
//   0x822EB964  IsAttached()                   assert h:1089
//   0x822EB994  RaceCar::GetModelId -> CgsIDUnCompress(id, &var_270)  -- the assert MESSAGE only
//   0x822EBA1C  1000.0f > Length3(bbox.Min())  assert :0x4FA == :1274  "\nInvalid handling
//                                                                      body on race car: <id>"
//   0x822EBB30  1000.0f > Length3(bbox.Max())  assert :0x4FB == :1275  (same message)
//   0x822EBBB0  lpVolume != NULL               assert :0x4FF == :1279  "NULL != lpVolume"
//   0x822EBBD4  [lpVolume+0x30].y -= halfDelta.y                       [FLAG BLOCKED 2]
//   0x822EBC08  AddDynamicVolume(mHandlingBodyVolumeId, lpVolume, 1)   [FLAG BLOCKED 1]
//   0x822EBC18  IsAttached()                   assert h:1089
//   0x822EBC54  RaceCar::GetTransform(mpRaceCar)
//   0x822EBC68  AddVolumeInstance(mHandlingBodyVolumeId, mHandlingBodyVolumeId, transform)
//   0x822EBC78  AddToCollision(lpSceneInterface, lpVehicleInterface)
//   0x822EBC80  stb 1, 0x78A                   mbAddedToScene = true
//
// FOUR THINGS A READER WILL GET WRONG, all measured:
//
//  1. THE ID PASSED TO AddVolumeInstance IS THE SAME 64-BIT WORD TWICE. `ld r5,0(r31)` and
//     `ld r4,0(r29)` (0x822EBC5C / 0x822EBC64) read two different stack slots that were both
//     just filled from the SAME `ld r11,0xD0(r30)` (0x822EBC4C / 0x822EBC50). The car's
//     VolumeInstanceId and its VolumeId are one and the same handle -- unlike a prop, which
//     carries a separate PropVolumeID.
//
//  2. THE BOX GROWS IN Y ONLY. Both `vrlimi128 ...,4,0` inserts take lane 1 (Y) and leave
//     x/z/w alone, and the pre-delta terms are splats of ONE lane each. Applying the delta to
//     all three axes -- the obvious misreading of a splat -- inflates the car's collision box
//     by up to a metre in X and Z.
//
//  3. THE ENTITY GETS THE 32-BIT ENTITY WORD, THE VOLUME/INSTANCE GET THE 64-BIT HANDLE.
//     `srdi r11,r11,32 ; clrlwi r31,r11,0` before AddEntity, a bare `ld` before the other
//     two. Exactly the asymmetry RemoveFromScene already documents in the other direction.
//
//  4. THE FLOAT ARGUMENT IS NEVER READ. f1 arrives and is never touched; the f31 spill in
//     the prologue is the callee-save of the scratch register the 1000.0f literal later
//     occupies (`lfs f31, flt_82009E10` @0x822EB9C0). It is not the parameter.
//
// PC-SAFETY GUARD (not console behaviour, stated so): the console asserts on a NULL
// lpSceneInterface and then dereferences it anyway. The early return below mirrors the one
// RemoveFromScene already carries in BrnActiveRaceCar.cpp -- same file, same convention.
// ============================================================================
void ActiveRaceCar::AddToScene(
        CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpSceneInterface,
        BrnPhysics::Vehicle::VehicleInputInterface* lpVehicleInterface,
        f32 lfTimeStep )
{
    CGS_ASSERT(IsActive(), "IsActive()");                              // X360 :1240
    CGS_ASSERT(lpSceneInterface != 0, "lpSceneInterface != NULL");     // X360 :1241
    if (lpSceneInterface == 0)
    {
        return;
    }

    (void)lfTimeStep;   // arrives in f1; no instruction in the console body reads it

    // ---- 1. the handling-body box half-extents -------------------------------------
    const BrnPhysics::Deformation::StreamedDeformationSpec* lpSpec =
        ResolveDeformationSpec(mDeformationModelHandle);
    CGS_ASSERT(lpSpec != 0,
               "Can not instance resource pointer - it has no main memory resource\n");
    if (lpSpec == 0)
    {
        return;
    }

    const Vector3& lrHandlingBodyDimensions = lpSpec->mHandlingBodyDimensions;   // spec + 0x40

    // fabs(COM.y) is how far the handling frame's origin sits from the model origin; the
    // box is stretched by half of whatever that exceeds the authored half-height, plus the
    // 0.05 m pad, and (leg 2) its centre is dropped by the same amount so the growth is
    // downward. Note both operands are SPLATS of one lane each in the asm -- the vector
    // arithmetic is scalar arithmetic on Y.
    const f32 lfComY      = mCentreOfMassTransform.wAxis.y;                       // this + 0xC0
    const f32 lfAbsComY   = fabsf(lfComY);                                        // vandc <sign mask>
    const f32 lfOverhang  = lfAbsComY - lrHandlingBodyDimensions.y;               // vsubfp
    // `(x > 0) ? x : 0` is vmaxfp(x, 0) INCLUDING NaN polarity (gotcha 4): the VMX compare is
    // unordered-false, so a NaN operand yields the second source, which is the zero here too.
    const f32 lfHalfDelta =
        0.5f * (((lfOverhang > 0.0f) ? lfOverhang : 0.0f) + KF_HANDLING_BODY_Y_PAD); // vmaxfp/vaddfp/vmulfp

    Vector3 lHalfExtents = lrHandlingBodyDimensions;
    lHalfExtents.y = lrHandlingBodyDimensions.y + lfHalfDelta;   // vrlimi128 v9, v0, 4, 0

    const f32 lfBoundingRadius = Length3(lHalfExtents);          // vmsum3fp128 + rsqrt refine

    // ---- 2. the scene entity --------------------------------------------------------
    CGS_ASSERT(IsAttached(), "IsAttached()");                    // BrnActiveRaceCar.h:1089

    // The console hands AddEntity only the embedded 32-bit entity word (`srdi r11,r11,32`),
    // never the whole handle -- the mirror image of RemoveFromScene's RemoveEntity post.
    const CgsSceneManager::EntityId lEntityId(
        static_cast<u32>(mHandlingBodyVolumeId.muId >> 32));

    lpSceneInterface->AddEntity(lEntityId,
                                KU_RACECAR_SCENE_ENTITY_TYPE_FLAG,
                                GetGlobalRaceCar()->GetPosition(),
                                lfBoundingRadius);

    // ---- 3. the collision volume ----------------------------------------------------
    // The staged 128-byte rw::collision volume image AddDynamicVolume block-copies. Built
    // exactly as the tree's other producer does (BrnTriggerEntityModule.cpp:255-260).
    u8 laVolumeStorage[128];
    rw::collision::BoxVolume* lpVolume =
        new (laVolumeStorage) rw::collision::BoxVolume();
    const bool lbVolumeOk =
        lpVolume->Initialize(lHalfExtents.x, lHalfExtents.y, lHalfExtents.z);

    CGS_ASSERT(IsAttached(), "IsAttached()");                    // BrnActiveRaceCar.h:1089

    // The console composes the assert MESSAGE here -- CgsIDUnCompress(mpRaceCar->GetModelId(),
    // buf) feeding "\nInvalid handling body on race car: " << buf into the assert buffer
    // (0x822EB994..0x822EBAA4 and again at 0x822EBB74..0x822EBB98). The condition is what
    // matters; per this file's established convention (see UpdatePhysicsState's banner in
    // BrnActiveRaceCar.cpp) the runtime string composition is diagnostic-only and is not
    // rebuilt by hand.
    //
    // THE TWO CONDITIONS ARE OVER THE BOX'S OWN AABB, and it is derivable exactly here
    // without rw::collision::BoxVolume::GetBBox (which has no body anywhere in this tree):
    // GetBBox @0x82BA9FC8 with a NULL transform argument reads the volume's four transform
    // rows, folds |row| * halfExtents + fatness, and returns {t - v, t + v}. The box was
    // built one instruction earlier by BoxVolume::BoxVolume @0x82BAA0F0, which writes the
    // IDENTITY rows (gIVector / unk_82181510 / unk_82181520), a ZERO translation row and a
    // ZERO fatness (`lfs f0, flt_82001CC0 == 0.0f ; stfs f0, 0x50(r3)`). So v == halfExtents,
    // t == 0, and the AABB is exactly {-halfExtents, +halfExtents}: both tripwires reduce to
    // the same magnitude test, which is why the console emits the identical VMX block twice.
    // (The leg-2 transform tweak happens AFTER GetBBox, so it does not enter either test.)
    CGS_ASSERT(KF_MAX_VALID_HANDLING_BODY_EXTENT > lfBoundingRadius,
               "Invalid handling body on race car");             // X360 :1274 (bbox.Min())
    CGS_ASSERT(KF_MAX_VALID_HANDLING_BODY_EXTENT > lfBoundingRadius,
               "Invalid handling body on race car");             // X360 :1275 (bbox.Max())

    CGS_ASSERT(lbVolumeOk, "NULL != lpVolume");                  // X360 :1279

    // ---- [FLAG BLOCKED 2] the volume's transform row --------------------------------
    // 0x822EBBD4..0x822EBBF8: lpVolume->mTransform.wAxis.y -= lfHalfDelta.
    // rw::collision::BoxVolume in this tree (vendor/renderware/collision/CollisionVolume.hpp)
    // is a flagged 128-byte placeholder with no transform member. Landing this needs the real
    // rw::collision::Volume (the 96-byte one in SDKs/EATech/rwcollision/volume_debug_access.h)
    // to become BoxVolume's base and BoxVolume::Initialize/GetBBox to gain bodies -- the
    // rwcollision owner's job, reported in scratchpad/waveQ5/caradd.owner.md §7.
    // WITHOUT IT the car's collision box is centred on the handling frame instead of hanging
    // below it: the box is the right SIZE and up to lfHalfDelta too high. DELETE-WHEN the
    // real BoxVolume lands.

    // ---- [FLAG BLOCKED 1] the AddDynamicVolume post ---------------------------------
    // The console call is exactly
    //     lpSceneInterface->AddDynamicVolume(
    //         CgsSceneManager::VolumeId(mHandlingBodyVolumeId.muId),   // ld r4 -- ALL 64 BITS
    //         laVolumeStorage,                                        // r5
    //         KU8_RACECAR_VOLUME_TYPE_FLAG);                          // r6 == 1
    // and it cannot be spelled until CgsSceneManagerIO_SceneUpdate.h grows the DWARF's own
    // 64-bit overload (:455). See this file's banner for why the committed 32-bit EntityId
    // form is not a substitute. One additive declaration + a six-line body in a file this
    // owner does not own; reported in scratchpad/waveQ5/caradd.owner.md §7.
    (void)lpVolume;
    (void)laVolumeStorage;

    if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0 && CgsDev::Log::gpDebugPrint != 0)
    {
        *CgsDev::Log::gpDebugPrint
            << "[Q5-CARADD] AddToScene: volume NOT posted (AddDynamicVolume(VolumeId,...) "
               "missing) for car handle " << mHandlingBodyVolumeId.muId
            << " halfExtents.y " << lHalfExtents.y << "\n";
    }

    // ---- 4. the volume instance -----------------------------------------------------
    CGS_ASSERT(IsAttached(), "IsAttached()");                    // BrnActiveRaceCar.h:1089

    // Both ids are the SAME 64-bit handle (see note 1 in the banner).
    lpSceneInterface->AddVolumeInstance(mHandlingBodyVolumeId,
                                        CgsSceneManager::VolumeId(mHandlingBodyVolumeId.muId),
                                        GetGlobalRaceCar()->GetTransform());

    // ---- 5. the collision registration ----------------------------------------------
    AddToCollision(lpSceneInterface, lpVehicleInterface);

    mbAddedToScene = true;                                       // 0x78A
}

// ============================================================================
// AddToCollision @ 0x822D41F0   (138 insns)
//
//   0x822D4208  IsActive()                assert :0x56B == :1387
//   0x822D423C  lpSceneInterface != NULL  assert :0x56C == :1388
//   0x822D4260  lpVehicleInput  != NULL   assert :0x56D == :1389   (the console's own
//                                                                   parameter spelling)
//   0x822D4284  !mbAddedForCollision      assert :0x56E == :1390
//   0x822D42AC  lwz 0x744 ; cmpwi 0 ; beq epilogue
//                    -> the WHOLE body is gated on meOnlineState != E_ONLINE_STATE_CONNECTING
//   0x822D42C0  UpdateCullingGroup(lpSceneInterface)
//   0x822D42C4  the gxMessageFilterFlags & 1 debug line ("Adding for collision: " id "\n")
//   0x822D4334  AddForCollision(mHandlingBodyVolumeId, mCurrentCullingGroup,
//                               ACTIVE_BODY, <zero padding>, E_DO_NOT_ADD_TO_CACHE_MANAGER)
//   0x822D433C  IsAttached()              assert BrnActiveRaceCar.h:0x441 == :1089
//   0x822D437C  mpRaceCar->GetType() == E_RACE_CAR_TYPE_NETWORK -> the re-add post
//   0x822D4404  stb 1, 0x78D / 0x78E / 0x78B
//
// THE ARGUMENT CONSTANTS, and why they are NOT the prop module's:
//   * BodyState is `li r6, 4` == rw::physics::ACTIVE_BODY. PropCellManager::
//     AddPropToContactGeneration passes 2 (FROZEN_BODY) at the same slot -- a car is a live
//     body, a prop is not. Do not harmonise them.
//   * CacheOptions is `li r7, 2` == E_DO_NOT_ADD_TO_CACHE_MANAGER, the same as the props'.
//   * The padding Vector3 is `lvx128 v1, [unk_82FAD5E0]`, a .data VecFloat that the
//     dynamic initialiser at 0x82C4B030 fills with {0,0,0} + a literal 0 w -- the ZERO
//     vector, NOT the prop path's authored pad.
//   * The culling group is mCurrentCullingGroup (`lwz r5, 0x7D0`), which UpdateCullingGroup
//     has just refreshed one call earlier. It is 6/5/1, never the prop module's 2.
//
// ⚠️ REPORTED, NOT FIXED (it is a scene-manager question, not this cluster's): those 6/5/1
// groups are exactly three of the ten pairs WorldModule::Prepare stage 3 clears against the
// prop groups (`SetCullingGroupPair(7,1,0) / (7,6,0) / (7,5,0)` and the 8-side twins, asm
// 0x827D565C..0x827D5740), while PropEntityModule::InitializePropPhysicsData only ever sets
// (2,7,1) / (2,8,1). Whether that means car-vs-prop overlaps are CULLED depends on the bit
// polarity of the culling table, and the consumer that reads it (SceneSweeper::
// BuildCollidingPairs @0x828C2190) is not reconstructed, so the polarity is undetermined in
// this tree. See scratchpad/waveQ5/caradd.owner.md §6 -- it is a wave-level question.
// ============================================================================
void ActiveRaceCar::AddToCollision(
        CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpSceneInterface,
        BrnPhysics::Vehicle::VehicleInputInterface* lpVehicleInterface )
{
    CGS_ASSERT(IsActive(), "IsActive()");                                // X360 :1387
    CGS_ASSERT(lpSceneInterface != 0, "lpSceneInterface != NULL");       // X360 :1388
    CGS_ASSERT(lpVehicleInterface != 0, "lpVehicleInput != NULL");       // X360 :1389
    CGS_ASSERT(!mbAddedForCollision, "!mbAddedForCollision");            // X360 :1390

    (void)lpVehicleInterface;   // asserted, then never read (r30 is reused at 0x822D42E8)

    if (meOnlineState == E_ONLINE_STATE_CONNECTING)
    {
        return;
    }
    if (lpSceneInterface == 0)   // PC-safety guard, as in AddToScene above
    {
        return;
    }

    UpdateCullingGroup(lpSceneInterface);

    if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0 && CgsDev::Log::gpDebugPrint != 0)
    {
        *CgsDev::Log::gpDebugPrint << "Adding for collision: "
                                   << mHandlingBodyVolumeId.muId << "\n";
    }

    lpSceneInterface->AddForCollision(
        mHandlingBodyVolumeId,
        static_cast<CgsSceneManager::SceneManagerIO::InEventAddForCollision::CullingGroup>(
            mCurrentCullingGroup),                                       // lwz 0x7D0
        rw::physics::ACTIVE_BODY,                                        // li r6, 4
        Vector3{ 0.0f, 0.0f, 0.0f, 0.0f },                               // lvx [unk_82FAD5E0]
        CgsSceneManager::SceneManagerIO::E_DO_NOT_ADD_TO_CACHE_MANAGER); // li r7, 2

    CGS_ASSERT(IsAttached(), "IsAttached()");                            // BrnActiveRaceCar.h:1089

    // [FLAG BLOCKED] the E_RACE_CAR_TYPE_NETWORK re-add post -- the exact mirror of the one
    // RemoveFromCollision @0x822BF668 already parks, and blocked on the same member:
    //   0x822D437C  if (mpRaceCar->GetType() == E_RACE_CAR_TYPE_NETWORK)
    //   0x822D4388      if (miLength >= miMaxLength) assert :0x583 == :1411
    //                       "Trying to add/remove race car more than <max> times in a frame"
    //   0x822D43FC      post { mHandlingBodyVolumeId, mbAdded = TRUE } onto THIS OBJECT'S own
    //                   mAddRemoveNetworkCarForCollisionQueue (r3 == this at the AddEvent).
    // That member is still `u8 maPad0000[144]` in BrnActiveRaceCar.h and ActiveRaceCar's
    // construction path never calls EventQueue::Construct on it, so naming it would put a
    // live mpEvents pointer at offset 0 of an unconstructed queue. The arm is UNREACHABLE on
    // this build (E_RACE_CAR_TYPE_NETWORK needs an online session). Note the polarity: the
    // ADD post sets mbAdded TRUE where RemoveFromCollision's sets it FALSE.
    // DELETE-WHEN mAddRemoveNetworkCarForCollisionQueue is named AND Construct constructs it.

    mbChangeCollisionState     = true;    // 0x78D
    mbCollisionStateToChangeTo = true;    // 0x78E   (RemoveFromCollision writes false here)
    mbAddedForCollision        = true;    // 0x78B
}

// ============================================================================
// SendSceneUpdatesPostPhysics @ 0x822EB6B0   (46 insns)
//
//   0x822EB6CC  lpSceneInterface != NULL  assert :0x443 == :1091
//   0x822EB6F8  IsActive()                assert :0x444 == :1092
//   0x822EB72C  UpdateCullingGroup(lpSceneInterface)
//   0x822EB730  lbz 0x77A ; cmplwi 1 ; bne skip     -- mbUncrashedThisFrame == true
//   0x822EB73C      lbz 0x78B ; bne skip            -- and NOT already added for collision
//   0x822EB754          AddToCollision(lpSceneInterface, lpVehicleInterface)
//   0x822EB758      stb 0, 0x77A                    -- consume the flag either way
//
// ⚠️ THE FLAG CLEAR IS OUTSIDE THE INNER TEST. A car that was un-crashed this frame while
// ALREADY added for collision still clears mbUncrashedThisFrame; only the AddToCollision
// call is skipped. Moving the clear inside the inner `if` would leave the flag latched.
//
// The float argument is again unread (f1 is never touched between prologue and epilogue);
// its caller SendRaceCarSceneUpdates @0x822F6B08 loads it from module+0x18398.
// ============================================================================
void ActiveRaceCar::SendSceneUpdatesPostPhysics(
        CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpSceneInterface,
        BrnPhysics::Vehicle::VehicleInputInterface* lpVehicleInterface,
        f32 lfTimeStep )
{
    CGS_ASSERT(lpSceneInterface != 0, "lpSceneInterface != NULL");   // X360 :1091
    CGS_ASSERT(IsActive(), "IsActive()");                            // X360 :1092
    if (lpSceneInterface == 0)   // PC-safety guard, as in AddToScene above
    {
        return;
    }

    (void)lfTimeStep;   // arrives in f1; no instruction in the console body reads it

    UpdateCullingGroup(lpSceneInterface);

    if (mbUncrashedThisFrame)                                        // 0x77A
    {
        if (!mbAddedForCollision)                                    // 0x78B
        {
            AddToCollision(lpSceneInterface, lpVehicleInterface);
        }
        mbUncrashedThisFrame = false;
    }
}

// ============================================================================
// UpdateCullingGroup @ 0x822BF5B8   (44 insns)
//
//   0x822BF5D4  IsActive()                     assert :0x59F == :1439
//   0x822BF608  DetermineCullingGroup()
//   0x822BF60C  lwz 0x7D0 ; cmplw ; beq epilogue   -- nothing to do when unchanged
//   0x822BF620  lbz r10, 0x78B                 -- mbAddedForCollision, READ BEFORE the stores
//   0x822BF624  stw 0x7D0                      mCurrentCullingGroup     = new
//   0x822BF62C  stw 0x790                      mCullingGrouptoChangeTo  = new
//   0x822BF630  stb 1, 0x78F                   mbChangeCullingGroup     = true
//   0x822BF634  beq epilogue                   -- only publish if already added
//   0x822BF64C  SetVolumeInstanceCullingGroup(mHandlingBodyVolumeId, new)
//
// The compare is `cmplw` (UNSIGNED). It matters: RemoveFromCollision parks
// mCurrentCullingGroup at 0xFFFF, so the first UpdateCullingGroup after a re-add compares
// 0xFFFF against 1/5/6 and always takes the change path -- which is the point of the park.
// ============================================================================
void ActiveRaceCar::UpdateCullingGroup(
        CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpSceneInterface )
{
    CGS_ASSERT(IsActive(), "IsActive()");                            // X360 :1439

    const BrnPhysics::Vehicle::SetRaceCarCullingGroupEvent::CullingGroup lCullingGroup =
        DetermineCullingGroup();

    if (lCullingGroup == static_cast<u32>(mCurrentCullingGroup))
    {
        return;
    }

    const bool lbWasAddedForCollision = mbAddedForCollision;          // read at 0x822BF620

    mCurrentCullingGroup    = static_cast<s32>(lCullingGroup);        // 0x7D0
    mCullingGrouptoChangeTo = static_cast<s32>(lCullingGroup);        // 0x790
    mbChangeCullingGroup    = true;                                   // 0x78F

    // (`lpSceneInterface != 0` is the same PC-safety guard the two callers already carry; the
    //  console's test here is the mbAddedForCollision byte alone.)
    if (lbWasAddedForCollision && lpSceneInterface != 0)
    {
        lpSceneInterface->SetVolumeInstanceCullingGroup(mHandlingBodyVolumeId,
                                                        static_cast<s32>(lCullingGroup));
    }
}

// ============================================================================
// DetermineCullingGroup @ 0x822BF4D8   (56 insns)
//
//   0x822BF4E8  IsAttached()                   assert BrnActiveRaceCar.h:0x441 == :1089
//   0x822BF524  lbz 0xA4(mpRaceCar) ; cmplwi 4 -- RaceCar::GetType()'s own inlined tripwire
//               "muType < E_RACE_CAR_TYPE_COUNT" (..\..\..\GameSource\World/EntityMod...:577)
//   0x822BF550  if (type == E_RACE_CAR_TYPE_PLAYER)  return 6
//   0x822BF56C  IsAttached()                   assert h:1089  (the second inlined GetGlobalRaceCar)
//   0x822BF598  RaceCar::IsAIDriven()
//   0x822BF5A0  subfic / subfe / rlwinm 0,29,29 / addi 1   ->   IsAIDriven() ? 5 : 1
//
// The branchless tail is `(-(bool) & 4) + 1`, i.e. 5 when AI-driven and 1 otherwise -- it is
// not a table lookup and there is no fourth value. E_RACE_CAR_TYPE_NETWORK and
// E_RACE_CAR_TYPE_INACTIVE both fall in the "1" bucket.
//
// Bodied here because it has no definition anywhere in the tree and UpdateCullingGroup above
// would otherwise be an unresolved external (gotcha 12: `cl /c` cannot see that).
// ============================================================================
BrnPhysics::Vehicle::SetRaceCarCullingGroupEvent::CullingGroup
ActiveRaceCar::DetermineCullingGroup()
{
    CGS_ASSERT(IsAttached(), "IsAttached()");                        // BrnActiveRaceCar.h:1089

    const RaceCar* lpRaceCar = GetGlobalRaceCar();
    if (lpRaceCar == 0)
    {
        // PC-SAFETY GUARD, not console behaviour: the console asserts IsAttached() (which IS
        // `mpRaceCar != NULL`) and then dereferences mpRaceCar unguarded. Unreachable whenever
        // the assert holds; the "other car" bucket is the arm a detached slot would fall into
        // anyway, and returning it keeps UpdateCullingGroup's compare well-defined.
        return KU_CULLING_GROUP_OTHER_CAR;
    }

    if (lpRaceCar->GetType() == E_RACE_CAR_TYPE_PLAYER)
    {
        return KU_CULLING_GROUP_PLAYER_CAR;
    }

    CGS_ASSERT(IsAttached(), "IsAttached()");                        // BrnActiveRaceCar.h:1089

    return lpRaceCar->IsAIDriven() ? KU_CULLING_GROUP_AI_CAR
                                   : KU_CULLING_GROUP_OTHER_CAR;
}

}
