// GameSource/Physics/PropManager/BrnPropManager.cpp
//
// BrnPhysics::Props::PropManager -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//   Construct()                               @ 0x82627390   (82 asm lines)  -- DONE below
//   ConstructContactGenerationPerfMonitors()  @ 0x825BAC60   (4 asm lines)   -- DONE below
//   ConstructPreScenePerfMonitors()           @ 0x825BAC70   (6 asm lines)   -- DONE below
//   CreateContactEvent()                      @ 0x825A53A0                   -- DONE below
//                                                             (a `class:` TU's function, bodied here)
//   SetupAndValidatePropContact()             @ 0x82628190                   -- REAL BODY, in the
//                                                             sibling partfile PropManager_wQ4_01.cpp
//                                                             (wave Q4; the trap stub that used to
//                                                             stand at the bottom of this file is
//                                                             deleted -- see the block there)
// The other ELEVEN ledger functions of this TU, plus ClampAcceleration @0x82627F00 (ledger-
// attributed to the rw/math/fpu/vector3.h catch-all -- an inlining artefact; it is this
// class's helper), are open. Their per-function triage, callee sets, include set and group
// split are in scratchpad/waveQ/PropManager.spec.md (breakable-props keystone, 2026-08-18).
//
// =========================================================================================
// ⚠️ CORRECTION (physics wave 4) -- THE PREVIOUS BLOCK NOTE IN THIS FILE WAS WRONG ON ITS
//    CENTRAL CLAIM, AND THAT CLAIM WAS THE THING BLOCKING THE LAYOUT.
//
// It asserted:  "PropManager derives from BaseCollisionGenerator ... the whole ~0x48-byte
//               prefix is the BaseCollisionGenerator sub-object ... NO member offset in any
//               body can be mapped to a name -> the class layout itself is ungroundable."
//
// Three independent facts say otherwise:
//
//   (a) The DecFIGS dwarfdump prints base classes. Twelve lines above PropManager in the same
//       generated header it prints
//           struct BrnPhysics::Props::PropRaceCarContactBuffer : public IOBuffer {
//       and for PropManager it prints
//           struct BrnPhysics::Props::PropManager {
//       -- no base. Its FIRST member is `PropDebugComponent mDebugComponent`.
//
//   (b) Construct @0x82627390 does `mr r4,r31 ; mr r3,r31 ; bl PropDebugComponent::Construct`.
//       r3 is the debug component and r4 is the owning PropManager*, and BOTH are `this`.
//       So &mDebugComponent == this, i.e. mDebugComponent occupies +0x00. There is no base
//       sub-object in front of it.
//
//   (c) Destruct @0x825E3398 opens by asserting "mpPropManager != NULL" on *(this+0xC) and
//       then nulling it -- that is PropDebugComponent::Destruct (DWARF BrnPropDebugComponent
//       .cpp:67) INLINED, reading the debug component's own mpPropManager member. A field of
//       mDebugComponent at +0xC only makes sense if mDebugComponent starts at +0.
//
// With mDebugComponent at +0x00 and sizeof == 0x48, the DWARF member sequence lands GAP-FREE
// on every offset the X360 asm touches, which is the proof that the sequence and the offsets
// are the same layout (each line below is an asm store or an asm load, none is inferred):
//
//    +0x0000  PropDebugComponent  mDebugComponent            Construct: r3=this,r4=this
//    +0x0048  bool                mbRenderCOM                = false           (stb 0x48)
//    +0x0049  bool                mbUseOverides              = false           (stb 0x49)
//    +0x004C  f32                 mfMassOverride             = 10.0f           (flt_82004A20)
//    +0x0050  f32                 mfMaxLeanAngleOverride     =  0.0f           (flt_82001CC0)
//    +0x0054  ResourcePtr<PropPhysicsDataHeader> mpPhysicsData   (untouched by Construct)
//    +0x0074  f32                 mfStaticFriction           =  0.3f           (flt_82004740)
//    +0x0078  f32                 mfDynamicFriction          =  0.6f           (flt_82004D00)
//    +0x007C  PropInstance*       mpaPropInstances
//    +0x0080  BitArray<15>        mUsedProps                 = 0               (std 0x80)
//    +0x0088  u32                 muNumberOfPropInstances
//    +0x008C  PropPartInstance*   mpaPartInstances
//    +0x0090  BitArray<30>        mUsedParts                 = 0               (std 0x90)
//    +0x0098  u32                 muNumberOfPartInstances
//    +0x009C  s32                 miNumJobsAdded
//    +0x00A0  SimpleDataStreamProducer* mpPrimitiveWithTriangleStream = 0       (stw 0xA0)
//    +0x00A4  s32                 miContactGeneratorWaitPM             ConstructContactGen...
//    +0x00A8  s32                 miProcessRemovePropPM                ConstructPreScene...
//    +0x00AC  s32                 miProcessRemovePartPM                ConstructPreScene...
//    +0x00B0  s32                 miProcessAddPropInstancePM           ConstructPreScene...
//    +0x00B4  s32                 miProcessAddPartInstancePM           ConstructPreScene...
//    +0x00B8  s32                 miProcessBreakPropPM                 (neither writes it)
//    +0x00C0  Vector3[15]         maPropJointPositions          (16-aligned, 0xF0 bytes)
//    +0x01B0  Vector3[15]         maLastJointRotation                   (0xF0 bytes)
//    +0x02A0  u8[15]              mauPropIndexForJoint
//    +0x02B0  Matrix44Affine[15]  maCurrentJointTransforms              (0x3C0 bytes)
//    +0x0670  BitArray<15>        mUsedPropJoints            = 0               (std 0x670)
//    +0x0678  BitArray<15>        mBreakPropJoints           = 0               (std 0x678)
//    +0x0680  EventQueue<UpdatePropEvent,200> mUpdatedProps   ::Construct(this+0x680)
//    +0x5E10  EventQueue<UpdatePropEvent,15>  mUpdatedJointedProps ::Construct(this+0x5E10)
//    +0x64B0  DebugWorldContactInfo* mpDebugWorldContacts  = DoAllocate(0x600, 0x10)
//    +0x64B4  s32                 miNumDebugWorldContacts    = 0               (stw 0x64B4)
//    +0x64B8  bool                mbDisableFreezing          = false           (stb 0x64B8)
//    +0x64BC  PropEntityID[45]    maPropsAddedToContactGen             (45*4 == 0xB4)
//    +0x6570  s32                 miNumPropsAddedToContactGen = 0              (stw 0x6570)
//
//    Two arithmetic self-checks that make this a proof rather than a story:
//      * mUpdatedJointedProps - mUpdatedProps == 0x5790 == 0x10 + 200*112, and
//        mpDebugWorldContacts - mUpdatedJointedProps == 0x6A0 == 0x10 + 15*112, i.e. both
//        queues have the X360-attested sizeof(UpdatePropEvent) == 112 already committed in
//        SharedIO/BaseEventQueue_UpdatePropEvent_AddEvent.cpp.
//      * the DoAllocate request is 0x600 == 32 * 48 bytes and the DWARF's own
//        KI_MAX_DEBUG_WORLD_CONTACTS is 32, so DebugWorldContactInfo is 48 bytes.
//      * 0x6570 - 0x64BC == 0xB4 == 45 * sizeof(PropEntityID), matching PropEntityID[45].
//
// ⚠️ WHAT IS STILL UNEXPLAINED, stated rather than smoothed over: Destruct's tail is
//    `mr r3,r31 ; bl CgsSceneManager::CgsCollision::BaseCollisionGenerator::Destruct` -- it
//    does pass `this`. That is the one observation the old note built its base-class theory
//    on. But BeginPropWorldContactGeneration @0x82628CB0, which the old note also cited,
//    calls BaseCollisionGenerator::Prepare on `r3 = r30 = its THIRD argument`, NOT on `this`
//    -- so the generator is a collaborator passed in, not a base. A plausible reading of the
//    Destruct tail is an ICF fold (an empty PropManager tail-Destruct folded onto the
//    identically-empty BaseCollisionGenerator::Destruct, with IDA naming the survivor), which
//    is a known hazard in this image. NOT ASSERTED -- flagged for whoever bodies Destruct.
// =========================================================================================
//
// =========================================================================================
// ⭐ THE OLD BLOCK LIST IS RETIRED (breakable-props KEYSTONE wave, 2026-08-18). Every item on
// it was re-derived against the tree as it stands today; here is what each one turned into,
// with the evidence, so nobody re-parks on a stale claim.
//
//   1. "PropDebugComponent has no bodies for its four virtual overrides, which is what keeps
//      Construct out."  ->  STALE. Construct IS bodied above, and BrnPropDebugComponent.cpp
//      is committed beside this file. Nothing in this TU is blocked on it.
//
//   2. "PropInstance / PropPartInstance are pointers here but dereferenced pervasively."
//      ->  CLOSED. Both have real homes (PropPhysics/BrnPropInstance.h and
//      PropPhysics/BrnPropPartInstance.h) and both are ledger-done. What they were still
//      MISSING was the accessor surface, and that landed in this wave: the DWARF's full
//      trivial-accessor set on each (inline, over the already-pinned members) plus the one
//      real out-of-line body the pipeline needs -- PropInstance::SetLinearVelocity
//      @0x825DE6C8, which has NO per-address JSON export and was pulled with headless IDA.
//
//   3. The dependency family, item by item, CHECKED TODAY rather than repeated:
//        BrnPhysics::Vehicle::RaceCarPhysics       -> HOMED. All five reads
//          ApplyAntiHerdingForce makes already have accessors (GetTransform,
//          GetLinearVelocity, GetSimpleAttribs()->mCOMOffset @+0x670, GetHalfExtent @+0x6A0,
//          GetSpeedMPH @+0x6C0). No request needed.
//        CgsPhysics::PhysicsSimulationIO::InApplyForce / InUpdateRigidBody / OutUpdateRigidBody
//          -> HOMED (CgsPhysicsSimulationIO_Events.h) and their queues exist. ⛔ but the two
//          WRITE-side InputBuffer accessors this TU needs do NOT -- see the request list.
//        CgsSceneManager::SceneManagerIO           -> HOMED; InputBuffer_Update::
//          GetInSceneUpdateInterface() exists. ⛔ InSceneUpdateInterface::
//          UpdateCachedObjectPosition (DWARF :506) does not -- request.
//        CgsSceneManager::TriangleCacheManagerIO   -> HOMED (InEventUpdateCachedPosition,
//          sizeof 32, slot @+0 / Vector3Plus @+0x10).
//        CgsMemory::DataStreamCommandPoster        -> HOMED (Begin/End), and
//          SimpleDataStreamProducer with it.
//        Prop/PropTypeData/PropPartTypeData/PropPhysicsDataHeader accessors -> HOMED, every
//          field the two inertia builders read (GetNumberOfVolumes / GetCollisionVolume /
//          GetMass / GetGraphicsId) is already named.
//        rw::physics::RigidBody helpers            -> operator= is homed. ⛔ BUT
//          rw::collision::Volume::GetBBox (the vtable slot the two inertia builders call) is
//          NOT, and the box type itself cannot even be NAMED next to BrnCommonTypes.h --
//          see the request list and the HAZARD note below.
//
//   4. "the header's mbRenderCentreOfMass offset is wrong"  ->  DISPROVEN, and now
//      double-witnessed: PropDebugComponent::OnActivate @0x825E39EC registers the debug
//      variable "Render prop centre of mass" at `mpPropManager + 72` == +0x48, which is
//      exactly where the rebuilt header puts mbRenderCOM (and +25784 == +0x64B8 for
//      "Disable prop freezing" == mbDisableFreezing, the same run). The committed layout
//      stands.
//
// ⚠️ HAZARD FOR WHOEVER BODIES GetPropInertia / GetPartInertia -- a PRE-EXISTING tree defect,
//    not something this TU can fix and not something to work around locally: the tree holds
//    TWO different definitions of rw::math::vpu::Vector3, one under the VENDOR include root
//    (vendor/renderware/include/rw/math/vpu/types.h, what BrnCommonTypes.h pulls) and one
//    under the SRC root (src/SDKs/EATech/include/rw/math/vpu/vector3.h, what
//    vendor/renderware/collision/AABBox.hpp pulls). Including both in one TU is a hard C2011
//    redefinition -- proved in scratchpad/waveQ/probe_PropManager/probe_aabbox_clash.cpp. So
//    the two inertia builders cannot name rw::collision::AABBox at all today.
//
// No types or bodies are fabricated here.

#include "GameSource/Physics/PropManager/BrnPropManager.h"

#include "GameSource/Resource/BrnResourceAllocator.h"   // BrnResource::GetDebugAllocator
#include "rw/rwcore_structs.h"                          // rw::Resource / rw::ResourceDescriptor
#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT (CreateContactEvent tripwires)
#include "GameShared/GameClasses/Physics/CgsPhysicsSimulationIO_Events.h" // CgsPhysics::PhysicsSimulationIO::OutContactSpy
#include "GameShared/GameClasses/SceneManager/SharedIO/CgsPotentialContact.h" // CgsSceneManager::SceneManagerIO::PotentialContact
#include "GameSource/Physics/ContactSpies/BrnContactSpyEvents.h"          // ContactSpy::{BaseContact, PropContact}
#include "SharedClasses/Physics/Props/BrnPropPhysicsDataHeader.h"         // PropPhysicsDataHeader::GetType (graphics-id flag bits)

// includes folded in from the PropManager_w*.cpp partfiles (2026-09-15)
#include "GameSource/Physics/PropManager/PropPhysics/BrnPropInstance.h"      // IsJointed / IsStatic / GetJointIndex
#include "GameSource/Physics/PropManager/PropPhysics/BrnPropPartInstance.h"  // GetType / GetPartId / GetPosition
#include "GameSource/Physics/PropManager/SharedIO/BrnPropInputInterface.h"   // KU_MAX_PHYSICAL_PROP{S,_PARTS} (the two assert bounds)
#include "GameSource/Physics/VehicleManager/BrnVehicleManager.h"             // GetRaceCarPhysics + the traffic remap
#include "GameSource/Physics/VehicleManager/VehiclePhysics/RaceCarPhysics.h" // ExternallySimulatedBody::GetTransform
#include "GameSource/World/BrnEntityTypes.h"                                 // BrnWorld::EEntityTypeID
#include "GameShared/GameClasses/Containers/CgsBitArray.h"                   // mBreakPropJoints.SetBit
#include "GameShared/GameClasses/Physics/CgsRigidBody.h"                     // CgsPhysics::RigidBodyId
#include "GameShared/GameClasses/SceneManager/CgsEntityId.h"                 // EntityId::SetOwner
#include "GameShared/GameClasses/SceneManager/CgsVolumeInstanceId.h"         // VolumeInstanceId
#include "SharedClasses/Physics/Props/BrnPhysicsPropTypeData.h"              // PropTypeData / PropPartTypeData
#include "SharedClasses/Physics/Props/BrnPropEntityID.h"                     // BrnWorld::PropEntityID
#include "rw/math/vpu/vector3_operation.h"                                   // Dot / Subtract / Negate
#include "rw/math/vpu/vector4_operation.h"                                   // Splat (the VecFloat broadcast == vspltw)
#include "rw/physics/rigidbody.h"                                            // rw::physics::WORLD_SPACE
#include "GameShared/GameClasses/Physics/CgsPhysicsSimulationModuleIO.h"      // InputBuffer + the write-side queue getter
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_SceneUpdate.h"// InSceneUpdateInterface + mRemoveFromCacheQueue
#include "GameShared/GameClasses/SceneManager/CacheManager/CgsTriangleCacheManagerIO.h" // InEventRemoveFromCache
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                     // [DIAG] gpDebugPrint only
#include <stdlib.h>                                                            // getenv -- [DIAG] BRN_PROP_DIAG only, host-side
#include "GameSource/Physics/PropManager/SharedIO/BrnPropEvents.h"          // UpdatePropEvent
#include "rw/math/vpu/matrix44affine_operation.h"
#include <cmath>     // std::cos -- the console `bl cos` @0x8260FC10 / @0x8261096C
#include "GameSource/Physics/PropManager/BrnPropDebugComponent.h"          // PropDebugComponent::Destruct
#include "GameShared/GameClasses/Development/DebugSystem/Interface/CgsDebugInterface.h"
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebugRender.h"
#include "GameShared/GameClasses/Development/CgsStrStream.h"                                  // CgsDev::StrStream (the :2478 / :2600 messages)
#include "GameShared/GameClasses/Memory/CgsLinearMalloc.h"                                    // CgsMemory::LinearMalloc (parameter)
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_TriangleCache.h"              // TriangleCacheInterface
#include "GameShared/GameClasses/SceneManager/Collision/ContactGenerator/CgsCollisionGenerator.h" // CollisionGenerator
#include "GameShared/GameClasses/SceneManager/Collision/Primitives/CgsCollisionResult.h"      // CollisionResultList / PrimitiveTestResult
#include "GameShared/GameClasses/SceneManager/Collision/Primitives/CgsPrimitivePairListBuilder.h" // PrimitivePairListBuilder
#include "GameShared/GameClasses/SceneManager/Collision/Primitives/CgsTriangleList.h"         // TriangleList
#include "GameSource/Physics/BrnPhysicsModuleIO_PotentialContactInterface.h"                  // PotentialContactInterface::AddEvent
#include "SDKs/EATech/rwcollision/volume_debug_access.h"                                      // rw::collision::Volume
#include "GameShared/GameClasses/Memory/DataStream/CgsSimpleDataStreamProducer.h" // SimpleDataStreamProducer::Begin/End
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO.h"                       // InputBuffer_Update::GetInSceneUpdateInterface
#include "rw/physics/inertia.h"                                               // rw::physics::Inertia setters
#include "GameSource/Physics/PropManager/SharedIO/BrnPropOutputInterface.h"  // PropOutputInterface (OutputUpdatedProps)
#include "GameSource/Physics/BrnPhysicsModuleIO.h"                           // PhysicsModuleIO::OutputBuffer (OutputUpdatedProps)

namespace BrnPhysics
{
namespace Props
{

// =========================================================================================
// The prop-physics tuning globals (DWARF BrnPropManager.cpp:45..51 -- this file, these lines).
//
// THE FIVE f32s: values read directly out of the shipped ARTIST image's .data, at the exact
// addresses PropDebugComponent::OnActivate @0x825E3628 hands to RegisterVariable. They sit as
// one contiguous run 0x82F2A388..0x82F2A398 in declaration order, which is itself the check
// that the address->name mapping is right:
//     0x82F2A388  3ba3d70a  0.005f   KF_PROP_ANGULAR_DRAG
//     0x82F2A38C  3b83126f  0.004f   KF_PROP_LINEAR_DRAG
//     0x82F2A390  41d80000  27.0f    KF_PROP_MAX_ANGULAR_VEL
//     0x82F2A394  41f00000  30.0f    KF_PROP_MAX_LINEAR_VEL
//     0x82F2A398  3ca3d70a  0.02f    KF_PROP_RESTITUTION
//
// ⭐⭐ THE SEVEN VecFloats: THEIR VALUES ARE RECOVERED AND SEATED, 2026-08-18 round 3b.
//    The note that stood here through three rounds said their values "must come from a dynamic
//    initialiser" and that "the ARTIST export set contains no such initialiser: a scan ...
//    returns only READERS". The first half was exactly right. The second half was FALSE, and it
//    was false for a mechanical reason worth writing down: the initialiser is a THUNK that sits
//    outside every IDA function, so it is invisible to any scan built on the per-address
//    function exports -- which is what every round-1/2/3 scan was built on. The globals really
//    do read 16 zero bytes on disk; that was never the disagreement.
//    Full recipe, and every constant's thunk / dynamic-initialiser-table slot / rodata address /
//    hex word, are on the declarations in BrnPropManager.h. Each definition below repeats its
//    own provenance so a reader never has to trust a value without its evidence.
//    ⚠️ THE ZEROES WERE NOT NEUTRAL, which is why this is a behavioural fix and not a comment
//    tidy-up: ApplyAntiHerdingForce with KVF_SPEED_CLAMP == 0 produced zero force for every
//    prop, and ReadUpdatedBodies with KVF_GRAVITY_SCALE == 0 posted a NEGATIVE extra-gravity
//    force (scale - 1 == -1), i.e. smashed props were being pushed UP.
// =========================================================================================
// Splat(3.0f). Thunk 0x82C5E6E8, tbl slot 0x82CD19A4, rodata flt_82004270 = 0x40400000.
::VecFloat KVF_GRAVITY_SCALE                   = { 3.0f, 3.0f, 3.0f, 3.0f };
// Splat(3.0f). Thunk 0x82C5E6C0, tbl slot 0x82CD19A0, rodata flt_82004270 = 0x40400000.
::VecFloat KVF_INERTIA_SCALE                   = { 3.0f, 3.0f, 3.0f, 3.0f };
// Splat(2.0f). Thunk 0x82C5EA00, tbl slot 0x82CD19F0, rodata flt_82001D9C = 0x40000000.
::VecFloat KVF_ANTI_HERD_UPWARD_SCALE          = { 2.0f, 2.0f, 2.0f, 2.0f };
// Splat(0.05f). Thunk 0x82C5EA28, tbl slot 0x82CD19F4, rodata flt_820047C8 = 0x3D4CCCCD.
::VecFloat KVF_ANTI_HERD_SIDE_SCALE            = { 0.05f, 0.05f, 0.05f, 0.05f };
// Splat(1.5f). Thunk 0x82C5EA50, tbl slot 0x82CD19F8, rodata flt_820945DC = 0x3FC00000.
::VecFloat KVF_ANTI_HERD_HIGH_SPEED_SIDE_SCALE = { 1.5f, 1.5f, 1.5f, 1.5f };
// Splat(60.0f). Thunk 0x82C5EA78, tbl slot 0x82CD19FC, rodata flt_82092BC4 = 0x42700000.
::VecFloat KVF_MAX_SPEED_FOR_SIDE_FORCE        = { 60.0f, 60.0f, 60.0f, 60.0f };
// Splat(120.0f). Thunk 0x82C5EAA0, tbl slot 0x82CD1A00, rodata flt_82092BC8 = 0x42F00000.
::VecFloat KVF_SPEED_CLAMP                     = { 120.0f, 120.0f, 120.0f, 120.0f };

f32 KF_PROP_ANGULAR_DRAG   = 0.005f;
f32 KF_PROP_LINEAR_DRAG    = 0.004f;
f32 KF_PROP_MAX_ANGULAR_VEL = 27.0f;
f32 KF_PROP_MAX_LINEAR_VEL  = 30.0f;
f32 KF_PROP_RESTITUTION     = 0.02f;

// =========================================================================================
// ⭐ ADDED 2026-08-18 (breakable-props keystone wave). THE OTHER SEVEN ZERO-PAGE CONSTANTS.
// Full evidence for each address->name mapping is on its declaration in BrnPropManager.h.
//
// ⭐⭐ VALUES RECOVERED AND SEATED, 2026-08-18 round 3b -- same correction as the seven above.
// The round-1/round-2 note here said "a ripgrep of every per-address export ... returns exactly
// FOUR files ... and every one of them is a READER. There is no initialiser in the image". The
// scan result was accurate; the CONCLUSION drawn from it was not. A per-address export scan
// cannot see these initialisers by construction -- they are MSVC dynamic-initialiser thunks
// that live outside every IDA function and therefore have no per-address export. Round 2 spent
// its effort auditing the export COUNT in that sentence (30,084 vs 30,095 vs 30,093) while the
// sentence's actual claim was the thing that was wrong; noted here because the lesson is
// re-usable -- when a scan says "nothing exists", check what the scan is structurally blind to
// before concluding it. The per-constant thunk / table slot / rodata evidence is on each
// declaration in BrnPropManager.h and repeated on each definition below.
//
// ⚠️⚠️ THE ZEROES WERE NOT HARMLESS -- and this is exactly the failure mode this block used to
// PREDICT while getting the cause wrong. With the placeholder zeroes ClampAcceleration compared
// against a threshold of 0 and clamped every moving body to zero acceleration every frame,
// GetPropInertia gave lampposts a zero inertia box, and the out-of-world floor sat at Y == 0.
// All three are now seated at the console's own values.
//
// NOTE ON THE FOURTH BRACE ELEMENT: this tree's `rw::math::vpu::Vector3` (vendor/renderware/
// include/rw/math/vpu/types.h:24) is a 4-lane aggregate `{ float x, y, z, w; }` -- the console
// type is one 16-byte register too, and the W lane is real storage in both. The thunks all
// write it explicitly (`stw r9` with r9 == 0), so the definitions do too rather than leaving
// it to depend on aggregate-initialisation defaults.
// =========================================================================================
// (0.0f, -9.8f, 0.0f), w == 0. Vector3 thunk 0x82C5E710, tbl slot 0x82CD19A8; lanes
// x/z <- flt_82001CC0 = 0x00000000, y <- flt_82013FC8 = 0xC11CCCCD, w <- `stw r9` (r9 == 0).
const Vector3  K_DEFAULT_GRAVITY        = { 0.0f, -9.8f, 0.0f, 0.0f };   // X360 0x82FB93F0
// (0.0f, 0.0f, -0.2f), w == 0. Vector3 thunk 0x82C5E7F0, tbl slot 0x82CD19BC; lanes
// x/y <- flt_82001CC0 = 0x00000000, z <- flt_82020A84 = 0xBE4CCCCD, w <- `stw r9` (r9 == 0).
const Vector3  K_PROP_EXTRA_COM_OFFSET  = { 0.0f, 0.0f, -0.2f, 0.0f };   // X360 0x82FB93C0
// (2.0f, 1.0f, 2.0f), w == 0. Vector3 thunk 0x82C5EAC8, tbl slot 0x82CD1A04; lanes
// x/z <- flt_82001D9C = 0x40000000, y <- flt_82001C98 = 0x3F800000, w <- `stw r9` (r9 == 0).
// Decoded per lane -- x and z share one rodata word and y another, so it is NOT a splat.
const Vector3  K_LAMPOST_INERTIA_BOX    = { 2.0f, 1.0f, 2.0f, 0.0f };    // X360 0x82FB9420

// Splat(30.0f). Thunk 0x82C5E830, tbl slot 0x82CD19C0, rodata flt_82004F5C = 0x41F00000.
const ::VecFloat KVF_MAX_LINEAR_ACCELERATION      = { 30.0f, 30.0f, 30.0f, 30.0f };      // X360 0x82FB94B0
// Splat(80.0f). Thunk 0x82C5E858, tbl slot 0x82CD19C4, rodata flt_82004A18 = 0x42A00000.
const ::VecFloat KVF_MAX_ANGULAR_ACCELERATION     = { 80.0f, 80.0f, 80.0f, 80.0f };      // X360 0x82FB9490

// ⚠️ THE TWO _SQ CONSTANTS ARE SEATED AS LITERALS, DELIBERATELY. Their thunks carry NO rodata
// word of their own -- they are the self-product shape:
//     0x82C5E880 (tbl slot 0x82CD19C8):  lvx128 v0, unk_82FB94B0 ; vmulfp128 v0,v0,v0 ; stvx128 -> 0x82FB9F40
//     0x82C5E8A0 (tbl slot 0x82CD19CC):  lvx128 v0, unk_82FB9490 ; vmulfp128 v0,v0,v0 ; stvx128 -> 0x82FB94D0
// i.e. on the console each is literally `sibling * sibling`, and the result is only well-defined
// because the _SQ slots sit AFTER their bases' slots in the initialiser table. Writing that
// dependency out here as `KVF_MAX_LINEAR_ACCELERATION * KVF_MAX_LINEAR_ACCELERATION` would make
// this tree's values depend on host dynamic-initialisation ORDER, which the standard does not
// guarantee across (or reliably within, once anyone splits this file) translation units -- the
// exact class of latent placeholder-zero bug this campaign has been burned by. So the products
// are evaluated here instead: 30^2 == 900 and 80^2 == 6400, both exact in f32, both equal to
// what the console computes at startup. NOT invented values -- derived ones, with the derivation
// and its inputs above.
const ::VecFloat KVF_MAX_LINEAR_ACCELERATION_SQ   = { 900.0f, 900.0f, 900.0f, 900.0f };  // X360 0x82FB9F40
const ::VecFloat KVF_MAX_ANGULAR_ACCELERATION_SQ  = { 6400.0f, 6400.0f, 6400.0f, 6400.0f }; // X360 0x82FB94D0

// ⚠️ AUTHORED NAME -- see the declaration's block in BrnPropManager.h. The VALUE is recovered:
// Splat(-1000.0f), thunk 0x82C5B570, tbl slot 0x82CD15E8, rodata flt_8200D4F8 = 0xC47A0000.
// (That slot is ~950 entries before this file's own contiguous run, i.e. a different TU emits
// the initialiser -- which is the standing lead on the missing name; see the header.)
const ::VecFloat KVF_PROP_OUT_OF_WORLD_HEIGHT     = { -1000.0f, -1000.0f, -1000.0f, -1000.0f }; // X360 0x82FB94C0

// ⭐⭐ NAME RECOVERED 2026-08-18 (wave Q4). This line read `KVF_MAX_CONTACT_GEN_PADDING` and was
// flagged AUTHORED; the DWARF names it (dwarfdump BrnPropManager.cpp:54 `const VecFloat
// KVF_MAX_PROP_PADDING;`) and the initialiser-table slot order pins it to that source line.
// Full evidence is on the declaration in BrnPropManager.h. This is PropManager_wQ2_03.cpp's
// HEADER REQUEST D, landed. Splat(0.3f), thunk 0x82C5E750, tbl slot 0x82CD19AC, rodata
// flt_82004740 = 0x3E99999A. (Request D's other half, KB_USE_CONTACT_GEN_STREAM, is NOT landed --
// no address, no thunk, nothing to measure. It stays open.)
const ::VecFloat KVF_MAX_PROP_PADDING             = { 0.3f, 0.3f, 0.3f, 0.3f };          // X360 0x82FB94F0

// ⭐ ADDED 2026-08-18 (wave Q4) -- the declaration landed in BrnPropManager.h this wave with NO
// definition anywhere, i.e. a latent LNK2001 the moment SetupAndValidatePropContact's real body
// mounts. This is that definition. Splat(0.01f); MEASURED, not inferred: initialiser-table slot
// 0x82CD19D0 -> thunk 0x82C5E8C0, dumped instruction by instruction with headless IDA over a
// private copy of the .i64 --
//     lis r11, flt_82002138@ha ; lfs f0 ; stfs -0x10(r1) ; lvlx v0 ; vspltw v0,v0,0 ;
//     stvx128 v0, r0, <unk_82FB93B0>
// and flt_82002138 == 0x3C23D70A == 0.01f. 0x82FB93B0 is the exact address
// SetupAndValidatePropContact loads at 0x826285AC before its `vcmpgtfp.`, so the reader and the
// writer are pinned to one another, not matched by plausibility.
const ::VecFloat KVF_MAX_LEAN_PROP_WORLD_PENETRATION = { 0.01f, 0.01f, 0.01f, 0.01f };   // X360 0x82FB93B0

// =========================================================================================
// ⭐ ADDED 2026-08-18 (round 3, fix round). THE SIX REMAINING ZERO-PAGE TUNABLES -- the five
// the jointed-prop/break legs read and the one ApplyPropRaceCarCollisionImpulse reads. Until
// now they were `extern`-declared file-locally in PropManager_wQ2_05.cpp with NO definition
// anywhere in the tree, i.e. a latent LNK2001; these are those definitions. Full evidence for
// each address->name mapping (and the INFERENCE flag on the last one) is on its declaration
// in BrnPropManager.h. The const-ness of each line is the DWARF's.
//
// ⭐⭐ VALUES RECOVERED AND SEATED, 2026-08-18 round 3b. Round 3's note here got closest and
// still landed wrong: it found "one unnamed six-instruction .text store stub in the 0x82C5Exxx
// run" for each of the six and classified it as "the debug-UI OnChange shape ... a runtime
// writer is not a static initialiser". That stub IS the static initialiser -- an entry of the
// MSVC dynamic-initialiser pointer table. The tell round 3 missed: the REAL OnChange handlers
// (PropDebugComponent::OnChangeInertiaScale @0x825BAF28 and friends) are named IDA functions
// inside the debug component, and every one of the six already had its own separate xref to
// one of those; the 0x82C5Exxx stub was a THIRD thing, sitting inside no function at all.
// Distinguishing them takes one membership test against the initialiser table -- see the recipe
// on the declarations in BrnPropManager.h.
// =========================================================================================
// Splat(0.1f). Thunk 0x82C5E778, tbl slot 0x82CD19B0, rodata flt_82004014 = 0x3DCCCCCD.
const ::VecFloat KVF_LEAN_PROP_LERP_SPEED           = { 0.1f, 0.1f, 0.1f, 0.1f };    // X360 0x82FB9500
// Splat(0.01f). Thunk 0x82C5E7A0, tbl slot 0x82CD19B4, rodata flt_82002138 = 0x3C23D70A.
const ::VecFloat KVF_LEAN_PROP_MIN_LERP             = { 0.01f, 0.01f, 0.01f, 0.01f }; // X360 0x82FB9F30
// Splat(0.1f). Thunk 0x82C5E7C8, tbl slot 0x82CD19B8, rodata flt_82004014 = 0x3DCCCCCD.
const ::VecFloat KVF_LEAN_PROP_ORTHOGONAL_TOLERANCE = { 0.1f, 0.1f, 0.1f, 0.1f };   // X360 0x82FB9390

// Splat(1.0f). Thunk 0x82C5E9B0, tbl slot 0x82CD19E8, rodata flt_82001C98 = 0x3F800000.
::VecFloat       KVF_BREAK_JOINT_LINEAR_VEL         = { 1.0f, 1.0f, 1.0f, 1.0f };    // X360 0x82FB9440
// Splat(1.0f). Thunk 0x82C5E9D8, tbl slot 0x82CD19EC, rodata flt_82001C98 = 0x3F800000.
::VecFloat       KVF_BREAK_JOINT_ANGULAR_VEL        = { 1.0f, 1.0f, 1.0f, 1.0f };    // X360 0x82FB9460

// ⚠️ INFERRED ADDRESS<->NAME MAPPING -- see the declaration's block in BrnPropManager.h.
// Splat(10.0f). Thunk 0x82C5E8E8, tbl slot 0x82CD19D4, rodata flt_82004A20 = 0x41200000.
::VecFloat       KVF_MAX_PROP_SPEED_MPS             = { 10.0f, 10.0f, 10.0f, 10.0f }; // X360 0x82FB9470

// =========================================================================================
// BrnPhysics::Props::PropManager::Construct @ 0x82627390 (82 asm).
//
// The three `bl`s in the body are hard ordering barriers, so the statement order below is not
// a guess -- it is the only order consistent with which store sits between which pair of calls:
//
//   before EventQueue<UpdatePropEvent,200>::Construct(this+0x680)
//       std 0x80  mUsedProps      std 0x90  mUsedParts
//   between it and EventQueue<UpdatePropEvent,15>::Construct(this+0x5E10)
//       (nothing)
//   between that and PropDebugComponent::Construct(this, this)
//       std 0x670 mUsedPropJoints   std 0x678 mBreakPropJoints
//       stfs 0.3f -> 0x74   stw 0 -> 0xA0   stw 0 -> 0x6570   stfs 0.6f -> 0x78
//   between that and the allocator call
//       stb 0 -> 0x48   stb 0 -> 0x49   stfs 10.0f -> 0x4C   stfs 0.0f -> 0x50
//   after it
//       stw 0 -> 0x64B4   stb 0 -> 0x64B8   stw <ptr> -> 0x64B0
//
// The four .rdata seeds were read out of the image, not chosen: flt_82004740 == 0.3f
// (mfStaticFriction), flt_82004D00 == 0.6f (mfDynamicFriction), flt_82004A20 == 10.0f
// (mfMassOverride), flt_82001CC0 == 0.0f (mfMaxLeanAngleOverride).
//
// The allocation is a plain RenderWare single-base-resource request:
//     descriptor[0] = { 0x600, 0x10 }, descriptor[1..] = { 0, 1 }
// and 0x600 == 32 * 48 == KI_MAX_DEBUG_WORLD_CONTACTS * sizeof(DebugWorldContactInfo), which
// is the arithmetic check that fixes the nested struct at three Vector3s. The X360 resolves
// the allocator statically (the object at 0x82F2C7DC) and reaches DoAllocate through the
// rw::IResourceAllocator vtable, and the `Allocators::mpInternalDebugAllocator != NULL` assert
// that precedes it has the file/line of BrnResourceAllocator (line 0x171 == 369) -- i.e. it is
// GetDebugAllocator()'s OWN inlined assert, not one PropManager wrote. So the source form is a
// bare GetDebugAllocator()->Allocate(...) and no assert is duplicated here.
//
// ⚠️ TWO OBSERVATIONS STATED RATHER THAN TIDIED:
//   * the shipped image stores zero to 0x670 and to 0x678 TWICE each (0x826273C4..0x826273D4)
//     while storing 0x80 and 0x90 once. Two identical stores of the same value to the same
//     address are behaviourally one; the likely source shape is a Construct()+UnSetAll() pair
//     on the two joint bit-sets, but nothing is invented to reproduce the duplication.
//   * the result of the allocation is NOT null-checked by this function. That is the console's
//     and it is left alone.
// =========================================================================================
void PropManager::Construct()
{
    mUsedProps.UnSetAll();
    mUsedParts.UnSetAll();

    mUpdatedProps.Construct();
    mUpdatedJointedProps.Construct();

    mUsedPropJoints.UnSetAll();
    mBreakPropJoints.UnSetAll();

    mfStaticFriction              = 0.3f;
    mfDynamicFriction             = 0.6f;
    mpPrimitiveWithTriangleStream = NULL;
    miNumPropsAddedToContactGen   = 0;

    mDebugComponent.Construct(this);

    mbRenderCOM            = false;
    mbUseOverides          = false;
    mfMassOverride         = 10.0f;
    mfMaxLeanAngleOverride = 0.0f;

    rw::ResourceDescriptor lDescriptor;
    lDescriptor.m_baseResourceDescriptors[0].m_size =
        static_cast<u32>(KI_MAX_DEBUG_WORLD_CONTACTS * sizeof(DebugWorldContactInfo));
    lDescriptor.m_baseResourceDescriptors[0].m_alignment = 16;
    for (u32 luIndex = 1; luIndex < rw::KU_RESOURCE_LANE_COUNT; ++luIndex)
    {
        lDescriptor.m_baseResourceDescriptors[luIndex].m_size      = 0;
        lDescriptor.m_baseResourceDescriptors[luIndex].m_alignment = 1;
    }

    const rw::Resource lResource = BrnResource::GetDebugAllocator()->Allocate(lDescriptor, 0);

    miNumDebugWorldContacts = 0;
    mbDisableFreezing       = false;
    mpDebugWorldContacts    =
        static_cast<DebugWorldContactInfo*>(lResource.m_baseResources[0]);
}

// BrnPhysics::Props::PropManager::ConstructContactGenerationPerfMonitors @ 0x825BAC60.
//
// The whole shipped function, verbatim:
//     li   r11, 0
//     stw  r11, 0xA4(r3)
//     blr
// i.e. one store of zero into the contact-generation perf-monitor id. Its NAME and the
// member's name agree, which is the cross-check that fixes +0xA4 == miContactGeneratorWaitPM.
//
// FLAG (honest, not settled): in a PerfMon-enabled build the source almost certainly called
// CgsDev::PerfMonCpu::AddMonitor here (that is what the sibling VehicleManager::Construct
// @0x8263B7C8 does for its own ~21 monitors). In the shipped ARTIST image the call is gone
// and only the `= 0` remains. The ASM IS THE SPECIFICATION, so the `= 0` is reproduced and
// no AddMonitor call is invented.
void PropManager::ConstructContactGenerationPerfMonitors()
{
    miContactGeneratorWaitPM = 0;
}

// BrnPhysics::Props::PropManager::ConstructPreScenePerfMonitors @ 0x825BAC70.
//
// The whole shipped function, verbatim:
//     li   r11, 0
//     stw  r11, 0xB0(r3)      miProcessAddPropInstancePM
//     stw  r11, 0xB4(r3)      miProcessAddPartInstancePM
//     stw  r11, 0xA8(r3)      miProcessRemovePropPM
//     stw  r11, 0xAC(r3)      miProcessRemovePartPM
//     blr
//
// The store ORDER is add-prop, add-part, remove-prop, remove-part; the DECLARATION order in
// the DWARF is remove-prop, remove-part, add-prop, add-part. Assignments to independent
// scalars are order-immaterial, so the source order is not recoverable from the asm; the
// DWARF declaration order is used here. Note the fifth pre-scene-looking id,
// miProcessBreakPropPM (+0xB8), is written by NEITHER constructor -- that is a fact of the
// shipped image, and nothing is added to "tidy" it.
void PropManager::ConstructPreScenePerfMonitors()
{
    miProcessRemovePropPM      = 0;
    miProcessRemovePartPM      = 0;
    miProcessAddPropInstancePM = 0;
    miProcessAddPartInstancePM = 0;
}

// =========================================================================================
// BrnPhysics::Props::PropManager::CreateContactEvent @ 0x825A53A0  (DWARF BrnPropManager.h:172)
// ADDED 2026-08-06 (bridge de-facade wave). Sole caller: PhysicsModule::StoreContact
// @0x825A5DB0 (the E_ENTITYTYPE_PROP arm). The console body was header-inline (its FireAsserts
// bake BrnPropManager.h:529/530/533/534/550/563); the 0x825A53A0 emission is its out-of-line
// copy, reconstructed branch-for-branch.
//
// Shape (X360 asm):
//   * null tripwires on the out record (:529) and the in spy (:530);
//   * owner tripwires: spy mIDA's high-dword owner byte (:533) and the potential contact's
//     muVolumeInstanceIdA owner byte (:534) must both be E_ENTITYTYPE_PROP (3);
//   * BaseContact::Construct(out, spy rows, potential contact) -- the shared stamp;
//   * out->mEntityIdA = mIDA's HIGH dword (overwriting the Construct seed -- the prop side
//     keys the event by its PropEntityID word; `stw` at out+0); muFlags = 0; the PropEntityID
//     owner tripwire ("mEntityId.GetOwner() == E_ENTITYTYPE_PROP", BrnPropEntityID.h:278);
//     muBeganMoving = 0;
//   * PART id (the id word's low-10-bit part field != 0 -- PropEntityID::GetPartIndex()):
//       muType = mpaPartInstances[low 16 bits of mIDA's LOW dword].muTypeId
//       (asm `(low << 6) & 0x3FFFC0` == (low & 0xFFFF) * sizeof(PropPartInstance)==64; the
//       type word is the part instance's +0x34), tripwired < 1000 (:550); muState = 1;
//   * WHOLE-PROP id: the instance is mpaPropInstances[LOW 16 BITS of mIDA's low dword]
//       (112-byte stride; asm `clrlwi r11,r11,16 ; mulli r11,r11,0x70` @0x825A5530 -- the index
//       is the handle's USER-ID-B, NOT the whole low dword: corrected 2026-08-18, see the body);
//       muType = instance.muTypeId (+0x64) masked to 16 bits, tripwired < 1000 (:563); if the
//       muMovementState < E_PROP_MOVESTATE_MOVING it is promoted to MOVING and
//       muBeganMoving = 1; muState = 0;
//   * if muType != KU_UNKNOWN_PROP_TYPE (0xFFFF): look the type up through the physics-data
//     resource header (mpPhysicsData->GetType; operator-> carries its own null assert) and
//     set muFlags bit KU_FLAG_SMASH_GATE when the type's graphics id is 396075, bit
//     KU_FLAG_BILLBOARD when it is 428180 or 428152 (asm-literal graphics ids, read via the
//     committed PropTypeData::GetGraphicsId over the console +0x58 word).
// =========================================================================================
void PropManager::CreateContactEvent( ContactSpy::PropContact* lpOutPropContact,
                                      const CgsPhysics::PhysicsSimulationIO::OutContactSpy* lpInContact,
                                      const CgsSceneManager::SceneManagerIO::PotentialContact* lpInPotentialContact )
{
    CGS_ASSERT(lpOutPropContact != nullptr, "lpOutPropContact != NULL");   // :529
    CGS_ASSERT(lpInContact != nullptr, "lpInContact != NULL");             // :530

    const u32 luSpyIdAHigh = static_cast<u32>(lpInContact->mIDA >> 32);
    CGS_ASSERT((luSpyIdAHigh >> 24) == 3u,
               "lpInContact->mIDA.GetEntityId().GetOwner() == BrnWorld::E_ENTITYTYPE_PROP");   // :533
    CGS_ASSERT(static_cast<u32>(lpInPotentialContact->muVolumeInstanceIdA.muId >> 56) == 3u,
               "lpInPotentialContact->muVolumeInstanceIdA.GetEntityIDOwner() == BrnWorld::E_ENTITYTYPE_PROP");   // :534

    // Shared BaseContact stamp (entity words from the potential contact, the five spy rows,
    // the poly-tag swap/sentinel) -- then the prop-specific overrides below.
    ContactSpy::BaseContact::Construct(lpOutPropContact,
                                       &lpInContact->mFrictionStress,   // the spy's five leading rows
                                       lpInPotentialContact);

    // The prop side keys the event by the spy's own PropEntityID word (mIDA's high dword),
    // overwriting the Construct seed (the asm's `stw` at out+0).
    lpOutPropContact->mEntityIdA.muValue = luSpyIdAHigh;
    lpOutPropContact->muFlags = 0;
    CGS_ASSERT((luSpyIdAHigh >> 24) == 3u,
               "mEntityId.GetOwner() == E_ENTITYTYPE_PROP");   // BrnPropEntityID.h:278 (inlined PropEntityID tripwire)
    lpOutPropContact->muBeganMoving = 0;

    const u32 luSpyIdALow = static_cast<u32>(lpInContact->mIDA & 0xFFFFFFFFu);

    if ((luSpyIdAHigh & 0x3FFu) != 0u)
    {
        // PART id: the part-instance table, 64-byte stride, index = the id's USER-ID-B, i.e.
        // the handle's low 16 bits (asm `clrlslwi r11,r11,16,6` == (low & 0xFFFF) << 6).
        const u16 luTypeId =
            static_cast<u16>(mpaPartInstances[luSpyIdALow & 0xFFFFu].GetType());  // asm masks the
        CGS_ASSERT(luTypeId < 1000u, "luTypeId < 1000");   // :550          //  type word to 16 bits
        lpOutPropContact->muType  = luTypeId;                               //  BEFORE the compare
        lpOutPropContact->muState = 1;
    }
    else
    {
        // Whole-prop id: the prop-instance table, 112-byte stride.
        // ⚠️⚠️ FIXED 2026-08-18 (wave Q4 re-verification). This line read
        //     `mpaPropInstances[luSpyIdALow]`
        // and its comment said "index = the id's low dword". BOTH WERE WRONG. The console masks
        // first -- 0x825A5530 `clrlwi r11, r11, 16` then `mulli r11, r11, 0x70` -- because the
        // index lives in the handle's USER-ID-B, the low 16 bits, exactly like the part branch
        // directly above and exactly where SetupAndValidatePropContact's four
        // RigidBodyId::SetUserIDB stamps put it. The handle's other 16-bit user field sits in
        // bits 16..31 of the same dword, so an id carrying a user-id-A indexed this table at
        // >= 65536 * 112 bytes -- an out-of-bounds READ of muTypeId and, three lines below, an
        // out-of-bounds WRITE of muMovementState.
        // ⚠️ HOW BAD IS IT, STATED HONESTLY RATHER THAN DRAMATICALLY: on today's pipeline the
        // difference is not observable, because the one producer of a prop-side id --
        // BrnPhysicsModuleBridgeFunctions.cpp:704 -- builds it as `(id >> 32) << 32`, i.e. with
        // the whole low dword cleared, so user-id-A is 0 and the mask is a no-op. This is
        // therefore a FAITHFULNESS fix with a latent-safety dividend, not a live corruption:
        // the console masks, so the reconstruction masks, and the day some other producer stamps
        // a user-id-A the two builds still agree instead of one of them indexing off the end.
        PropInstance& lrInstance = mpaPropInstances[luSpyIdALow & 0xFFFFu];
        const u16 luTypeId = static_cast<u16>(lrInstance.muTypeId);   // asm `clrlwi r30,r11,16`
        CGS_ASSERT(luTypeId < 1000u, "luTypeId < 1000");   // :563
        lpOutPropContact->muType = luTypeId;
        if (lrInstance.muMovementState < static_cast<u8>(E_PROP_MOVESTATE_MOVING))
        {
            lrInstance.muMovementState      = static_cast<u8>(E_PROP_MOVESTATE_MOVING);
            lpOutPropContact->muBeganMoving = 1;
        }
        lpOutPropContact->muState = 0;
    }

    if (lpOutPropContact->muType != ContactSpy::PropContact::KU_UNKNOWN_PROP_TYPE)
    {
        const PropTypeData* lpType = mpPhysicsData->GetType(lpOutPropContact->muType);
        if (lpType->GetGraphicsId() == 396075u)
        {
            lpOutPropContact->muFlags |= ContactSpy::PropContact::KU_FLAG_SMASH_GATE;
        }
        if (lpType->GetGraphicsId() == 428180u || lpType->GetGraphicsId() == 428152u)
        {
            lpOutPropContact->muFlags |= ContactSpy::PropContact::KU_FLAG_BILLBOARD;
        }
    }
}

// =================================================================================================
// PropManager::SetupAndValidatePropContact  @0x82628190  (PS3 DecFIGS 0x79008C)
//
// ⭐⭐ THE TRAP STUB THAT STOOD HERE IS GONE, 2026-08-18 (wave Q4, physics-contact seam). The REAL
// 573-instruction body is reconstructed in this TU's sibling partfile
//     GameSource/Physics/PropManager/PropManager_wQ4_01.cpp
// and a definition in both files is an LNK2005, so the stub was DELETED in the same edit that
// landed the body -- not left behind.
//
// ⚠️⚠️ CONSEQUENCE FOR THE LINK: PropManager_wQ4_01.cpp MUST be mounted in
// tools/build/build_game_exe.bat. Its only caller, PhysicsModule::BridgeContactsToSimulation, is
// in the MOUNTED BrnPhysicsModuleBridgeFunctions.cpp, so an exe built with this file but without
// the partfile is an immediate LNK2019 on
// `BrnPhysics::Props::PropManager::SetupAndValidatePropContact`. The exact echo line to add, and
// the rest of the PropManager mount closure, are in scratchpad/waveQ4/physcontact.owner.md.
// =================================================================================================

}
}

// ============================================================================
// FOLDED FROM PropManager_wQ4_01.cpp (wave Q4) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================
// GameSource/Physics/PropManager/PropManager_wQ4_01.cpp
//
// BrnPhysics::Props::PropManager -- breakable-props / smash-gates wave Q4 (SEAM WAVE),
// partfile 01. Reconstructed from BURNOUT_X360_ARTIST.XEX:
//
//   SetupAndValidatePropContact @0x82628190   (573 instructions, 0x82628190..0x82628A80 --
//                                              COUNTED: (0x82628A80-0x82628190)/4 + 1 == 573;
//                                              the ledger's "572" is off by one)
//
// A slice of the TU's own BrnPropManager.cpp, split out per this wave's partfile convention
// (same spirit as BrnPropManager_PropInstanceQueries.cpp / PropManager_wQ2_08.cpp).
//
// ⚠️⚠️ MOUNT + RETIRE IS ONE COMMIT. The body below REPLACES the TRAP STUB that still stands in
// the MOUNTED BrnPropManager.cpp (that file's `SetupAndValidatePropContact` block). The trap
// stub was deliberately left in place by this wave so the shared checkout keeps linking -- its
// only caller, PhysicsModule::BridgeContactsToSimulation, lives in the MOUNTED
// BrnPhysicsModuleBridgeFunctions.cpp (build_game_exe.bat:781), so deleting the stub without
// mounting this file in the same commit is an immediate LNK2019. When this partfile is added to
// tools/build/build_game_exe.bat, DELETE BrnPropManager.cpp's trap-stub definition in the SAME
// commit or the link is an LNK2005 duplicate. Exact line range is in
// scratchpad/waveQ4/physcontact.owner.md.
//
// ==========================================================================================
// WHAT THIS FUNCTION IS FOR (and why wave Q4 needed it)
//
// This is the physics-side gate every car-versus-prop contact passes through. The scene
// manager hands PhysicsModule::BridgeContactsToSimulation a stream of PotentialContacts; for
// any pair where either side's owner byte is E_ENTITYTYPE_PROP (3) the bridge calls this
// function, and `false` DROPS the contact. Everything the prop pipeline does on impact
// starts here: the anti-herding shove, the impulse the car takes back, the jointed
// (lean/tilt) resolvers -- and, for a leaning prop pushed into the world, the SetBit into
// mBreakPropJoints that is what actually breaks a smash gate's joint.
//
// ==========================================================================================
// REGISTER MAP, measured from the prologue (0x826281A4..0x826281C8):
//   r3  -> r30 = this
//   r4  -> r23 = lpOutContact                 (InAddPotentialContact*)
//   r5  -> r22 = lpInPotentialContact         (const PotentialContact*)
//   r6  -> r24 = lpVehicleManager
//   r7  -> r16 = lpSimInputBuffer
//   r8  =  lpPropRaceCarContactBuffer         ⚠️ NEVER SAVED, NEVER READ  (see below)
//   r9  =  lWorldRigidBodyId                  ⚠️ NEVER SAVED, NEVER READ  (see below)
//   r10 -> r25 = lbOtherEntityIsFrozen        (its single use is 0x82628580)
//   f1  -> f31 = lfTimeStep                   (gotcha 3: the float rides f1 and SKIPS its GPR)
//
// ⚠️ TWO PARAMETERS ARE GENUINELY DEAD IN THE ARTIST EMISSION and are spelled unnamed below:
// `lpPropRaceCarContactBuffer` (r8) and `lWorldRigidBodyId` (r9). Neither register is copied in
// the prologue and neither is read anywhere in the 573 instructions (both are later reused as
// scratch -- r8 at 0x826289AC as a stack address, r9 at 0x826289A0 as an index literal -- which
// is the compiler proving to itself they were dead). They are KEPT in the signature because the
// DecFIGS DWARF declares them (dwarfdump BrnPropManager.h:191 / BrnPropManager.cpp:2653) and the
// asm cannot disprove an unused parameter; the caller in BrnPhysicsModuleBridgeFunctions.cpp
// already passes both. Flagged so nobody "discovers" the drop later and deletes them.
// ⚠️ NOTE what that means for wave Q4's seam question: this function does NOT write the
// PropRaceCarContactBuffer. Whoever is looking for the producer of that buffer must look
// elsewhere (PhysicsModule::Update pushes it; nothing here fills it).
//
// PARAMETER NAMES are the DWARF's (dwarfdump BrnPropManager.cpp:2653). The committed header
// spelled four of them differently (lpAddContactEvent / lpPotentialContact /
// lpSimModuleInputBuffer / lbFrozen); those are corrected to the DWARF's in BrnPropManager.h in
// this same wave. NAME-ONLY -- no type, order or count changed, so no call site moved.
//
// ==========================================================================================
// THE 573 INSTRUCTIONS, in emission order. Every console offset quoted below is reached BY
// NAME in the C++ (gotcha 1 -- they are meaningless on the LLP64 host).
//
//  (1) 0x826281C0..0x8262826C -- THE TWO TRAFFIC REMAPS, one per side, identical shape.
//      `ld 0x30(potential)` / `ld 0x38(potential)` == muVolumeInstanceIdA/B; `srdi 32 ; srwi 24`
//      == the embedded entity word's owner byte; == 2 (E_ENTITYTYPE_TRAFFIC_VEHICLE) selects
//      the remap. VehicleManager::GetTrafficPhysicsEntityIDFromGlobalEntityID_Safe resolves the
//      GLOBAL id to a PHYSICS one; a false return RETURNS FALSE (drops the contact). The
//      resolved id then gets `EntityId::SetOwner(0xC)` == E_ENTITYTYPE_PROP_COLLISION_TRAFFIC
//      and is spliced into the OUT event's own id (`stw 0,0xNN ; ld ; extldi 32 ; or ; std` ==
//      RigidBodyId::SetEntityId). DWARF blocks at BrnPropManager.cpp:1313/:1314 and :1333/:1334
//      name `bool lbTrafficExists` + `EntityId lDummyTrafficEntityId` twice -- once per side.
//      This is the exact twin of RoutePropVsRaceCarContactToDummyCar's owner-11 stamp.
//
//  (2) 0x82628270..0x82628298 -- the friction/restitution seed:
//      mStaticFriction <- mfStaticFriction (+0x74), mDynamicFriction <- mfDynamicFriction
//      (+0x78), mRestitution <- flt_82F2A398 == KF_PROP_RESTITUTION. (That address is the fifth
//      and last of this file's contiguous f32 tuning run 0x82F2A388..0x82F2A398, which is the
//      cross-check that the committed name for it is right.)
//
//  (3) 0x8262829C..0x826282AC -- `lbPropIsEntityA = (lpOutContact->mIDA owner == 3)`, computed
//      with the branchless `addi -3 ; cntlzw ; extrwi 1,26` idiom.
//
//  (4) 0x826282B0..0x8262855C -- THE MIRRORED PROP-SIDE RESOLUTION (branch A at 0x826282B0,
//      branch B at 0x8262840C; the two are instruction-for-instruction the same with 0x30/0x38
//      and mPointOnA/mPointOnB swapped). Per side:
//        * lPropRigidBodyId  = the OUT event's id on the prop side, lOtherRigidBodyId the other;
//        * lPropEntityId     = PropEntityID(potential->muVolumeInstanceIdX entity word);
//        * lu8OtherOwner     = lOtherRigidBodyId.GetEntityIDOwner();
//        * PART vs WHOLE PROP on `clrlwi r11,r31,22` == PropEntityID::GetPartIndex() != 0;
//        * liPhysicsIndex    = (lu8OtherOwner == WORLD) ? potential->muPolyTagA
//                                                       : Find{Prop,Part}Index(lPropEntityId)
//          -- see THE POLY-TAG SEAM below, this is measured end to end, not guessed;
//        * the bounds assert (baked lines 0x568/0x580/0x5A7/0x5BF of BrnPropManager.cpp);
//        * `== -1` (KI_PROP_INDEX_NOT_FOUND) RETURNS FALSE;
//        * Has{Prop,Part}JustBeenRemoved RETURNS FALSE;
//        * whole-prop only: lpPropInstance = &mpaPropInstances[liPhysicsIndex];
//        * then the join: lPointOnProp / lPointOnOtherRigidBody are lifted out of the potential
//          contact PROP-SIDE FIRST (v124 is always the prop's point, v126 always the other's),
//          and the OUT event's prop-side id gets its user-id-B set to liPhysicsIndex
//          (`sth 0,0x36|0x3E ; ld ; or ; std` == RigidBodyId::SetUserIDB).
//
//  (5) 0x82628560..0x82628728 -- THE JOINTED/STATIC PROP GATE, i.e. THE SMASH-GATE BREAK.
//      Only entered for a WHOLE prop (lpPropInstance != NULL) that IsJointed() or IsStatic().
//        * lbOtherEntityIsFrozen RETURNS FALSE (its only use in the whole body);
//        * other side is the WORLD -> the penetration test. lFlatNormal = potential->mNormal
//          with Y zeroed (`vrlimi128 v11,v12,4,0` -- mask 4 == lane 1); lvfPenetration =
//          Dot(lFlatNormal, mPointOnB - mPointOnA); if it exceeds
//          KVF_MAX_LEAN_PROP_WORLD_PENETRATION the prop's joint index is SET IN
//          mBreakPropJoints -- and the contact is dropped either way (the SetBit falls straight
//          into the `li r3,0` return). ⭐ THAT SetBit IS THE BREAK: UpdateJointedProps drains
//          mBreakPropJoints and calls BreakJoint.
//        * other side is another WHOLE PROP -> if that prop is itself jointed or static the
//          contact is dropped (a leaning prop may not lean on another leaning/static prop).
//
//  (6) 0x82628730..0x82628A00 -- THE RACE-CAR LEG (lu8OtherOwner == 1). Frictions are zeroed
//      (`flt_82001CC0` == 0.0f into both +0x40 and +0x44 -- a car/prop contact carries no
//      friction), then VehicleManager::GetRaceCarPhysics resolves the car (the console inlines
//      that accessor verbatim: eight `lwz`/`cmplw` over maRaceCarEntityIDs at +43584, then
//      `mulli 0x1460` + `addi 0x740` into maRaceCarVehicles). Then:
//        WHOLE PROP: lpProp / lCarTransform / lpPropType; RoutePropVsRaceCarContactToDummyCar
//          when the prop is non-static or jointed; lPropRigidBodyId.SetUserIDB(liPhysicsIndex);
//          lNormal flipped to point at the car; ApplyAntiHerdingForce unless the type is a
//          lamppost; then either the lean/tilt joint resolver (keyed on
//          PropTypeData::GetJointType()) or ApplyPropRaceCarCollisionImpulse.
//        PART:       FindPartIndex on the prop rigid-body id's own entity word, lPartRigidBodyId
//          = lPropRigidBodyId with user-id-B = liPartIndex, the part's mass out of the prop
//          type's part table, ApplyAntiHerdingForce, then unconditionally
//          RoutePropVsRaceCarContactToDummyCar.
//
//  (7) 0x82628A04..0x82628A80 -- the shared tail. When the OTHER side is a prop too, resolve
//      ITS index (prop or part, on its own part-index field) and stamp it into the OUT event's
//      mIDB; a miss RETURNS FALSE. Then return true.
//      ⚠️ The tail writes mIDB unconditionally, which is only correct because it is
//      UNREACHABLE when the prop is entity B: `lu8OtherOwner == 3` plus `lbPropIsEntityA ==
//      false` would mean A is a prop AND lbPropIsEntityA was false -- a contradiction, since
//      lbPropIsEntityA is exactly "A's owner == 3". Checked rather than assumed, because the
//      asymmetry reads like a bug on first pass.
//
// ==========================================================================================
// ⭐ THE POLY-TAG SEAM -- measured PRODUCER-to-CONSUMER, because it looks wrong until it does
// not. When the other side of the contact is the WORLD, this function does NOT search for the
// prop; it reads `lwz r27, 0x40(r22)` == PotentialContact::muPolyTagA and uses that word AS
// liPhysicsIndex. The producer chain confirms it, and all three links are in the tree today:
//   * DoPropInstanceWorldContactGeneration / DoPartWorldContactGeneration pass the prop's own
//     slot index as the PER-PRIMITIVE tag --
//     `lPrimPairList.AddPrimitive(volume, matrix, padding, static_cast<u16>(liPropIndex))`
//     (console `clrlwi r26,r26,16`), parked at scratchpad/waveQ2/parked/
//     PropManager_03_Do{PropInstance,Part}WorldContactGeneration.cpp;
//   * AddContactResultsToQueue (PropManager_wQ2_03.cpp:463) copies that tag straight through:
//     `lContact.muPolyTagA = lrResult.muPrimitive0Tag;`
//   * and primitive 0 is always the PROP side on that path (muVolumeInstanceIdA is built from
//     the prop entity word, muVolumeInstanceIdB from the world's).
// The same "a locally generated contact carries its own pool index in muPolyTagA" convention is
// already committed for the deformation pools (BrnDeformationManager_ContactFixups.cpp:71/:122).
// ⚠️ CONSEQUENCE FOR THE MOUNT: the two Do*WorldContactGeneration bodies are the only producers
// of that tag, and they are NOT in the tree (parked on two collision-generator declarations).
// Until they land, no prop-vs-WORLD contact reaches this function at all, so leg (5)'s break
// path is dead -- which is exactly the hole wave Q4 should report rather than paper over.
//
// ==========================================================================================
// ⚠️ NaN POLARITY (gotcha 4) -- two decisions here are vector compares, and both are written in
// the console's polarity rather than the "obvious" one:
//   * the penetration gate is `vcmpgtfp.` + a CR6[0] ("all lanes true") test, so an unordered
//     compare DROPS the contact. Spelled `if (!(penetration > K)) return false;`.
//   * the normal flip is `vcmpgefp` followed by `vnot128`, so an unordered compare FLIPS the
//     normal. Spelled `!(dot >= 0.0f)`, NOT `dot < 0.0f` -- those differ exactly on NaN.
// Both compares are between BROADCAST lanes (vmsum3fp128 splats its 3-lane dot, and both
// constants are splats), so the all-lanes CR6 test and the scalar compare below agree on every
// input, ordered or not.
//
// ⚠️ ONE UNGUARDED DEREFERENCE IS THE CONSOLE'S AND IS KEPT. `GetRaceCarPhysics` returns NULL on
// a miss (`li r29,0` at 0x82628770) and 0x826287A8 `lvx128 v125, r29, r24` then loads through it
// with no check. The console gets away with it because owner == E_ENTITYTYPE_RACECAR means the
// car table has the entity; it is still an unchecked deref and it is reproduced, not "fixed" --
// if this ever faults on the host, the bug is upstream in the owner byte, not here.
//
// ==========================================================================================
// TWO HEADER REQUESTS (both one declaration each; both stood in for with a file-local helper so
// this partfile links, and neither header is in this cluster's ownership):
//
//   REQUEST 1 -- GameShared/GameClasses/Physics/CgsRigidBody.h, public section:
//                    inline void SetUserIDB( u16 lu16UserIDB );
//                DWARF-attested: dwarfdump GameShared/GameClasses/Physics/CgsRigidBody.h:102
//                `void SetUserIDB(uint16_t);`, beside the already-committed GetIndex() (:94)
//                which reads the SAME field (`mId & 0xFFFF`). X360 shape, four sites in this
//                one function: `sth 0, 0x36(id) ; ld ; or <index & 0xFFFF> ; std`, i.e.
//                `mId = (mId & ~0xFFFFull) | lu16UserIDB;`. Stood in for by
//                SetRigidBodyIdUserIDB below -- fold the four call sites onto the method when
//                it lands.
//
//   REQUEST 2 -- GameShared/GameClasses/SceneManager/CgsVolumeInstanceId.h, public section:
//                    CgsSceneManager::EntityId GetEntityId() const;
//                DWARF-attested: this function's own DecFIGS scope lists
//                `CgsSceneManager::VolumeInstanceId::GetEntityId(...)` three times. The header
//                already owns GetEntityIDOwner() and GetEntityIDEntityIndex() over the same
//                embedded word; only the whole-word getter is missing. Stood in for by
//                GetVolumeInstanceEntityWord below. (⚠️ A CONCURRENT wave-Q4 owner is editing
//                that header right now -- its GetEntityIDOwner/SetVolumeIndex blocks are dated
//                today -- which is the second reason this partfile does not touch it.)
//
// ==========================================================================================
// LINK-LEVEL, INVISIBLE TO `cl /c` (gotcha 12). Every callee of this body is DECLARED, so the
// compile gate is green. ✅ ALL THREE FORMER HOLES ARE CLOSED -- re-grepped 2026-08-19 (wave Q6
// round 2); this list used to read "(parked, waveQ2/parked/)" for each and that is history now:
//     PropManager::HandleContactWithLeanProp        @0x8260FB60  -- BODIED, PropManager_wQ6_02.cpp
//                                                                  (MOUNTED, bat:1802)
//     PropManager::HandleContactWithTiltProp        @0x826108B8  -- BODIED, PropManager_wQ6_02.cpp
//                                                                  (MOUNTED, bat:1802)
//     PropManager::ApplyPropRaceCarCollisionImpulse @0x825E3560  -- BODIED, PropManager_wQ4_04.cpp
//                                                                  (MOUNTED, bat:1791), landed at
//                                                                  the wave-Q4 integration
// Everything else it calls is bodied: FindPropIndex / FindPartIndex / HasPropJustBeenRemoved /
// HasPartJustBeenRemoved / RoutePropVsRaceCarContactToDummyCar / ApplyAntiHerdingForce, plus
// PropPhysicsDataHeader::GetType, PropTypeData::IsLamppost, PropInstance::GetJointIndex and
// VehicleManager::GetTrafficPhysicsEntityIDFromGlobalEntityID_Safe -- but four of those TUs are
// NOT MOUNTED today. The full mount closure is in scratchpad/waveQ4/physcontact.owner.md.
// ==========================================================================================


namespace BrnPhysics
{
namespace Props
{
namespace
{
    // ⚠️ STANDS IN FOR HEADER REQUEST 1 -- CgsPhysics::RigidBodyId::SetUserIDB(uint16_t)
    // (DWARF CgsRigidBody.h:102). The field is the handle's low 16 bits, which is the SAME
    // field the committed RigidBodyId::GetIndex() (CgsRigidBody.h:70-74) already reads back;
    // the console spelling is `sth 0, +6(id) ; ld ; or index ; std`. Written once here rather
    // than four times inline so the geometry lives in ONE place until the method lands.
    inline void SetRigidBodyIdUserIDB( CgsPhysics::RigidBodyId& lrId, u16 lu16UserIDB )
    {
        lrId = CgsPhysics::RigidBodyId(
            ( static_cast<u64>( lrId ) & ~static_cast<u64>( 0xFFFFu ) )
            | static_cast<u64>( lu16UserIDB ) );
    }

    // ⚠️ STANDS IN FOR HEADER REQUEST 2 -- CgsSceneManager::VolumeInstanceId::GetEntityId().
    // The embedded entity word is the packed id's HIGH dword (the header's own
    // KU_ENTITY_ID_START_INDEX == 32); the console reads it as `ld ; srdi r,r,32`. The same
    // spelling is already committed in the mounted BrnPhysicsModuleBridgeFunctions.cpp, which
    // is this function's caller.
    inline u32 GetVolumeInstanceEntityWord( const CgsSceneManager::VolumeInstanceId& lrId )
    {
        return static_cast<u32>(
            lrId.muId >> CgsSceneManager::VolumeInstanceId::KU_ENTITY_ID_START_INDEX );
    }

    // PropTypeData's nested joint-type enum. The console compares mu8JointType against 1 then
    // 2, and the assert string baked at 0x826288E8 names the second value:
    // "lpType->mu8JointType == BrnPhysics::Props::PropTypeData::E_TILT".
    // ⚠️ The enum has NO home in this tree (BrnPhysicsPropTypeData.h declares only the u8 field
    // and its GetJointType()/GetLeanState() accessors) and this cluster does not own that
    // header, so the two values are named locally. E_TILT's NAME is attested by the assert
    // text; the lean value's name is NOT -- only its value (1) is measured. Do not promote the
    // authored name into a shared header without recovering the real enumerator.
    static const u8 KU8_JOINT_TYPE_LEAN = 1;   // ⚠️ AUTHORED NAME -- value measured, name is not
    static const u8 KU8_JOINT_TYPE_TILT = 2;   // == PropTypeData::E_TILT (assert text @0x826288E8)
}

// =============================================================================================
// SetupAndValidatePropContact -- X360 0x82628190, 573 instructions.
// DecFIGS: dwarfdump BrnPropManager.h:191 (declaration) / BrnPropManager.cpp:2653 (scope,
// source :1300 onwards). Parameter names and every local name below are the DWARF's.
// =============================================================================================
bool
PropManager::SetupAndValidatePropContact(
    CgsPhysics::PhysicsSimulationIO::InAddPotentialContact*   lpOutContact,
    const CgsSceneManager::SceneManagerIO::PotentialContact*  lpInPotentialContact,
    BrnPhysics::Vehicle::VehicleManager*                      lpVehicleManager,
    CgsPhysics::PhysicsSimulationIO::InputBuffer*             lpSimInputBuffer,
    PropRaceCarContactBuffer*                                 /*lpPropRaceCarContactBuffer*/,
    CgsPhysics::RigidBodyId                                   /*lWorldRigidBodyId*/,
    bool                                                      lbOtherEntityIsFrozen,
    f32                                                       lfTimeStep )
{
    namespace vpu = rw::math::vpu;

    // -----------------------------------------------------------------------------------------
    // (1) The two traffic remaps -- 0x826281C0..0x8262826C. DWARF blocks at :1313/:1314 (side A)
    //     and :1333/:1334 (side B) name `bool lbTrafficExists` + `EntityId lDummyTrafficEntityId`
    //     once per side, which is why this is written twice rather than folded into a loop.
    // -----------------------------------------------------------------------------------------
    if ( lpInPotentialContact->muVolumeInstanceIdA.GetEntityIDOwner()
             == static_cast<u8>( BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE ) )
    {
        // ⚠️ HEADER DEFECT (REPORTED, NOT FIXED HERE -- VehicleManager/** is outside this
        // cluster's ownership). BrnVehicleManager.h:1246 declares the out-parameter as the
        // BrnCommonTypes.h storage-word `::EntityId`, but the DWARF declares it as the real
        // handle class (dwarfdump BrnVehicleManager.h:1028 `bool
        // GetTrafficPhysicsEntityIDFromGlobalEntityID_Safe(EntityId, EntityId*)`, and this
        // function's own DecFIGS scope resolves that name by calling
        // `CgsSceneManager::EntityId::SetOwner` on the very slot it wrote). The console proves
        // they are one object: 0x826281E0 passes `r1+var_100` as the out-param and 0x826281FC
        // passes THE SAME slot as `this` to CgsSceneManager::EntityId::SetOwner. So the two
        // spellings are one 32-bit word and the hop below is exact, not a reinterpretation --
        // it is written by VALUE rather than by cast for exactly that reason.
        ::EntityId lRawTrafficEntityId;
        const bool lbTrafficExists =
            lpVehicleManager->GetTrafficPhysicsEntityIDFromGlobalEntityID_Safe(
                GetVolumeInstanceEntityWord( lpInPotentialContact->muVolumeInstanceIdA ),
                &lRawTrafficEntityId );
        if ( !lbTrafficExists )
        {
            return false;                                             // 0x826281F4 beq
        }

        CgsSceneManager::EntityId lDummyTrafficEntityId( lRawTrafficEntityId.muValue );
        lDummyTrafficEntityId.SetOwner(
            static_cast<u8>( BrnWorld::E_ENTITYTYPE_PROP_COLLISION_TRAFFIC ) );   // li r4,0xC

        CgsPhysics::RigidBodyId lIdA( lpOutContact->mIDA );
        lIdA.SetEntityId( lDummyTrafficEntityId );
        lpOutContact->mIDA = lIdA;
    }

    if ( lpInPotentialContact->muVolumeInstanceIdB.GetEntityIDOwner()
             == static_cast<u8>( BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE ) )
    {
        ::EntityId lRawTrafficEntityId;                               // see the block above
        const bool lbTrafficExists =
            lpVehicleManager->GetTrafficPhysicsEntityIDFromGlobalEntityID_Safe(
                GetVolumeInstanceEntityWord( lpInPotentialContact->muVolumeInstanceIdB ),
                &lRawTrafficEntityId );
        if ( !lbTrafficExists )
        {
            return false;                                             // 0x82628248 beq
        }

        CgsSceneManager::EntityId lDummyTrafficEntityId( lRawTrafficEntityId.muValue );
        lDummyTrafficEntityId.SetOwner(
            static_cast<u8>( BrnWorld::E_ENTITYTYPE_PROP_COLLISION_TRAFFIC ) );

        CgsPhysics::RigidBodyId lIdB( lpOutContact->mIDB );
        lIdB.SetEntityId( lDummyTrafficEntityId );
        lpOutContact->mIDB = lIdB;
    }

    // -----------------------------------------------------------------------------------------
    // (2) The prop friction/restitution seed -- 0x82628270..0x82628294.
    // -----------------------------------------------------------------------------------------
    lpOutContact->mStaticFriction  = mfStaticFriction;                // lfs 0x74 / stfs 0x40
    lpOutContact->mDynamicFriction = mfDynamicFriction;               // lfs 0x78 / stfs 0x44
    lpOutContact->mRestitution     = KF_PROP_RESTITUTION;             // flt_82F2A398 / stfs 0x48

    // -----------------------------------------------------------------------------------------
    // (3) Which side is the prop -- 0x8262829C..0x826282AC (DWARF `bool lbPropIsEntityA` :1355).
    // -----------------------------------------------------------------------------------------
    const bool lbPropIsEntityA =
        ( CgsPhysics::RigidBodyId( lpOutContact->mIDA ).GetEntityIDOwner()
              == static_cast<u8>( BrnWorld::E_ENTITYTYPE_PROP ) );

    // -----------------------------------------------------------------------------------------
    // (4) The mirrored prop-side resolution -- 0x826282B0..0x8262855C.
    //     DWARF locals :1300..:1306.
    // -----------------------------------------------------------------------------------------
    CgsPhysics::RigidBodyId lPropRigidBodyId(
        lbPropIsEntityA ? lpOutContact->mIDA : lpOutContact->mIDB );          // DWARF :1302
    const CgsPhysics::RigidBodyId lOtherRigidBodyId(
        lbPropIsEntityA ? lpOutContact->mIDB : lpOutContact->mIDA );          // DWARF :1303

    const PropEntityID lPropEntityId(                                          // DWARF :1301
        GetVolumeInstanceEntityWord( lbPropIsEntityA
                                         ? lpInPotentialContact->muVolumeInstanceIdA
                                         : lpInPotentialContact->muVolumeInstanceIdB ) );

    const u8 lu8OtherOwner = lOtherRigidBodyId.GetEntityIDOwner();

    // The prop's own contact-generation legs stamp their slot index into the per-primitive tag,
    // which AddContactResultsToQueue copies into muPolyTagA -- see THE POLY-TAG SEAM above. Only
    // a world contact carries it; anything else has to be searched for.
    const bool lbOtherIsWorld = ( lu8OtherOwner == static_cast<u8>( BrnWorld::E_ENTITYTYPE_WORLD ) );

    int32_t       liPhysicsIndex = KI_PROP_INDEX_NOT_FOUND;                    // DWARF :1300
    PropInstance* lpPropInstance = NULL;                                       // DWARF :1304

    if ( lPropEntityId.GetPartIndex() == 0u )
    {
        liPhysicsIndex = lbOtherIsWorld
                             ? static_cast<int32_t>( lpInPotentialContact->muPolyTagA )
                             : FindPropIndex( lPropEntityId );

        CGS_ASSERT( liPhysicsIndex < static_cast<int32_t>( KU_MAX_PHYSICAL_PROPS ),
                    "liPhysicsIndex < static_cast< int32_t > ( KU_MAX_PHYSICAL_PROPS )" );  // :1384

        if ( liPhysicsIndex == KI_PROP_INDEX_NOT_FOUND )
        {
            return false;                                             // 0x82628344 beq
        }
        if ( HasPropJustBeenRemoved( lPropEntityId, liPhysicsIndex ) )
        {
            return false;                                             // 0x82628360 bne
        }

        lpPropInstance = &mpaPropInstances[ liPhysicsIndex ];          // mulli 0x70 == the stride
    }
    else
    {
        liPhysicsIndex = lbOtherIsWorld
                             ? static_cast<int32_t>( lpInPotentialContact->muPolyTagA )
                             : FindPartIndex( lPropEntityId );

        CGS_ASSERT( liPhysicsIndex < static_cast<int32_t>( KU_MAX_PHYSICAL_PROP_PARTS ),
                    "liPhysicsIndex < static_cast< int32_t > ( KU_MAX_PHYSICAL_PROP_PARTS )" );  // :1408

        if ( liPhysicsIndex == KI_PROP_INDEX_NOT_FOUND )
        {
            return false;                                             // 0x826283C4 beq
        }
        if ( HasPartJustBeenRemoved( lPropEntityId, liPhysicsIndex ) )
        {
            return false;                                             // 0x826283E0 bne
        }
    }

    // The two contact points, PROP SIDE FIRST (v124 is always the prop's point, v126 the
    // other body's -- measured in both mirrored branches), and the out event's prop-side
    // user-id-B. DWARF :1305 / :1306.
    const Vector3 lPointOnProp           = lbPropIsEntityA ? lpInPotentialContact->mPointOnA
                                                           : lpInPotentialContact->mPointOnB;
    const Vector3 lPointOnOtherRigidBody = lbPropIsEntityA ? lpInPotentialContact->mPointOnB
                                                           : lpInPotentialContact->mPointOnA;
    {
        CgsPhysics::RigidBodyId lOutPropSideId(
            lbPropIsEntityA ? lpOutContact->mIDA : lpOutContact->mIDB );
        SetRigidBodyIdUserIDB( lOutPropSideId, static_cast<u16>( liPhysicsIndex ) );
        if ( lbPropIsEntityA ) { lpOutContact->mIDA = lOutPropSideId; }
        else                   { lpOutContact->mIDB = lOutPropSideId; }
    }

    // -----------------------------------------------------------------------------------------
    // (5) The jointed/static prop gate -- 0x82628560..0x82628728.
    // -----------------------------------------------------------------------------------------
    if ( lpPropInstance != NULL
         && ( lpPropInstance->IsJointed() || lpPropInstance->IsStatic() ) )   // lbz 0x6C / lbz 0x6D
    {
        if ( lbOtherEntityIsFrozen )
        {
            return false;                                             // 0x82628588 bne
        }

        if ( lbOtherIsWorld )
        {
            // ⭐ THE SMASH-GATE BREAK. DWARF block :1497/:1499.
            Vector3 lFlatNormal = lpInPotentialContact->mNormal;
            lFlatNormal.y = 0.0f;   // the DWARF spells this SetY(GetVecFloat_Zero()); this
                                    // tree's Vector3 has named lanes and no SetY -- same store.

            // `vmsum3fp128` is the 3-lane dot and it BROADCASTS, so the console's all-lanes
            // CR6[0] test below reduces exactly to this scalar compare.
            const f32 lvfPenetration =
                vpu::Dot( lFlatNormal, vpu::Subtract( lpInPotentialContact->mPointOnB,
                                                      lpInPotentialContact->mPointOnA ) );

            // gotcha 4: `vcmpgtfp.` + "all lanes true" -- an unordered compare DROPS the contact.
            if ( !( lvfPenetration > KVF_MAX_LEAN_PROP_WORLD_PENETRATION.x ) )
            {
                return false;                                         // 0x826285D8 beq
            }

            // GetJointIndex carries its own IsJointed() tripwire; the console additionally
            // inlines BitArray<15>::SetBit's index assert (baked CgsBitArray.h:222), which the
            // committed assert-free CgsBitArray owns rather than this body.
            mBreakPropJoints.SetBit(
                static_cast<u32>( lpPropInstance->GetJointIndex() ) );

            // ⚠️ The contact is dropped EITHER WAY: the SetBit falls straight into the console's
            // `li r3,0` return at 0x826286BC. A jointed prop resolves through the joint, never
            // through the simulation's contact solver.
            return false;
        }

        if ( lu8OtherOwner == static_cast<u8>( BrnWorld::E_ENTITYTYPE_PROP )
             && ( static_cast<u32>( lOtherRigidBodyId.GetEntityId() ) & 0x3FFu ) == 0u )
        {
            // A leaning/static prop may not lean on another leaning/static prop.
            // DWARF locals :1513 (liOtherPropIndex) / :1516 (lpOtherProp).
            // ⚠️ the part-index test above is the console's raw `clrlwi r11,r14,22` on the entity
            // word, taken BEFORE the PropEntityID is constructed; spelled with the same mask
            // rather than through PropEntityID::GetPartIndex() so the ORDER matches (that getter
            // runs an out-of-line AssertIsProp the console does not emit here).
            const PropEntityID lOtherEntityId(
                static_cast<u32>( lOtherRigidBodyId.GetEntityId() ) );
            const int32_t liOtherPropIndex = FindPropIndex( lOtherEntityId );
            if ( liOtherPropIndex == KI_PROP_INDEX_NOT_FOUND )
            {
                return false;                                         // 0x82628708 beq
            }

            const PropInstance* lpOtherProp = &mpaPropInstances[ liOtherPropIndex ];
            if ( lpOtherProp->IsJointed() )
            {
                return false;                                         // 0x82628720 bne
            }
            if ( lpOtherProp->IsStatic() )
            {
                return false;                                         // 0x8262872C bne
            }
        }
    }

    // -----------------------------------------------------------------------------------------
    // (6) The race-car leg -- 0x82628730..0x82628A00.
    // -----------------------------------------------------------------------------------------
    if ( lu8OtherOwner == static_cast<u8>( BrnWorld::E_ENTITYTYPE_RACECAR ) )
    {
        // flt_82001CC0 == 0.0f into both -- a prop/car contact carries no surface friction.
        lpOutContact->mStaticFriction  = 0.0f;                        // stfs 0x40
        lpOutContact->mDynamicFriction = 0.0f;                        // stfs 0x44

        // DWARF :1538. The console INLINES this accessor (the eight-slot maRaceCarEntityIDs
        // scan at +43584 then `mulli 0x1460 ; addi 0x740`); it is re-rolled into the named call.
        // ⚠️ NOT null-checked -- see the banner. The console's own deref is unguarded.
        BrnPhysics::Vehicle::RaceCarPhysics* lpRaceCar =
            lpVehicleManager->GetRaceCarPhysics( lOtherRigidBodyId );

        if ( lPropEntityId.GetPartIndex() == 0u )
        {
            // ---- whole prop ------------------------------------------------------------------
            // DWARF :1543..:1557. lpProp aliases lpPropInstance above (the console recomputes
            // &mpaPropInstances[liPhysicsIndex]); both locals are the DWARF's.
            PropInstance*         lpProp        = &mpaPropInstances[ liPhysicsIndex ];   // :1544
            const Matrix44Affine  lCarTransform = lpRaceCar->GetTransform();             // :1543
            const PropTypeData*   lpPropType    = mpPhysicsData->GetType( lpProp->GetTypeId() ); // :1545

            if ( !lpProp->IsStatic() || lpProp->IsJointed() )
            {
                RoutePropVsRaceCarContactToDummyCar( lbPropIsEntityA, lpOutContact );
            }

            SetRigidBodyIdUserIDB( lPropRigidBodyId, static_cast<u16>( liPhysicsIndex ) );

            // :1556 / :1557 -- flip the normal so it points at the car.
            // gotcha 4: the console is `vcmpgefp` then `vnot128`, so an unordered compare FLIPS.
            const f32 lfNormalDotToCar =
                vpu::Dot( lpInPotentialContact->mNormal,
                          vpu::Subtract( lCarTransform.wAxis, lPointOnOtherRigidBody ) );
            const bool    lNormalTowardsCar = !( lfNormalDotToCar >= 0.0f );
            const Vector3 lNormal           = lNormalTowardsCar
                                                  ? vpu::Negate( lpInPotentialContact->mNormal )
                                                  : lpInPotentialContact->mNormal;

            if ( !lpPropType->IsLamppost() )
            {
                ApplyAntiHerdingForce( lpSimInputBuffer,
                                       lpRaceCar,
                                       lPropRigidBodyId,
                                       lpProp->GetTransform().wAxis,          // lvx +0x30
                                       vpu::Splat( lpPropType->GetMass() ),   // lvlx +0x38 / vspltw
                                       lpProp->GetLinearVelocity(),           // lvx +0x40
                                       lpInPotentialContact->mNormal );       // the UNREAD 7th
            }

            if ( lpPropInstance->IsJointed() )
            {
                // DWARF :1575 -- the joint resolvers re-look-up the type through the instance.
                const PropTypeData* lpType =
                    mpPhysicsData->GetType( lpPropInstance->GetTypeId() );

                if ( lpType->GetJointType() == KU8_JOINT_TYPE_LEAN )
                {
                    HandleContactWithLeanProp( lpPropInstance, liPhysicsIndex, lpType, lpRaceCar,
                                               lNormal, lPointOnProp, lPointOnOtherRigidBody,
                                               lpOutContact, lbPropIsEntityA, lfTimeStep );
                }
                else
                {
                    CGS_ASSERT( lpType->GetJointType() == KU8_JOINT_TYPE_TILT,
                                "lpType->mu8JointType == BrnPhysics::Props::PropTypeData::E_TILT" ); // :1592
                    HandleContactWithTiltProp( lpPropInstance, liPhysicsIndex, lpType, lpRaceCar,
                                               lNormal, lPointOnProp, lPointOnOtherRigidBody,
                                               lpOutContact, lbPropIsEntityA, lfTimeStep );
                }
            }
            else
            {
                ApplyPropRaceCarCollisionImpulse( lpRaceCar, lpProp, lpPropType,
                                                  lNormal, lPointOnOtherRigidBody );
            }
        }
        else
        {
            // ---- a broken-off part ------------------------------------------------------------
            // DWARF :1613..:1618. The part index is searched on the PROP RIGID BODY's own entity
            // word (not on lPropEntityId) -- `srdi r11, r20, 32` at 0x8262894C.
            const PropEntityID lPartEntityId(
                static_cast<u32>( lPropRigidBodyId.GetEntityId() ) );
            const int32_t     liPartIndex = FindPartIndex( lPartEntityId );          // :1614
            PropPartInstance* lpPart      = &mpaPartInstances[ liPartIndex ];        // :1617

            CgsPhysics::RigidBodyId lPartRigidBodyId = lPropRigidBodyId;             // :1613
            SetRigidBodyIdUserIDB( lPartRigidBodyId, static_cast<u16>( liPartIndex ) );

            // The part's own type record: the PROP type's part table indexed by the part id.
            // ⚠️ The console walks it at its own 48-byte stride (`(id + id*2) << 4`) off the
            // console +0x40 slot; on the host both the slot and the stride are the compiler's
            // (PropPartTypeData is 64 bytes here) -- gotcha 1, reached by name.
            const PropTypeData* lpPropType = mpPhysicsData->GetType( lpPart->GetType() );
            const f32 lfMass = lpPropType->GetParts()[ lpPart->GetPartId() ].GetMass();  // :1618

            ApplyAntiHerdingForce( lpSimInputBuffer,
                                   lpRaceCar,
                                   lPartRigidBodyId,
                                   lpPart->GetPosition(),              // lvx +0x00
                                   vpu::Splat( lfMass ),               // lfs +0x20 / vspltw
                                   lpPart->GetLinearVelocity(),        // lvx +0x10
                                   lpInPotentialContact->mNormal );    // the UNREAD 7th

            RoutePropVsRaceCarContactToDummyCar( lbPropIsEntityA, lpOutContact );
        }
    }

    // -----------------------------------------------------------------------------------------
    // (7) The shared tail -- 0x82628A04..0x82628A80. Reachable with the prop as entity B only
    //     when lu8OtherOwner != PROP (see the banner), so writing mIDB is always the OTHER side.
    // -----------------------------------------------------------------------------------------
    if ( lu8OtherOwner == static_cast<u8>( BrnWorld::E_ENTITYTYPE_PROP ) )
    {
        const u32          luOtherEntityWord = static_cast<u32>( lOtherRigidBodyId.GetEntityId() );
        const PropEntityID lOtherEntityId( luOtherEntityWord );

        const int32_t liOtherIndex = ( ( luOtherEntityWord & 0x3FFu ) == 0u )
                                         ? FindPropIndex( lOtherEntityId )   // DWARF :1639
                                         : FindPartIndex( lOtherEntityId );  // DWARF :1651
        if ( liOtherIndex == KI_PROP_INDEX_NOT_FOUND )
        {
            return false;                                             // 0x82628A50 beq
        }

        CgsPhysics::RigidBodyId lIdB( lpOutContact->mIDB );
        SetRigidBodyIdUserIDB( lIdB, static_cast<u16>( liOtherIndex ) );
        lpOutContact->mIDB = lIdB;
    }

    return true;                                                      // 0x82628A6C li r3,1
}

}
}

// ============================================================================
// FOLDED FROM PropManager_wQ4_02.cpp (wave Q4) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================
// GameSource/Physics/PropManager/PropManager_wQ4_02.cpp
//
// BrnPhysics::Props::PropManager -- breakable-props / smash-gates wave Q4 (SEAM WAVE),
// partfile 02. Reconstructed from BURNOUT_X360_ARTIST.XEX:
//
//   ProcessRemovePropInstanceEvents() @ 0x82627778   (39 instructions,
//                                                     0x82627778..0x82627810 --
//                                                     COUNTED: (0x82627810-0x82627778)/4 + 1 == 39)
//
// A slice of the TU's own BrnPropManager.cpp, split out per this wave's partfile convention.
//
// ==========================================================================================
// WHY IT IS HERE, AND WHY IT IS THIS FUNCTION AND NOT ANOTHER
//
// Wave Q4's second job is the MOUNT CLOSURE for the PropManager partfiles. Of the fourteen
// PropManager symbols the mount leaves with no body anywhere in the tree, this was the ONE that
// was neither parked-behind-a-foreign-header nor large: 39 instructions, and every callee it has
// is already real. It is the last of ProcessInputsPreScene's SIX drains to have no body
// (ProcessRemove/AddPartInstanceEvents are in PropManager_wQ_02.cpp, ProcessAddPropInstanceEvents
// in PropManager_wQ2_06.cpp, UpdateJointedProps in PropManager_wQ2_05.cpp), so landing it turns
// ProcessInputsPreScene from five-sixths real into five-sixths-plus-one, leaving exactly one --
// RemoveAllPropsAndParts @0x8260F010, 331 instructions -- for a gate. That count matters to the
// conductor's decision, so it is measured, not guessed: see the owner record.
//
// ⭐ THE PER-ADDRESS JSON FOR THIS FUNCTION DOES NOT EXIST
// (.ida-exports/BURNOUT_X360_ARTIST.XEX/0x82627778.json is absent) -- AGENTS.md gotcha 6, "a
// missing JSON is often only an export-run gap". It was: headless IDA 9.3 over a PRIVATE COPY of
// the .i64 (scratchpad/waveQ4/ida/wq4.i64, never the original) resolves 0x82627778 to a real
// function, fully named `BrnPhysics::Props::PropManager::ProcessRemovePropInstanceEvents`, with
// the 39-instruction listing this file is reconstructed from
// (scratchpad/waveQ4/ida/asm_82627778.txt). The identity ledger has no entry for this name at
// all, which is why it never reached a reconstruction wave.
//
// ==========================================================================================
// THE 39 INSTRUCTIONS. Register map from the prologue (0x82627784..0x82627794):
//   r3  -> r27 = this
//   r4  -> r24 = &lpInput->mRemovePropQueue     (`addi r24, r4, 0x1F60`, see CONSOLE OFFSET below)
//   r5  -> r26 = lpSceneInput
//   r6  -> r25 = lpSimModuleInputBuffer
//   r31 = luEventIndex (li r31,0)
//   r28 = luQueueSize  (`lwz r28, 8(r24)` == BaseEventQueue<T>::miLength == GetLength(),
//                       READ ONCE BEFORE THE LOOP, with the empty-queue early-out at 0x826277A0)
//
// Body, in emission order:
//   0x826277B4..0x826277BC  lpEvent = lpQueue->GetEvent(luEventIndex)   (the `bl` symbol is
//                           TRUNCATED in the export to `BrnPhysics__Pr` -- gotcha 6 again; the
//                           DWARF names the callee exactly:
//                           CgsModule::BaseEventQueue<RemovePhysicalPropEvent>::GetEvent)
//   0x826277C0              liPropIndex = lpEvent->miPhysicalIndex      (`lwz r30, 4(r3)`)
//   0x826277C4              lEntityId   = lpEvent->mEntityId            (`lwz r29, 0(r3)`)
//   0x826277C8..0x826277E4  ONE assert, SIGNED compare against -1, baked line 0x2B1 == 689,
//                           message "liPropIndex != KI_PROP_INDEX_NOT_FOUND". ⚠️ NON-GATING --
//                           `bne` skips only the assert call; the console falls straight into
//                           RemoveProp either way, so a fired assert removes nothing extra and
//                           skips nothing.
//   0x826277E8..0x826277FC  RemoveProp(lEntityId, liPropIndex, lpSceneInput, lpSimModuleInputBuffer)
//   0x82627800..0x82627808  ++luEventIndex; `cmplw` == UNSIGNED loop-back test
//
// ⚠️ THE PART TWIN HAS TWO ASSERTS AND THIS ONE HAS ONE. PropManager_wQ_02.cpp's
// ProcessRemovePartInstanceEvents carries the same message twice (baked lines 735 and 737); this
// function's listing contains exactly one BeginAssert/FireAssert/EndAssert triple. Checked
// deliberately, because the two drains are otherwise instruction-for-instruction the same shape
// and it would have been easy to copy the twin's pair across.
//
// ---- CONSOLE VALUES ARE EVIDENCE ONLY (gotcha 1) ------------------------------------------
// `addi r24, r4, 0x1F60` is the CONSOLE offset of PropInputInterface::mRemovePropQueue, and
// `lwz r,8(queue)` is the console offset of BaseEventQueue<T>::miLength. Both are meaningless on
// the LLP64 host and are recorded here as evidence only; the source-level route is the DWARF's
// own accessor (BrnPropInputInterface.h:108, landed as GetRemovePhysicalPropQueue() const) and
// GetLength(). The host offset happens to agree for this queue -- BrnPropInputInterface.h
// documents mRemovePropQueue at +0x1F60 -- which is a cross-check on the accessor being the right
// one, not a licence to use the number.
//
// ---- THE DWARF, WHICH SETTLES THE LOCALS --------------------------------------------------
// dwarfdump GameSource/Physics/PropManager/BrnPropManager.cpp:844-871 (source :665) declares
// exactly seven locals -- luEventIndex (:666), luQueueSize (:667), RigidBodyId lRigidBodyId
// (:668), lpQueue (:669), lpEvent (:670), lEntityId (:671), liPropIndex (:672) -- and its callee
// list is GetLength / GetEvent. Every name below is one of those.
// ⚠️ `RigidBodyId lRigidBodyId` IS DECLARED BY THE DWARF AND NEVER USED BY THE ARTIST EMISSION
// (no `std`/`ld` of an 8-byte handle anywhere in the 39 instructions, and RemoveProp takes none).
// It is NOT written below -- an unused local is not a side effect, and materialising one would be
// inventing a store the binary does not have. Recorded here so the divergence from the DWARF is
// deliberate and visible. The PART twin's banner records the identical situation.
//
// ---- GOTCHA SWEEP -------------------------------------------------------------------------
// gotcha 1 console literals: the two offsets above, both reached by name instead.
// gotcha 2 embedded sub-objects: no site -- this body performs no store at all.
// gotcha 3 PPC float args: none; RemoveProp takes no float or vector.
// gotcha 4 NaN polarity: no float compare in the function.
// gotcha 5 rodata tables: none.
// gotcha 12 gate-green != link-green: every callee here has a real body in the tree --
//           BaseEventQueue<RemovePhysicalPropEvent>::{GetLength,GetEvent} (header inlines) and
//           PropManager::RemoveProp (PropManager_wQ2_04.cpp). This partfile adds NO unresolved
//           external of its own; that is the whole point of landing it.
// ==========================================================================================


namespace BrnPhysics
{
namespace Props
{

// =============================================================================================
// ProcessRemovePropInstanceEvents -- X360 0x82627778, 39 instructions.
// DecFIGS: dwarfdump BrnPropManager.cpp:844 (scope, source :665). Parameter names are the
// DWARF's and match the committed declaration in BrnPropManager.h.
// =============================================================================================
void
PropManager::ProcessRemovePropInstanceEvents(
    const PropInputInterface*                                lpInput,
    CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpSceneInput,
    CgsPhysics::PhysicsSimulationIO::InputBuffer*            lpSimModuleInputBuffer )
{
    const PropInputInterface::RemovePropEventQueue* lpQueue =
        &lpInput->GetRemovePhysicalPropQueue();                          // DWARF :669

    // 0x82627798 -- read ONCE, before the loop, with the empty-queue early-out at 0x826277A0.
    const u32 luQueueSize = static_cast<u32>( lpQueue->GetLength() );    // DWARF :667

    for ( u32 luEventIndex = 0; luEventIndex < luQueueSize; ++luEventIndex )   // DWARF :666
    {
        const RemovePhysicalPropEvent* lpEvent =
            &lpQueue->GetEvent( static_cast<s32>( luEventIndex ) );       // DWARF :670

        const BrnWorld::PropEntityID lEntityId   = lpEvent->mEntityId;        // DWARF :671
        const s32                    liPropIndex = lpEvent->miPhysicalIndex;  // DWARF :672

        // 0x826277C8 -- SIGNED compare (`cmpwi cr6, r30, -1`). NON-GATING: the console falls
        // straight through into RemoveProp whether or not this fires.
        CGS_ASSERT( liPropIndex != KI_PROP_INDEX_NOT_FOUND,
                    "liPropIndex != KI_PROP_INDEX_NOT_FOUND" );           // :689 (baked 0x2B1)

        RemoveProp( lEntityId,
                    static_cast<u32>( liPropIndex ),
                    lpSceneInput,
                    lpSimModuleInputBuffer );
    }
}

}
}

// ============================================================================
// FOLDED FROM PropManager_wQ4_03.cpp (wave Q4) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================
// =========================================================================================
// PARKED (ROUND 2) -- scratchpad/waveQ2/parked/PropManager_09_GetPropInertia_GetPartInertia.cpp
//
// waveQ (breakable props, 2026-08-18) round-2 lander, BOTH functions:
//     BrnPhysics::Props::PropManager::GetPropInertia @ 0x82612640   (289 instructions)
//     BrnPhysics::Props::PropManager::GetPartInertia @ 0x82612AC8   (272 instructions)
// (COUNTED, not estimated -- I counted the exported instruction lines and cross-checked the
//  address arithmetic: (0x82612AC0-0x82612640)/4+1 == 289, (0x82612F04-0x82612AC8)/4+1 == 272.
//  The ROUND-1 park banner said "~180"/"~170"; that was wrong.)
//
// This is the intended drop-in for
//     b5-decomp/src/GameSource/Physics/PropManager/PropManager_wQ2_09.cpp
// It is parked, not landed, because of ONE remaining blocker that is not this TU's to fix.
//
// -----------------------------------------------------------------------------------------
// STATE OF THE TWO ROUND-1 BLOCKERS, RE-MEASURED 2026-08-18 BY THIS LANDER
// -----------------------------------------------------------------------------------------
// BLOCKER 1a -- CLOSED. `rw::collision::Volume::GetBBox` now exists, non-virtual, at
//   b5-decomp/src/SDKs/EATech/rwcollision/volume_debug_access.h:257
//       RwBool GetBBox(const Matrix44Affine* lpTransform, RwBool lbTight, AABBox& lrBBox) const
//   dispatching through the rwcollision per-TYPE descriptor at `volume+0x40`, function pointer
//   at descriptor+0x04. (Landed by the waveQ2 rwcollision owner; scratchpad/waveQ2/rwvol.owner.md
//   §1.)  Do NOT re-request it. MEASURED closed here: zero diagnostics in the probes below
//   mention GetBBox, and the C2664 in probe_land_noaabbox.cpp quotes the landed signature
//   verbatim out of volume_debug_access.h(257).
//
// BLOCKER 1b -- STILL OPEN AS OF 2026-08-18, and it is the ONLY thing between this file and
//   the tree. `rw::collision::AABBox` cannot be NAMED as a complete type in any TU that also
//   names the game's `Vector3`, because two definitions of `rw::math::vpu::Vector3` exist:
//       b5-decomp/vendor/renderware/include/rw/math/vpu/types.h:24
//           struct alignas(16) Vector3 { float x, y, z, w; }   <- what BrnCommonTypes.h pulls,
//                                                                 i.e. what `Vector3` IS
//       b5-decomp/src/SDKs/EATech/include/rw/math/vpu/vector3.h:26
//           class Vector3 { VectorIntrinsic mV; }              <- what AABBox.hpp:4 pulls
//   Both bodies need TWO AABBox OBJECTS BY VALUE (`lAccumulatedAABBox` and `lVolumeAABBox` --
//   the DWARF's own local names; the second is the 32-byte out-parameter GetBBox writes), so
//   a forward declaration is not enough and no include ordering avoids it.
//
//   ⚠️ THE EXACT MISSING LINE, as the compiler names it:
//       b5-decomp/src/vendor/renderware/collision/AABBox.hpp:4
//           #include "SDKs/EATech/include/rw/math/vpu/vector3.h"
//       must become the vendor POD home
//           #include "rw/math/vpu/types.h"
//       (and :5's `matrix44.h` with it -- `AABBox::Transform` names `math::vpu::Matrix44Affine`).
//
//   ⚠️ BUT DO NOT LAND THAT ONE LINE ALONE. The waveQ2 rwcollision owner RAN exactly that
//   experiment and MEASURED the fallout: 8 of the 11 TUs that reach AABBox.hpp then fail, in
//   three independent ways (`.mV.mafLane[...]` member access; a missing 3-arg Vector3 ctor;
//   a missing `VectorIntrinsic`), plus an `AggregateVolume.hpp` path that pulls the EATech
//   `matrix44.h` independently so the clash does not even leave the collision family. The
//   complete costed work list is rwvol.owner.md §4.5 -- six items, one of which
//   (`GameShared/GameClasses/SceneManager/CgsAABBoxBuilder.cpp`, 10 sites) sits outside the
//   rwcollision ownership and needs its own grant. THAT is the header_request: the §4.5
//   collapse, not a one-line repoint. (The hazard is already recorded in the tree at
//   GameSource/Physics/PropManager/BrnPropManager.cpp:149-157.)
//
// -----------------------------------------------------------------------------------------
// THE BLOCK IS EXACTLY THAT AND NOTHING ELSE -- MEASURED TODAY, BOTH BOUNDS
// (probes in scratchpad/waveQ2/probe_wQ2_09/, re-run by this lander)
// -----------------------------------------------------------------------------------------
//   probe_land.cpp        = this file's CODE verbatim (banner trimmed) -> STATUS=fail, first
//                           diagnostic `C2011 "rw::math::vpu::Vector3": Typneudefinition`,
//                           SDKs/EATech/include/rw/math/vpu/vector3.h(26) against
//                           vendor/renderware/include/rw/math/vpu/types.h(24). ZERO
//                           diagnostics mention GetBBox or HACKShouldMoveComOffset.
//   probe_land_podshim.cpp = the same code with ONLY the AABBox.hpp include swapped for a
//                           probe-local `class AABBox { public: Vector3 mMin; Vector3 mMax; };`
//                           over the VENDOR POD (i.e. the post-§4.5 world) -> STATUS=pass.
//                           So every other declaration this file needs is landed and binding:
//                           Volume::GetBBox, PropTypeData::HACKShouldMoveComOffset,
//                           Matrix44Affine::SetIdentity, Get{NumberOfVolumes,CollisionVolume,
//                           Mass} on BOTH type records, and K_LAMPOST_INERTIA_BOX.
//   probe_land_noaabbox.cpp = the same code with the AABBox include simply deleted ->
//                           STATUS=fail with exactly two `C2079 undefined class
//                           "rw::collision::AABBox"` (the two locals) and the one C2664 they
//                           cause on GetBBox's third argument (plus its cascade). Nothing
//                           else is missing.
//
// =========================================================================================
// SOURCE OF TRUTH -- WHAT IS MEASURED AND WHAT IS INFERRED
// (Raw asm read by this lander straight out of
//  .ida-exports/BURNOUT_X360_ARTIST.XEX/0x82612640.json and 0x82612AC8.json, both walked in
//  full; dumps kept at scratchpad/waveQ2/probe_wQ2_09/raw_0x8261*.txt.)
// =========================================================================================
//
// MEASURED -- calling convention (from the ASM, not the pseudocode):
//   * Hidden-pointer return. `mr r29,r5` @0x82612664 parks lpType, `mr r25,r3` @0x8261266C
//     parks &result; the tail is `mr r3,r25` @0x82612A6C + `stvx128 v0,r0,r25` @0x82612AA0
//     (GetPartInertia: `mr r29,r5` @0x82612AEC, `mr r25,r3` @0x82612AF4, `mr r3,r25`
//     @0x82612DD8, `stvx128 v0,r0,r25` @0x82612EE4).
//   * r4 == `this` and is NEVER READ by either body -- the only writes to r4 are
//     `addi r4,r1,var_B0` (the &lIdentity argument). Both are effectively static helpers.
//     Hex-Rays renders both as `int f(int,int,int)`; that is the hidden-pointer artefact.
//
// MEASURED -- the volume walk:
//   * Volume count is a u8: PropTypeData +0x5E (`lbz r11,0x5E(r29)` @0x82612678 for the
//     pre-test AND `lbz r10,0x5E(r29)` @0x826128F8 AGAIN at the bottom of every iteration);
//     PropPartTypeData +0x2C (@0x82612B00 and @0x82612D80). The induction variable is
//     re-masked to 8 bits every iteration (`addi r10,r30,1` @0x826128E4 ->
//     `clrlwi r30,r11,24` @0x826128FC -> `cmplw cr6,r30,r10` @0x82612900).
//     -> the loop below therefore calls GetNumberOfVolumes() IN THE LOOP CONDITION and keeps
//        the counter a `u8`: that is what the console does. (Round-1 NIT: the round-1 park
//        hoisted the count into a loop-invariant local. Not hoisted here.)
//   * Volume run: PropTypeData +0x3C (`lwz r10,0x3C(r29)` @0x82612710);
//     PropPartTypeData +0x24 (`lwz r10,0x24(r29)` @0x82612B98). Stride 96
//     (`slwi r11,r30,1 ; add r11,r30,r11 ; slwi r11,r11,5` @0x8261270C/0x82612718/0x8261272C)
//     -- the size of the SERIALISED rw::collision::Volume record. ⚠️ CONSOLE VALUE,
//     DOCUMENTATION ONLY (AGENTS gotcha 1): the code below indexes through the committed
//     GetCollisionVolume(i) accessor and never spells 96 (nor 0x5E/0x3C/0x2C/0x24/0x38/0x20).
//   * GetBBox dispatch: `lwz r11,0x40(r3) ; lwz r11,4(r11) ; mtctr ; bctrl` at
//     0x82612750..0x8261275C (GetPartInertia 0x82612BD8..0x82612BE4), with r3 = the volume,
//     r4 = &lIdentity, r5 = 1 (`li r5,1` @0x8261271C; GetPartInertia @0x82612BA4),
//     r6 = &lVolumeAABBox. The RwBool result in r3 is NOT tested by either caller -- do not
//     add a check.
//     ⚠️ `volume+0x40` is the rwcollision per-TYPE DESCRIPTOR pointer (`maType`), NOT a C++
//     vptr; the function pointer is at descriptor+0x04. GetBBox MUST NOT be virtual --
//     `sizeof(rw::collision::Volume) == 96` is static_asserted (BrnPhysicsPropTypeData.h) and
//     a vptr would shift every field of a serialised wire record.
//     ⚠️ WHICH Volume: the 96-byte serialised record in
//     `SDKs/EATech/rwcollision/volume_debug_access.h` -- the one PropTypeData /
//     PropPartTypeData actually hold. The unrelated 128-byte `rw::collision::Volume`
//     placeholder in `vendor/renderware/collision/CollisionVolume.hpp` is a separate
//     pre-existing fork and has nothing to do with this call.
//   * The stack identity matrix is rebuilt EVERY iteration from four rows: three rdata rows
//     loaded via `w__math__vpu__detail__gIVector` (address formed @0x826126EC, loaded
//     @0x82612708), `unk_82181510` (@0x826126F4/@0x82612728) and `unk_82181520`
//     (@0x826126FC/@0x82612740), plus a `vspltisw128 v125,0` ZERO row
//     (`stvx128 v125,r0,r9` @0x8261274C). MEASURED here: the four stores and their order.
//     CROSS-CITED, not re-dumped by me: those three rdata rows decode to {1,0,0,0} /
//     {0,1,0,0} / {0,0,1,0} (already decoded in
//     GameSource/Director/Camera/Utils/CameraUtils.cpp:128-130). With a zero fourth row that
//     is exactly the committed `Matrix44Affine::SetIdentity()`, whose wAxis is {0,0,0,0} --
//     checked against vendor/renderware/include/rw/math/vpu/types.h:72-76, not assumed.
//     (`w__math__vpu__detail__gIVector` is an IDA-TRUNCATED symbol, AGENTS gotcha 6.)
//
// MEASURED -- the accumulate. All SIX per-lane selects walked individually by this lander,
// with the stack pointer table decoded first:
//     var_120 = the accumulator MAX row (v127)      var_130 = the accumulator MIN row (v126)
//     var_D0  = lVolumeAABBox.mMax (AABBox +0x10)   var_E0  = lVolumeAABBox.mMin (AABBox +0x00)
//   The twelve `stw` at 0x82612688..0x826126E8 build six POINTER PAIRS, and every pair is
//   {&volume row, &accumulator row}:
//     x-max var_100=&var_D0 / var_E4=&var_120     x-min var_FC=&var_E0 / var_EC=&var_130
//     y-max var_F0 =&var_D0 / var_F8=&var_120     y-min var_F4=&var_E0 / var_110=&var_130
//     z-max var_104=&var_D0 / var_E8=&var_120     z-min var_108=&var_E0 / var_10C=&var_130
//   Each lane then: splat both candidates, `vcmpgtfp.`, `mfocrf`+`extrwi`+`bne` to pick one
//   POINTER, `lvx128` the whole selected row back, and `vrlimi128` one lane into the
//   accumulator (masks 8/4/2 == x/y/z). That pointer-select shape is why the DWARF names the
//   source helpers Max/Min<VecFloatRef{X,Y,Z}> -- the REFERENCE-returning per-axis selects.
//   * BOTH accumulator rows start at ZERO: `vspltisw128 v125,0` @0x82612668 then
//     `vmr128 v126,v125` @0x82612674 and `vmr128 v127,v125` @0x8261267C. NOT +/-FLT_MAX.
//     Load-bearing: a type whose volumes all sit off the origin still accumulates a box that
//     contains the origin.
//   * MAX lanes -- TRUE selects the ACCUMULATOR pointer in all three:
//       x @0x82612780 `vcmpgtfp. v0,v12,v0`   v12=accum.max.x, v0 =vol.max.x  -> bne -> var_E4
//       y @0x826127C4 `vcmpgtfp. v12,v11,v12` v11=accum.max.y, v12=vol.max.y  -> bne -> var_F8
//       z @0x82612800 `vcmpgtfp. v13,v12,v13` v12=accum.max.z, v13=vol.max.z  -> bne -> var_E8
//     i.e. `(accum > vol) ? accum : vol`.
//   * MIN lanes -- TRUE also selects the ACCUMULATOR pointer, with the operands the other way:
//       x @0x82612848 `vcmpgtfp. v0,v11,v12`  v11=vol.min.x, v12=accum.min.x -> bne -> var_EC
//       y @0x82612888 `vcmpgtfp. v12,v12,v11` v12=vol.min.y, v11=accum.min.y -> bne -> var_110
//       z @0x826128C4 `vcmpgtfp. v13,v13,v12` v13=vol.min.z, v12=accum.min.z -> bne -> var_10C
//     i.e. `(vol > accum) ? accum : vol`.
//   * `vcmpgtfp` is an ORDERED greater-than: FALSE when either operand is a NaN, so the
//     branch then takes the OTHER pointer. Writing these as `(a > b) ? .. : ..` with the
//     operands in the asm's order reproduces the tie/NaN winner exactly; a library Max/Min or
//     an `fsel`-shaped clamp would not (PPC NaN polarity, AGENTS gotcha 4).
//   * (GetPartInertia's six twins are at 0x82612C08 / 0x82612C4C / 0x82612C88 and
//     0x82612CD0 / 0x82612D10 / 0x82612D4C, same operand order lane for lane.)
//
// MEASURED -- the dimension fold:
//   * `vspltisw v0,-1` @0x82612910 then `vslw v0,v0,v0` (== 0x80000000), then `vandc` on BOTH
//     corners @0x8261294C..0x82612978, then `vmaxfp` @0x8261295C/0x82612970/0x82612980:
//       x: vandc v13(accum.min.x) / vandc v12(accum.max.x) -> vmaxfp v13,v12,v13
//       y: vandc v12(accum.min.y) / vandc v11(accum.max.y) -> vmaxfp v12,v11,v12
//       z: vandc v11(accum.min.z) / vandc v0 (accum.max.z) -> vmaxfp v0 ,v0 ,v11
//     That is `d_i = Max( |min_i| , |max_i| )` -- a sign-bit clear, i.e. fabsf, which also
//     keeps a NaN a NaN where `x < 0 ? -x : x` would not.
//   * THERE IS NO SUBTRACTION. I grepped both full dumps: ZERO `vsubfp` in either body. So
//     d_i is a HALF-EXTENT about the ORIGIN -- not `max - min`, and not about the box centre.
//     (GetPartInertia twin: 0x82612DB8..0x82612E44.)
//
// MEASURED -- the fold and the mass:
//   * The scale is the rodata float flt_820065E0, loaded @0x826129F0, stored into THREE
//     separate stack slots (@0x826129F4/0x82612A0C/0x82612A14) whose other three lanes are
//     zeroed with `stw r31` and then splatted lane-0 into three registers -- i.e. the DWARF's
//     three distinct `operator/` calls, the source's `/ 3.0f` folded to a multiply.
//     Its VALUE, 0.33333334f (0x3EAAAAAB) == 1/3 and NOT 1/12, is NOT re-measured by me: it
//     is taken from the waveQ2 rwcollision owner's headless-IDA dump of the image
//     (rwvol.owner.md §3) and from three committed TUs that already decode the same address
//     (vendor/renderware/physics/FatBoxInertia.cpp, .../audio/core/CpuLoadBalancer.cpp,
//     GameSource/World/ShadowMap/BrnShadowMap.cpp).
//   * Self-check, and it is a strong one: 1/3 with a HALF-extent is the same physics as 1/12
//     with a FULL extent -- (m/12)(2h)^2 == (m/3)h^2. The abs/max reading of the dimensions
//     and the 1/3 reading of the constant confirm each other. BrnPropManager.h:386-407 now
//     carries both halves; do not "restore" either one alone.
//   * Squares @0x826129E4/0x826129EC/0x826129FC from the three splatted lanes of lDims, then
//     pairwise sums @0x82612A40/0x82612A50/0x82612A5C:
//       v6  = d.y^2 + d.z^2 -> multiplied by K @0x82612A7C -> lane x (`vrlimi128` mask 8)
//       v12 = d.x^2 + d.z^2 -> multiplied by K @0x82612A84 -> lane y (mask 4)
//       v0  = d.y^2 + d.x^2 -> multiplied by K @0x82612A8C -> lane z (mask 2)
//     Lane->axis read off the `vrlimi128` masks @0x82612A90/0x82612A94/0x82612A98
//     (GetPartInertia 0x82612ED0/0x82612ED8/0x82612EDC).
//   * Mass: `lfs f0,0x38(r29)` @0x82612A28 (PropTypeData::mfMass) and `lfs f0,0x20(r29)`
//     @0x82612E38 (PropPartTypeData::mfMass); each is stored to var_C0
//     (@0x82612A48 / @0x82612E7C), reloaded (@0x82612A80 / @0x82612EA4), splatted
//     (`vspltw v7,v7,0` @0x82612A88 / @0x82612EB0) and multiplied into ALL FOUR lanes
//     (`vmulfp128 v0,v11,v7` @0x82612A9C / `vmulfp128 v0,v9,v7` @0x82612EE0). Reached below
//     through the committed GetMass() on each record.
//
// MEASURED -- GetPropInertia ONLY, the substitution:
//   * `lwz r11,0x58(r29)` @0x82612924 (muSceneUriId) is compared against 0x6894C (immediate
//     built @0x8261291C/0x8261292C, compare @0x82612948) and, on a miss, against 0x68964
//     (@0x82612998/0x8261299C, compare @0x826129A0). The two arms materialise a 1/0 into r11
//     (@0x826129AC / @0x826129A4), truncate it `clrlwi r11,r11,24` @0x826129B0 and test it
//     @0x826129B4 -- the shape of an inlined bool-returning accessor, not a bare `||`.
//   * On a hit, `lvx128 v0,r0,r11` @0x826129C4 loads unk_82FB9420 (== K_LAMPOST_INERTIA_BOX)
//     straight into the register holding lDims, i.e. the accumulated dimensions are DISCARDED
//     and replaced WHOLESALE before the fold.
//   * That two-id predicate is exactly `PropTypeData::HACKShouldMoveComOffset()`
//     (DWARF BrnPhysicsPropTypeData.h:110; the DWARF places the call in GetPropInertia's outer
//     block at BrnPropManager.cpp:277). It is now a landed header inline at
//     BrnPhysicsPropTypeData.h:222 and is CALLED below rather than re-spelled inline -- the
//     two magic ids no longer appear in this file at all.
//     It is deliberately NARROWER than `IsLamppost()` @0x822A1A00, which tests EIGHT ids --
//     do not unify them.
//   * ⚠️ K_LAMPOST_INERTIA_BOX reads ALL-ZERO out of the shipped image (BrnPropManager.cpp:244
//     defines it zero for exactly that reason; independently re-dumped by the rwcollision
//     owner, rwvol.owner.md §3), so on the console these two prop types get a ZERO inertia
//     here. Faithful, not a bug in this body.
//   * GetPartInertia has NO such branch: I grepped its whole dump -- no `0x58(r29)` load, no
//     0x6894C/0x68964 immediate anywhere, no unk_82FB94xx reference. Parts are never
//     substituted.
//
// INFERRED -- called out so no later sweep mistakes any of it for a measurement:
//   * The returned Vector3's W lane. The console splices lanes x/y/z into a register loaded
//     from an UNWRITTEN stack slot. GetPropInertia: `lvx128 v11,r0,r7` with r7 == var_C0
//     @0x82612A44, and the mass `stfs f0,var_C0(r1)` lands @0x82612A48 -- ONE instruction
//     later (the ROUND-1 park said "four instructions"; that was wrong -- it is four BYTES).
//     GetPartInertia is the same pattern two instructions apart: `lvx128 v9,r0,r10` with
//     r10 == var_C0 (set @0x82612E68) @0x82612E74 vs `stfs f0,var_C0(r1)` @0x82612E7C.
//     The same stale slot also seeds lDims' W lane (`lvx128 v10,r0,r9` with r9 == var_C0 set
//     @0x82612914, load @0x82612944; GetPartInertia r8 == var_C0 set @0x82612D9C, load
//     @0x82612DCC). So W is stack garbage times the mass -- genuinely undefined in the
//     shipped build. Zeroed below rather than left indeterminate: a deliberate, documented
//     divergence on a lane nothing reads (AddPropToSim @0x826274D8 scales the result by
//     KVF_INERTIA_SCALE and posts it as a rigid-body inertia, whose W the simulation ignores).
//   * De-optimisations, per AGENTS.md: the six unrolled per-lane selects are re-rolled into
//     one axis loop, and the per-iteration identity matrix is built with SetIdentity() instead
//     of three rdata row loads plus a zero row. Note the DWARF shows the SOURCE was itself
//     per-axis (three distinct Max<VecFloatRef{X,Y,Z}> instantiations), so the loop re-rolls
//     the source's own hand-unrolling as well as the compiler's.
//   * The outlining of `AccumulateVolumeHalfExtents` / `FoldBoxInertia` is a PRESENTATION
//     choice -- the two emissions are instruction-for-instruction identical across that span
//     and the original almost certainly had it inline in both.
//   * ⚠️ EVERY LOCAL HELPER AND CONSTANT IDENTIFIER IN THIS FILE IS AUTHORED, NOT RECOVERED:
//     MaxLane, MinLane, MaxFp, Lane, KU_AXIS_COUNT, KF_BOX_INERTIA_SCALE,
//     AccumulateVolumeHalfExtents, FoldBoxInertia. The DWARF's OWN names for the helpers the
//     compiler folded here are Max/Min<VecFloatRef{X,Y,Z}>, Abs<VectorAxis{X,Y,Z}>,
//     Max<VecFloat>, operator/, operator*, operator+, operator*=, GetVector3_Zero and
//     GetMatrix44Affine_Identity -- all of which live in the EATech Vector3 vocabulary and are
//     unusable here until blocker 1b is collapsed. When it is, revisit this file and replace
//     the authored helpers with the real ones.
//
// MEASURED -- from the DecFIGS DWARF for this exact .cpp
// (references/DecFIGS/dwarfdump/GameSource/Physics/PropManager/BrnPropManager.cpp:148-352):
//   * Local names + source lines. GetPropInertia: lAccumulatedAABBox (:2681), lu8Vol (:2687),
//     lVolumeAABBox (:2690), lpVolume (:2692), lIdentity (:2694), lMax (:2699), lMin (:2700),
//     lDims (:2712), lInertia (:2723). GetPartInertia: the same nine at
//     :2743/:2750/:2753/:2755/:2757/:2762/:2763/:2775/:2780. Every local below carries its
//     DWARF name.
//   * The accumulator is ONE AABBox (lAccumulatedAABBox), not two loose Vector3s -- which is
//     why blocker 1b bites twice per body.
//   * `lpVolume` is spelled `VolRef::Volume*`, and volume.h:39 typedefs
//     `VolRef::Volume == rw::collision::Volume` -- i.e. the 96-byte SDK record.
//
// NO CONSOLE LITERAL IS USED AS A HOST VALUE ANYWHERE BELOW (AGENTS gotcha 1). Every X360
// offset, stride and record size quoted above lives in a comment; the C++ reaches its data by
// member name and by the committed accessors only. The ONLY numeric literal in the code is
// the 1/3 rodata float (and the axis count 3); the two graphics ids now live inside the
// header's HACKShouldMoveComOffset.
// =========================================================================================

// Include set: rwvol.owner.md §5, verbatim.
                                                                  //   (this already pulls
                                                                  //    volume_debug_access.h, so
                                                                  //    Volume::GetBBox is in scope)
// NOT included: vendor/renderware/collision/AABBox.hpp. It pulls SDKs/EATech/include/rw/math/vpu/
// vector3.h (the SDK Vector3 CLASS) into namespace rw::math::vpu, which this TU already has as
// the vendor 4-lane POD via BrnCommonTypes.h -- the two cannot coexist in one TU (measured,
// rwvol.owner.md s4.5; 8 of 11 AABBox.hpp consumers break under the naive repoint). What
// this TU needs from AABBox is ONLY its byte image: two 16-byte float4 rows, mMin @+0x00 and
// mMax @+0x10 (AABBox.hpp:  Vector3 mMin; Vector3 mMax;  over a 16-byte VectorIntrinsic).
// So GetBBox's out-parameter is an opaque 32-byte 16-aligned local viewed as AABBox& (the
// class is only forward-declared here), and the lanes are read as f32[8]. That layout is
// PINNED by PropManager_wQ4_03_embed_check.cpp (a separate TU that CAN include AABBox.hpp:
// sizeof(AABBox)==32, offsetof(mMax)==16, offsetof(mMin.mV)==0), so a change to AABBox
// breaks the gate rather than this reader. LANDED 2026-08-18 (wave Q4 integration) from
// scratchpad/waveQ2/parked/PropManager_09_GetPropInertia_GetPartInertia.cpp; the code below
// is that body with the two AABBox locals expressed through the opaque image.
namespace rw { namespace collision { class AABBox; } }
                                                                  //   <-- ONLY after rwvol.owner.md §4
#include <cmath>                                                  // fabsf (the vandc sign-bit clear)

namespace BrnPhysics
{
namespace Props
{
namespace
{
    // -------------------------------------------------------------------------------------
    // The two per-lane selects the emission performs, written so their NaN behaviour is the
    // console's. Operand order is preserved from the asm, so the tie/NaN winner is preserved.
    //   MaxLane: `vcmpgtfp. v0, v12(accum), v0(vol)`  @0x82612780 -> TRUE picks the accumulator.
    //   MinLane: `vcmpgtfp. v0, v11(vol), v12(accum)` @0x82612848 -> TRUE picks the accumulator.
    // AUTHORED names (see the INFERRED block).
    // -------------------------------------------------------------------------------------
    inline f32 MaxLane( f32 lfAccumulated, f32 lfCandidate )
    {
        return ( lfAccumulated > lfCandidate ) ? lfAccumulated : lfCandidate;
    }

    inline f32 MinLane( f32 lfCandidate, f32 lfAccumulated )
    {
        return ( lfCandidate > lfAccumulated ) ? lfAccumulated : lfCandidate;
    }

    // The final `vmaxfp v13, v12, v13` -- AltiVec vmaxfp is `(a > b) ? a : b`, i.e. the same
    // ordered compare, so it reduces the same way.
    inline f32 MaxFp( f32 lfA, f32 lfB )
    {
        return ( lfA > lfB ) ? lfA : lfB;
    }

    // Vector3 is four adjacent f32 lanes {x,y,z,w} (rw/math/vpu/types.h:24) and lane 0/1/2 ==
    // x/y/z. This is the re-roll of the source's own VectorAxisX/Y/Z templates, not a layout
    // guess.
    inline f32&       Lane( Vector3& lrVector, u32 luAxis )       { return ( &lrVector.x )[ luAxis ]; }
    inline const f32& Lane( const Vector3& lrVector, u32 luAxis ) { return ( &lrVector.x )[ luAxis ]; }

    static const u32 KU_AXIS_COUNT = 3u;

    // flt_820065E0 == 0.33333334f (0x3EAAAAAB) -- the reciprocal the compiler folded the
    // source's `/ 3.0f` into (the DWARF's three rw::math::vpu::operator/ calls). Written as
    // the emitted MULTIPLY because that, not an exact division by three, is what the shipped
    // image computes. 1/3 rather than 1/12 is correct precisely because the dimensions below
    // are HALF-extents: (m/12)*(2h)^2 == (m/3)*h^2.
    static const f32 KF_BOX_INERTIA_SCALE = 0.33333334f;             // flt_820065E0

    // -------------------------------------------------------------------------------------
    // The shared half of both bodies: walk a volume run under an identity transform,
    // accumulate the per-lane extremes of every volume's bounding box, and reduce that to the
    // per-axis half-extent the inertia fold consumes. Outlined as a PRESENTATION choice --
    // the two emissions are instruction-for-instruction identical here and only the accessor
    // supplying the count and the volumes differs.
    // -------------------------------------------------------------------------------------
    // The byte image of rw::collision::AABBox (mMin @+0x00, mMax @+0x10, 16-byte float4 rows)
    // over the vendor POD Vector3 this TU already speaks -- see the banner; layout pinned by
    // PropManager_wQ4_03_embed_check.cpp.
    struct alignas(16) AABBoxRows
    {
        Vector3 mMin;
        Vector3 mMax;
    };
    static_assert( sizeof( AABBoxRows ) == 32, "AABBox image is two 16-byte rows" );

    template< typename TTypeData >
    Vector3 AccumulateVolumeHalfExtents( const TTypeData* lpType )
    {
        // DWARF lAccumulatedAABBox. BOTH rows start at ZERO (`vspltisw128 v125,0` @0x82612668
        // then `vmr128 v126,v125` @0x82612674 / `vmr128 v127,v125` @0x8261267C) -- measured,
        // and load-bearing.
        AABBoxRows lAccumulatedAABBox;            // DWARF lAccumulatedAABBox (the AABBox image, see banner)
        lAccumulatedAABBox.mMin.SetZero();
        lAccumulatedAABBox.mMax.SetZero();

        // DWARF lu8Vol -- a u8 induction variable (the emission re-masks it `clrlwi r30,r11,24`
        // @0x826128FC every iteration, so the wrap behaviour is a u8's). The count is RE-READ
        // from the type record every iteration, exactly as the console does
        // (`lbz ...,0x5E(r29)` @0x82612678 and again @0x826128F8; GetPartInertia
        // `lbz ...,0x2C(r29)` @0x82612B00 and again @0x82612D80) -- deliberately NOT hoisted
        // into a loop-invariant local.
        for ( u8 lu8Vol = 0u; lu8Vol < lpType->GetNumberOfVolumes(); ++lu8Vol )
        {
            // DWARF lIdentity, rebuilt every iteration (the four `stvx128` at the top of the
            // loop body, @0x82612720/0x82612738/0x82612744/0x8261274C). Three rdata rows
            // {1,0,0,0} / {0,1,0,0} / {0,0,1,0} plus a zero row -- exactly SetIdentity(),
            // whose wAxis is {0,0,0,0}.
            Matrix44Affine lIdentity;
            lIdentity.SetIdentity();

            // DWARF lpVolume / lVolumeAABBox. Dispatched through the rwcollision per-TYPE
            // descriptor at volume+0x40, function pointer at descriptor+0x04
            // (`lwz r11,0x40(r3) ; lwz r11,4(r11) ; mtctr ; bctrl` @0x82612750..0x8261275C;
            // GetPartInertia @0x82612BD8..0x82612BE4). The `1` is the RwBool `tight` flag
            // (`li r5,1` @0x8261271C / @0x82612BA4); the RwBool result in r3 is not tested by
            // either caller.
            AABBoxRows                 lVolumeAABBox;   // DWARF lVolumeAABBox: GetBBox's 32-byte out-param
            ::rw::collision::Volume*   lpVolume = lpType->GetCollisionVolume( lu8Vol );
            lpVolume->GetBBox( &lIdentity, 1, *reinterpret_cast< ::rw::collision::AABBox* >( &lVolumeAABBox ) );

            // DWARF lMax (:2699/:2762) and lMin (:2700/:2763). Six unrolled reference selects
            // in the emission -- three Max<VecFloatRef{X,Y,Z}> against the volume box's MAX
            // row and three Min<VecFloatRef{X,Y,Z}> against its MIN row -- re-rolled here into
            // one axis loop. (The console updates the accumulator lane by lane in place and
            // re-splats from the UPDATED accumulator; the snapshot below is equivalent
            // because no lane reads another lane.)
            Vector3 lMax = lAccumulatedAABBox.mMax;
            Vector3 lMin = lAccumulatedAABBox.mMin;

            for ( u32 luAxis = 0u; luAxis < KU_AXIS_COUNT; ++luAxis )
            {
                Lane( lMax, luAxis ) = MaxLane( Lane( lMax, luAxis ),
                                                Lane( lVolumeAABBox.mMax, luAxis ) );
                Lane( lMin, luAxis ) = MinLane( Lane( lVolumeAABBox.mMin, luAxis ),
                                                Lane( lMin, luAxis ) );
            }

            lAccumulatedAABBox.mMax = lMax;
            lAccumulatedAABBox.mMin = lMin;
        }

        // DWARF lDims (:2712/:2775): per axis, Max( Abs(min_i), Abs(max_i) ). The emission is
        // `vandc` against 0x80000000 (a sign-bit clear, i.e. fabsf) followed by `vmaxfp`.
        // ⚠️ THERE IS NO (max - min) HERE -- neither body emits a single `vsubfp` (grepped,
        // zero hits in both dumps). This is a half-extent about the ORIGIN, which is what
        // pairs with the 1/3 scale below.
        Vector3 lDims;
        lDims.SetZero();
        for ( u32 luAxis = 0u; luAxis < KU_AXIS_COUNT; ++luAxis )
        {
            Lane( lDims, luAxis ) = MaxFp( fabsf( Lane( lAccumulatedAABBox.mMax, luAxis ) ),
                                           fabsf( Lane( lAccumulatedAABBox.mMin, luAxis ) ) );
        }

        return lDims;
    }

    // -------------------------------------------------------------------------------------
    // The solid-box inertia fold, shared by both bodies verbatim (DWARF lInertia):
    //     lInertia.x = (d.y^2 + d.z^2) / 3;    lane x, vrlimi mask 8
    //     lInertia.y = (d.x^2 + d.z^2) / 3;    lane y, mask 4
    //     lInertia.z = (d.x^2 + d.y^2) / 3;    lane z, mask 2
    //     lInertia  *= lfMass;                 DWARF operator*=; `vmulfp128 v0,v11,v7`
    // Lane->axis read off the vrlimi128 masks @0x82612A90/0x82612A94/0x82612A98
    // (GetPartInertia 0x82612ED0/0x82612ED8/0x82612EDC).
    // -------------------------------------------------------------------------------------
    Vector3 FoldBoxInertia( const Vector3& lrDims, f32 lfMass )
    {
        const f32 lfXSq = lrDims.x * lrDims.x;
        const f32 lfYSq = lrDims.y * lrDims.y;
        const f32 lfZSq = lrDims.z * lrDims.z;

        Vector3 lInertia;
        lInertia.x = ( lfYSq + lfZSq ) * KF_BOX_INERTIA_SCALE;
        lInertia.y = ( lfXSq + lfZSq ) * KF_BOX_INERTIA_SCALE;
        lInertia.z = ( lfXSq + lfYSq ) * KF_BOX_INERTIA_SCALE;

        lInertia.x *= lfMass;
        lInertia.y *= lfMass;
        lInertia.z *= lfMass;

        // ⚠️ INFERRED, not measured -- see the W-lane note in the banner. The console's W lane
        // is `<stale var_C0> * lfMass` (the vector is loaded ONE instruction before the mass is
        // stored into that slot in GetPropInertia @0x82612A44/0x82612A48, and two instructions
        // before it in GetPartInertia @0x82612E74/0x82612E7C), so it is undefined in the
        // shipped build. Zeroed here; nothing reads it.
        lInertia.w = 0.0f;

        return lInertia;
    }
}

// =========================================================================================
// BrnPhysics::Props::PropManager::GetPropInertia @ 0x82612640   (289 instructions)
// DWARF BrnPropManager.h:222, definition at BrnPropManager.cpp:2678.
//
// Only caller: AddPropToSim @0x826274D8 (single xref; the call is at 0x826275EC), which then
// scales the result by KVF_INERTIA_SCALE @0x82627604 -- that scale is NOT applied here.
// =========================================================================================
Vector3 PropManager::GetPropInertia( const PropTypeData* lpType )
{
    // Volume count u8 @+0x5E, volume run @+0x3C, both via the committed accessors.
    Vector3 lDims = AccumulateVolumeHalfExtents( lpType );

    // The inlined PropTypeData::HACKShouldMoveComOffset() (DWARF BrnPhysicsPropTypeData.h:110;
    // landed as a header inline at BrnPhysicsPropTypeData.h:222). `lwz r11,0x58(r29)`
    // @0x82612924 == muSceneUriId, compared against 0x6894C @0x82612948 then 0x68964
    // @0x826129A0; on a match the accumulated dimensions are DISCARDED and replaced wholesale
    // by the rdata vector at unk_82FB9420 (`lvx128 v0,r0,r11` @0x826129C4). It is NOT
    // IsLamppost(), which tests eight ids -- do not unify them.
    //
    // ⚠️ K_LAMPOST_INERTIA_BOX reads ALL-ZERO out of the shipped image, so on the console these
    // two prop types get a ZERO inertia here. Faithful, not a bug in this body.
    if ( lpType->HACKShouldMoveComOffset() )
    {
        lDims = K_LAMPOST_INERTIA_BOX;
    }

    // `lfs f0, 0x38(r29)` @0x82612A28 == mfMass.
    return FoldBoxInertia( lDims, lpType->GetMass() );
}

// =========================================================================================
// BrnPhysics::Props::PropManager::GetPartInertia @ 0x82612AC8   (272 instructions)
// DWARF BrnPropManager.h:226, definition at BrnPropManager.cpp:2740.
//
// The same function as GetPropInertia modulo exactly four things, all checked against the raw
// asm rather than assumed from the sibling:
//   * the volume count is the u8 at PropPartTypeData +0x2C (`lbz r11,0x2C(r29)` @0x82612B00
//     and again @0x82612D80) instead of +0x5E;
//   * the volume run is at +0x24 (`lwz r10,0x24(r29)` @0x82612B98) instead of +0x3C;
//   * the mass is at +0x20 (`lfs f0,0x20(r29)` @0x82612E38) instead of +0x38;
//   * and THERE IS NO HACKShouldMoveComOffset / lamppost branch -- grepped the whole dump: no
//     `0x58(r29)` load, no 0x6894C/0x68964 immediate, no unk_82FB94xx reference anywhere.
// Same hidden-pointer return, same unread `this`. Every offset above is reached through the
// committed PropPartTypeData accessors, so no console offset survives into the host code.
// =========================================================================================
Vector3 PropManager::GetPartInertia( const PropPartTypeData* lpType )
{
    const Vector3 lDims = AccumulateVolumeHalfExtents( lpType );

    // `lfs f0, 0x20(r29)` @0x82612E38 == mfMass.
    return FoldBoxInertia( lDims, lpType->GetMass() );
}

}   // namespace Props
}   // namespace BrnPhysics

// ============================================================================
// FOLDED FROM PropManager_wQ4_04.cpp (wave Q4) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================
// LANDED 2026-08-18 (wave Q4 integration) from scratchpad/waveQ2/parked/PropManager_08_ApplyPropRaceCarCollisionImpulse.cpp:
// both blockers named in the banner below are closed (RaceCarPhysics::AddPropCollisionImpulse is now the header inline it is on X360;
// KVF_MAX_PROP_SPEED_MPS is declared+seated). The banner is kept as the derivation record.
// ==========================================================================================
// PARKED -- COMPLETE BODY, NOT MOUNTED.
//
// BrnPhysics::Props::PropManager::ApplyPropRaceCarCollisionImpulse  (X360 0x825E3560).
// Wave Q (breakable props / smash gates), ROUND 2, group 08.  2026-08-18.
//
// Intended home: b5-decomp/src/GameSource/Physics/PropManager/PropManager_wQ2_08.cpp
// (drop the two functions into the same namespace block; the include set below is complete
//  and was gate-proved -- see "PROOF" at the bottom).
//
// ------------------------------------------------------------------------------------------
// WHAT UNBLOCKS THIS -- EXACTLY TWO LINES, BOTH IN HEADERS THIS PARTFILE MAY NOT EDIT.
// (Both were MEASURED, not guessed: the verbatim body below compiles with EXACTLY these two
//  diagnostics and no others -- see PROOF.)
//
//  (1) b5-decomp/src/GameSource/Physics/VehicleManager/VehiclePhysics/RaceCarPhysics.h
//      -- add to a PUBLIC section:
//
//          void AddPropCollisionImpulse( Vector3 lImpulse );
//
//      Grounding: DecFIGS dwarfdump RaceCarPhysics.h:340 declares
//      `void AddPropCollisionImpulse(Vector3);`, and the PS3 compile unit
//      references/DecFIGS/dwarfdump/_compile/BrnPhysicsUnity2.cpp:19688 has the out-of-line
//      body `void BrnPhysics::Vehicle::RaceCarPhysics::AddPropCollisionImpulse(const
//      rw::math::vpu::Vector3 lImpulse)`. ARTIST has NO symbol for it -- the X360 compiler
//      INLINED it, and this function's own last three instructions ARE that inline:
//          0x825E35E8  addi   r10, r4, 0x13F0        <- &mPropCollisionImpulseSum
//          0x825E35FC  lvx128 v8,  r0, r10
//          0x825E361C  vmaddfp v0, v1, v8, v0        <- sum += lNormal * lImpulseMagnitude
//          0x825E3620  stvx128 v0, r0, r10
//      RaceCarPhysics.h:589 already declares that member BY NAME (`Vector3
//      mPropCollisionImpulseSum;  // @+0x13F0`, X360-pinned by ApplyPropCollisionImpulseSum's
//      own `addi r31,this,0x13F0`) -- but it is PRIVATE (the private section runs :483..:636)
//      and PropManager is not a friend (the only friend is `class VehicleManager`, :47).
//      So the accessor, not the member, is what has to be added. Whether it is written as a
//      one-line header inline (`mPropCollisionImpulseSum += lImpulse;`, which is what the
//      console emits) or as an out-of-line body is the RaceCarPhysics owner's call.
//      ⚠️ DO NOT reach the member by offset instead: +0x13F0 is a CONSOLE offset and
//      RaceCarPhysics is explicitly NOT byte-pinned on the host (RaceCarPhysics.h:16, :506).
//
//  (2) b5-decomp/src/GameSource/Physics/PropManager/BrnPropManager.h
//      -- add beside the other namespace-scope prop tuning constants (the block at :103..:182):
//
//          extern VecFloat KVF_MAX_PROP_SPEED_MPS;                // X360 0x82FB9470
//
//      ...and its zero definition beside the others in BrnPropManager.cpp (:238..:252):
//
//          ::VecFloat KVF_MAX_PROP_SPEED_MPS;                     // X360 0x82FB9470
//
//      Grounding, and the one INFERENCE in it, stated plainly:
//        * MEASURED: the only rodata/bss datum this body loads is `unk_82FB9470`
//          (0x825E35C8/0x825E35D4/0x825E35E4). A ripgrep of the whole
//          .ida-exports/BURNOUT_X360_ARTIST.XEX/ directory for "82FB9470" returns exactly ONE
//          file -- 0x825E3560.json, this function. There is no writer and no other reader.
//        * MEASURED (headless IDA 9.3 byte read of IDA Files/BURNOUT_X360_ARTIST.XEX.i64,
//          this wave): 0x82FB9470 holds 16 zero bytes == (0.0f, 0.0f, 0.0f, 0.0f). The same
//          read returned all-zero for 0x82FB9450 (KVF_ANTI_HERD_SIDE_SCALE) and 0x82FB9490
//          (KVF_MAX_ANGULAR_ACCELERATION), which is the already-recorded zero-page finding
//          (BrnPropManager.cpp:186-198) independently re-confirmed. So the value is NOT
//          recovered and NOTHING is invented for it -- see the CONSOLE-VALUE WARNING below.
//        * INFERENCE (address -> name): the DecFIGS file-scope dump for this exact .cpp lists
//          `// BrnPropManager.cpp:1678   VecFloat KVF_MAX_PROP_SPEED_MPS;` and this function's
//          own DWARF scope is `// BrnPropManager.cpp:1685`. :1678 is the file-scope constant
//          immediately preceding :1685, it is the only DWARF constant of that name, it is a
//          VecFloat (matching the 16-byte `lvx128` load), and its semantics -- a maximum prop
//          SPEED -- fit the one use, a speed-derived falloff. That proximity + uniqueness +
//          type + semantic fit is the whole of the argument; the binary does not name it.
//        * ⚠️ AND A DIMENSIONAL ODDITY, REPORTED NOT "FIXED": the constant is subtracted from
//          `vmsum3fp128 v0,v0,v0` == the prop's speed SQUARED (0x825E35C4), while the DWARF
//          name says MPS (metres per second). Either the source name is loose or the source
//          stores the square in it. Do not "correct" the reconstruction to Magnitude() to make
//          the name fit -- the asm has no sqrt.
//
// ------------------------------------------------------------------------------------------
// ⚠️⚠️ CONSOLE-VALUE WARNING (the same class of finding as BrnPropManager.cpp:222-244).
// With KVF_MAX_PROP_SPEED_MPS == 0 -- which is what the shipped image holds --
// `Clamp(0 - lSquaredPropSpeed, 0, 1)` is ZERO for every prop (a squared magnitude is never
// negative), so lImpulseMagnitude is zero and the car takes NO impulse back from any prop it
// smashes, ever. That is a statement about the missing dynamic initialiser, not about this
// reconstruction. Do NOT pick a number to make it "work" -- report it.
//
// ------------------------------------------------------------------------------------------
// THE 50 INSTRUCTIONS  (COUNTED: (0x825E3624 - 0x825E3560)/4 + 1 == 50).
//
// Register map, MEASURED from the body (there is no prologue -- this is a leaf, `blr` at
// 0x825E3624 with no frame set up):
//   r3 = this          (`lbz r7,0x49(r3)` == mbUseOverides; `lfs f0,0x4C(r3)` == mfMassOverride)
//   r4 = lpRaceCar     (+0x10 mTransform, +0x40 mTransform.wAxis, +0x50 mLinearVelocity,
//                       +0x60 mAngularVelocity, +0x13F0 mPropCollisionImpulseSum)
//   r5 = lpProp        (+0x40 mLinearVelocity)
//   r6 = lpType        (+0x38 mfMass)
//   v1 = lNormal, v2 = lPointOnCar     <- AGENTS.md gotcha 3: the two Vector3s ride v1/v2 and
//                                         consume NO GPR slot, which is why lpType lands in r6.
//
//   0x825E3560  addi      r11, r4, 0x10          <- &lpRaceCar->mTransform
//   0x825E3564  vspltisw  v11, 0                 <- the zero used by both clamps
//   0x825E356C  lbz       r7,  0x49(r3)          <- mbUseOverides
//   0x825E3574  lfs       f0,  0x38(r6)          <- lpType->GetMass()
//   0x825E3580  lvx128    v13, r11, r9(0x30)     <- mTransform.wAxis  (0x10+0x30 == +0x40)
//   0x825E3584  vsubfp    v12, v2,  v13          <- r = lPointOnCar - carPos
//   0x825E3588  lvx128    v13, r11, r8(0x50)     <- mAngularVelocity  (0x10+0x50 == +0x60)
//   0x825E3590  lvx128    v10, r11, r10(0x40)    <- mLinearVelocity   (0x10+0x40 == +0x50)
//   0x825E3594  lvx128    v0,  r5,  r10(0x40)    <- lpProp->mLinearVelocity
//   0x825E3598  vmr       v8,  v12
//   0x825E359C  vpermwi128 v12, v12, 0x63        \
//   0x825E35A0  vmulfp128 v13, v13, v12           |  the SDK one-shuffle CROSS PRODUCT:
//   0x825E35A4  vnmsubfp  v13, v9,  v13, v8       |  t = w*r.yzx - w.yzx*r ; cross = t.yzx
//   0x825E35A8  vpermwi128 v13, v13, 0x63        /   (0x63 == lane order 1,2,0,3 == yzx)
//   0x825E35AC  vaddfp    v13, v13, v10          <- + mLinearVelocity
//                                                   ==> the INLINED ExternalPhysicsBody::
//                                                       GetLocalVelocity(pt, WORLD_SPACE)
//   0x825E35B0  vsubfp    v13, v0,  v13          <- lRelativeVelocity = propVel - carPointVel
//   0x825E35B4  vmsum3fp128 v13, v13, v1         <- Dot(lRelativeVelocity, lNormal), broadcast
//   0x825E35B8  vmaxfp    v13, v11, v13          <- Max(0, that)
//   0x825E35BC  beq       cr6, 0x825E35C4        \  if (mbUseOverides) mass = mfMassOverride
//   0x825E35C0  lfs       f0,  0x4C(r3)          /  (the branch SKIPS the override load when
//                                                    the flag is zero -- i.e. the type mass is
//                                                    the default and the override wins)
//   0x825E35C4  vmsum3fp128 v0,  v0,  v0         <- MagnitudeSquared(propVel)
//   0x825E35E4  lvx128    v12, r0,  r10          <- KVF_MAX_PROP_SPEED_MPS (unk_82FB9470)
//   0x825E35D8/E0 vspltisw v10,1 ; vcfsx v10,v10,0  <- 1.0f  (DWARF: GetVecFloat_One())
//     ⚠️ NAME NOTE (re-checked 2026-08-18): the DWARF callee is `rw::math::vpu::
//     GetVecFloat_One()`, and that exact spelling does NOT exist in the recovered vendor
//     headers -- a grep of b5-decomp/vendor + b5-decomp/src for it returns nothing. The
//     tree's one-vector helper is `GetVector4_One()` (vendor/renderware/include/rw/math/vpu/
//     vector4_operation.h:38, already used by SharedClasses/Traffic/BrnTrafficFuzzyLogic.h).
//     The body below deliberately calls the EXISTING tree name; it is not a second helper and
//     nothing new is invented. If the vendor owner ever lands the VecFloat-typed spelling,
//     this one call should follow it.
//   0x825E3600  vsubfp    v0,  v12, v0           <- K - lSquaredPropSpeed
//   0x825E3610  vmaxfp    v0,  v11, v0           \  Clamp(x, 0, 1)  == max-then-min, which is
//   0x825E3614  vminfp    v0,  v10, v0           /  exactly vpu::Clamp's Min(Max(v,lo),hi)
//   0x825E35D0/0604/0608  stfs f0,var_10 ; lvx128 v9 ; vspltw v9,v9,0   <- float -> VecFloat
//                                                                          broadcast of the mass
//   0x825E360C  vmulfp128 v13, v13, v9           \  lImpulseMagnitude =
//   0x825E3618  vmulfp128 v0,  v13, v0           /    speedAlongNormal * mass * modifier
//   0x825E361C  vmaddfp   v0,  v1,  v8,  v0      <- sum + lNormal*magnitude   (see (1) above)
//   0x825E3620  stvx128   v0,  r0,  r10
//   0x825E3624  blr
//
// vmaddfp/vnmsubfp OPERAND ORDER, stated because getting it wrong silently corrupts the math:
// IDA prints the ENCODED field order `vD, vA, vB, vC`, while the operation is
// `vD = vA*vC + vB` (and `vnmsubfp` = `vB - vA*vC`). Cross-checked against the last
// instruction, which must be `accumulator + lNormal * magnitude`: with vA=v1(lNormal),
// vB=v8(the loaded accumulator), vC=v0(the magnitude) that reads correctly, and under the
// other convention it would read `lNormal*accumulator + magnitude`, which is nonsense.
//
// DEAD STORES NOT REPRODUCED, and why: 0x825E35EC/F0/F4 write three zero words at
// `r1 + back_chain` +0/+4/+8 and nothing ever reads them (the only stack RELOAD, 0x825E3604,
// is 16 bytes at `r1 + var_10`, which is where the mass float was spilled at 0x825E35D0).
// They are the compiler's dead zero-init of a Vector3 local -- almost certainly the DWARF's
// `Vector3 lImpulse` (source :1710) -- and carry no behaviour. Stated rather than silently
// dropped.
//
// LOCALS are the DecFIGS scope's, verbatim (dwarfdump BrnPropManager.cpp:389-431, source
// :1685): lRelativeVelocity :1687, lRelativeSpeedAlongNormal :1688, lCarTransform :1689,
// lfPropMass :1695, lSquaredPropSpeed :1706, lPropSpeedModifier :1707, lImpulseMagnitude
// :1709, lImpulse :1710. The same scope lists the callees, and the body below is item-for-item
// that list: ExternalPhysicsBody::GetLocalVelocity, GetVecFloat_One, operator+=,
// RaceCarPhysics::AddPropCollisionImpulse, MagnitudeSquared, operator-, Dot, Max<VecFloat>,
// operator*, operator+=, operator-, Clamp, operator*, operator*.
//   ⚠️ `Matrix44Affine lCarTransform` (:1689) has NO ARTIST counterpart and is deliberately
//   ABSENT below. Negative evidence: the 50 instructions read exactly ONE 16-byte quantity out
//   of the car transform -- `lvx128 v13, r11, 0x30` == mTransform.wAxis -- and that read is
//   INSIDE the inlined GetLocalVelocity (it is the `lPoint - mTransform.wAxis` moment arm).
//   There is no matrix copy, no basis-row read, and no second use. Writing a whole
//   Matrix44Affine copy here to honour the DWARF local would add work the binary does not do.
//
// GetLocalVelocity's SPACE ARGUMENT is not a guess: rw::physics::InputSpace is
// {WORLD_SPACE=0, BODY_SPACE=1} (DWARF-authoritative, rw/physics/rigidbody.h:88-93), and the
// committed body ExternalPhysicsBody.cpp:839 makes WORLD_SPACE the leg that subtracts
// mTransform.wAxis and then crosses -- which is exactly and only what the asm above does.
// BODY_SPACE would have rotated the point through the basis rows first; there is no such
// rotation in the emission.
//
// NaN POLARITY (AGENTS.md gotcha 4), stated because it is a real host/console delta: the
// console's `vmaxfp`/`vminfp` are hardware max/min; the committed vpu::Max/Min are
// `a > b ? a : b` / `a < b ? a : b` ternaries, which resolve a NaN operand differently. The
// operand ORDER below is written to match the asm exactly (`vmaxfp v13, v11(zero), v13` ->
// `Max(zero, dot)`), so the two agree on every ordered input.
//
// ------------------------------------------------------------------------------------------
// PROOF (this wave, scratchpad/waveQ2/probe_wq2_08/):
//   * `selfcheck probe_apply_impulse.cpp`         -> STATUS=fail with EXACTLY THREE lines:
//       C2065 "KVF_MAX_PROP_SPEED_MPS": undeclared identifier            (blocker 2)
//       C2737 "lPropSpeedModifier": const object must be initialised     (fallout of the same)
//       C2039 "AddPropCollisionImpulse" is not a member of RaceCarPhysics (blocker 1)
//     -- i.e. every other name in the body (GetLocalVelocity, WORLD_SPACE, GetLinearVelocity,
//     GetMass, mbUseOverides, mfMassOverride, the whole vpu vocabulary) already binds.
//   * `selfcheck probe_apply_impulse_shimmed.cpp` -> STATUS=pass. Same file with a file-local
//     zero constant and the accessor call replaced by `(void)lImpulse;` -- proving the residual
//     is exactly those two declarations and nothing structural.
// ------------------------------------------------------------------------------------------
// LINK-LEVEL: ApplyPropRaceCarCollisionImpulse has NO inert gate anywhere (grepped the stub TUs
// -- neither names it) and no other body in the tree, so mounting it created no duplicate. Its one caller is
// PropManager::SetupAndValidatePropContact @0x82628190, which is itself a trap stub today.
// ==========================================================================================

                                                                             // Clamp/GetVector4_One

namespace BrnPhysics
{
namespace Props
{
    // ======================================================================================
    // ApplyPropRaceCarCollisionImpulse  --  X360 0x825E3560, 50 instructions.
    // DecFIGS: dwarfdump BrnPropManager.h:428 (declaration) / BrnPropManager.cpp:389
    // (scope, source line :1685). Parameter names/order are the committed header's, which
    // round 2 already corrected to the DWARF's (lNormal, lPointOnCar).
    // ======================================================================================
    void
    PropManager::ApplyPropRaceCarCollisionImpulse(
        BrnPhysics::Vehicle::RaceCarPhysics* lpRaceCar,
        PropInstance*                        lpProp,
        const PropTypeData*                  lpType,
        Vector3                              lNormal,
        Vector3                              lPointOnCar )
    {
        namespace vpu = rw::math::vpu;

        // 0x825E3580..0x825E35B0. The velocity of the prop relative to the point of the car it
        // struck. The car side is ExternalPhysicsBody::GetLocalVelocity, which the X360
        // inlined into the cross-product chain transcribed in the banner.
        const Vector3 lRelativeVelocity =
            vpu::Subtract( lpProp->GetLinearVelocity(),
                           lpRaceCar->GetLocalVelocity( lPointOnCar, rw::physics::WORLD_SPACE ) );

        // 0x825E35B4 / 0x825E35B8. Only separation counts: a prop moving away from the car
        // along the contact normal contributes nothing. Operand order matches `vmaxfp v13,
        // v11, v13` (zero first).
        const VecFloat lRelativeSpeedAlongNormal =
            vpu::Max( vpu::Splat( 0.0f ),
                      vpu::Splat( vpu::Dot( lRelativeVelocity, lNormal ) ) );

        // 0x825E356C / 0x825E3574 / 0x825E35BC / 0x825E35C0. The debug mass override wins over
        // the prop type's own mass.
        f32 lfPropMass = lpType->GetMass();
        if ( mbUseOverides )
        {
            lfPropMass = mfMassOverride;
        }

        // 0x825E35C4 / 0x825E3600 / 0x825E3610 / 0x825E3614. A fast-moving prop hands the car
        // proportionally less back. See the CONSOLE-VALUE WARNING in the banner: with the
        // shipped (zero) constant this modifier is identically zero.
        const VecFloat lSquaredPropSpeed =
            vpu::Splat( vpu::MagnitudeSquared( lpProp->GetLinearVelocity() ) );

        const VecFloat lPropSpeedModifier =
            vpu::Clamp( KVF_MAX_PROP_SPEED_MPS - lSquaredPropSpeed,
                        vpu::Splat( 0.0f ),
                        vpu::GetVector4_One() );

        // 0x825E360C / 0x825E3618 / 0x825E361C.
        const VecFloat lImpulseMagnitude =
            lRelativeSpeedAlongNormal * vpu::Splat( lfPropMass ) * lPropSpeedModifier;

        const Vector3 lImpulse = vpu::Mult( lNormal, lImpulseMagnitude.x );

        // 0x825E35E8/0x825E35FC/0x825E361C/0x825E3620 -- the inlined
        // `mPropCollisionImpulseSum += lImpulse`, flushed later by ApplyPropCollisionImpulseSum.
        lpRaceCar->AddPropCollisionImpulse( lImpulse );
    }
}
}

// ============================================================================
// FOLDED FROM PropManager_wQ5_01.cpp (wave Q5) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================
// =================================================================================================
// PropManager_wQ5_01.cpp  --  BrnPhysics::Props::PropManager, wave Q5 round-3 integration partfile.
//
//   PropManager::ProcessInputs_Prepare  @ 0x825E3400   (3 insns, a tail call; DWARF
//                                                       BrnPropManager.h:140)
//
// Partfile of BrnPropManager.cpp (fold back into it when that TU is consolidated). One body,
// landed 2026-08-19 after the FIRST car-vs-prop potential contact reached the prop module and the
// resulting AddPhysicalProp event died in ProcessAddPropInstanceEvents on 'Can not instance
// resource pointer - it has no main memory resource' -- mpPhysicsData had never been bound.
//
// EXPORT HOLE: the function has no progress/identity.json row and no per-address export. It was
// recovered by resolving the `bl` at 0x825A14E0 inside PhysicsModule::PropPrepareTypes @0x825A14A8
// with headless IDA on a private .i64 copy (scratchpad/waveQ5/ida_pip/dump_pip.py -> out.json):
//
//   0x825E3400  addi  r4, r4, 0x2BF8        ; &lpInput->mpPhysicsData  (PropInputInterface +0x2BF8,
//                                            ;  the ResourceHandle PropEntityModule::Prepare posted)
//   0x825E3404  addi  r3, r3, 0x54          ; &this->mpPhysicsData     (ResourcePtr<PropPhysicsDataHeader>)
//   0x825E3408  b     CgsResource::BaseResourcePtr::CreateFromHandle
//
// Both console offsets are reached by NAME below (the handle widens on x64; +0x2BF8/+0x54 are
// comments only). CreateFromHandle is PROTECTED on BaseResourcePtr, so from PropManager the call
// is spelled through ResourcePtr<T>::operator=(const ResourceHandle&) -- the tree's inline
// assign-from-handle, which is exactly ONE CreateFromHandle(this, &handle) (CgsResourcePtr.h
// documents that shape at every X360 assign site). Same single instruction sequence, legal access.
// =================================================================================================

namespace BrnPhysics
{
namespace Props
{
    void PropManager::ProcessInputs_Prepare( const PropInputInterface* lpInput )
    {
        mpPhysicsData = lpInput->GetPropPhysicsData();   // == BaseResourcePtr::CreateFromHandle(&mpPhysicsData, &handle)
    }
}
}

// ============================================================================
// FOLDED FROM PropManager_wQ6_01.cpp (wave Q6) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================
// GameSource/Physics/PropManager/PropManager_wQ6_01.cpp
//
// BrnPhysics::Props::PropManager -- breakable-props wave Q6, partfile 01.
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   RemoveAllPropsAndParts @0x8260F010  (331 instructions, 0x8260F010..0x8260F538 --
//                                        COUNTED: (0x8260F538-0x8260F010)/4 + 1 == 331)
//
// A slice of the TU's own BrnPropManager.cpp, split out per the wave's partfile convention
// (same spirit as PropManager_wQ2_08.cpp, which reconstructed this function's only caller).
//
// ==========================================================================================
// ⚠️ PROVENANCE -- THIS FUNCTION IS A GENUINE EXPORT HOLE, CLOSED THIS WAVE.
//
// There is NO .ida-exports/BURNOUT_X360_ARTIST.XEX/0x8260F010.json, and no
// `BrnPhysics::Props::PropManager::RemoveAllPropsAndParts` row in progress/identity.json --
// which is why three separate committed banners could only name the address and the instruction
// count, taken from ProcessInputsPreScene's xrefs_from.
//
// The 331 instructions below were dumped 2026-08-19 by a headless `idat` run over a PRIVATE
// copy of "IDA Files/BURNOUT_X360_ARTIST.XEX.i64" (never the original):
//     scratchpad/waveQ6/ida_rmall/dump_rmall.py   -> scratchpad/waveQ6/ida_rmall/out.json
//     scratchpad/waveQ6/ida_rmall/rmall.asm.txt   (the 331 lines, one per instruction)
// IDA DOES name the function in that database (`BrnPhysics::Props::PropManager::
// RemoveAllPropsAndParts`, 0x8260F010..0x8260F53C) -- the hole was in the per-address EXPORT
// RUN, not in the analysis. AGENTS.md gotcha 6, third time in this subsystem.
//
// xrefs_to (measured, not inherited): 0x8263B228 inside ProcessInputsPreScene -- the live call
// at PropManager_wQ2_08.cpp:162 -- and NOTHING ELSE. The second xref IDA lists, 0x821CF120, was
// chased rather than assumed (scratchpad/waveQ6/ida_rmall/probe_xref.py): it is a `.pdata`
// UNWIND-TABLE slot (`.long BrnPhysics__Props__PropManager__RemoveAllPropsAndParts`, sitting
// between the Prepare and RemoveProp entries in the same table), not a caller. So the function
// has exactly ONE call site in the image. That table also cross-checks the span: the next entry
// is RemoveProp @0x8260F540, and 0x8260F540 - 0x8260F010 == 0x530 == 332 words == the 331
// instructions plus one alignment pad.
//
// ==========================================================================================
// WHAT THE 331 INSTRUCTIONS DO. All offsets quoted below are CONSOLE offsets read straight
// out of the asm; every one of them is reached BY NAME in the C++ (AGENTS.md gotcha 1 -- they
// are meaningless on the LLP64 host).
//
// Register map, MEASURED from the prologue (0x8260F010..0x8260F028):
//   r3 -> r30 = this                         (spilled to arg_14 at 0x8260F028 and reloaded
//                                             at 0x8260F2C8 / 0x8260F518)
//   r4 -> r3  = lpSimModuleInputBuffer       (consumed immediately, never saved)
//   r5 -> r26 = lpSceneInput
//   r14 = this + 0x80 == &mUsedProps         (set once at 0x8260F038, live to the very end)
//   r24 = 0                                  (the zero the six trailing stores use)
//   r15 = 1                                  (the IsBitSet mask seed)
//
// Body, in emission order:
//
//  1. 0x8260F02C..0x8260F048 -- the SIMULATION request, and it is a SINGLE event for the whole
//     owner, not one per body:
//         bl sub_825BD000   == InputBuffer::GetRemoveAllRigidBodiesQueue()  [WRITE side]
//         bl sub_825E4020   == BaseEventQueue<InRemoveAllRigidBodies>::AddEvent()  (no-arg
//                              slot reserve; returns the slot -- `add r3, mLength, mpEvents`
//                              with NO stride multiply, which is itself the proof that
//                              sizeof(InRemoveAllRigidBodies) == 1)
//         li r11, 3 ; stb r11, 0(r3)   == mu8OwnerId = E_ENTITYTYPE_PROP
//     Both callees were dumped in the same idat run (out.json "callee_bodies"):
//       * sub_825BD000, 42 insns: `lbz r11,0(r28) ; extrwi r11,r11,1,28` == status bit 3 ==
//         the WRITE lock, assert "Not locked for writing\n" with `li r5,0x43F` ==
//         CgsPhysicsSimulationModuleIO.h:1087, then `addis r3,r28,2 ; addi r3,r3,-0x5CD0` ==
//         this + 0x1A330 == +107312 == &mRemoveAllRigidBodiesQueue -- the SAME offset the
//         already-committed const accessor @0x8289E750 returns. The non-const overload was
//         missing from the tree and is added this wave (see the LINK-LEVEL note below).
//       * sub_825E4020, 40 insns: asserts "mpEvents != NULL" (line 0x168 == 360) and
//         "GetLength() < GetMaxLength()" (line 0x169 == 361) -- byte-for-byte the two source
//         lines the committed CgsBaseEventQueue.h:42-70 records for its no-arg reserve.
//     ⚠️ The tree spells that member `T& AddEvent()`. The DecFIGS DWARF's real name for it is
//     `T* AllocateEvent()` @CgsBaseEventQueue.h:356, and the DWARF for THIS function names its
//     local `InRemoveAllRigidBodies* lpEvent` -- i.e. the console source held a POINTER. That
//     name/return-type defect is already FILED by the header's owner (CgsBaseEventQueue.h:51-62,
//     "NAME DEFECT, FILED NOT FIXED 2026-08-18"); it is deliberately not re-discovered or
//     re-fixed here. The `&` below is what bridges the two, and it costs nothing.
//
//  2. 0x8260F040..0x8260F2C8 -- the PROP loop. The fully inlined BitArray<15>::
//     GetFirstNonZeroBit / GetNextNonZeroBit / IsBitSet walk over `addi r14, r30, 0x80` ==
//     &mUsedProps, re-rolled here into the named calls (AGENTS.md "inlining reversal"), exactly
//     as its caller ProcessInputsPreScene does with the identical inline.
//     Per live prop: `addi r11, r31, 0x1C` == KI_PROP_CACHE_START_INDEX + liPropIndex written
//     into a 4-byte stack record, then `bl 0x825E48C0` ==
//     BaseEventQueue<InEventRemoveFromCache>::AddEvent(const T*) on the queue at
//     `addis r27,r26,0xC ; addi r27,r27,0x77E0` == lpSceneInput + 0xC77E0 ==
//     &mRemoveFromCacheQueue.
//     ⚠️ The bit is NOT cleared inside the loop -- both bit-sets are wiped wholesale in step 4.
//     That is why GetNextNonZeroBit is handed the CURRENT index and still advances.
//
//  3. 0x8260F2CC..0x8260F518 -- the PART loop, the same shape over `addi r27, r30, 0x90` ==
//     &mUsedParts (the BitArray<30>; every bound in this half is 0x1E == 30), with
//     `addi r11, r31, 0x2B` == KI_PROP_PART_CACHE_START_INDEX + liPartIndex. The scene queue
//     pointer is re-derived into r26 this time (`addis r26,r26,0xC ; addi r26,r26,0x77E0`) --
//     same address, different register, because r27 has been retasked to &mUsedParts.
//
//  4. 0x8260F51C..0x8260F530 -- SIX stores, all of the r24 zero, and this is the whole tail:
//         std r24, 0(r14)     mUsedProps.UnSetAll()          (+0x80, one 64-bit field)
//         std r24, 0(r27)     mUsedParts.UnSetAll()          (+0x90, one 64-bit field)
//         stw r24, 0x88(r30)  muNumberOfPropInstances = 0
//         stw r24, 0x98(r30)  muNumberOfPartInstances = 0
//         std r24, 0x670(r30) mUsedPropJoints.UnSetAll()
//         std r24, 0x678(r30) mBreakPropJoints.UnSetAll()
//     `UnSetAll` (not `Prepare`) is the spelling this class already uses for these four
//     members in both Construct (BrnPropManager.cpp:393/:394/:399/:400) and Prepare
//     (PropManager_wQ2_06.cpp:259/:260) -- no second synonym is introduced.
//
//     ⭐ THIS RESOLVES A CAVEAT THE HEADER LEFT OPEN. BrnPropManager.h:502-513 states that no
//     PropManager function with a per-address export writes +0x88 / +0x98, and explicitly
//     scopes that to "the three export holes have no JSON to scan". One of those three holes is
//     THIS function, and it DOES write both -- with zero, on the teardown path only. So the two
//     counters still have no producer of a NON-ZERO value anywhere in the class; what changes is
//     that they now have an attested WRITER. Reported, and the header line should be narrowed to
//     "no non-zero producer" by its owner rather than left as-is.
//
//     ⚠️ WHAT IS *NOT* HERE, stated because its absence is load-bearing: the function does NOT
//     post a single InRemoveRigidBody (the owner-scoped remove-all covers every body -- the
//     drain PhysicsSimulationModule::ProcessRemoveAllRigidBodiesQueue @0x8289F1D8 sweeps all
//     200 slots matching the owner byte), does NOT touch mpaPropInstances / mpaPartInstances,
//     does NOT call RemoveProp or RemovePart, and does NOT clear mbPhysical on the world side.
//     Grepped over all 331 instructions: the ONLY `bl`s are the two in step 1, the two
//     InEventRemoveFromCache AddEvents, and the two unreachable assert blocks.
//
// ---- THE ONE ASSERT THAT IS EMITTED TWICE AND IS PROVABLY UNREACHABLE --------------------
// 0x8260F120..0x8260F218 (props) and 0x8260F370..0x8260F468 (parts) are two verbatim copies of
// IsBitSet's index precondition, inlined THROUGH GetNextNonZeroBit: the StrStream
// "invalid index : " << i << " < " << 15/30 with `li r5, 0xCB` == 203 (the same
// CgsBitArray.h:203 tripwire RemoveProp/RemovePart spell out at their own call sites).
// Neither copy can be entered, and the argument is the same one PropManager_wQ2_08.cpp:54-58
// makes for the identical inline:
//     r28 = min((r31 & ~63) + 64, LIMIT)          0x8260F0F8..0x8260F108 / 0x8260F348..0x8260F358
//     the inner-scan guard `cmplw r29,r28 ; bge` 0x8260F110/0x8260F250 (0x8260F360/0x8260F4A0)
//     means r29 < r28 <= LIMIT whenever `cmplwi r29,LIMIT ; blt` 0x8260F118 (0x8260F368) runs,
// so the fall-through into the assert block is dead code. Behaviour is identical, provably, so
// nothing is spelled here. (This is also why the function's callee list names
// CgsContainers::BasePriorityQueue::Clear @0x82815E58 twice -- both calls are INSIDE those dead
// blocks and are an ICF fold of the assert message-stream reset, not a container this TU uses.)
//
// ---- DWARF (DecFIGS) --------------------------------------------------------------------
// dwarfdump GameSource/Physics/PropManager/BrnPropManager.cpp:2265 (source BrnPropManager.cpp:420)
// and BrnPropManager.h:256/:540. The scope names the parameters and all three locals:
//     void RemoveAllPropsAndParts(InputBuffer* lpSimModuleInputBuffer,
//                                 InSceneUpdateInterface* lpSceneInput)
//     :422  InRemoveAllRigidBodies* lpEvent
//     :426  int32_t                 liPropIndex
//     :431  int32_t                 liPartIndex
// Those names are used verbatim below.
// ⚠️ DECLARATION NIT, REPORTED NOT CHANGED (BrnPropManager.h is another owner's file, and it
// MOVED under this reconstruction mid-session -- the declaration slid from :746 to :772 in the
// course of one afternoon, so no line number is quoted): the committed declaration
// (`void RemoveAllPropsAndParts(`, in the public block) names the parameters `lpSimInputBuffer` /
// `lpSceneInterface`. The DWARF names them `lpSimModuleInputBuffer` / `lpSceneInput`, which is
// also what the caller's committed body passes. Types and order are identical and MEASURED --
// `mr r5,r31 ; mr r4,r14` at 0x8263B21C/0x8263B220 hands over (simInputBuffer, sceneInput) in
// that order -- so this is a names-only divergence with no behavioural edge.
//
// ⚠️ ALSO: the DWARF's source-level call for the two cache evictions is
// InSceneUpdateInterface::RemoveCachedObject (dwarfdump BrnPropManager.cpp's ProcessInputsPreScene
// scope lists it twice, once per loop, because PS3 inlined this whole function there). That
// method is not declared in this tree, and the console emits the queue append directly -- the
// same situation, with the same committed resolution, as RemoveProp (PropManager_wQ2_04.cpp:599)
// and RemovePart (:513). The direct `mRemoveFromCacheQueue.AddEvent` form is used here to match
// its two siblings; a future RemoveCachedObject landing should convert all three together.
//
// ---- LINK-LEVEL, NOT VISIBLE TO `cl /c` (AGENTS.md gotcha 7 / 12) -------------------------
//   * RemoveAllPropsAndParts once had an inert conductor-gate twin; that gate was retired when
//     this partfile mounted, so the body below is the symbol's only definition.
//   * This TU introduces NO new unresolved external. Its five callees are:
//       InputBuffer::GetRemoveAllRigidBodiesQueue()  -- bodied this wave in the already-mounted
//                                                      CgsPhysicsSimulationModuleIO_InputBuffer.cpp
//       BaseEventQueue<InRemoveAllRigidBodies>::AddEvent()          header inline
//       BaseEventQueue<InEventRemoveFromCache>::AddEvent(const T&)  header inline
//       BitArray<15>/<30>::{GetFirstNonZeroBit,GetNextNonZeroBit,UnSetAll}  header inline
//     Everything it touches on `this` is a data member.
// ==========================================================================================


namespace BrnPhysics
{
namespace Props
{
    // The two triangle-cache slot bases. Same file-static form and same DWARF attribution the
    // two sibling partfiles use (PropManager_wQ2_02.cpp:97-98, PropManager_wQ2_04.cpp:121-122):
    // BrnTriangleCacheConstants.h has no committed home in this tree yet, so each partfile that
    // needs them carries its own internal-linkage copy rather than forking a header.
    static const s32 KI_PROP_CACHE_START_INDEX      = 28;   // DWARF BrnTriangleCacheConstants.h:36
    static const s32 KI_PROP_PART_CACHE_START_INDEX = 43;   // DWARF BrnTriangleCacheConstants.h:37

    // ======================================================================================
    // RemoveAllPropsAndParts  --  X360 0x8260F010, 331 instructions.
    // DecFIGS: dwarfdump BrnPropManager.h:256 (declaration) / BrnPropManager.cpp:2265 (scope,
    // source line :420). Parameter names and all three locals are the DWARF's.
    //
    // Drain every live prop and part back out of the simulation and out of the triangle cache.
    // Reached from ProcessInputsPreScene (PropManager_wQ2_08.cpp:162) whenever the world side
    // has set PropInputInterface::mbRemoveAllPropsAndParts -- i.e. on every world unload.
    // ======================================================================================
    void
    PropManager::RemoveAllPropsAndParts(
        CgsPhysics::PhysicsSimulationIO::InputBuffer*            lpSimModuleInputBuffer,
        CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpSceneInput )
    {
        // Step 1 -- one owner-scoped "remove every rigid body I own" request. 0x8260F02C..0x8260F048.
        // The `&` is the AddEvent/AllocateEvent return-type defect filed at
        // CgsBaseEventQueue.h:51-62; the DWARF local (:422) is a pointer, so a pointer is what
        // the console source held.
        CgsPhysics::PhysicsSimulationIO::InRemoveAllRigidBodies* const lpEvent =
            &lpSimModuleInputBuffer->GetRemoveAllRigidBodiesQueue()->AddEvent();

        lpEvent->mu8OwnerId = static_cast<u8>( BrnWorld::E_ENTITYTYPE_PROP );   // li r11,3 ; stb

        // [DIAG] NOT IN THE X360 BINARY. Opt in with BRN_PROP_DIAG; budgeted to the first four
        // teardowns (a world unload is a rare event, so a strict one-shot would hide the
        // second junkyard). Until this wave the whole function was an inert BRN_CONDUCTOR_GATE,
        // so the counts below are the first evidence that (a) the teardown request reaches
        // physics at all and (b) how many bodies and cache slots the gate was discarding with no trace.
        // They are also the direct cross-check on the three ~163 s assert families
        // (scout.md §0.3): a NON-ZERO count here on a frame the world is also posting
        // RemovePropInstance events is the double-retire the asserts are reporting.
        static const bool sbPropDiag    = ( getenv( "BRN_PROP_DIAG" ) != 0 );
        static s32        siDiagBudget  = 4;
        const bool        lbDiag        = sbPropDiag && CgsDev::Log::gpDebugPrint != 0
                                          && siDiagBudget > 0;
        s32               liDiagProps   = 0;
        s32               liDiagParts   = 0;

        // Step 2 -- evict every live PROP's triangle-cache slot. 0x8260F040..0x8260F2C8.
        // The loop guard is the console's own `cmpwi r31,-1 ; bne` at 0x8260F2C0/0x8260F2C4;
        // its `cmpwi r31,0xF ; bge` companion at 0x8260F0CC is the same exit, already folded
        // into the container's ">= tuNumBits returns KI_INVALID_BITINDEX" leg.
        for ( s32 liPropIndex = mUsedProps.GetFirstNonZeroBit();
              liPropIndex != CgsContainers::BitArray<15>::KI_INVALID_BITINDEX;
              liPropIndex = mUsedProps.GetNextNonZeroBit( liPropIndex ) )
        {
            CgsSceneManager::TriangleCacheManagerIO::InEventRemoveFromCache lRemoveFromCacheEvent;
            lRemoveFromCacheEvent.miCacheSlot = KI_PROP_CACHE_START_INDEX + liPropIndex;

            lpSceneInput->mRemoveFromCacheQueue.AddEvent( lRemoveFromCacheEvent );

            ++liDiagProps;   // [DIAG]
        }

        // Step 3 -- the same for every live PART. 0x8260F2CC..0x8260F518. Every bound in this
        // half is 0x1E == 30 == mUsedParts' capacity.
        for ( s32 liPartIndex = mUsedParts.GetFirstNonZeroBit();
              liPartIndex != CgsContainers::BitArray<30>::KI_INVALID_BITINDEX;
              liPartIndex = mUsedParts.GetNextNonZeroBit( liPartIndex ) )
        {
            CgsSceneManager::TriangleCacheManagerIO::InEventRemoveFromCache lRemoveFromCacheEvent;
            lRemoveFromCacheEvent.miCacheSlot = KI_PROP_PART_CACHE_START_INDEX + liPartIndex;

            lpSceneInput->mRemoveFromCacheQueue.AddEvent( lRemoveFromCacheEvent );

            ++liDiagParts;   // [DIAG]
        }

        // [DIAG] NOT IN THE X360 BINARY -- emitted before the wipe so the counts describe what
        // was actually live. `jointsUsed` is read here rather than counted in a loop because
        // the joint bit-set is cleared by step 4 without ever being walked, which is itself
        // worth seeing: any jointed prop (lamppost/pole) loses its joint on unload with nothing logging it.
        if ( lbDiag )
        {
            --siDiagBudget;
            *CgsDev::Log::gpDebugPrint
                << "[Q6-rmall] world unload: props=" << liDiagProps
                << " parts=" << liDiagParts
                << " jointsUsed=" << static_cast<s32>( mUsedPropJoints.CountSetBits() )
                << " (one InRemoveAllRigidBodies owner=3 posted)\n";
        }

        // Step 4 -- the six zero stores at 0x8260F51C..0x8260F530, in emission order.
        mUsedProps.UnSetAll();
        mUsedParts.UnSetAll();
        muNumberOfPropInstances = 0;
        muNumberOfPartInstances = 0;
        mUsedPropJoints.UnSetAll();
        mBreakPropJoints.UnSetAll();
    }
}
}

// ============================================================================
// FOLDED FROM PropManager_wQ6_02.cpp (wave Q6) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================
// =================================================================================================
// GameSource/Physics/PropManager/PropManager_wQ6_02.cpp
//
// Partfile of the TU GameSource/Unity/../Physics/PropManager/BrnPropManager.cpp
// (breakable-props wave Q6, ROUND 2, cluster "lean", 2026-08-19). Folds back into
// BrnPropManager.cpp.
//
// TWO FUNCTIONS -- the JOINTED-PROP contact response, i.e. what makes a car hitting a lamppost,
// a pole or a swinging sign LEAN or TILT about its joint instead of ignoring the hit:
//
//   * BrnPhysics::Props::PropManager::HandleContactWithLeanProp  @0x8260FB60  (854 instructions,
//         (0x826108B4 - 0x8260FB60)/4 + 1 == 854).  DWARF: BrnPropManager.h:442 / .cpp:1771.
//   * BrnPhysics::Props::PropManager::HandleContactWithTiltProp  @0x826108B8  (720 instructions,
//         (0x826113F4 - 0x826108B8)/4 + 1 == 720).  DWARF: BrnPropManager.h:456 / .cpp:1903.
//
// Both are called live from PropManager::SetupAndValidatePropContact
// (PropManager_wQ4_01.cpp:581 / :589, behind `lpPropInstance->IsJointed()` and the
// KU8_JOINT_TYPE_LEAN / _TILT split), which is real and mounted. Until today both were inert
// BRN_CONDUCTOR_GATEs; the gate text ("a car hitting a jointed lamppost/pole gets no lean/tilt
// response") described exactly the user-visible hole this file closes.
//
// -------------------------------------------------------------------------------------------------
// PROVENANCE. The two bodies were written COMPLETE in wave Q round 2 (2026-08-18) and parked at
//   scratchpad/waveQ2/parked/PropManager_07_HandleContactWithLeanProp.cpp
//   scratchpad/waveQ2/parked/PropManager_07_HandleContactWithTiltProp.cpp
// on EIGHT missing declarations. All eight landed with this file (see "WHAT UNBLOCKED THIS"
// below). The parked banners' §A (register map), §B (the vmaddfp vs vmaddfp128 operand-order
// split, derived from known-answer sites inside these very functions), §C (the four file-scope
// VecFloat constants recovered from their CRT-init thunks) and §D (assert-stream collapse) are
// the derivation record and are NOT repeated here; read them for the "why".
//
// ⚠️ THE PARKED TEXT WAS RE-VERIFIED AGAINST THE RAW ASM BEFORE LANDING, not copied. Sources:
//   * .ida-exports/BURNOUT_X360_ARTIST.XEX/0x8260FB60.json and 0x826108B8.json, `assembly` array
//     (dumped at scratchpad/waveQ2/probe_wq2_07/asm_0x8260FB60.txt / asm_0x826108B8.txt);
//   * references/DecFIGS/dwarfdump/GameSource/Physics/PropManager/BrnPropManager.cpp:1400-1702
//     (Lean scope) and :1703-1902 (Tilt scope) -- the local list AND the callee list;
//   * the shipped rwmath 1.02.00 vpu SDK headers,
//     which is what pinned every vpu spelling used below.
// THREE THINGS CHANGED relative to the parked text; each is a defect the park carried, each is
// flagged at its site below:
//   (1) `Select` -- the park used the tree's Vector4 argument order (false, true, mask). The SDK
//       order is Select(mask, trueValue, falseValue) (vector3_operation.h). The
//       SELECTION IS THE SAME either way; only the spelling was wrong. Fixed.
//   (2) the Mask3 -> MaskScalar lane broadcasts -- the park spelled them as free functions
//       `vpu::GetX(mask)` and flagged the spelling as a PLACEHOLDER. They are MEMBERS:
//       `Mask3::GetX/GetY/GetZ() const` (mask3.h + mask3_type_inline.h).
//       Fixed; the park's flag is discharged.
//   (3) ⭐ THE POINT-VELOCITY CALL WAS IN THE WRONG PLACE -- a real, if small, numeric defect.
//       See the ⚠️ block at step 3 of the Lean body. It is the one substantive correction.
//
// -------------------------------------------------------------------------------------------------
// WHAT UNBLOCKED THIS -- the eight declarations, all landed 2026-08-19 by this cluster:
//   game headers (2)
//     ExternalPhysicsBody::GetLinearMomentum(VecFloat) const     ExternalPhysicsBody.h/.cpp
//     Vehicle::Wheel::GetRoadLongSpeed() const                   Wheel.h (header-inline)
//   vendor rw-math (6), all in b5-decomp/vendor/renderware/include/rw/math/vpu/
//     Matrix44AffineFromAxisRotationAngle(Vector3, VecFloat)     matrix44affine_operation.h
//     GetVector3_XAxis / _YAxis / _ZAxis                         vector3_operation.h
//     struct Mask3 (+ GetX/GetY/GetZ)                            vector3_operation.h
//     Mask3 CompLessThan / CompGreaterThan (Vector3, Vector3)    vector3_operation.h
//     Vector3 Select(MaskScalar, Vector3, Vector3)               vector3_operation.h
//     MaskScalar CompLessThan / CompGreaterThan (VecFloat x2)    vector4_operation.h
// Every one is callable with exactly the shape used below -- proven by the compile probe
// scratchpad/waveQ6/probe_lean/probe_lean.cpp (STATUS=pass), which pins all eight signatures
// with function/member-pointer typedefs and carries a NaN-polarity tripwire.
//
// ⚠️ LINK, for the conductor (AGENTS.md gotcha 12) -- ONE hole, re-grepped 2026-08-19 across
//    b5-decomp/src AND b5-decomp/vendor:
//      * BrnPhysics::ExternallySimulatedBody::Translate(Vector3) -- DECLARED at
//        GameSource/Physics/PhysicsUtilities/ExternallySimulatedBody.h:105, NO definition
//        anywhere in the tree. Both bodies below call it (step 2). `cl /c` is green; the link
//        is not. That header/TU is NOT in this cluster's ownership, so it is REPORTED, not
//        written. On the console the call is INLINED to a single `stvx128` into the car body's
//        mTransform.wAxis (Lean @0x8260FD10, Tilt @0x82610A7C), so the missing body is exactly
//        `mTransform.wAxis = mTransform.wAxis + lvTranslation`.
//      * ✅ ExternalPhysicsBody::GetLocalVelocity is NOT a hole -- real body at
//        ExternalPhysicsBody.cpp:839. (Both parked banners claim it is; that claim is STALE and
//        PropManager_wQ2_07.cpp:53-56 already corrected it. Do not write a second definition.)
//
// ODR: HandleContactWithLean/TiltProp each had one inert conductor-gate twin; both gates were
//    retired when this file mounted, so the bodies below are their only definitions.
// =================================================================================================


namespace BrnPhysics
{
namespace Props
{
    namespace vpu = ::rw::math::vpu;

    namespace
    {
        // -----------------------------------------------------------------------------------
        // The four file-scope tuning constants, DWARF BrnPropManager.cpp:1755-1758 -- declared
        // immediately above HandleContactWithLeanProp at :1771, which is why they belong in
        // THIS TU and are defined exactly once for both bodies (the parked Tilt file's own ODR
        // note asked for precisely this).
        //
        // ⚠️ AGENTS.md gotcha 13 IN THE FLESH: all four read as ZERO in the shipped image. They
        // are NOT unrecoverable and they are NOT placeholders -- they are written by CRT-init
        // thunks in the un-functionised region 0x82C5E910..0x82C5E9AC, and an IDA `DataRefsTo`
        // on each returns exactly three sites (this function, the Tilt twin, and its thunk).
        // Reading the thunks gives the source float:
        //     unk_82FB9480 <- flt_8200D528 = 0.07f  -> KVF_ROTATION_FACTOR
        //     unk_82FB9410 <- flt_820047C8 = 0.05f  -> KVF_MAX_ROTATION
        //     unk_82FB9430 <- flt_82001DA0 = 0.5f   -> KVF_PENETRATION_RESOLUTION_FACTOR
        //     unk_82FB93D0 <- flt_82001DA0 = 0.5f   -> KVF_MOMENTUM_RESOLUTION_FACTOR
        // The name<->address assignment is by ROLE, not by declaration order, and each role is a
        // one-to-one match against the asm: 0x82FB9480 scales the speed that feeds the rotation
        // angle (`vmulfp128 v13, v11, v13` @0x8260FEF0) and 0x82FB9410 is the Min() ceiling on it
        // (`vminfp128 v123, v13, v12` @0x8260FF08); 0x82FB9430 scales the penetration depth
        // (`vmulfp128 v0, v13, v0` @0x8260FCF8); 0x82FB93D0 scales the momentum-derived impulse
        // (`vmulfp128 v1, v0, v13` @0x8260FCCC). INFERENCE only in that the DWARF does not pin a
        // name to an address.
        // -----------------------------------------------------------------------------------
        const VecFloat KVF_ROTATION_FACTOR               = { 0.07f, 0.07f, 0.07f, 0.07f };
        const VecFloat KVF_MAX_ROTATION                  = { 0.05f, 0.05f, 0.05f, 0.05f };
        const VecFloat KVF_PENETRATION_RESOLUTION_FACTOR = { 0.5f,  0.5f,  0.5f,  0.5f  };
        const VecFloat KVF_MOMENTUM_RESOLUTION_FACTOR    = { 0.5f,  0.5f,  0.5f,  0.5f  };

        // `lfs f0, flt_8208F5F4` @0x8260FC08 == 0.01745329238474369f -- degrees to radians, used
        // only by the mbUseOverides debug path.
        const f32 KF_DEG_TO_RAD = 0.01745329238474369f;

        // The lazily-built function-local statics of the Lean body, behind the guard word
        // dword_82FBA2A0 (bit 0 for K_EPSILON @unk_82FBA290, bit 1 for K_EPSILON3
        // @unk_82FBA280) -- the classic MSVC magic-static pattern, MEASURED
        // @0x8260FF58..0x8260FFE8, seeded from `flt_82013F90 == 0.001f`. DWARF locals
        // K_EPSILON @BrnPropManager.cpp:1841 / K_EPSILON3 @:1842. Hoisted to TU scope because
        // the host has no reason to pay the guard word; the VALUES are what matter.
        // ⚠️ The console keeps the scalar K_EPSILON only to BUILD the Vector3 (the `vperm` +
        // `vrlimi128` pair @0x8260FFCC/D4 packs the splat into {e,e,e,0}); it is never compared
        // against anything, so only the Vector3 form is reproduced.
        const Vector3 K_EPSILON3 = { 0.001f, 0.001f, 0.001f, 0.0f };

        // -----------------------------------------------------------------------------------
        // [DIAG] NOT IN THE X360 BINARY. Wave-Q6 bring-up probe, opt-in via BRN_PROP_DIAG.
        // ONE-SHOT PER ARM -- the question it answers is "does a car-vs-jointed-prop contact reach
        // this response at all?", which is a yes/no, and these two run per contact per frame once
        // a pole is being leaned on. The getenv latch is a function-local static so it costs one
        // predicted branch, never a per-contact syscall.
        //
        // ⚠️ TWO LATCHES, ONE PER JOINT KIND -- and that is a fix, not a preference. It was a
        // SINGLE TU-scope bool until 2026-08-19 (wave Q7), which meant a run that leaned could
        // never report whether TILT also ran: the first LEAN contact consumed the only latch and
        // the TILT arm went silent for the rest of the run. That is exactly how the wave-Q6
        // campaign closed with the TILT arm UNEXERCISED and nobody able to tell. Now the first
        // LEAN contact and the first TILT contact each print once, independently.
        //
        // ⚠️⚠️ WHY THIS LINE STAYED SILENT FOR A WHOLE CAMPAIGN: LEAN/TILT IS A SPEED GATE, and it
        // is a slow-speed one. PropEntityModule::GetDesiredState hands a prop E_LEANING only when
        //     mfLeanThreshold < carSpeedMPH <= mfMoveThreshold
        // and the shipped PROPS/PROPPHYSICS.BUNDLE says all 35 jointed types (of 219) carry
        // leanThreshold 0.0 with moveThreshold 20 / 35 / 40 / 70 mph. So a full-throttle 80-130 mph
        // drive classifies EVERY prop E_PHYSICAL, mu8JointIndex stays KU_NOT_JOINTED, IsJointed()
        // is false, and the contact always takes the ApplyPropRaceCarCollisionImpulse arm instead
        // of either of these two bodies. It was never a code defect.
        // ⚠️ TWO TRAPS THAT LOOK LIKE DEFECTS AND ARE NOT:
        //   * prop type 6 (a TILT type, and the FIRST prop on the straight junkyard route) ships
        //     moveThreshold 0.0, so GetDesiredState early-returns E_PHYSICAL and it can NEVER
        //     lean. "We hit a jointed prop and nothing happened" is correct behaviour there.
        //   * the [prop-diag] "speed=" field is the NORMAL COMPONENT
        //     (|Dot(carVel, contactNormal)| * MPS_TO_MPH), NOT the car speed GetDesiredState is
        //     fed (GetRaceCarSpeed == mfSpeedMPH). A glancing 130 mph pass logs speed=0.10 and
        //     still classifies E_PHYSICAL. Do not read one for the other.
        // PROVEN AT RUN TIME 2026-08-19: this line fired twice, on two different lamppost
        // instances (both type 53, joint=LEAN), at 7.6 mph and at 22.1 mph, with no new assert --
        // evidence and the throttle/steer schedules that reached it are in scratchpad/waveQ7/
        // leantest/. TILT types (3,4,6,10,29,30,38,51,52,58) all have a <= 20 mph window, tighter
        // than the type 53 that was reached, so a TILT run needs a slower schedule still.
        // -----------------------------------------------------------------------------------
        bool gbQ6LeanDiagFiredLean = false;
        bool gbQ6LeanDiagFiredTilt = false;

        enum EQ6JointKind { E_Q6_JOINT_LEAN, E_Q6_JOINT_TILT };

        void
        Q6LeanDiag( const CgsPhysics::PhysicsSimulationIO::InAddPotentialContact* lpOutContact,
                    bool                                                          lbPropIsEntityA,
                    u32                                                           luPropTypeId,
                    EQ6JointKind                                                  leJointKind )
        {
            static const bool sbPropDiag = ( getenv( "BRN_PROP_DIAG" ) != 0 );

            bool& lrbFired = ( leJointKind == E_Q6_JOINT_LEAN ) ? gbQ6LeanDiagFiredLean
                                                                : gbQ6LeanDiagFiredTilt;
            const char* const lpcJointType = ( leJointKind == E_Q6_JOINT_LEAN ) ? "LEAN" : "TILT";

            if ( !sbPropDiag || lrbFired || CgsDev::Log::gpDebugPrint == 0 )
            {
                return;
            }
            lrbFired = true;

            // Same id split the Tilt body's UpdatePropEvent tail uses (see step 7 there).
            const CgsPhysics::RigidBodyId lPropRigidBodyId(
                lbPropIsEntityA ? lpOutContact->mIDA : lpOutContact->mIDB );

            *CgsDev::Log::gpDebugPrint
                << "[Q6-lean] first lean/tilt contact prop="
                << static_cast<u32>( lPropRigidBodyId.GetEntityId() )
                << " type=" << luPropTypeId
                << " joint=" << lpcJointType
                << "\n";
        }
    }

    // =============================================================================================
    // HandleContactWithLeanProp @0x8260FB60 (854)
    //
    // §A REGISTER MAP (MEASURED from the prologue; AGENTS.md gotcha 3 -- the f32 rides f1 and
    //   SKIPS its GPR slot, and the three Vector3s ride v1/v2/v3 consuming no GPR at all):
    //     r3 = this(r18)  r4 = lpPropInstance(r20)  r5 = liPropIndex  r6 = lpType(r19)
    //     r7 = lpRaceCar(r26)  r8 = lpOutContact  r9 = lbPropIsEntityA  f1 = lfTimeStep(f31)
    //     v1 = lNormal(v127)   v2 = lPointOnProp(v120)  v3 = lPointOnCar(v122)
    //   ⚠️ MEASURED, and worth stating because it looks like a bug and is not: liPropIndex and
    //   lbPropIsEntityA are NEVER READ BY THE CONSOLE BODY. r5 is dead after the prologue and r9
    //   is overwritten at 0x8260FBD4 by `lbz r9, 0x49(r18)` (this->mbUseOverides) before any use.
    //   The Tilt twin DOES read both (it queues an UpdatePropEvent). The two share a declaration
    //   block, so the parameters are present here and unused -- except by the [DIAG] line, which
    //   is not in the X360 binary and is called out as such at its site.
    // =============================================================================================
    void
    PropManager::HandleContactWithLeanProp(
        PropInstance*                                            lpPropInstance,
        s32                                                      liPropIndex,
        const PropTypeData*                                      lpType,
        BrnPhysics::Vehicle::RaceCarPhysics*                     lpRaceCar,
        Vector3                                                  lNormal,
        Vector3                                                  lPointOnProp,
        Vector3                                                  lPointOnCar,
        CgsPhysics::PhysicsSimulationIO::InAddPotentialContact*   lpOutContact,
        bool                                                     lbPropIsEntityA,
        f32                                                      lfTimeStep )
    {
        // §A: genuinely unread by the console body. Kept named, not renamed away.
        (void)liPropIndex;

        // [DIAG] NOT IN THE X360 BINARY -- the wave-Q6 one-shot. This is the ONLY reader of
        // lbPropIsEntityA in this function; see §A.
        Q6LeanDiag( lpOutContact, lbPropIsEntityA, lpPropInstance->GetTypeId(), E_Q6_JOINT_LEAN );

        // `lfs f0, flt_82001CC0(==0.0f)` ; `stfs f0, 0x48(r8)` @0x8260FBA0/0x8260FBA8.
        // +0x48 in InAddPotentialContact is mRestitution (CgsPhysicsSimulationIO_Events.h:143,
        // DWARF CgsPhysicsSimulationModuleIO.h:207). A jointed prop absorbs the hit -- the solver
        // must not bounce the car off a lamppost that is about to swing.
        lpOutContact->mRestitution = 0.0f;

        // MEASURED: the car transform is read ONCE, at the top, from `lpRaceCar + 0x10` -- the
        // embedded ExternallySimulatedBody::mTransform (gotcha 2: the leaf's vptr occupies +0x00,
        // so the body sub-object starts at +0x10). Both the pre-Translate position
        // (`lvx128 v8, r31, 0x30` @0x8260FC4C) and the xAxis (`lvx128 v119, r0, r31` @0x8260FBE4)
        // come from this ONE read, which is why nothing below may re-read it after the Translate.
        const Matrix44Affine lCarTransform = lpRaceCar->GetTransform();

        // The prop's world transform: four 16-byte rows at lpPropInstance+0x00..0x30
        // (`lvx128 v117/v118/v116/v125` @0x8260FBB0/BCC/BDC/BF4, spilled to the stack frame).
        Matrix44Affine lTransform = lpPropInstance->GetTransform();
        const Vector3  lPos       = lTransform.wAxis;   // v115, saved before v125 is reused

        // ⚠️ The DWARF names a local `Matrix44Affine lInverseTransform` (:1775) that has NO
        // instructions of its own in the ARTIST body -- a dead local. Stated, not smoothed over;
        // deliberately not declared here.

        // lMaxAngleCos: `lvlx v0, r19, 0x48` + `vspltw v11, v0, 0` @0x8260FBC4/BD8 -- lpType+0x48
        // is PropTypeData::mfMaxJointAngleCos (console +0x48; the host packs it elsewhere --
        // gotcha 1, the console offset is a COMMENT and the code goes by name). Overridden from
        // the debug knob when this->mbUseOverides (`lbz r9, 0x49(r18)` @0x8260FBD4) is set.
        VecFloat lMaxAngleCos = vpu::Splat( lpType->GetLeanCosAngle() );
        if ( mbUseOverides )
        {
            lMaxAngleCos = vpu::Splat( std::cos( mfMaxLeanAngleOverride * KF_DEG_TO_RAD ) );
        }

        // lbRotatedTooFar: `vmsum3fp128 v0, v118, jVector` (dot3 of the prop's UP row with the
        // world Y axis) then `vcmpgefp v0, v0, v11` @0x8260FC9C then `vnot128 v114, v0`
        // @0x8260FCAC. The vnot is what makes it CompLessThan: "the prop has leaned past its
        // authored limit". Under PPC polarity (gotcha 4) a NaN lane reads TRUE, which is exactly
        // what vendor CompLessThan reproduces. Consumed only at the very end
        // (`vcmpeqfp128. v0, v114, v126` @0x82610654).
        const vpu::MaskScalar lbRotatedTooFar =
            vpu::CompLessThan( vpu::Splat( vpu::Dot( lTransform.yAxis, vpu::GetVector3_YAxis() ) ),
                               lMaxAngleCos );

        // ---- 1. how fast the car is closing on the prop AT the contact point -------------------
        // ⚠️⚠️ THIS BLOCK MOVED, AND THE MOVE IS THE ONE SUBSTANTIVE CORRECTION TO THE PARKED
        // BODY. The park computed it AFTER the Translate at step 3 and passed the pre-computed
        // lRaceCarToPropVector to GetLocalVelocity. Both halves of that were wrong against this
        // tree:
        //   * WHAT THE CONSOLE COMPUTES (MEASURED, and unambiguous):
        //       `lvx128 v8, r31, 0x30`      @0x8260FC4C   the car position, PRE-Translate
        //       `vsubfp128 v123, v122, v8`  @0x8260FC5C   r = lPointOnCar - thatPosition
        //       `lvx128 v125, r31, 0x50`    @0x8260FC98   mAngularVelocity
        //       `vpermwi128 v0, v123, 0x63` @0x8260FD08   the cross-product permute, on v123
        //     i.e. mLinearVelocity + Cross(mAngularVelocity, r) with r built from the position
        //     BEFORE the resolve store at 0x8260FD10. v123 is used NOWHERE else in the 854
        //     instructions (grepped), so the moment arm has exactly one producer and one consumer.
        //   * WHAT THE TREE'S GetLocalVelocity DOES: for WORLD_SPACE it re-derives the moment arm
        //     itself -- `lvR = lPoint - mTransform.wAxis` (ExternalPhysicsBody.cpp:842-845). So
        //     handing it the ALREADY-RELATIVE lRaceCarToPropVector subtracts the car position a
        //     SECOND time, and calling it after Translate reads a position the console never used.
        //     The park did both, which would have put the moment arm at
        //     (lPointOnCar - 2*carPos + resolveVector) instead of (lPointOnCar - carPos).
        // The faithful spelling is therefore: pass the WORLD point, and call it BEFORE the
        // Translate. That reproduces the console's arithmetic exactly while still going through
        // the by-name callee the DWARF lists (:1493 ExternalPhysicsBody::GetLocalVelocity).
        // Nothing between here and the Translate reads or writes mLinearVelocity/mAngularVelocity,
        // so no other value shifts. lRaceCarToPropVector stays as the DWARF's named local (:1777)
        // and as the documentation of what the callee rebuilds.
        //
        // ⚠️ AND THAT IS WHY THE DWARF'S `Vector3 lRaceCarToPropVector` (BrnPropManager.cpp:1777)
        // HAS NO LOCAL HERE: it is precisely the `lPoint - mTransform.wAxis` the by-name callee
        // computes for itself on the WORLD_SPACE arm. Declaring it again and passing it would BE
        // the double subtraction. The console has both because it inlined the callee and CSE'd the
        // subtraction out to the top; a by-name call cannot.
        //
        // FLAG (InputSpace): the tag itself is not recoverable from the asm -- the console inlined
        // the callee and the tag with it. WORLD_SPACE is the only one consistent with the measured
        // arithmetic (BODY_SPACE would rotate the point through the transform's 3x3, which the asm
        // plainly does not do).
        const Vector3 lPointVelocity =
            lpRaceCar->GetLocalVelocity( lPointOnCar, rw::physics::WORLD_SPACE );

        // `vmsum3fp128 v12, v12, -lNormal` -> `vmaxfp128 v12, v12, 0` (Lean's copy is scheduled
        // into the 0x8260FD20..0x8260FE00 stretch; the Tilt twin shows the same pair cleanly at
        // 0x82610AD8/0x82610B10).
        const VecFloat lVelocityAlongNormal =
            vpu::Max( vpu::Splat( vpu::Dot( lPointVelocity, -lNormal ) ), vpu::Splat( 0.0f ) );

        // ---- 2. bleed the car's momentum along the contact normal into the prop ---------------
        // MEASURED @0x8260FCA4..0x8260FCD0: GetLinearMomentum(dt) dotted with -lNormal, clamped
        // at zero, re-scaled by the normal and by KVF_MOMENTUM_RESOLUTION_FACTOR, then handed to
        // AddWorldSpaceImpulse with r3 == lpRaceCar+0x10 (the ExternalPhysicsBody sub-object).
        // ⚠️ The ARTIST image `bl`s the symbol BrnPhysics__ExternalPhysicsBody__AddWorldSpaceImpulse
        // (@0x8260FCD0); the DWARF spells the same sink `ExternalPhysicsBody::AddImpulse` (:1507).
        // The ARTIST spelling is the one this tree has, and it is what is called.
        const VecFloat lMomentumTowardsProp =
            vpu::Max( vpu::Splat( vpu::Dot( lpRaceCar->GetLinearMomentum( vpu::Splat( lfTimeStep ) ),
                                            -lNormal ) ),
                      vpu::Splat( 0.0f ) );

        lpRaceCar->AddWorldSpaceImpulse(
            lNormal * ( lMomentumTowardsProp.x * KVF_MOMENTUM_RESOLUTION_FACTOR.x ) );

        // ---- 3. push the car back out of the prop ---------------------------------------------
        // MEASURED @0x8260FCD4..0x8260FD10: Max(Dot(n, pOnCar - pOnProp) * K, 0) along n, added
        // straight into the car's mTransform.wAxis (`vmaddfp128 v13, v127, v0, v13` then
        // `stvx128 v13, r0, lpRaceCar+0x40`), i.e. ExternallySimulatedBody::Translate -- which the
        // DWARF names at :1510 and which is the LINK HOLE recorded in this file's banner.
        const VecFloat lResolveDistance =
            vpu::Max( vpu::Splat( vpu::Dot( lNormal, lPointOnCar - lPointOnProp )
                                  * KVF_PENETRATION_RESOLUTION_FACTOR.x ),
                      vpu::Splat( 0.0f ) );
        const Vector3 lResolveVector = lNormal * lResolveDistance.x;
        lpRaceCar->Translate( lResolveVector );

        // ---- 4. the joint ---------------------------------------------------------------------
        // MEASURED @0x8260FD60..0x8260FD70: `lbz r11,0x6C(r20)` (PropInstance::mu8JointIndex),
        // `addi r11,r11,0xC`, `slwi r11,r11,4`, `lvx128 v121, r11, r18` -- i.e. this + 0xC0 +
        // 16*jointIndex == maPropJointPositions[jointIndex]. The `cmplwi r11,0xFF` + assert
        // "IsJointed()" (BrnPropInstance.h:326, `li r5,0x146`) immediately before it IS
        // GetJointIndex()'s own baked tripwire, so the by-name call carries it and it is not
        // re-spelled here.
        const s32     liJointIndex        = lpPropInstance->GetJointIndex();
        const Vector3 lWorldSpaceJointPos = maPropJointPositions[ liJointIndex ];
        const Vector3 lJointToPointOnProp = lPointOnProp - lWorldSpaceJointPos;

        // BrnPropManager.cpp:1827 (`li r5,0x723`). The console streams the two vectors into the
        // message; the literal text really is the stale "lContactPoint: " (the parameter has since
        // been renamed lPointOnProp in the DWARF). §D: the StrStream construction collapses to
        // CGS_ASSERT, exactly as every committed body in this cluster does.
        CGS_ASSERT( vpu::IsValid( lJointToPointOnProp ),
                    "lContactPoint / lWorldSpaceJointPos" );

        // `lfs f0, flt_82001C98(==1.0f)` ; `fdivs f0, f0, f31` @0x8260FEE0/FEE8.
        const f32 lfInvTimeStep = 1.0f / lfTimeStep;

        // ---- 5. the lean angle and its axis ----------------------------------------------------
        // MEASURED @0x8260FEA8..0x8260FF34. The speed that drives the lean is
        // Max(rear-left wheel road-long speed, lVelocityAlongNormal), scaled by
        // KVF_ROTATION_FACTOR and ceilinged by KVF_MAX_ROTATION:
        //     addi      r11, r26, 0x360        maWheels[eRearLeftWheel].mSpeedAndMassOnWheel...
        //     vspltw    v12, v0, 0             ... lane .x  == the road-long speed
        //     vmaxfp128 v11, v12, v125         Max(that, lVelocityAlongNormal)
        //     vmulfp128 v13, v11, v13          * KVF_ROTATION_FACTOR  (unk_82FB9480)
        //     vminfp128 v123, v13, v12         Min(that, KVF_MAX_ROTATION) (unk_82FB9410)
        // 0x360 == maWheels(+0x130) + 2*0xE0 + 0x70, i.e. the REAR-LEFT wheel specifically -- not
        // an average, not the driven pair. Reproduced as written.
        const VecFloat lfAngleToRotate =
            vpu::Min( vpu::Max( lpRaceCar->GetWheel( BrnPhysics::Vehicle::eRearLeftWheel )
                                    .GetRoadLongSpeed(),
                                lVelocityAlongNormal ) * KVF_ROTATION_FACTOR,
                      KVF_MAX_ROTATION );

        // Cross(YAxis, -lNormal): the two `vpermwi128(_, 0x63)` (the .yzx swizzle) around a
        // vmulfp/vnmsubfp pair @0x8260FEC4..0x8260FF2C is the canonical VMX cross product, with
        // the Y axis loaded from unk_82181510 (`addi r28, r11, unk_82181510` @0x8260FC48).
        Vector3 lRotationAxis = vpu::Cross( vpu::GetVector3_YAxis(), -lNormal );

        // ⚠️ MEASURED ORDERING, and it is not what it looks like: maLastJointRotation is written
        // with the RAW (un-normalised) axis -- `stvx128 v125, r9, r18` @0x8260FF6C happens BEFORE
        // the normalise block at 0x8260FFEC. r9 == (jointIndex + 0x1B) << 4 == this + 0x1B0 +
        // 16*jointIndex == maLastJointRotation[jointIndex]. Do NOT "tidy" this by moving the store
        // below the normalise: the stored value is an angular RATE consumed elsewhere
        // (BrnPropManager.h:417), and normalising it first would change its magnitude.
        maLastJointRotation[ liJointIndex ] =
            lRotationAxis * ( lfAngleToRotate.x * lfInvTimeStep );

        // "is the axis (near) zero in all three lanes" -- the SDK's per-lane compares reduced by
        // three GetX/GetY/GetZ lane broadcasts and FIVE Ands @0x82610018..0x82610048. The count
        // of five is MEASURED (five `vand`), and the DWARF's callee list for this stretch is
        // literally five consecutive `rw::math::vpu::And` entries (BrnPropManager.cpp:1548-1552).
        // Under PPC polarity (gotcha 4) a NaN lane reads TRUE from CompLessThan and FALSE from
        // CompGreaterThan -- the vendor helpers reproduce both, so a NaN axis lands in the
        // "degenerate" arm below rather than propagating into the rotation matrix.
        const vpu::Mask3 lLessThan    = vpu::CompLessThan( lRotationAxis, K_EPSILON3 );
        const vpu::Mask3 lGreaterThan = vpu::CompGreaterThan( lRotationAxis, -K_EPSILON3 );

        const vpu::MaskScalar lAllLessThan =
            vpu::And( vpu::And( lLessThan.GetX(), lLessThan.GetY() ), lLessThan.GetZ() );
        const vpu::MaskScalar lAllGreaterThan =
            vpu::And( vpu::And( lGreaterThan.GetX(), lGreaterThan.GetY() ), lGreaterThan.GetZ() );
        const vpu::MaskScalar lIsZero = vpu::And( lAllLessThan, lAllGreaterThan );

        // `vsel128 v125, v0, v119, v125` @0x8261005C. PPC `vsel vD,vA,vB,vC` is `vC ? vB : vA`,
        // so vA = v0 = the normalised axis is the FALSE value and vB = v119 = lCarTransform.xAxis
        // (read at the top of the function) is the TRUE value: when the contact normal is parallel
        // to world-up the cross product degenerates and the car's own right vector becomes the
        // lean axis. SDK order is Select(mask, trueValue, falseValue).
        lRotationAxis = vpu::Select( lIsZero,
                                     lCarTransform.xAxis,
                                     vpu::NormalizeFast( lRotationAxis ) );

        CGS_ASSERT( vpu::IsValid( lRotationAxis ), "lRotationAxis" );   // .cpp:1851 (`li r5,0x73B`)

        const Matrix44Affine lRotationMatrix =
            vpu::Matrix44AffineFromAxisRotationAngle( lRotationAxis, lfAngleToRotate );

        // .cpp:1855 (`li r5,0x73F`): all four rows of the built matrix are NaN-swept before use --
        // the four-row `vspltw`/`vcmpeqfp.` cascade at 0x82610350..0x82610478.
        CGS_ASSERT( vpu::IsValid( lRotationMatrix.xAxis ) && vpu::IsValid( lRotationMatrix.yAxis )
                        && vpu::IsValid( lRotationMatrix.zAxis )
                        && vpu::IsValid( lRotationMatrix.wAxis ),
                    " Rotation matrix" );

        // ---- 6. swing the prop about its joint --------------------------------------------------
        // MEASURED @0x82610580..0x82610648. The console inlines the affine product; the DWARF's
        // call list for this stretch is exactly operator*, operator+=, TransformPoint, operator-,
        // operator-=, SetTransform. Putting -mJointLocator in the translation row BEFORE the
        // product is what makes the rotation happen ABOUT THE JOINT rather than about the origin
        // (lRotationMatrix's own wAxis is zero, so the product's wAxis is TransformVector(R,-J)).
        lTransform.wAxis = -lpType->GetJointLocator();
        lTransform       = lTransform * lRotationMatrix;
        lTransform.wAxis = lTransform.wAxis + lpType->GetJointLocator();
        lTransform.wAxis = lTransform.wAxis + lPos;

        // The joint has to end up exactly where the manager says it is; subtract the drift.
        const Vector3 lCurrentWorldSpaceJointPos =
            vpu::TransformPoint( lTransform, lpType->GetJointLocator() );
        const Vector3 lSeparation = lCurrentWorldSpaceJointPos - lWorldSpaceJointPos;
        lTransform.wAxis          = lTransform.wAxis - lSeparation;

        lpPropInstance->SetTransform( lTransform );

        // ---- 7. flag the joint for breaking ------------------------------------------------------
        // `vcmpeqfp128. v0, v114, v126` + `bne cr6, <epilogue>` @0x82610654/0x82610664, where
        // v114 == lbRotatedTooFar and v126 == zero (`vspltisw128 v126, 0` @0x8260FC44). The
        // record form sets CR6's EQ bit to mean "NO lane compared equal"; `bne cr6` branches when
        // that bit is CLEAR, i.e. as soon as ANY lane DID compare equal to zero. So the branch is
        // taken -- this block SKIPPED -- the moment a lane of the mask is zero, and the block runs
        // only when every lane is non-zero: the prop HAS leaned past its limit.
        // Both operands of the CompLessThan that produced the mask are broadcast splats, so all
        // four lanes always agree and MaskScalar::GetBool() -- which the DWARF lists at exactly
        // this point -- is an exact reproduction, not a narrowing.
        if ( lbRotatedTooFar.GetBool() )
        {
            // .cpp:1877 (`li r5,0x755` @0x826106A0, message aLppropinstance, file string r21 ==
            // aDP4B5MainBurno_225 == BrnPropManager.cpp) -- the manager's OWN joint-index
            // tripwire, distinct from the `li r5,0x146` IsJointed() assert that GetJointIndex()
            // bakes in and that rides the by-name calls bracketing it
            // (0x82610674 / 0x826106C0).
            CGS_ASSERT( liJointIndex != 0xff, "lpPropInstance->GetJointIndex() != 0xff" );

            // .cpp:1878 (`li r5,0x756`). The CgsBitArray.h:203 "invalid index : i < 15" tripwire
            // around it is IsBitSet's own baked assert and rides the by-name call.
            CGS_ASSERT( mUsedPropJoints.IsBitSet( static_cast<u32>( liJointIndex ) ),
                        "mUsedPropJoints.IsBitSet( lpPropInstance->GetJointIndex() )" );

            // `addi r25, r18, 0x678` + the sld/or/stdx set @0x826107F4/0x8261088C..0x826108A0 ==
            // mBreakPropJoints.SetBit(jointIndex).
            mBreakPropJoints.SetBit( static_cast<u32>( liJointIndex ) );
        }

        // MEASURED: unlike the Tilt twin, this body queues NO UpdatePropEvent -- there is no
        // `bl ...AddEvent` in the 854 instructions (the only non-assert `bl`s are cos,
        // AddWorldSpaceImpulse and PropInstance::SetTransform). The leaning prop's transform
        // reaches the world through the normal PostPhysicsUpdate sweep instead.
    }

    // =============================================================================================
    // HandleContactWithTiltProp @0x826108B8 (720)
    //
    // §A of the Lean body applies verbatim, with two register differences MEASURED in the Tilt
    // prologue: `mr r14, r5` @0x82610930 and `mr r15, r9` @0x82610910 -- Tilt DOES consume both
    // liPropIndex and lbPropIsEntityA, in the UpdatePropEvent tail at step 7.
    //
    // HOW TILT DIFFERS FROM LEAN (all MEASURED, side by side against 0x8260FB60):
    //   * The rotation axis is not built from a cross product and is never normalised. It is the
    //     prop's own xAxis, SIGN-FLIPPED when the contact normal points along +Z -- so there is no
    //     epsilon/Mask3 block at all here.
    //   * The body ENDS by queueing one UpdatePropEvent onto mUpdatedJointedProps, UNCONDITIONALLY
    //     (outside the break-joint `if`). Lean queues nothing.
    //     ⚠️ The DecFIGS scope names TWO `UpdatePropEvent lUpdateEvent` locals (BrnPropManager.cpp
    //     :2008 and :2025) and TWO AddEvent calls; the ARTIST image has exactly ONE `bl` to
    //     AddEvent (all 720 instructions grepped -- the only non-assert `bl`s are cos,
    //     AddWorldSpaceImpulse, PropInstance::SetTransform and that AddEvent). INFERENCE: the two
    //     source branches have identical tails and the ARTIST compiler merged them, or the second
    //     is FIGS-only. Stated, not smoothed over -- ONE AddEvent is what is reconstructed.
    // =============================================================================================
    void
    PropManager::HandleContactWithTiltProp(
        PropInstance*                                            lpPropInstance,
        s32                                                      liPropIndex,
        const PropTypeData*                                      lpType,
        BrnPhysics::Vehicle::RaceCarPhysics*                     lpRaceCar,
        Vector3                                                  lNormal,
        Vector3                                                  lPointOnProp,
        Vector3                                                  lPointOnCar,
        CgsPhysics::PhysicsSimulationIO::InAddPotentialContact*   lpOutContact,
        bool                                                     lbPropIsEntityA,
        f32                                                      lfTimeStep )
    {
        // [DIAG] NOT IN THE X360 BINARY -- the wave-Q6 one-shot, shared with the Lean arm.
        Q6LeanDiag( lpOutContact, lbPropIsEntityA, lpPropInstance->GetTypeId(), E_Q6_JOINT_TILT );

        // `stfs f0(flt_82001CC0 == 0.0f), 0x48(r16)` @0x826108F8/0x82610900 -- mRestitution.
        lpOutContact->mRestitution = 0.0f;

        // MEASURED: unlike the Lean twin (`lvx128 v119, r0, r31` @0x8260FBE4 == the car's
        // mTransform.xAxis), Tilt NEVER reads the car body's transform rows -- there is no +0x00
        // load off the body pointer anywhere in the 720 instructions. `addi r3, r30, 0x10`
        // @0x826109AC is only the ExternalPhysicsBody this-pointer for the AddWorldSpaceImpulse
        // call at 0x82610A2C, and the base register for the +0x30/+0x40/+0x50/+0xD0/+0xE0/+0x100
        // loads. The tilt axis comes from the PROP's xAxis, so no fallback basis is needed and
        // there is no lCarTransform local here.

        // `lvx128 v120/v118/v116/v124` @0x82610908/0x8261091C/0x82610944/0x8261094C -- the prop's
        // four transform rows at +0x00/+0x10/+0x20/+0x30.
        Matrix44Affine lTransform = lpPropInstance->GetTransform();
        const Vector3  lPos       = lTransform.wAxis;   // v117, saved before v124 is reused

        // `lvlx v0, r27, 0x48` + `vspltw v11, v0, 0` @0x82610934/0x82610948 ==
        // PropTypeData::mfMaxJointAngleCos. Debug override behind `lbz r8, 0x49(r18)` ==
        // this->mbUseOverides.
        VecFloat lMaxAngleCos = vpu::Splat( lpType->GetLeanCosAngle() );
        if ( mbUseOverides )
        {
            lMaxAngleCos = vpu::Splat( std::cos( mfMaxLeanAngleOverride * KF_DEG_TO_RAD ) );
        }

        // `vmsum3fp128 v0, v118, jVector` -> `vcmpgefp v0, v0, v11` -> `vnot128 v115, v0`
        // @0x826109C8/0x826109F8/0x82610A04. Same construction as Lean; consumed at the very end.
        const vpu::MaskScalar lbRotatedTooFar =
            vpu::CompLessThan( vpu::Splat( vpu::Dot( lTransform.yAxis, vpu::GetVector3_YAxis() ) ),
                               lMaxAngleCos );

        // ---- 1. closing speed at the contact point ----------------------------------------------
        // Same correction, and the same reason the DWARF's `lRaceCarToPropVector` (:1777) has no
        // local here -- read the Lean twin's block. The measurement in THIS body:
        //   `lvx128 v9, r3, r29(0x30)`   @0x826109D0  the car position, PRE-Translate
        //   `vsubfp128 v123, v122, v9`   @0x826109D8  r = lPointOnCar - thatPosition
        //   `vmr128 v121, v11`           @0x82610A00  mLinearVelocity, captured
        //   `vpermwi128 v8, v123, 0x63`  @0x82610A3C  the cross-product permute, on v123
        //   `vaddfp128 v12, v12, v121`   @0x82610AD4  + mLinearVelocity
        // all of it before the resolve store at 0x82610A7C.
        const Vector3 lPointVelocity =
            lpRaceCar->GetLocalVelocity( lPointOnCar, rw::physics::WORLD_SPACE );

        // `vmsum3fp128 v12, v12, v11(-lNormal)` @0x82610AD8 -> `vmaxfp128 v12, v12, v127(0)`
        // @0x82610B10.
        const VecFloat lVelocityAlongNormal =
            vpu::Max( vpu::Splat( vpu::Dot( lPointVelocity, -lNormal ) ), vpu::Splat( 0.0f ) );

        // ---- 2. bleed the car's momentum along the contact normal --------------------------------
        // `vmulfp128 v0, mTotalLinearForce, dt` ; `vmaddfp v0, mLinearVelocity, v0, mfMass` ;
        // `vaddfp v0, v0, mTotalLinearImpulse` ; dot with -lNormal ; Max(0) ; * lNormal *
        // KVF_MOMENTUM_RESOLUTION_FACTOR -> AddWorldSpaceImpulse @0x82610A08..0x82610A2C.
        const VecFloat lMomentumTowardsProp =
            vpu::Max( vpu::Splat( vpu::Dot( lpRaceCar->GetLinearMomentum( vpu::Splat( lfTimeStep ) ),
                                            -lNormal ) ),
                      vpu::Splat( 0.0f ) );

        lpRaceCar->AddWorldSpaceImpulse(
            lNormal * ( lMomentumTowardsProp.x * KVF_MOMENTUM_RESOLUTION_FACTOR.x ) );

        // ---- 3. push the car back out of the prop ------------------------------------------------
        // `vmsum3fp128 v13, v126, (lPointOnCar - lPointOnProp)` @0x82610A64 ; *
        // KVF_PENETRATION_RESOLUTION_FACTOR @0x82610A6C ; Max(0) @0x82610A74 ;
        // `vmaddfp128 v13, v126, v0, v13` @0x82610A78 into the car's position row, stored at
        // 0x82610A7C == ExternallySimulatedBody::Translate (the LINK HOLE, see the banner).
        const VecFloat lResolveDistance =
            vpu::Max( vpu::Splat( vpu::Dot( lNormal, lPointOnCar - lPointOnProp )
                                  * KVF_PENETRATION_RESOLUTION_FACTOR.x ),
                      vpu::Splat( 0.0f ) );
        const Vector3 lResolveVector = lNormal * lResolveDistance.x;
        lpRaceCar->Translate( lResolveVector );

        // ---- 4. the tilt axis ---------------------------------------------------------------------
        // `vmsum3fp128 v12, v126, kVector` @0x82610AA8 (kVector == unk_82181520, loaded at
        // 0x82610A94) then `vcmpgtfp128 v126, v12, v127(0)` @0x82610ACC. A BARE vcmpgtfp -- no
        // vnot -- so this one is CompGreaterThan and a NaN lane reads FALSE (gotcha 4; the pair
        // with lbRotatedTooFar above is deliberately asymmetric). DWARF local `MaskScalar
        // lNormalPointsAlongZ` @BrnPropManager.cpp:1964.
        const vpu::MaskScalar lNormalPointsAlongZ =
            vpu::CompGreaterThan( vpu::Splat( vpu::Dot( lNormal, vpu::GetVector3_ZAxis() ) ),
                                  vpu::Splat( 0.0f ) );

        // `vsel128 v126, v6, v9, v126` @0x82610AF4 with v6 == lTransform.xAxis (`vmr128 v6, v120`
        // @0x82610A58) and v9 == its sign flip (`vxor128 v9, v120, v9` @0x82610AEC, v9 being the
        // 0x80000000 splat from `vslw128 v9, v125, v125`). PPC vsel is `mask ? vB : vA`, so the
        // TRUE value is -xAxis and the FALSE value is +xAxis. No normalise: it is already a unit
        // basis row.
        const Vector3 lRotationAxis =
            vpu::Select( lNormalPointsAlongZ, -lTransform.xAxis, lTransform.xAxis );

        // `lfs f0, flt_82001C98(==1.0f)` ; `fdivs f0, f0, f31` @0x82610AB0/0x82610ABC.
        const f32 lfInvTimeStep = 1.0f / lfTimeStep;

        // Max(rear-left wheel road-long speed, lVelocityAlongNormal) * KVF_ROTATION_FACTOR,
        // ceilinged by KVF_MAX_ROTATION -- `vmaxfp v12, v10, v12` / `vmulfp128 v13, v12, v13` /
        // `vminfp128 v124, v13, v0` @0x82610B14/B18/B1C, with v10 == `vspltw v10, v12, 0` on the
        // register loaded from `addi r10, r30, 0x360` (@0x82610A48) ==
        // maWheels[eRearLeftWheel].mSpeedAndMassOnWheelVariables, lane .x.
        const VecFloat lfAngleToRotate =
            vpu::Min( vpu::Max( lpRaceCar->GetWheel( BrnPhysics::Vehicle::eRearLeftWheel )
                                    .GetRoadLongSpeed(),
                                lVelocityAlongNormal ) * KVF_ROTATION_FACTOR,
                      KVF_MAX_ROTATION );

        // `stvx128 v123, r11, r18` with r11 == (jointIndex + 0x1B) << 4 @0x82610B4C..0x82610B54,
        // i.e. this + 0x1B0 + 16*jointIndex == maLastJointRotation[jointIndex]. The `cmplwi 0xFF`
        // + assert "IsJointed()" (BrnPropInstance.h:326, `li r5,0x146`) immediately before it is
        // GetJointIndex()'s own baked tripwire and rides the by-name call.
        const s32 liJointIndex = lpPropInstance->GetJointIndex();
        maLastJointRotation[ liJointIndex ] =
            lRotationAxis * ( lfAngleToRotate.x * lfInvTimeStep );

        const Matrix44Affine lRotationMatrix =
            vpu::Matrix44AffineFromAxisRotationAngle( lRotationAxis, lfAngleToRotate );

        // BrnPropManager.cpp:1971 (`li r5,0x7B3`) -- all four rows NaN-swept before use.
        CGS_ASSERT( vpu::IsValid( lRotationMatrix.xAxis ) && vpu::IsValid( lRotationMatrix.yAxis )
                        && vpu::IsValid( lRotationMatrix.zAxis )
                        && vpu::IsValid( lRotationMatrix.wAxis ),
                    " Rotation matrix" );

        // ---- 5. swing the prop about its joint -----------------------------------------------------
        // MEASURED @0x82610FCC..0x826110D4, identical in shape to the Lean twin.
        const Vector3 lWorldSpaceJointPos = maPropJointPositions[ liJointIndex ];

        lTransform.wAxis = -lpType->GetJointLocator();
        lTransform       = lTransform * lRotationMatrix;
        lTransform.wAxis = lTransform.wAxis + lpType->GetJointLocator();
        lTransform.wAxis = lTransform.wAxis + lPos;

        const Vector3 lCurrentWorldSpaceJointPos =
            vpu::TransformPoint( lTransform, lpType->GetJointLocator() );
        const Vector3 lSeparation = lCurrentWorldSpaceJointPos - lWorldSpaceJointPos;
        lTransform.wAxis          = lTransform.wAxis - lSeparation;

        lpPropInstance->SetTransform( lTransform );

        // ---- 6. flag the joint for breaking ---------------------------------------------------------
        // `vcmpeqfp128. v0, v115, v127` + `bne cr6, loc_8261132C` @0x826110DC/0x826110EC -- the
        // same CR6 reading as the Lean twin (v115 == lbRotatedTooFar, v127 == zero), i.e.
        // MaskScalar::GetBool().
        if ( lbRotatedTooFar.GetBool() )
        {
            // .cpp:1994 (`li r5,0x7CA` @0x82611128, message aLppropinstance, file string r21 ==
            // aDP4B5MainBurno_225 == BrnPropManager.cpp) -- the Tilt twin of the Lean body's
            // .cpp:1877 tripwire; again distinct from the `li r5,0x146` IsJointed() asserts that
            // bracket it (0x826110FC / 0x82611148).
            CGS_ASSERT( liJointIndex != 0xff, "lpPropInstance->GetJointIndex() != 0xff" );

            // .cpp:1995 (`li r5,0x7CB`).
            CGS_ASSERT( mUsedPropJoints.IsBitSet( static_cast<u32>( liJointIndex ) ),
                        "mUsedPropJoints.IsBitSet( lpPropInstance->GetJointIndex() )" );

            // `addi r23, r18, 0x678` + sld/or/stdx @0x8261127C/0x82611314..0x82611328.
            mBreakPropJoints.SetBit( static_cast<u32>( liJointIndex ) );
        }

        // ---- 7. publish the new pose -----------------------------------------------------------------
        // MEASURED @0x8261132C..0x826113E0. The event is built on the stack at var_220 and its
        // field offsets fall out of the stores: +0x00..0x3F the four transform rows, +0x40 and
        // +0x50 two ZEROED vectors, +0x60 the entity id, +0x64 liPropIndex (sth), +0x66 zero (sth),
        // +0x68 zero (stb) -- exactly UpdatePropEvent's committed member sequence
        // (BrnPropEvents.h:16-23: mTransform, mLinearVelocity, mAngularVelocity, mEntityId,
        // miPhysicsSlot, miTypeId, mbFrozen).
        UpdatePropEvent lUpdateEvent;
        lUpdateEvent.mTransform = lTransform;
        lUpdateEvent.mLinearVelocity.SetZero();
        lUpdateEvent.mAngularVelocity.SetZero();

        // `ld r11, 0x30(r16)` when lbPropIsEntityA else `ld r11, 0x38(r16)` ; `srdi r11, r11, 32`
        // -- RigidBodyId::GetEntityId() is the HIGH dword of the 64-bit id (CgsRigidBody.h:53-57).
        // ⚠️ The parked body reached these two fields through a reinterpret_cast on a byte offset,
        // because the CgsPhysics event payload used to be modelled as an opaque span. THAT IS
        // STALE: mIDA/mIDB are typed, named u64 members today
        // (CgsPhysicsSimulationIO_Events.h:139-140, with offsetof static_asserts at :879-880), so
        // the by-name read below is both shorter and safer. The owner tripwire that follows
        // ("mEntityId.GetOwner() == E_ENTITYTYPE_PROP", BrnPropEntityID.h:278, `li r5,0x116`) is
        // PropEntityID's own explicit-ctor assert and rides the by-name ctor.
        const CgsPhysics::RigidBodyId lPropRigidBodyId(
            lbPropIsEntityA ? lpOutContact->mIDA : lpOutContact->mIDB );

        lUpdateEvent.mEntityId =
            BrnWorld::PropEntityID( static_cast<u32>( lPropRigidBodyId.GetEntityId() ) );

        lUpdateEvent.miPhysicsSlot = static_cast<s16>( liPropIndex );
        lUpdateEvent.miTypeId      = 0;
        lUpdateEvent.mbFrozen      = false;

        // `addi r3, r18, 0x5E10` == &mUpdatedJointedProps (the EventQueue<UpdatePropEvent,15>,
        // BrnPropManager.h:779/:838).
        mUpdatedJointedProps.AddEvent( lUpdateEvent );
    }
}
}

// ============================================================================
// FOLDED FROM PropManager_wQ_01.cpp (wave Q) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================
// GameSource/Physics/PropManager/PropManager_wQ_01.cpp
//
// BrnPhysics::Props::PropManager -- breakable-props keystone wave (waveQ, 2026-08-18), group 1.
// Part-file of the TU GameSource/Unity/../Physics/PropManager/BrnPropManager.cpp; the rest of
// this class's bodies live in BrnPropManager.cpp beside it. The block banner, the full member
// map and the retired park list are there and are NOT repeated here.
//
//     Release()   @ 0x825BAC88   (9 instructions + one `.long 0` pad, no callees) -- DONE below
//     Destruct()  @ 0x825E3398   (25 instructions, 0x825E3398..0x825E33F8)         -- DONE below
//
// (Both counts were wrong in round 1 -- "10" counted the pad word and "20" was simply short by
//  five. RE-COUNTED 2026-08-18 from the `assembly` arrays: (0x825E33F8-0x825E3398)/4+1 == 25.)
//
// Both bodies are the same reading: THE COMPILER DELETED A LOOP OVER mpaPropInstances BECAUSE
// THE CALLEE IS EMPTY. That is not a story told to explain a short function -- the DecFIGS
// dwarfdump for THIS .cpp names the loops' own locals and their source lines:
//
//     // BrnPropManager.cpp:243
//     void BrnPhysics::Props::PropManager::Release() {
//         { // BrnPropManager.cpp:245   uint32_t luIndex;
//           // BrnPropManager.cpp:246   bool     lbSuccess;  }
//     }
//     // BrnPropManager.cpp:273
//     void BrnPhysics::Props::PropManager::Destruct() {
//         { // BrnPropManager.cpp:275   uint32_t luIndex; }
//     }
//
// A `luIndex` in a function whose emission contains no loop is a loop the optimiser removed,
// and the two callees it looped over -- PropInstance::Release() / PropInstance::Destruct() --
// are already committed (PropPhysics/BrnPropInstance.h) as constant-true and empty, measured
// from exactly these two emissions. Nothing here is fabricated: the loop bounds, the array and
// the fold are each pinned below against the raw ARTIST asm.
//
// ⚠️ NOTE ON THE `bl BaseCollisionGenerator::Destruct` IN Destruct'S TAIL -- DO NOT RESURRECT
//    THE BASE-CLASS THEORY. PropManager has no base (the dwarfdump prints base classes and
//    prints none for it; Construct @0x82627390 calls PropDebugComponent::Construct with
//    r3 == r4 == this, so mDebugComponent is AT +0x00). 0x8284CB38 is an ICF-folded empty
//    `void f(T*)` body that THREE different empty bodies in this one subsystem resolve to --
//    PropDebugComponent::Construct @0x825BAD74 (where CgsDev::DebugComponent::Construct
//    belongs), PropDebugComponent::OnRegister @0x822A9750 (a bare `b` to it, where
//    DebugComponent::OnRegister belongs), and this tail. Here it is the base-class call at the
//    end of PropDebugComponent::Destruct, which is INLINED into this function -- the committed
//    PropDebugComponent::Destruct body (BrnPropDebugComponent.cpp) is exactly
//    `CGS_ASSERT(mpPropManager, ...); mpPropManager = NULL; CgsDev::DebugComponent::Destruct();`
//    and that STATEMENT RUN is what 0x825E3398 emits.
//
// ⚠️⚠️ CORRECTED 2026-08-18 (round 2) -- THE OLD WORDING HERE SAID "that is, instruction for
//    instruction, the whole of what 0x825E3398 emits", AND THAT IS FALSE IN THE COMMITTED TREE.
//    Measured, both halves:
//      * On the CONSOLE the fold target really is empty: 0x8284CB38 is a single `blr` (plus a
//        `.long 0` pad), with 193 xrefs_to. So the console's CgsDev::DebugComponent::Destruct()
//        emits nothing, and 0x825E3398's last state-changing instruction is `stw r11, 0xC(r31)`.
//      * In THIS TREE it is not empty. CgsDev::DebugComponent::Destruct()
//        (GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugComponent.cpp:57-60)
//        executes `mbActive = false;`. So on the host this function performs one store into the
//        DebugComponent base sub-object that the X360 emission does not perform.
//    THE CALL BELOW IS STILL CORRECT AND MUST STAY -- the console really does `bl` the base
//    Destruct from here; what diverges is the CALLEE's body, in a file this TU does not own.
//    The same divergence sits on the Construct side and is visible from this subsystem's own
//    asm: CgsDebugComponent.cpp:49-53 gives DebugComponent::Construct() two stores (mbActive,
//    mpDebugLinkedListNext), yet PropDebugComponent::Construct @0x825BAD58 stores ONLY +0xC,
//    +0x10 and +0x11 and `bl`s the same bare `blr` -- nothing lands in +0x00..+0x0B. Four
//    sibling teardowns agree (0x82586578 is literally `li r11,0 ; stw r11,0xC(r3) ;
//    b 0x8284CB38`; 0x825852C0 / 0x824EC9D0 / 0x825864FC likewise store only their own members
//    at >=0xC), and progress/identity.json contains no CgsDev::DebugComponent::Construct or
//    ::Destruct symbol at all. That foreign body's own comment admits it is inference ("the X360
//    inlines them; bodies reconstructed from the attested member set"); five emissions refute it.
//    CONDUCTOR ITEM (not landable from here -- foreign TU): empty CgsDev::DebugComponent::
//    Construct() and ::Destruct(), leaving the member seeding where the tree already also does
//    it, in the real C++ constructor at CgsDebugComponent.cpp:34-38.
//
//    Corroborating that PropManager has no base: BeginPropWorldContactGeneration @0x82628CB0
//    calls the generator's Prepare on its SECOND DECLARED PARAMETER, lpCollisionGenerator --
//    which arrives in r5, the third GPR slot, because `this` occupies r3 (0x82628CC4 `mr r30,r5`;
//    0x82628CD4 `mr r31,r3`; 0x82628CE0 `mr r3,r30`; 0x82628CE8 `bl BaseCollisionGenerator::
//    Prepare`) -- and NOT on `this`. Round 1 called it "the THIRD ARGUMENT", which reads as a
//    parameter position and is wrong: register number != parameter position in this repo (the
//    VecFloat lvfTimeStep rides v1 and consumes no GPR at all, so r6 is lpLinearMalloc).
//
// No console offset, stride or object size from the asm is used as a host value anywhere in
// this file -- every X360 immediate quoted below is in a comment, and the C++ reaches its data
// by member name only.


namespace BrnPhysics
{
namespace Props
{

// =========================================================================================
// BrnPhysics::Props::PropManager::Release @ 0x825BAC88   (DWARF BrnPropManager.h:132,
// definition at BrnPropManager.cpp:243).  THE WHOLE SHIPPED FUNCTION, VERBATIM:
//
//     0x825BAC88  lwz     r11, 0x88(r3)        r11 = muNumberOfPropInstances
//     0x825BAC8C  li      r3, 1                lbSuccess = true
//     0x825BAC90  cmplwi  cr6, r11, 0
//     0x825BAC94  beqlr   cr6                  zero instances -> return true
//     0x825BAC98  addi    r11, r11, -1     <-+ the loop, run BACKWARDS as a count-down
//     0x825BAC9C  clrlwi  r3, r3, 31         | r3 &= 1   <- the only surviving loop BODY
//     0x825BACA0  cmplwi  cr6, r11, 0        |
//     0x825BACA4  bne     cr6, 0x825BAC98  --+
//     0x825BACA8  blr                          return lbSuccess
//
// Reading, term by term (measured, not inferred):
//   * `+0x88` is muNumberOfPropInstances -- the trip count, so the loop bound.
//   * the count-down is the optimiser reversing a loop whose induction variable is unused in
//     the body; `luIndex` (DWARF :245) is therefore gone from the emission, which is exactly
//     why the DWARF is the evidence for it and the asm is not.
//   * `clrlwi r3,r3,31` == `r3 = r3 & 1`. A `&= 1` per iteration, with no load and no call, is
//     `lbSuccess &= <constant true>` -- i.e. the per-element call was folded to the literal 1.
//     That fold is the measurement behind the committed `PropInstance::Release() { return true; }`
//     (see BrnPropInstance.h); the two facts are one fact and must stay in sync.
//   * mpaPropInstances (+0x7C) is consequently never LOADED here. Naming it in the C++ below is
//     INFERENCE from the DWARF loop shape, not from this emission -- stated plainly because a
//     later sweep must not "discover" the missing load and delete the indexing.
//
// ⚠️ LANDING-ORDER HAZARD, MEASURED 2026-08-18 (round 2) -- for the conductor, not a code change.
//    Both loops in this file iterate on muNumberOfPropInstances (+0x88), and NOTHING WRITES IT.
//    That is now measured rather than suspected: a scan of every BrnPhysics::Props::PropManager
//    function in the ARTIST export set for a store to +0x88 or +0x98 returns ZERO hits, and in
//    particular PropManager::Prepare @0x8260EE18 -- the only plausible initialiser, and now
//    correctly addressed in BrnPropManager.h -- stores to exactly four `this` offsets (+0x80
//    mUsedProps, +0x90 mUsedParts, +0x7C mpaPropInstances, +0x8C mpaPartInstances) and neither
//    counter is among them. Prepare is ALSO still an inert boot gate (WorldLinkStubs.cpp:516),
//    so on a real boot mpaPropInstances is null as well.
//    Release is faithful either way (the X360 spins the same garbage count), but Destruct's loop
//    is one the console emits ZERO instructions for, so an UNOPTIMISED host build gets a
//    shutdown spin of up to 2^32 empty iterations that the console does not have. An optimised
//    build deletes it (PropInstance::Destruct is `{}` inline). Sequence Prepare before anything
//    calls PhysicsModule::Release/Destruct on a real boot.
//
// Sole caller: BrnPhysics::PhysicsModule::Release @0x8259C1C0.
// =========================================================================================
bool PropManager::Release()
{
    bool lbSuccess = true;

    for (u32 luIndex = 0; luIndex < muNumberOfPropInstances; ++luIndex)
    {
        lbSuccess &= mpaPropInstances[luIndex].Release();
    }

    return lbSuccess;
}

// =========================================================================================
// BrnPhysics::Props::PropManager::Destruct @ 0x825E3398   (DWARF BrnPropManager.h:136,
// definition at BrnPropManager.cpp:273).  THE WHOLE SHIPPED BODY, prologue/epilogue elided:
//
//     0x825E33AC  lwz     r11, 0xC(r31)                 mDebugComponent.mpPropManager
//     0x825E33B0  cmplwi  cr6, r11, 0
//     0x825E33B4  bne     cr6, 0x825E33D8
//     0x825E33B8  bl      CgsDev::Assert::BeginAssert
//     0x825E33BC  lis     r11, aDP4B5MainBurno_205@ha   (the @ha half of the file-string addr)
//     0x825E33C0  li      r5, 0x45                      == line 69
//     0x825E33C4  addi    r4, r11, ...@l  "d:\p4\b5_main\burnout\main\code\gamesource\unity\
//                                          ../Physics/PropManager/BrnPropDebugComponent.cpp"
//     0x825E33C8  lis     r11, aMppropmanagerN@ha
//     0x825E33CC  addi    r3, r11, ...@l  "mpPropManager != NULL"
//     0x825E33D0  bl      CgsDev::Assert::FireAssert
//     0x825E33D4  bl      CgsDev::Assert::EndAssert
//     0x825E33D8  li      r11, 0
//     0x825E33DC  mr      r3, r31                       this  (== &mDebugComponent, +0x00)
//     0x825E33E0  stw     r11, 0xC(r31)                 mpPropManager = NULL
//     0x825E33E4  bl      <0x8284CB38>                  the ICF-folded body -- see the banner;
//                                                       EMPTY on the console, NOT empty in this
//                                                       tree (one added mbActive store)
//
// (Transcript order CORRECTED 2026-08-18: round 1 printed 0x825E33C4 before 0x825E33C0 and
//  dropped both `lis ...@ha` setups, which made the two `addi r4/r3, r11, ...` read as if r11
//  were still live from the earlier `lwz r11, 0xC(r31)`. It is not -- each `addi` has its own
//  `lis` immediately above it. Prologue/epilogue (mflr/stw/std/stwu/mr and the four-instruction
//  restore + blr) are the remaining 9 of the 25 and are elided as boilerplate.)
//
// The FILE AND LINE baked into the assert are BrnPropDebugComponent.cpp:69, not
// BrnPropManager.cpp: this whole run is PropDebugComponent::Destruct INLINED, and it is
// reproduced here as the one call `mDebugComponent.Destruct()` rather than re-open-coded, so
// the assert keeps its real owner. The committed body of that callee matches the run exactly.
//
// The per-instance loop is DELETED FROM THE EMISSION -- not one instruction of it survives,
// because PropInstance::Destruct() is empty (measured from this very absence; the +0x88 load
// that Release keeps is not even present here). Its existence rests entirely on the DWARF
// local `luIndex` at BrnPropManager.cpp:275, and it is written back below because the DWARF
// is authoritative for source shape. It is a no-op at run time either way.
//
// ⚠️ THE RELATIVE ORDER OF THE TWO STATEMENTS IS NOT RECOVERABLE, and is stated rather than
//    smoothed over: one of them emitted nothing at all, so the emission cannot order it
//    against the other. `mDebugComponent.Destruct()` is written FIRST here only because it is
//    the statement the image actually shows, at the top of the body. Both are side-effect-free
//    with respect to each other (the deleted loop touches neither mDebugComponent nor
//    mpPropManager), so the choice is behaviourally immaterial.
//
// ⚠️ Nothing else is destructed here -- not mUpdatedProps, not mUpdatedJointedProps, and the
//    mpDebugWorldContacts block Construct allocated is NOT freed. That is the shipped image's
//    behaviour (the emission has no other call), and it is left alone rather than "completed".
//
// Sole caller: BrnPhysics::PhysicsModule::Destruct @0x8259C310.
// =========================================================================================
void PropManager::Destruct()
{
    mDebugComponent.Destruct();

    for (u32 luIndex = 0; luIndex < muNumberOfPropInstances; ++luIndex)
    {
        mpaPropInstances[luIndex].Destruct();
    }
}

}
}

// ============================================================================
// FOLDED FROM PropManager_wQ_02.cpp (wave Q) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================
// GameSource/Physics/PropManager/PropManager_wQ_02.cpp
//
// BrnPhysics::Props::PropManager -- breakable-props wave Q, lander 02.
// Part-file of the TU GameSource/Unity/../Physics/PropManager/BrnPropManager.cpp; the class
// banner, the member map and the tuning-global caveats live there and are NOT repeated here.
//
//     ProcessAddPartInstanceEvents()    @ 0x826280F8  (37 instructions, 0x826280F8..0x82628188)
//     ProcessRemovePartInstanceEvents() @ 0x82627818  (45 instructions, 0x82627818..0x826278C8)
//
// Both counts were COUNTED from each export's own newline-delimited `assembly` text and agree
// with (last - first)/4 + 1. Every statement below is one of those 82 instructions; nothing is
// added, removed or reordered across a `bl` barrier.
//
// ⚠️⚠️ PROVENANCE, STATED PLAINLY (2026-08-18, wave Q round 3): the round-1 copy of this file was
//    LOST FROM THE WORKING TREE during the round-3 fix pass (it was never committed, so there was
//    nothing to restore from). The two bodies below were RE-DERIVED from scratch against the raw
//    `assembly` arrays of .ida-exports/BURNOUT_X360_ARTIST.XEX/{0x826280F8,0x82627818}.json plus
//    the DecFIGS DWARF local sets -- they are a fresh reconstruction, not a recovered text, and
//    the round-1 verifier's three NITs (DWARF local names/types, the RigidBodyId gap note, the
//    instruction counts) are folded in at their final values rather than as corrections.
//
// ---- WHAT THE TWO BODIES ARE -------------------------------------------------------------------
// The two PART halves of the per-frame instance-event drain. ProcessInputsPreScene calls four of
// these in a fixed order (remove-prop, remove-part, add-prop, add-part); this file owns the two
// PART ones. Each walks one PropInputInterface queue front to back and forwards every event to
// the matching lifetime function (CreatePart / RemovePart). Neither body filters, early-outs
// mid-loop, or stores anything: they are pure drains.
//
// ---- CONSOLE VALUES ARE EVIDENCE ONLY (AGENTS.md gotcha 1) --------------------------------------
// The X360 emissions fold PropInputInterface's accessors away and land on raw offsets --
//   0x8262810C  addi r30, r4, 0xFB0    == &PropInputInterface::mAddPartQueue
//   0x82627824  addi r24, r4, 0x28CC   == &PropInputInterface::mRemovePartQueue
// and then `lwz r,8(queue)` == BaseEventQueue<T>::miLength == GetLength().
// Those two immediates are CONSOLE offsets and are meaningless on the LLP64 host (each queue
// element carries a Matrix44Affine and the queue headers differ), so they are recorded here as
// evidence ONLY. The source-level route is the DWARF's own accessor pair
// (BrnPropInputInterface.h:105 / :114, landed as GetAddPhysicalPartQueue() /
// GetRemovePhysicalPartQueue()); the drains hold a `const PropInputInterface*` and so bind the
// const overloads. The event-field offsets +0x40/+0x44/+0x46/+0x48 are likewise comments: every
// field is reached by member name (AddPhysicalPartEvent's host offsets happen to coincide with the
// console's, and RemovePhysicalPartEvent carries a static_assert(sizeof == 8) pinning the stride-8
// GetEvent I measured).
//
// ---- THE DWARF, WHICH SETTLES THE LOCALS -------------------------------------------------------
// references/DecFIGS/dwarfdump/GameSource/Physics/PropManager/BrnPropManager.cpp:1006-1030
// (ProcessAddPartInstanceEvents, source :1246) declares exactly five locals -- `lpQueue`,
// `lpEvent`, `uint32_t luQueueSize`, `uint32_t luEventIndex`, `Vector3 lVelocity` -- and its callee
// list is GetLength / GetEvent / Matrix44Affine::Matrix44Affine / Vector3::SetZero. The same file
// :698-726 (ProcessRemovePartInstanceEvents, source :711) declares `luEventIndex`, `luQueueSize`,
// `RigidBodyId lRigidBodyId`, `lpQueue`, `lpEvent`, `PropEntityID lEntityId`, `int32_t liPartIndex`,
// with callee list GetLength / GetEvent. The names and the u32 counter type below are the DWARF's,
// not an inference (both queues cap at 50 / 100, so the walk is identical either way -- but the
// spelling is free faithfulness). Two corroborations that the emission is read right: the single
// `Vector3 lVelocity` confirms ONE hoisted zero vector serving BOTH velocity arguments, and the
// `Matrix44Affine::Matrix44Affine` entry confirms the source really did materialise a by-value
// Matrix44Affine copy that the compiler elided into `r7 = the event pointer`.
//
// ---- GOTCHA SWEEP ------------------------------------------------------------------------------
// gotcha 2 (embedded sub-objects): no site -- neither body performs a single store.
// gotcha 3 (a float/vector arg skips its GPR slot): CreatePart's two Vector3s ride v1/v2 and
//   consume no GPR, which is exactly why r7 (&transform) is immediately followed by r8 (the slot).
//   The two vectors' POSITION in the parameter list is therefore NOT decidable from registers; it
//   comes from the committed CreatePart declaration in BrnPropManager.h, which this call matches
//   (⚠️ this citation used to read "BrnPropManager.h:766-774"; that line range is
//   AddContactResultsToQueue's, not CreatePart's -- the number had already drifted onto the wrong
//   declaration, which is why it is now a name)
//   positionally. No float parameters anywhere, so no FPR site.
// gotcha 4 (NaN polarity): no fcmpu / fsel / vcmp anywhere in either emission.
//
// ✅ THE TWO CALLEES THIS FILE FORWARDS TO ARE BODIED (re-grepped 2026-08-18; the round-1 banner
//    reported them as un-bodied and that went stale inside the wave):
//      PropManager::CreatePart @0x826278D0 and PropManager::RemovePart @0x8260F988 both have real
//      bodies in the sibling round-2 part-file
//      b5-decomp/src/GameSource/Physics/PropManager/PropManager_wQ2_04.cpp.
//    wQ2_04 is mounted alongside this file, which is what closes the link -- the fix was never to
//    stub either name. A trap stub beside a real body is an LNK2005 that `cl /c` cannot see.
//
// ODR: neither function has a second definition, a trap stub, or a one-shot gate anywhere
//    (grepped b5-decomp/src, the stub TUs included). This file introduces no LNK2005 and retires
//    no gate.
// ==========================================================================================


namespace BrnPhysics
{
namespace Props
{

// =================================================================================================
// BrnPhysics::Props::PropManager::ProcessAddPartInstanceEvents @ 0x826280F8  (37 instructions)
// DWARF: class decl BrnPropManager.h:372; body scope BrnPropManager.cpp:1246.
//
// Drain the add-PART queue: every event becomes one CreatePart, with ZERO initial linear and
// angular velocities.
//
// ---- REGISTER MAP, read off the prologue --------------------------------------------------------
//   r3 = this (r28) - r4 = lpInput (r30 := r4 + 0xFB0, i.e. the queue, not the interface) -
//   r5 = lpSceneInput (r27) - r6 = lpSimModuleInputBuffer (r26) - r31 = luEventIndex
//
// ---- DECODE, in emission order ------------------------------------------------------------------
//   0x82628120  lwz r29, 8(r30)          luQueueSize = GetLength(), read ONCE, before the loop
//   0x82628124  cmplwi / beq             the whole loop is skipped when the queue is empty
//   0x8262812C  vspltisw128 v127, 0      the zero velocity, hoisted OUT of the loop and AFTER the
//                                        early-out (so an empty queue materialises nothing)
//   0x82628138  bl sub_825BC5D0          BaseEventQueue<AddPhysicalPartEvent>::GetEvent(i)
//   0x82628154  lhz 0x48 + extsh -> r8   miSlot,        SIGN-extended  -> liSlotIndex (s32)
//   0x82628158  lhz 0x44 + extsh -> r5   miPropTypeId,  SIGN-extended  -> luPropTypeIndex (u32)
//   0x82628160  lhz 0x46         -> r6   miPartId,      NOT extended   -> li16PartIndex (s16)
//   0x82628168  lwz 0x40         -> r4   mEntityId
//   0x8262816C  bl CreatePart            r3=this, r7=the event (== &mTransform, the Matrix44Affine
//                                        by hidden reference), v1/v2 = the zero velocity twice,
//                                        r9=lpSceneInput, r10=lpSimModuleInputBuffer
//   0x82628174  cmplw r31, r29 / blt     UNSIGNED compare against the hoisted length
//
// ⚠️ THE miPartId ASYMMETRY IS REAL AND IMMATERIAL: the caller zero-extends it (`lhz`, no `extsh`)
//    where it sign-extends the other two halfwords. It does not matter, and that is MEASURED, not
//    assumed: r15 (the parameter) is used inside CreatePart only by `stb r15, 0x38(r31)` and
//    `extsh r31, r15` -- the callee re-sign-extends it itself -- so the committed `s16
//    li16PartIndex` spelling is exactly right and no cast is needed at the call site.
//
// ---- CALLEE CENSUS (3 `bl` targets, one of them a save helper) ----------------------------------
//   __savegprlr_26                              -- compiler helper
//   sub_825BC5D0 (no per-address export)        -- BaseEventQueue<AddPhysicalPartEvent>::GetEvent.
//        Identified by elimination, not by name: its neighbours 0x825BC520 (stride 80, xref'd only
//        by ProcessAddPropInstanceEvents) and 0x825BC728 (stride 8, xref'd only by
//        ProcessRemovePartInstanceEvents) bracket it, it occupies exactly the 0xAC-byte gap
//        (43 instructions == the stride-80 GetEvent's own length), and it is in this function's
//        xrefs_from. The committed template's GetEvent carries the mpEvents/bounds asserts.
//   PropManager::CreatePart @0x826278D0         -- bodied in PropManager_wQ2_04.cpp
// =================================================================================================
void PropManager::ProcessAddPartInstanceEvents(
    const PropInputInterface*                                lpInput,
    CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpSceneInput,
    CgsPhysics::PhysicsSimulationIO::InputBuffer*            lpSimModuleInputBuffer )
{
    const PropInputInterface::AddPhysicalPartEventQueue* lpQueue = &lpInput->GetAddPhysicalPartQueue();

    // 0x82628120 -- the bound is read ONCE, before the loop, and the loop is guarded by an
    // early-out on zero. Not re-read per iteration (unlike the sibling prop drains).
    const u32 luQueueSize = static_cast<u32>( lpQueue->GetLength() );

    // 0x8262812C `vspltisw128 v127, 0` -- ONE zero vector, hoisted out of the loop, feeding BOTH
    // velocity arguments of every CreatePart call (v1 and v2 are both `vmr128` copies of it).
    // The DWARF's single `Vector3 lVelocity` (:1252) is this. vspltisw zeroes all four lanes,
    // w included, which is exactly Vector3::SetZero's semantics.
    Vector3 lVelocity;
    lVelocity.SetZero();

    for ( u32 luEventIndex = 0; luEventIndex < luQueueSize; ++luEventIndex )
    {
        const AddPhysicalPartEvent* lpEvent = &lpQueue->GetEvent( static_cast<s32>( luEventIndex ) );

        // [DIAG] NOT IN THE X360 BINARY. Wave-Q6 pre-flight probe (opt in with BRN_PROP_DIAG),
        // FIRST EIGHT events only. It answers the wave's cheapest open question, scout.md §4.1:
        // "has a PART rigid body ever reached the simulation on this build?" -- every
        // `prop fell out of the world` id in the 2026-08-19 drive log has part index 0, i.e. all
        // six were WHOLE props, so no part had been PROVEN to reach the sim. If this line never
        // prints, the AddPartInstance events are not surviving the buffer hop and the whole
        // read-back cluster is looking at the wrong seam. Printed BEFORE CreatePart so it fires
        // even if CreatePart asserts. The env latch is a function-local static: a getenv per
        // event would be a syscall inside the drain loop.
        {
            static const bool sbPropDiag  = ( getenv( "BRN_PROP_DIAG" ) != 0 );
            static u32        suDiagCount = 0;
            if ( sbPropDiag && suDiagCount < 8u && CgsDev::Log::gpDebugPrint != 0 )
            {
                ++suDiagCount;
                *CgsDev::Log::gpDebugPrint
                    << "[Q6-part] add part entity=" << lpEvent->mEntityId.GetValue()
                    << " slot=" << static_cast<s32>( lpEvent->miSlot )
                    << " type=" << static_cast<s32>( lpEvent->miPropTypeId )
                    << "\n";
            }
        }

        // 0x8262816C -- r7 is the event pointer itself, which IS &lpEvent->mTransform (the event
        // starts with the affine); the by-value Matrix44Affine parameter is passed by hidden
        // reference, and the DWARF's `Matrix44Affine::Matrix44Affine` entry is that copy.
        CreatePart( lpEvent->mEntityId,
                    static_cast<u32>( lpEvent->miPropTypeId ),   // lhz 0x44 + extsh
                    lpEvent->miPartId,                           // lhz 0x46, no extsh -- see banner
                    lpEvent->mTransform,
                    lVelocity,
                    lVelocity,
                    lpEvent->miSlot,                             // lhz 0x48 + extsh
                    lpSceneInput,
                    lpSimModuleInputBuffer );
    }
}

// =================================================================================================
// BrnPhysics::Props::PropManager::ProcessRemovePartInstanceEvents @ 0x82627818  (45 instructions)
// DWARF: class decl BrnPropManager.h:384; body scope BrnPropManager.cpp:711.
//
// Drain the remove-PART queue: every event becomes one RemovePart.
//
// ---- REGISTER MAP, read off the prologue --------------------------------------------------------
//   r3 = this (r27) - r4 = lpInput (r24 := r4 + 0x28CC, the queue) - r5 = lpSceneInput (r26) -
//   r6 = lpSimModuleInputBuffer (r25) - r31 = luEventIndex
//
// ---- DECODE, in emission order ------------------------------------------------------------------
//   0x82627838  lwz r28, 8(r24)          luQueueSize = GetLength(), read ONCE, before the loop
//   0x8262783C  cmplwi / beq             empty-queue early-out
//   0x82627844..0x82627850               BOTH assert strings hoisted out of the loop
//                                        ("liPartIndex != KI_PROP_INDEX_NOT_FOUND" and the baked
//                                        source path) -- evidence that the asserts below are one
//                                        message repeated, not two different ones
//   0x8262785C  bl sub_825BC728          BaseEventQueue<RemovePhysicalPartEvent>::GetEvent(i)
//                                        (dumped in full: the mpEvents != NULL / liIndex <
//                                        GetLength() / liIndex >= 0 asserts, then
//                                        `slwi r11,r29,3 ; add r3,r11,r10` == stride 8)
//   0x82627860  lwz r30, 4(r3)           miPhysicalIndex
//   0x82627864  lwz r29, 0(r3)           mEntityId
//   0x82627868  cmpwi cr6, r30, -1       SIGNED, against KI_PROP_INDEX_NOT_FOUND
//   0x8262786C  bne -> 0x826278A0        so the assert block runs when the index IS -1 ...
//   0x82627870..0x8262789C               ... and it is TWO complete Begin/Fire/End triples with
//                                        baked lines 0x2DF == 735 and 0x2E1 == 737 ...
//   0x826278A0  (fall through)           ... and it does NOT skip the call: RemovePart is called
//                                        either way. Reproduced exactly: two CGS_ASSERTs, then the
//                                        unconditional call.
//   0x826278B4  bl RemovePart            r3=this, r4=mEntityId, r5=miPhysicalIndex,
//                                        r6=lpSceneInput, r7=lpSimModuleInputBuffer
//   0x826278BC  cmplw r31, r28 / blt     UNSIGNED compare against the hoisted length
//
// ⚠️ THE 735 -> 737 GAP, and why NOTHING is synthesised for it: the two asserts bake consecutive
//    ODD source lines, so source line 736 emitted no code. The DWARF supplies the leading
//    candidate for what lived there -- this function declares a local `RigidBodyId lRigidBodyId`
//    (BrnPropManager.cpp:715) that has NO counterpart anywhere in the emission. That is not a
//    dropped side effect and it is checked, not assumed: the 45 instructions contain zero
//    RigidBodyId traffic (the only `bl` targets are GetEvent, the six assert entry points and
//    RemovePart), and the DWARF's own callee list for the function is just GetLength / GetEvent.
//    So the local is dead in this build -- most likely a compiled-out / debug-only statement --
//    and inventing a statement for it is exactly what a later sweep must NOT do.
//
// ---- CALLEE CENSUS (5 `bl` targets plus the save helper) ---------------------------------------
//   __savegprlr_22                                        -- compiler helper
//   sub_825BC728 = BaseEventQueue<RemovePhysicalPartEvent>::GetEvent  -- committed template
//   CgsDev::Assert::{Begin,Fire,End}Assert                -- committed
//   PropManager::RemovePart @0x8260F988                   -- bodied in PropManager_wQ2_04.cpp
// =================================================================================================
void PropManager::ProcessRemovePartInstanceEvents(
    const PropInputInterface*                                lpInput,
    CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpSceneInput,
    CgsPhysics::PhysicsSimulationIO::InputBuffer*            lpSimModuleInputBuffer )
{
    const PropInputInterface::RemovePartEventQueue* lpQueue = &lpInput->GetRemovePhysicalPartQueue();

    // 0x82627838 -- read ONCE, before the loop, with the empty-queue early-out.
    const u32 luQueueSize = static_cast<u32>( lpQueue->GetLength() );

    for ( u32 luEventIndex = 0; luEventIndex < luQueueSize; ++luEventIndex )
    {
        const RemovePhysicalPartEvent* lpEvent =
            &lpQueue->GetEvent( static_cast<s32>( luEventIndex ) );

        const BrnWorld::PropEntityID lEntityId  = lpEvent->mEntityId;
        const s32                    liPartIndex = lpEvent->miPhysicalIndex;

        // 0x82627868 -- SIGNED compare. Both asserts carry the SAME message and the same baked
        // file; only the line differs (735 then 737). NON-GATING: the console falls straight
        // through into the call, so nothing is skipped when they fire.
        CGS_ASSERT( liPartIndex != KI_PROP_INDEX_NOT_FOUND,
                    "liPartIndex != KI_PROP_INDEX_NOT_FOUND" );   // :735
        CGS_ASSERT( liPartIndex != KI_PROP_INDEX_NOT_FOUND,
                    "liPartIndex != KI_PROP_INDEX_NOT_FOUND" );   // :737

        RemovePart( lEntityId,
                    static_cast<u32>( liPartIndex ),
                    lpSceneInput,
                    lpSimModuleInputBuffer );
    }
}

}
}

// ============================================================================
// FOLDED FROM PropManager_wQ2_01.cpp (wave Q2) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================
// GameSource/Physics/PropManager/PropManager_wQ2_01.cpp
//
// BrnPhysics::Props::PropManager -- breakable-props wave Q, ROUND 2 (2026-08-18), lander 01.
// Part-file of the TU GameSource/.../Physics/PropManager/BrnPropManager.cpp; the class block
// banner, the member map and the retired-park list live there and are NOT repeated here.
//
//     ClampAcceleration()      @ 0x82627F00   (125 instructions, 0x82627F00..0x826280F0)
//     ApplyAntiHerdingForce()  @ 0x826113F8   (114 instructions, 0x826113F8..0x826115BC)
//     ReadUpdatedBodies()      @ 0x82632918   (752 instructions, 0x82632918..0x826334D4)
//
// (All three counts RE-COUNTED this round from the exports' own `assembly` arrays -- non-blank
//  lines, first and last address printed. Round 1's banners said 78 / 87 and cited no count for
//  the third; 78 and 87 were wrong and are corrected here and nowhere else claimed.)
//
// ==================================================================================================
// WHY THIS FILE EXISTS -- three round-1 bodies, complete but parked on missing DECLARATIONS
// ==================================================================================================
// All three reconstructions were finished in round 1 and parked out-of-tree because four
// declarations they call did not exist. Round 2's shared-header owners landed all four; this file
// is the landing, with every MUST_FIX / NIT the round-1 verifiers filed against the parked text
// applied. Parked originals (kept for provenance, superseded by this file):
//     scratchpad/waveQ/parked/PropManager_05_ClampAcceleration.cpp
//     scratchpad/waveQ/parked/PropManager_05_ApplyAntiHerdingForce.cpp
//     scratchpad/waveQ/parked/PropManager_06_ReadUpdatedBodies.cpp
//
// THE FOUR DECLARATIONS, AS THEY NOW STAND IN THE TREE (verified by reading them this round):
//
//   1. GameShared/GameClasses/Physics/CgsPhysicsSimulationModuleIO.h:190, class InputBuffer,
//          InUpdateRigidBodyQueue*  GetUpdateRigidBodyQueue();            // @0x825BCCB8
//      the NON-const write twin, beside its const twin at :175. Body at
//      CgsPhysicsSimulationModuleIO_InputBuffer.cpp:232. X360: write-lock guard
//      (`lbz r11,0(r28)` + `extrwi r11,r11,1,28` == MSB0 bit 28 == LSB bit 3 ==
//      eStatusLockedForWrite -- NOT the const twin's `,1,27`) firing "Not locked for writing\n"
//      with `li r5,0x414` == source line 1044, then `addis r3,r28,1 ; addi r3,r3,-0x69E0`
//      == this + 0x9620 == +38432 == &mUpdateRigidBodyQueue. BOTH instructions matter.
//
//   2. same header :201,
//          InApplyForceQueue*       GetApplyForceQueue();                 // @0x825BCD60
//      body at CgsPhysicsSimulationModuleIO_InputBuffer.cpp:248. Same guard, baked line
//      :1051 (`li r5,0x41B`), then `addis r3,r28,1 ; addi r3,r3,0x2C30`
//      == this + 0x12C30 == +76848 == &mApplyForceQueue.
//      ⚠️ BOTH instructions are quoted deliberately. Round 1 quoted only the `addi` and asserted
//      "== this + 76848"; 0x2C30 alone is 11312 and the `addis` carries the other 0x10000. The
//      OFFSET was right and matches the committed member, but the quote as written was
//      arithmetically false. (Round-1 G6 NIT 4, applied.)
//
//   3. GameShared/GameClasses/Module/CgsBaseEventQueue.h:120,
//          T* AllocateEventSafe();                                        // @0x825E3C30
//      the bounds-GATED reserve: assert "mpEvents != NULL" (CgsBaseEventQueue.h:381,
//      `li r5,0x17D`), then `lwz 8 / lwz 4 / cmpw / bge -> li r3,0 ; blr` == SIGNED
//      miLength >= miMaxLength returns NULL WITHOUT appending, else reserve the tail slot and
//      bump miLength. ⚠️ NOT interchangeable with the committed no-arg `T& AddEvent()`, which
//      appends unconditionally.
//
//   4. BrnPropManager.h -- the ClampAcceleration and ApplyAntiHerdingForce parameter NAMES were
//      corrected to the DecFIGS DWARF's in round 2 (name-only; no type, order or count change).
//      Declaration and the definitions below now agree spelling for spelling.
//
// ⛔⛔ LINK-LEVEL ITEM FOR THE CONDUCTOR -- REPORTED, NOT ACTED ON (AGENTS.md gotcha 7)
//     A LOUD one-shot inert gate for ReadUpdatedBodies is committed at
//         b5-decomp/src/GameSource/Physics/BrnPhysicsConductorGates.cpp:499
//         BRN_CONDUCTOR_GATE("PropManager::ReadUpdatedBodies @0x82632918 (752)")
//     with a byte-identical signature. The real body below is therefore a DUPLICATE AT LINK TIME
//     (LNK2005) the moment this file is mounted. Gate retirement is conductor-only and must ship
//     in the SAME commit that adds this file to tools/build/build_game_exe.bat. This lander did
//     NOT delete it. ClampAcceleration and ApplyAntiHerdingForce have no gate or stub anywhere
//     (re-grepped BrnPhysicsConductorGates.cpp and WorldLinkStubs.cpp this round).
//
// ⛔ THE LINK HOLE THIS FILE ACTUALLY REPORTS (re-grepped 2026-08-18, round-3 fix):
//     CgsDev::DebugRender::DrawAxis(const f32*) -- declared CgsDebugRender.h:104 (that header's own
//     banner marks it DECLARATION-ONLY), with NO definition in CgsDebugRender.cpp, in any other
//     .cpp under the DebugSystem Render directory, or in WorldLinkStubs.cpp.
//     PRE-EXISTING, not introduced here: BehaviourRig.cpp:189
//     is the first caller and ReadUpdatedBodies' mbRenderCOM arm is the second. Alongside it,
//     CgsDev::DebugInterface::GetRender resolves only to the inert-but-ASSERTING link stub at
//     WorldLinkStubs.cpp:1605. Both sit on the same debug arm.
//     ✅ NOT a hole any more (round-2 correction; the round-1/round-2 banner claimed these two were
//     un-bodied and that was FALSE by the time this wave landed): PropManager::RemoveProp
//     @0x8260F540 and PropManager::RemovePart @0x8260F988 are BODIED by the sibling round-2 part-file
//     b5-decomp/src/GameSource/Physics/PropManager/PropManager_wQ2_04.cpp -- as
//     `PropManager::RemoveProp` and `PropManager::RemovePart` in that file (cited by NAME, not by
//     line: the line numbers this banner used to carry went stale the first time either file was
//     edited). Mount that file alongside this one. Do NOT stub either -- a trap stub beside
//     the real body is an LNK2005 that `cl /c` cannot see. Every other callee of all three bodies is
//     homed (census below).
//
// ⚠️ CONSOLE-VALUE DISCIPLINE (AGENTS.md gotcha 1): not one console offset, stride or record size
//    appears as a host value in any of the three bodies. Every field is reached by name or through
//    a committed accessor; the +0xNN / *192 / *112 / *64 numbers are in COMMENTS only, as the
//    evidence that fixes WHICH member or WHICH accessor.
// ==================================================================================================


namespace BrnPhysics
{
namespace Props
{

namespace
{
    // MEASURED (round 1, headless IDA 9.3 byte read of IDA Files/BURNOUT_X360_ARTIST.XEX.i64):
    // X360 flt_82004014 == `3D CC CC CD` == 0.1f. It is the slack the "always smash" test adds to
    // the move threshold (`lfs f0,0x50(type) ; fadds f0,f0,f31`).
    const f32 KF_ALWAYS_SMASH_THRESHOLD_SLACK = 0.1f;

    // MEASURED (same byte read): word 0 of X360 stru_8208F620 == 0x34000000 == 1.1920929e-07f
    // (FLT_EPSILON). It is the tolerance the inlined rw::math::vpu::IsZero uses for the "has this
    // prop started moving" test (`vandc128` the sign bits, `vrlimi128 v12,v0,1,1` to replicate x
    // into w so the 4-lane compare is a 3-lane one, then `vcmpgtfp.` and the CR6 "none true" bit).
    // ⚠️ It is NOT the 1.0e-6f default the committed IsZero declaration carries, so it is passed
    // explicitly -- an order of magnitude apart, and this decides when a prop is reported moved.
    //
    // ⚠️⚠️ ONE KNOWN NaN-POLARITY DELTA, STATED RATHER THAN PAPERED OVER (AGENTS.md gotcha 4).
    // The console's test is the CR6 "none true" bit of `vcmpgtfp.`, i.e. "no lane is ORDERED-
    // greater than eps", which is TRUE for a NaN lane. The committed rw::math::vpu::IsZero is
    // spelled `fabs(lane) <= tolerance`, which is FALSE for a NaN lane. So a prop whose linear
    // velocity contains a NaN would be treated as stationary by the console and as moving here.
    // IsZero is still called -- it is the DWARF-named helper and the tree's single home for it,
    // and forking a local three-lane copy to chase a NaN case would be worse. The exposure is
    // bounded: PropInstance::SetLinearVelocity's own RwMath::IsValid tripwire fires on the very
    // next statements of this same loop iteration. Fix it IN vector3_operation.h if it ever
    // matters, not here.
    const f32 KF_PROP_MOVED_EPSILON = 1.1920929e-07f;
}

// =================================================================================================
// BrnPhysics::Props::PropManager::ClampAcceleration  @ 0x82627F00   (125 instructions, MEASURED:
// 125 non-blank lines in the export's `assembly`, 0x82627F00 `mflr r12` .. 0x826280F0
// `b __restgprlr_28`)
//
// DWARF: dwarfdump GameSource/Physics/PropManager/BrnPropManager.cpp, dumpfile line 1049 ==
// SOURCE line :1188 (the dumpfile prints `// BrnPropManager.cpp:1188` immediately above the
// signature). Body locals at source :1191 / :1193 / :1194 / :1199 / :1211 / :1212 / :1217 / :1231.
// ⚠️ Round 1 cited ":1049" as if it were a source line. It is a DUMPFILE line. The two numbering
// systems are not interchangeable and were mixed inside one clause; corrected here.
//
// One per-frame sanity clamp on a single updated rigid body: the simulation has already produced
// new linear/angular velocities for a smashed prop, and this rejects any pair whose implied
// acceleration this frame exceeds the tuning ceiling, rewriting the velocity in place and (only
// if it actually clamped) re-posting the corrected body to the input buffer.
//
// ---- REGISTER MAP (measured, from the prologue) -------------------------------------------------
//   r3  = this            (read only as the implicit member-function receiver -- nothing loaded)
//   v1  = lLinearVelocity        (the PREVIOUS frame's linear velocity)
//   v2  = lAngularVelocity       (the PREVIOUS frame's angular velocity)
//   r4  -> r29             = lRigidBodyId (the 64-bit handle; `std r29, sp+0x60` = the event's mID)
//   r5                     = lpUpdateBodyEvent (the body itself at +0x10 -- `addi r4,r5,0x10`)
//   r6  -> r31             = &lUpdatedLinearVelocity   (in/out, `lvx128`/`stvx128 v0,r0,r31`)
//   r7  -> r30             = &lUpdatedAngularVelocity  (in/out)
//   v3                     = lvfTimeStep
//   v4                     = lvfOneOverTimeStep
//   r8  -> r28             = lpSimModuleInputBuffer
// The DWARF parameter list at source :1188 matches this positionally and supplies every name used
// below. AGENTS.md gotcha 3 in action: the four vector/VecFloat parameters ride v1..v4 and consume
// NO GPR slot, which is what makes (v1,v2,r4,r5,r6,r7,v3,v4,r8) a 9-parameter list and not a 5.
//
// ---- THE TWO SYMMETRIC BLOCKS -------------------------------------------------------------------
// Linear  @0x82627F20..0x82627FD4, angular @0x82627FD8..0x82628088. Identical instruction for
// instruction apart from which velocity pair and which pair of tuning globals they read:
//   linear : threshold unk_82FB9F40 == KVF_MAX_LINEAR_ACCELERATION_SQ,
//            magnitude unk_82FB94B0 == KVF_MAX_LINEAR_ACCELERATION
//   angular: threshold unk_82FB94D0 == KVF_MAX_ANGULAR_ACCELERATION_SQ,
//            magnitude unk_82FB9490 == KVF_MAX_ANGULAR_ACCELERATION
// (the address<->name mapping is BrnPropManager.h's, from the DecFIGS global list for this .cpp.)
//
// Each block:
//     vsubfp        dv     = *lrVelocity - lLastVelocity
//     vmulfp128     accel  = dv * lvfOneOverTimeStep
//     vmsum3fp128   lenSq  = dot3(accel, accel)                 <- THREE lanes, not four
//     vcmpgtfp.  +  beq    if (lenSq > threshold) { ... }
//     ...renormalise, then
//     vmaddfp    +  stvx128   *lrVelocity = clamped * lvfTimeStep + lLastVelocity
//     li r10, 1             the shared `lbUpdateBody` latch (ONE latch, set by either block)
//
// ⚠️ vmaddfp OPERAND ORDER, ESTABLISHED EMPIRICALLY BEFORE ANY MULTIPLY-ADD WAS TRUSTED (a wrong
//    rule here corrupts correct math silently, which is why it is re-derived rather than quoted):
//    IDA prints four fields; calibrating against the rsqrt Newton sequence at 0x82627F8C..0x82627F90
//    (`vnmsubfp v7, v0, v13, v31` must be `1.0 - x*y0^2` and `vmaddfp v12, v30, v12, v7` must be
//    `0.5*y0*resid + y0`, the only reading that is a Newton step at all) fixes the rule as
//    printed(op1,op2,op3,op4) => op1 = op2*op4 + op3. Under that rule `vmaddfp v0, v0, v1, v3`
//    @0x82627FD0 is accel*lvfTimeStep + lLinearVelocity, which is what is spelled below.
//
//    ⚠️⚠️ THAT RULE HOLDS FOR THE **PLAIN** `vmaddfp` / `vnmsubfp` MNEMONICS ONLY. The VMX128 forms
//    (`vmaddfp128` / `vnmsubfp128`) print in the OPPOSITE order: op1 = op2*op3 + op4 (and the
//    nmsub form is -(op2*op3) + op4). Re-derived in round 3 from the SAME Newton idiom, where this
//    build emits one of each back to back inside ReadUpdatedBodies below:
//        0x826331E0 `vnmsubfp128 v31, v4, v2, v31`  (v4 = lenSq, v2 = y0^2, v31 = 1.0)
//                        is the residual ONLY as -(op2*op3) + op4  == 1 - lenSq*y0^2
//        0x826331E4 `vmaddfp    v11, v1, v11, v31`  (v1 = 0.5*y0, v11 = y0)
//                        is the Newton step ONLY as   op2*op4 + op3 == 0.5*y0*resid + y0
//    (it repeats identically at 0x826331F4 / 0x82633200, and the extra-COM cascade at
//     0x82632FF4/0x82632FF8 -- `vmaddfp128 v13, v125, v12, v13` -- is a basis-row accumulation only
//     under the VMX128 order; the plain order turns it into row1*(row0*off.x) + off.y, i.e. noise.)
//    ⚠️ NEITHER ClampAcceleration NOR ApplyAntiHerdingForce CONTAINS A SINGLE VMX128 MULTIPLY-ADD:
//    re-checked instruction by instruction in round 3 -- all 18 multiply-adds in 0x82627F00 and all
//    8 in 0x826113F8 are the plain mnemonics (counted: 18/0 and 8/0 plain-vs-128), so the plain rule
//    is the applicable one at every site
//    quoted in this file's two banners and NO landed expression changes. The distinction is written
//    down because a reader who carries the plain rule across into decode (G)'s VMX128 lines below
//    would "correct" correct code.
//
// ---- THE RENORMALISE (0x82627F58..0x82627FCC), decoded instruction by instruction ---------------
//   vrsqrtefp + two Newton steps  -> rsqrt(lenSq)
//   vmulfp128 v0, v0, v12         -> lenSq * rsqrt(lenSq) == |accel|         (a LENGTH, not a rsqrt)
//   vsel v0, v0, v5(=0), v10      -> v10 is `vcmpeqfp v10, 0, lenSq`: the zero-length lane is
//                                    forced to ZERO rather than left as the estimate's garbage.
//   vrefp + two Newton steps      -> 1 / |accel|
//   vmulfp128 v0, v0, v8          -> KVF_MAX_*_ACCELERATION / |accel|
//   vmulfp128 v0, v9, v0          -> accel * (MAX / |accel|)   == the clamped acceleration
//
//   ⚠️⚠️ THE ZERO-LENGTH GUARD, STATED CORRECTLY (round-1 MUST_FIX, applied). Round 1 claimed the
//   console's `vsel` guard "is exactly the guard vpu::Magnitude/Normalize document in the vendor
//   header". THAT IS FALSE FOR Magnitude AND IT IS THE ONE USED HERE:
//     * vendor/renderware/include/rw/math/vpu/vector3_operation.h:140-143 -- vpu::Magnitude is a
//       bare `return std::sqrt(MagnitudeSquared(lrVector));`. NO guard, and its banner claims none.
//     * The guard lives on Normalize (:151-160) and NormalizeReturnMagnitude (:178-191). NEITHER
//       is called here.
//   So the host spelling below IS an unguarded std::sqrt. It is safe here for one reason and one
//   reason only: the enclosing branch requires lenSq > KVF_MAX_*_ACCELERATION_SQ, and that
//   threshold is a squared magnitude (>= 0), so the zero-length lane cannot reach the divide.
//   Do NOT "restore" a guard that was never there, and do NOT assume one is protecting this line.
//   (For the record, the two spellings agree even at zero: the console's vsel forces |a| := 0 and
//   then takes vrefp(0); the host takes MAX/0. Both produce an infinity, not a quiet zero.)
//
//   ⚠️ THE `MAX / Magnitude(a)` DIVIDE SPELLING IS DWARF-ATTESTED, NOT INFERENCE (round-1 NIT,
//   applied -- round 1 labelled it INFERENCE). The dumpfile's clamp-branch scopes declare
//   `VecFloat lvfAccelerationMagnitude` at source :1199 and `lvfAngularAccelerationMagnitude` at
//   :1217, each followed by, in order, Magnitude / operator/ / operator*= / operator* / operator+.
//   That is a sqrt then a divide -- which is also why the console emits sqrt THEN a reciprocal
//   rather than one rsqrt scale. ⚠️ Note the DWARF's `operator*=`: the SOURCE mutated the
//   acceleration local (`lAcceleration *= (MAX / mag)`); this reconstruction spells a fresh
//   non-mutating Mult on a `const` local. Numerically identical, recorded so a later 1:1 pass does
//   not re-derive it as a divergence.
//
// ---- NaN POLARITY (AGENTS.md gotcha 4) ----------------------------------------------------------
// The guard is `vcmpgtfp.` + `mfocrf`/`extrwi ...,1,24` (CR6 bit 0 == "all lanes true") + `beq`,
// i.e. a VECTOR compare tested for truth -- NOT an `fcmpu`+`bge`. Operand order re-read this round:
// `vcmpgtfp. v13, v0, v13` with v0 == lenSq and v13 == the threshold, so it is lenSq > threshold,
// not the reverse. vcmpgtfp is FALSE for an unordered pair, and C++ `>` is FALSE for NaN, so the
// plain `>` below has the console's polarity with no negated-predicate rewrite. (The bge/ble trap
// applies to fcmpu branches; this function emits none.) lenSq is a broadcast, so "all lanes" is
// "the lane".
//
// ---- THE RE-POST (0x8262808C..0x826280E8) --------------------------------------------------------
//   if (lbUpdateBody):
//       sp+0x60 = the event.   `std r29, sp+0x60`         == lOutUpdateRigidBodyEvent.mID
//       `addi r3, sp+0x70` / `addi r4, r5, 0x10` / `bl rw::physics::RigidBody::operator=`
//                                                          == event.mRigidBody = lpUpdateBodyEvent->mRigidBody
//       two `vrlimi128 vD, vOld, 1, 0` splices at sp+0x90 and sp+0xA0 -- mask 1 == the **w** field
//       only, i.e. x/y/z come from the clamped velocity and the body's existing w lane is
//       PRESERVED. sp+0x90 / sp+0xA0 are (body+0x20) / (body+0x30) == mVel / mOmega, which is
//       exactly what rw::physics::RigidBody::Set{Linear,Angular}Velocity already spell (that
//       header's own banner records the same vrlimi w-preservation).
//       then GetUpdateRigidBodyQueue() -> AddEvent(event).
//   ⚠️ Those event/body offsets are CONSOLE stack offsets, quoted as evidence only -- the code
//      below reaches every field through a named member, never through arithmetic.
//
// ---- CALLEE CENSUS (measured: 4 `bl` in the export, one of them a save helper) -------------------
//   __savegprlr_28                                    -- compiler helper
//   "CgsPhysics_" (IDA-TRUNCATED symbol) @0x825BCCB8  -- InputBuffer::GetUpdateRigidBodyQueue, LANDED
//   InUpdateRigidBody::AddEvent          @0x82614928  -- committed (the one-arg bool AddEvent(const T&))
//   rw::physics::RigidBody::operator=    @0x825E3410  -- committed
//   No StrStream/assert call at all. ⚠️ The DWARF DOES carry two local-`StrStream` diagnostic
//   blocks for this function (dumpfile 1118-1129, the second ending in
//   BaseEventQueue<InUpdateRigidBody>::GetMaxLength). They have NO ARTIST counterpart -- there is
//   no StrStream `bl` anywhere in the 125 instructions -- so they are correctly absent. Negative
//   evidence recorded so the next pass does not "restore" invented diagnostics.
//
// ⭐ THE CONSOLE-VALUE WARNING THAT STOOD HERE IS RETIRED (2026-08-18 round 3b). It said "the
// ARTIST image contains no initialiser for any of these four globals", predicted that
// KVF_MAX_*_ACCELERATION_SQ == 0 would make this function clamp EVERY body every frame down to
// last frame's velocities, and asked for that to be reported rather than fixed. The prediction
// was right about the consequence and wrong about the cause: the initialisers exist, as MSVC
// dynamic-initialiser thunks outside every IDA function. All four are now seated at their
// measured console values in BrnPropManager.cpp:
//     KVF_MAX_LINEAR_ACCELERATION     = Splat(30.0f)    KVF_MAX_LINEAR_ACCELERATION_SQ  = Splat(900.0f)
//     KVF_MAX_ANGULAR_ACCELERATION    = Splat(80.0f)    KVF_MAX_ANGULAR_ACCELERATION_SQ = Splat(6400.0f)
// so the clamp is a real 30 m/s^2 / 80 rad/s^2 ceiling, and the two _SQ thresholds are exactly the
// squares of their siblings (which is how the console computes them -- see the _SQ note in
// BrnPropManager.cpp). Thunk / table-slot / rodata provenance per constant: BrnPropManager.h.
// =================================================================================================
void PropManager::ClampAcceleration( Vector3                                                    lLinearVelocity,
                                     Vector3                                                    lAngularVelocity,
                                     CgsPhysics::RigidBodyId                                    lRigidBodyId,
                                     const CgsPhysics::PhysicsSimulationIO::OutUpdateRigidBody* lpUpdateBodyEvent,
                                     Vector3&                                                   lUpdatedLinearVelocity,
                                     Vector3&                                                   lUpdatedAngularVelocity,
                                     VecFloat                                                   lvfTimeStep,
                                     VecFloat                                                   lvfOneOverTimeStep,
                                     CgsPhysics::PhysicsSimulationIO::InputBuffer*              lpSimModuleInputBuffer )
{
    namespace vpu = rw::math::vpu;

    // [DIAG] NOT IN THE X360 BINARY -- 2026-09-02 props-at-speed EXPERIMENT KNOB. BRN_PROP_NOCLAMP=1
    // skips this whole clamp (and its re-post) so a boot-drive can measure what the 30 m/s^2 /
    // 80 rad/s^2 ceiling costs a flung prop. The console value and argument order were re-verified
    // against the image the same day (thunk 0x82C5E830 = 30.0f; v3 = dt, v4 = 1/dt), so with the
    // knob OFF this function is the console's. DELETE-WHEN the prop-launch question is settled.
    {
        static const bool sbNoClamp = ( getenv( "BRN_PROP_NOCLAMP" ) != 0 );
        if ( sbNoClamp )
        {
            return;
        }
    }

    // :1191
    bool lbUpdateBody = false;

    // ---- linear block @0x82627F20 ---------------------------------------------------------------
    {
        // :1193 / :1194
        const Vector3  lAcceleration =
            vpu::Mult(vpu::Subtract(lUpdatedLinearVelocity, lLinearVelocity), lvfOneOverTimeStep.x);
        const VecFloat lvfAccelerationMagnitudeSquared = vpu::Splat(vpu::MagnitudeSquared(lAcceleration));

        if (lvfAccelerationMagnitudeSquared.x > KVF_MAX_LINEAR_ACCELERATION_SQ.x)
        {
            // :1199  accel * (MAX / |accel|) * dt + lastVelocity -- see the "renormalise" note.
            // vpu::Magnitude is an UNGUARDED std::sqrt; the branch above is what makes the
            // zero-length lane unreachable.
            const f32 lfClampScale =
                KVF_MAX_LINEAR_ACCELERATION.x / vpu::Magnitude(lAcceleration);

            lUpdatedLinearVelocity =
                vpu::Add(vpu::Mult(vpu::Mult(lAcceleration, lfClampScale), lvfTimeStep.x),
                         lLinearVelocity);
            lbUpdateBody = true;
        }
    }

    // ---- angular block @0x82627FD8 --------------------------------------------------------------
    {
        // :1211 / :1212
        const Vector3  lAngularAcceleration =
            vpu::Mult(vpu::Subtract(lUpdatedAngularVelocity, lAngularVelocity), lvfOneOverTimeStep.x);
        const VecFloat lvfAngularAccelerationMagnitudeSquared =
            vpu::Splat(vpu::MagnitudeSquared(lAngularAcceleration));

        if (lvfAngularAccelerationMagnitudeSquared.x > KVF_MAX_ANGULAR_ACCELERATION_SQ.x)
        {
            // :1217
            const f32 lfClampScale =
                KVF_MAX_ANGULAR_ACCELERATION.x / vpu::Magnitude(lAngularAcceleration);

            lUpdatedAngularVelocity =
                vpu::Add(vpu::Mult(vpu::Mult(lAngularAcceleration, lfClampScale), lvfTimeStep.x),
                         lAngularVelocity);
            lbUpdateBody = true;
        }
    }

    // ---- the corrected re-post @0x8262808C ------------------------------------------------------
    if (lbUpdateBody)
    {
        // :1231
        CgsPhysics::PhysicsSimulationIO::InUpdateRigidBody lOutUpdateRigidBodyEvent;

        lOutUpdateRigidBodyEvent.mID        = lRigidBodyId;                     // `std r29, sp+0x60`
        lOutUpdateRigidBodyEvent.mRigidBody = lpUpdateBodyEvent->mRigidBody;    // RigidBody::operator= @0x825E3410

        // The two `vrlimi128 ...,1,0` splices: xyz from the clamped velocities, w preserved.
        lOutUpdateRigidBodyEvent.mRigidBody.SetLinearVelocity(lUpdatedLinearVelocity);
        lOutUpdateRigidBodyEvent.mRigidBody.SetAngularVelocity(lUpdatedAngularVelocity);

        // @0x826280E0 `bl sub_825BCCB8` then @0x826280E8 the one-arg AddEvent. The accessor is the
        // NON-const twin landed at CgsPhysicsSimulationModuleIO.h:190 this round.
        const bool lbPosted =
            lpSimModuleInputBuffer->GetUpdateRigidBodyQueue()->AddEvent(lOutUpdateRigidBodyEvent);

        // [DIAG] NOT IN THE X360 BINARY -- 2026-09-06 props lane (b5-decomp#2). The POST side of
        // the pair whose CONSUMER side is the [prop-updrb] line in
        // PhysicsSimulationModule::ProcessUpdateRigidBodyQueue. Measured on run
        // scratch\bugtest\runs\props_hit_lean\20260906_101608: a hit prop's position keeps
        // tracking the SIM's velocity (~17.7 m/s) while this clamped value creeps at the
        // console's 30 m/s^2 -- i.e. the correction is computed and stored into the
        // PropInstance but never reaches the rigid body. These two lines say which half fails.
        // Opt-in, first-N. DELETE-WHEN b5-decomp#2 is closed.
        {
            static const bool sbPostDiag = ( getenv( "BRN_PROP_DIAG" ) != 0 );
            static s32        siPostLinesLeft = 400;
            if ( sbPostDiag && siPostLinesLeft > 0 && CgsDev::Log::gpDebugPrint != 0 )
            {
                --siPostLinesLeft;
                *CgsDev::Log::gpDebugPrint
                    << "[prop-clamp-post] entity="
                    << static_cast<u32>( lRigidBodyId.GetEntityId() )
                    << " posted=" << ( lbPosted ? 1 : 0 )
                    << " queueLen="
                    << lpSimModuleInputBuffer->GetUpdateRigidBodyQueue()->GetLength()
                    << " v=(" << lUpdatedLinearVelocity.x
                    << "," << lUpdatedLinearVelocity.y
                    << "," << lUpdatedLinearVelocity.z << ")"
                    << "\n";
            }
        }
    }
}

// =================================================================================================
// BrnPhysics::Props::PropManager::ApplyAntiHerdingForce  @ 0x826113F8   (114 instructions, MEASURED:
// 114 non-blank lines, 0x826113F8 `mflr r12` .. 0x826115BC `blr`)
//
// DWARF: dwarfdump BrnPropManager.cpp dumpfile line 2318 == SOURCE line :2111 (`// BrnPropManager
// .cpp:2111` sits immediately above the signature). Body lines :2113..:2165 ARE source lines.
// ⚠️ Round 1 cited ":2318" as a source line. Corrected, same defect as ClampAcceleration's.
//
// A prop that a race car is pushing along ("herding") gets shoved sideways and, above a speed
// threshold, upward as well -- so it stops riding the bumper. Posted as one InApplyForce.
//
// ---- PARAMETER NAMES: THE ROUND-1 HEADER DEFECT IS NOW FIXED AT SOURCE ---------------------------
// Round 1 filed BrnPropManager.h's names for this function as WRONG and the parked body carried a
// long banner describing the divergence. Round 2's header owner applied the rename, so THAT BANNER
// IS RETIRED -- keeping it would read as a live defect against a header that no longer has one.
// What remains true and worth keeping is the EVIDENCE for the names, because it is also the
// evidence for what each parameter means:
//   * lPropWorldPos       -- v1 is transformed by the car's INVERSE affine => a world POSITION.
//   * lvfPropMass         -- v2 multiplies a force => a MASS. (The old name was `lvfScale`; a
//                            future caller reading "scale" would pass a dimensionless number.)
//   * lPropLinearVelocity -- v3 is differenced against the car's linear velocity and dotted with
//                            the car's up axis; the DWARF's own local for that value is
//                            `lvfPropsUpwardVelocity` => a VELOCITY, not a normal.
//   * lCollisionNormal    -- position 7, and the genuinely UNUSED one: v4 is clobbered at
//                            0x82611448 by `vspltw v4, v11, 0` (the COM-offset splat) before any
//                            read. Declared per the DWARF anyway (the asm cannot disprove an
//                            unused parameter) and named in the definition below with the name
//                            commented out, so the "unused" fact is visible at the definition too.
//
// ---- REGISTER MAP (measured) --------------------------------------------------------------------
//   r3 = this  -- ⚠️ NEVER READ. `mr r3, r4` at 0x82611558 overwrites it before the only call.
//   r4 = lpSimInputBuffer      r5 = lpRaceCar      r6 = lPropRigidBodyId (`std r6, sp+0x70`)
//   v1 = lPropWorldPos   v2 = lvfPropMass   v3 = lPropLinearVelocity   v4 = lCollisionNormal (dead)
//
// ---- THE FIVE RaceCarPhysics READS (all have committed accessors -- no offset arithmetic) --------
//   +0x10..+0x40  ExternallySimulatedBody::GetTransform()            (the four affine rows)
//   +0x50         ExternallySimulatedBody::GetLinearVelocity()
//   +0x670        GetSimpleAttribs()->mCOMOffset
//   +0x6A0        GetHalfExtent()          -- only lane .y is read (`vspltw v26, v5, 1`)
//   +0x6C0        GetSpeedMPH()            -- a broadcast VecFloat
//   (those +0xNNN are CONSOLE offsets, quoted as the evidence that fixes WHICH accessor; the
//    code below never does offset arithmetic.)
//
// ---- DECODE, in emission order (re-read this round) ---------------------------------------------
//   0x82611470  vandc  v11, speedMPH, 0x80000000     |speedMPH|
//   0x8261148C  vminfp v11, v11, KVF_SPEED_CLAMP     lvfRaceCarSpeed
//   0x826114A8  vmaddfp cascade over the three rotation rows by mCOMOffset.x/.y/.z, then
//   0x826114B0  vsubfp v9, wAxis, that               lCarTransform.Pos() -= TransformVector(...)
//   0x826114C0  vmsum3fp128(lPropWorldPos - Pos(), xAxis)               -> Dot #1
//   0x826114E8/0x826114EC/0x826114F0/0x826114F8  vcmpgtfp + vcmpgefp + two vsel  == Sgn(Dot #1)
//               (self-consistency check: `vmr v7, v8` @0x82611490 and `vsubfp v4, v0, v8`
//                @0x82611494 make v7 == +1.0 and v4 == -1.0 only if v8 == 1.0, which is the
//                reading that makes the pair of vsels a sign function.)  -> lSidewaysDirection
//   0x826114F4..0x82611518  the vmrghw/vmrglw 4x4 transpose of (xAxis,yAxis,zAxis,0), plus the
//               splatted -Pos() cascade: this is InverseOfMatrixWithOrthonormal3x3 inlined --
//               its rows ARE the transposed columns and its wAxis IS -(Pos() . row_i).
//   0x82611534..0x8261153C  the same cascade applied to lPropWorldPos == TransformPoint(inverse, p)
//                                                                      -> lPropRelativePosition
//   0x82611548  vsubfp   lPropRelativePosition.y - GetHalfExtent().y    -> lvfPropHeightAboveCar
//   0x82611550  vcmpgtfp against 0                                      -> lAboveRaceCar
//   0x82611508/0x82611520/0x82611528  lSideForce = (xAxis * Sgn) * lvfRaceCarSpeed * lvfPropMass
//   0x82611530  v5  = lSideForce * KVF_ANTI_HERD_HIGH_SPEED_SIDE_SCALE  (flt_82FB9D70)
//   0x8261157C  v11 = lSideForce * KVF_ANTI_HERD_SIDE_SCALE             (flt_82FB9450)
//   0x82611554/0x82611580  vmsum3fp128(lPropLinearVelocity - GetLinearVelocity(), yAxis)  -> Dot #2
//   0x8261156C  lvfTargetUpwardVel = lvfRaceCarSpeed * KVF_ANTI_HERD_UPWARD_SCALE (flt_82FB93E0)
//   0x82611584/0x82611588/0x8261158C  lForceMagnitude = Max(target - upwardVel, 0) * lvfPropMass
//   0x82611590  vmaddfp v0, v12, v5, v0  == yAxis * lForceMagnitude + v5   (the PLAIN-mnemonic
//                                           operand-order rule, op1 = op2*op4 + op3, re-derived in
//                                           ClampAcceleration's banner. This function emits EIGHT
//                                           multiply-adds and ZERO `vmaddfp128`/`vnmsubfp128`
//                                           (counted), so the VMX128 exception noted there does not
//                                           reach any line here. Under the VMX128 order this
//                                           instruction would read yAxis*(lSideForce*scale) +
//                                           lForceMagnitude, which is not a force at all.)
//   0x82611594  vsel v0, v11, v0, v24  -- v24 == `vcmpgtfp v24, v11, flt_82FB93A0` ==
//               (lvfRaceCarSpeed > KVF_MAX_SPEED_FOR_SIDE_FORCE): TRUE -> the sum, FALSE -> v11
//   0x82611598  vsel v0, v0, v25, v9   -- v9 == lAboveRaceCar: TRUE -> ZERO (v25, `vmr v25, v0`
//               @0x82611480 while v0 was still the zero register)
//   0x826115A0/0x826115A8  GetApplyForceQueue() -> AddEventSafe(event)
//   (vsel vD,vA,vB,vC == per-lane `mask ? vB : vA`, which is why each TRUE arm above is the THIRD
//    printed operand, not the second.)
//
// ⭐ CORROBORATION, not just this reading: the DecFIGS callee list for this function (dumpfile
// references/DecFIGS/dwarfdump/GameSource/Physics/PropManager/BrnPropManager.cpp:2366-2395, quoted
// COMPLETE and in order -- round-2 NIT, applied; the earlier transcription omitted two entries
// while calling itself exact) names:
//   TransformVector / Sgn / TransformPoint / ExternallySimulatedBody::GetTransform / operator- /
//   InverseOfMatrixWithOrthonormal3x3 / Min<VecFloat> / CompGreaterThan / Dot / Abs<VecFloat> /
//   operator-<VectorAxisY, VectorAxisY> / GetApplyForceQueue / operator* / operator+ / operator* /
//   Select / operator-= / operator- / Dot / operator- / Max<VecFloat> / six operator* / Select /
//   AddEventSafe
// -- item for item the decode above, including BOTH Dots, BOTH Selects, the `Pos() -=`, and the
// `lPropRelativePosition.y - GetHalfExtent().y` axis subtract (that is the VectorAxisY operator-).
//
// ---- WHY THE DWARF'S MASK VOCABULARY IS NOT SPELLED OUT, HELPER BY HELPER (round-1 NIT, applied) -
// The DWARF names `MaskScalar lAboveRaceCar` (source :2122) plus CompGreaterThan / Select /
// Min<VecFloat> / Max<VecFloat> / Abs<VecFloat> / Sgn. The reconstruction below reduces the mask
// machinery to `bool` + ternaries and uses std::fabs. That reduction is EXACT (both vsel masks come
// from compares of BROADCAST operands -- a splatted speed against a splatted gain, and a splatted
// lane difference against zero -- so all four lanes always agree and a scalar bool is a
// reproduction, not a narrowing). The per-helper state of the tree, so this reads as six separate
// judgements rather than one blanket one:
//   * MaskScalar          EXISTS -- vendor rw/math/vpu/types.h:115.
//   * Select              EXISTS -- vector4_operation.h:88, but its signature is
//                         `Vector4 Select(Vector4, Vector4, MaskScalar)`. The value being selected
//                         here is a **Vector3** (lFinalForce) and Vector3/Vector4 are DISTINCT
//                         structs in this tree (types.h:24/:26). Spelling it would need a lane-type
//                         cast at every use -- strictly worse than the exact scalar reduction, so
//                         the ternary stays.
//   * CompGreaterThan     does NOT exist under that name; the tree's home for it is
//                         `IsGreater` (vector4_operation.h:98), again Vector4-typed.
//   * Min / Max<VecFloat> EXIST and ARE used below (vector4_operation.h:45/:53), already written in
//                         the console's select form: vminfp(a,b) == (a<b)?a:b, vmaxfp likewise.
//   * Abs<VecFloat>       does NOT exist -- vector3_operation.h:237's Abs is Vector3-only. std::fabs
//                         on the broadcast lane is the exact scalar equivalent of the `vandc`.
//   * Sgn                 does NOT exist anywhere under vendor rw/math/. Inlined below as the
//                         two-compare/two-select nested conditional the console emits.
//
// ---- NaN POLARITY (AGENTS.md gotcha 4) ----------------------------------------------------------
// Every decision here is a VECTOR compare feeding a `vsel`/`vminfp`/`vmaxfp`, not an `fcmpu`
// branch, so the C++ below deliberately spells each one in its select form with the same operand
// order the console used. vcmpgtfp/vcmpgefp are FALSE when unordered, exactly like C++ > and >=.
// In particular Sgn(NaN) is -1.0f on this hardware (both compares false), and that is what the
// nested conditional below produces. Do NOT "simplify" it to copysign or to fpu::Clamp.
//
// ---- CALLEE CENSUS (measured: exactly 2 `bl` in the export, no save helper) ----------------------
//   sub_825BCD60                          @0x825BCD60  -- InputBuffer::GetApplyForceQueue, LANDED
//   InApplyForce::AddEventSafe            @0x825E3E20  -- committed
//   ⚠️ AddEventSafe, NOT AddEvent -- the bounds-gated variant. The console drops the shove rather
//   than overflowing a full queue; the DWARF callee list names AddEventSafe too.
//
// ⭐ THE CONSOLE-VALUE WARNING THAT STOOD HERE IS RETIRED (2026-08-18 round 3b). "No initialiser
// exists in the ARTIST image" was measured false -- the initialisers are dynamic-initialiser
// thunks that live outside every IDA function, so no export scan could see them. All five gains
// are now seated at their measured console values in BrnPropManager.cpp:
//     KVF_SPEED_CLAMP                     = Splat(120.0f)
//     KVF_MAX_SPEED_FOR_SIDE_FORCE        = Splat(60.0f)
//     KVF_ANTI_HERD_HIGH_SPEED_SIDE_SCALE = Splat(1.5f)
//     KVF_ANTI_HERD_UPWARD_SCALE          = Splat(2.0f)
//     KVF_ANTI_HERD_SIDE_SCALE            = Splat(0.05f)
// The old note's own reading of the shape now pays off: with a 120 m/s speed clamp and a 60 m/s
// knee, the side scale steps from 0.05 to 1.5 -- a 30x -- once the car is over the knee, which is
// what makes the anti-herding shove a high-speed behaviour. The five ARE the exact set the debug
// UI groups under "Anti herding...", which remains the cross-check that the NAMING is right; the
// VALUES are no longer unrecovered. Provenance per constant: BrnPropManager.h.
// =================================================================================================
void PropManager::ApplyAntiHerdingForce( CgsPhysics::PhysicsSimulationIO::InputBuffer* lpSimInputBuffer,
                                         BrnPhysics::Vehicle::RaceCarPhysics*          lpRaceCar,
                                         CgsPhysics::RigidBodyId                       lPropRigidBodyId,
                                         Vector3                                       lPropWorldPos,
                                         VecFloat                                      lvfPropMass,
                                         Vector3                                       lPropLinearVelocity,
                                         Vector3                                       /*lCollisionNormal*/ )
{
    namespace vpu = rw::math::vpu;

    // NOTE: `this` is deliberately unread -- the X360 body overwrites r3 with r4 (the input
    // buffer) at 0x82611558 before its only call and never touches a member. Measured, not
    // an omission.

    // :2120  |speedMPH| clamped -- `vandc` (sign-bit clear) then `vminfp`.
    const VecFloat lvfRaceCarSpeed =
        vpu::Min(vpu::Splat(std::fabs(lpRaceCar->GetSpeedMPH().x)), KVF_SPEED_CLAMP);

    // :2116  the car basis, with the centre-of-mass offset taken back out of the position
    // (`Pos() -= TransformVector(transform, mCOMOffset)`).
    // ⚠️ WHY THE HOMED HELPER *IS* CALLED HERE, WHERE ReadUpdatedBodies' decode (G) declines it:
    // the committed vpu::TransformVector (matrix44affine_operation.h:41-53) forces `lvResult.w = 0`,
    // whereas the console's cascade at 0x826114A8..0x826114B0 is full 4-lane and carries the basis
    // rows' packed w scalars through. At THIS site the w lane is provably DEAD: the mutated Pos() is
    // consumed only by the 3-lane `vmsum3fp128 v8,v8,v13` @0x826114C0 (Dot #1), by the `vspltw` of
    // lanes 0/1/2 @0x826114C4..0x826114CC, and by InverseOfMatrixWithOrthonormal3x3, which reads
    // lrPos.x/.y/.z only (same header, :199-215) and writes its own wAxis.w = 0. So the two policies
    // in this file are deliberate, not a contradiction -- decode (G)'s site keeps a LIVE w lane.
    Matrix44Affine lCarTransform = lpRaceCar->GetTransform();
    lCarTransform.Pos() = vpu::Subtract(
        lCarTransform.Pos(),
        vpu::TransformVector(lCarTransform, lpRaceCar->GetSimpleAttribs()->mCOMOffset));

    // :2123  vpu::Sgn of the prop's sideways offset along the car's right axis.
    // Emitted as its own `vmsum3fp128` at 0x826114C0, ahead of (and separate from) the full
    // inverse transform below -- reproduced as its own Dot for that reason. Numerically it is
    // the same value as lPropRelativePosition.x.
    const f32 lfSidewaysOffset =
        vpu::Dot(vpu::Subtract(lPropWorldPos, lCarTransform.Pos()), lCarTransform.Right());
    const f32 lSidewaysDirection =
        (lfSidewaysOffset >= 0.0f) ? ((lfSidewaysOffset > 0.0f) ? 1.0f : 0.0f) : -1.0f;

    // :2165 / :2124  the prop's position in the car's frame.
    const Matrix44Affine lInverseCarTransform =
        vpu::InverseOfMatrixWithOrthonormal3x3(lCarTransform);
    const Vector3 lPropRelativePosition = vpu::TransformPoint(lInverseCarTransform, lPropWorldPos);

    // :2121 / :2122  is the prop riding ABOVE the car's roof line? (only lane .y is read)
    // The DWARF's type for this is MaskScalar; the scalar bool is exact -- see the banner's
    // helper-by-helper note.
    const VecFloat lvfPropHeightAboveCar =
        vpu::Splat(lPropRelativePosition.y - lpRaceCar->GetHalfExtent().y);
    const bool lAboveRaceCar = (lvfPropHeightAboveCar.x > 0.0f);

    // :2114 / :2125  the sideways shove, before either scale.
    const Vector3 lForceDirection = vpu::Mult(lCarTransform.Right(), lSidewaysDirection);
    const Vector3 lSideForce      =
        vpu::Mult(vpu::Mult(lForceDirection, lvfRaceCarSpeed.x), lvfPropMass.x);

    // :2117..:2119 / :2115  the upward component: only ever pushes the prop up towards the target
    // relative velocity, never down (`vmaxfp` against zero).
    const Vector3  lCarsWorldLinearVelocity = lpRaceCar->GetLinearVelocity();
    const VecFloat lvfPropsUpwardVelocity   = vpu::Splat(
        vpu::Dot(vpu::Subtract(lPropLinearVelocity, lCarsWorldLinearVelocity), lCarTransform.Up()));
    const VecFloat lvfTargetUpwardVel =
        vpu::Splat(lvfRaceCarSpeed.x * KVF_ANTI_HERD_UPWARD_SCALE.x);
    const VecFloat lForceMagnitude = vpu::Splat(
        vpu::Max(vpu::Splat(lvfTargetUpwardVel.x - lvfPropsUpwardVelocity.x), vpu::Splat(0.0f)).x
        * lvfPropMass.x);

    // :2126  the two `vsel`s, in the console's order.
    Vector3 lFinalForce =
        (lvfRaceCarSpeed.x > KVF_MAX_SPEED_FOR_SIDE_FORCE.x)
            ? vpu::Add(vpu::Mult(lCarTransform.Up(), lForceMagnitude.x),
                       vpu::Mult(lSideForce, KVF_ANTI_HERD_HIGH_SPEED_SIDE_SCALE.x))
            : vpu::Mult(lSideForce, KVF_ANTI_HERD_SIDE_SCALE.x);

    if (lAboveRaceCar)
    {
        lFinalForce = Vector3{ 0.0f, 0.0f, 0.0f, 0.0f };
    }

    // [DIAG] NOT IN THE X360 BINARY -- 2026-09-06 props lane (bug #2, "props sent flying way too
    // much at medium/high speed"). This function is the only place the game deliberately BLASTS a
    // prop upward, and the blast is speed-gated at exactly the speed the report names
    // (KVF_MAX_SPEED_FOR_SIDE_FORCE == 60 mph), so a run has to be able to say whether it fired
    // and how big it was. Every value printed is already computed above; the acceleration column
    // is force/mass because InApplyForce is scaled by the body's INVERSE MASS at the consumer
    // (RigidBody::AddForce, witnessed inline in ProcessApplyForceQueue @0x828A6C1C).
    // Opt-in (BRN_PROP_DIAG), first-N, never per-frame unbounded. DELETE-WHEN bug #2 is closed.
    {
        static const bool sbHerdDiag   = ( getenv( "BRN_PROP_DIAG" ) != 0 );
        static s32        siHerdLinesLeft = 400;
        if ( sbHerdDiag && siHerdLinesLeft > 0 && CgsDev::Log::gpDebugPrint != 0 )
        {
            --siHerdLinesLeft;
            const f32 lfMass  = ( lvfPropMass.x != 0.0f ) ? lvfPropMass.x : 1.0f;
            *CgsDev::Log::gpDebugPrint
                << "[prop-herd] entity=" << static_cast<u32>( lPropRigidBodyId.GetEntityId() )
                << " carMph=" << lvfRaceCarSpeed.x
                << " mass=" << lvfPropMass.x
                << " aboveCar=" << ( lAboveRaceCar ? 1 : 0 )
                << " relY=" << lPropRelativePosition.y
                << " halfY=" << lpRaceCar->GetHalfExtent().y
                << " targetUp=" << lvfTargetUpwardVel.x
                << " propUp=" << lvfPropsUpwardVelocity.x
                << " forceMag=" << lForceMagnitude.x
                << " |F|=" << rw::math::vpu::Magnitude( lFinalForce )
                << " a=" << ( rw::math::vpu::Magnitude( lFinalForce ) / lfMass )
                << "\n";
        }
    }

    // :2113  the event: the 8-byte handle at +0x00, the force at +0x10 (`std r6, sp+0x70`,
    // `stvx128 v0, sp+0x80`). AddEventSafe, not AddEvent.
    CgsPhysics::PhysicsSimulationIO::InApplyForce lApplyForceEvent;
    lApplyForceEvent.mID    = lPropRigidBodyId;
    lApplyForceEvent.mForce = lFinalForce;

    // @0x826115A0 `bl sub_825BCD60` -> the NON-const accessor landed at
    // CgsPhysicsSimulationModuleIO.h:201 this round; @0x826115A8 the bounds-gated post.
    lpSimInputBuffer->GetApplyForceQueue()->AddEventSafe(lApplyForceEvent);
}

// =================================================================================================
// BrnPhysics::Props::PropManager::ReadUpdatedBodies  @ 0x82632918   (752 instructions, MEASURED:
// 752 non-blank lines, 0x82632918 `mflr r12` .. 0x826334D4 `b __restgprlr`)
//
// DWARF: dwarfdump BrnPropManager.h dumpfile line 217 (class declaration) and dwarfdump
// BrnPropManager.cpp dumpfile line 1133 == SOURCE line :962.
// ⚠️ Round 1's banner cited "BrnPropManager.h:154 / .cpp:1133" as source lines. Both are DUMPFILE
// lines; the real source line for the definition is :962. Same defect class as the two functions
// above, corrected here. The per-statement citations below (:964, :969, :970, :974, :1008..:1011,
// :1027, :1035, :1041..:1047, :1078..:1109) ARE source lines -- the dumpfile prints them as
// `// BrnPropManager.cpp:NNN` comments above each local.
//
// ⚠️ PARAMETER NAMES: the definition below uses the DWARF's own spellings (source :962 --
// lpUpdatedBodies / lpSceneInput / lpSimModuleInputBuffer / lvfTimeStep). The committed
// ReadUpdatedBodies declaration in BrnPropManager.h still
// spells them lpUpdatedBodyQueue / lpSceneInterface / lpSimInputBuffer -- the 2026-08-09 conductor
// wave's names, which predate the DWARF read. C++ lets a definition rename parameters, so this
// compiles either way; the header rename is filed as a name-only request, not applied here (this
// lander may not edit headers). The DWARF additionally spells the fourth parameter
// `const VecFloat` -- a top-level const on a by-value parameter, which is not part of the
// signature and is dropped, as everywhere else in this tree.
//
// -------------------------------------------------------------------------------------------------
// WHAT THIS FUNCTION IS
// -------------------------------------------------------------------------------------------------
// The per-frame prop leg of PhysicsModule::Update @0x825B0640 (its only caller, from xrefs_to). It
// drains the simulation's OutUpdateRigidBody queue, keeps the events whose RigidBodyId owner byte
// is E_ENTITYTYPE_PROP (3), and for each one writes the resolved pose/velocity back into the
// PropManager's own PropInstance / PropPartInstance table and re-publishes it as an
// UpdatePropEvent on mUpdatedProps. Along the way it (a) fires the extra prop gravity, (b) drops
// props that froze or fell out of the world, (c) clamps per-frame acceleration, and (d) pins the
// "always smash" props (smash gates, fences, billboards) back to their authored transform.
//
// TRANSCRIPTION BASIS
//   * X360 ARTIST 0x82632918, read instruction by instruction (.ida-exports JSON `assembly`).
//   * DecFIGS dwarfdump BrnPropManager.cpp source :962.. -- it names every local (liIndex /
//     lpUpdateBodyEvent / lRigidBodyId / lUpdatePropEvent / lbFrozen / lbIsPart /
//     lvfOneOverTimeStep / lUpdatedTransform / lUpdatedPosition / lUpdatedLinearVelocity /
//     lUpdatedAngularVelocity / lEntityId / liPartIndex / lpPart / liPropIndex / lpProp / lpEvent)
//     AND the inlined callee set, which is what identifies the two big VMX blocks below.
//   * Two rodata floats byte-read out of IDA Files/BURNOUT_X360_ARTIST.XEX.i64 with headless
//     IDA 9.3 (flt_82004014 and word 0 of stru_8208F620); see the two file-scope constants.
//
// -------------------------------------------------------------------------------------------------
// L. DWARF CONSTRUCTS WITH NO ARTIST COUNTERPART -- CORRECTLY ABSENT, WITH THE NEGATIVE EVIDENCE
//    (round-1 G6 NIT 3, applied. Written down so the next verifier does not re-derive it for an
//     hour, or worse, "restore" invented code.)
//    The DecFIGS scope for this function contains things this build did not emit:
//      * a top-level `InRemoveRigidBody lRemoveBodyEvent;` at source :970.
//      * SIX local-`StrStream` diagnostic blocks (dumpfile 1331-1338, 1350-1368, 1377-1380,
//        1381-1388, 1389-1392, 1393-1396), two of which end in a queue-full warning built from
//        BaseEventQueue<UpdatePropEvent>::GetMaxLength and BaseEventQueue<InApplyForce>::GetMaxLength.
//    NEGATIVE EVIDENCE, measured this round:
//      * the `bl` census of the 752 instructions is 26 unique NON-HELPER targets (29 unique targets
//        minus __savegprlr_14 / __savevmx_119 / __restvmx_119; 60 `bl` LINES in total -- all three
//        numbers re-counted in round 3, the earlier "29 lines" was the unique-target count
//        mislabelled). NONE of them is
//        InputBuffer::GetRemoveRigidBodyQueue, an InRemoveRigidBody AddEvent, or
//        CgsDev::StrStream::StrStream.
//      * a grep of the whole disassembly for `0x684` returns ZERO hits -- and mUpdatedProps+0x684
//        is where a GetMaxLength read would land. (`0x680`/`0x688` DO appear: the Append source
//        and the Clear store.)
//    The one logging path that IS emitted is the fell-out-of-the-world line, and it goes through
//    the gpDebugPrint GLOBAL, not a local StrStream -- which is exactly what the DWARF's four
//    BARE `StrStreamBase::operator<<` calls (dumpfile 1342-1345, outside every StrStream block)
//    describe: four inserts, one of them the u64 through sub_82203EE8.
//
// -------------------------------------------------------------------------------------------------
// THE DECODES A BODY AUTHOR WILL OTHERWISE GET WRONG (all MEASURED unless marked)
// -------------------------------------------------------------------------------------------------
//
// A. `lbFrozen` IS THE RIGID BODY'S OWN STATE BIT, not an event flag word.
//    The asm is `lwz r11, 0x9C(event)` + `extrwi r10,r11,1,30` == (word >> 1) & 1. event+0x9C is
//    event+0x10 (mRigidBody) + 0x8C, and rigidbody.h's committed layout puts mIsplt at +0x80 with
//    the console packing mState in its w lane at +0x8C. So the word IS rw::physics::BodyState and
//    bit 1 is FROZEN_BODY (== 2). Reading it as "some flags field at +0x9C" would compile, run,
//    and freeze the wrong props. (The wave brief's "(bodyFlags >> 1) & 1" is this, named.)
//
// B. THE ~200-INSTRUCTION VMX BLOCK IN THE ALWAYS-SMASH ARM IS `RigidBody::SetTransform`
//    FOLLOWED BY `RigidBody::InertiaUpdate`, BOTH ALREADY BODIED IN THE TREE.
//    Its shape gives it away and the DWARF confirms it: four w-preserving `vrlimi128 vD,vOld,1,0`
//    row copies into +0x40/+0x50/+0x60/+0x10 from the PropInstance's own four transform rows, then
//    the branchless matrix->quaternion network (`vcmpgtfp` trace comparisons, the ±0.5f from
//    `vcsxwfp128 v121,1`, `vrsqrtefp` + two Newton steps, the `vsel` cascade) stored FULL-WIDTH to
//    +0x00 == mQuat -- i.e. rw::math::vpu::QuaternionFromMatrix33, whose epsilon here is
//    `lvlx` of flt_8208F60C == 0.0f (byte-read; identical to the committed inline's default, so no
//    explicit epsilon is passed). Then `lwz r8, 0x5C(body)` -- mUp's w lane == mInertia -- guards
//    the `vpermwi128 0x97 / 0x9B` + three `vmulfp128` + four `vmaddfp` tensor rebuild into
//    +0x70/+0x80 plus the `lvlx v13, r10, 0x10` read of mInertia->mInvMass. That is exactly and
//    only RigidBody::InertiaUpdate, with the caller-side `if (mInertia != NULL)` guard the
//    committed rigidbody.h banner says every call site carries.
//    ⚠️ Hand-transcribing that block instead of calling the two methods is how a tensor gets
//    written twice and transposed once, which is the risk rigidbody.h factored InertiaUpdate out
//    to avoid. It is called, not copied.
//
// C. THE ALWAYS-SMASH ARM `continue`s -- no UpdatePropEvent, no PropInstance write-back.
//    `b loc_82633490` from both its exits (the mInertia==NULL early-out at 0x8263327C and the tail
//    at 0x82633340) jumps past the AddEvent. The prop is snapped back to its authored pose with
//    zero velocity and zero accumulated force/torque and is NOT reported as moved. That is the
//    right behaviour for a smash gate: it breaks, it does not tumble.
//
// D. THE OUT-OF-WORLD TEST IS AN ORDERED COMPARE AND STAYS ONE (AGENTS.md gotcha 4).
//    `vcmpgtfp. v13(K splat), v0(pos.y splat)` + the CR6 "all true" bit. vcmpgtfp is false for
//    unordered, and so is C++ `<`, so `lUpdatedPosition.y < KVF_PROP_OUT_OF_WORLD_HEIGHT.x` is the
//    faithful spelling (the DWARF spells it `operator< <VectorAxisY>`). No negated-predicate
//    rewrite is needed here -- unlike the `bge`/`ble` forms that need `!(a < b)`.
//
// E. THE TWO SLOT-BOUNDS ASSERTS ARE A PAIR, AND THE FIRST OF EACH PAIR CAN NEVER FIRE.
//    liPartIndex / liPropIndex are `clrlwi r27,r11,16` == the LOW 16 BITS of the RigidBodyId,
//    zero-extended -- i.e. RigidBodyId::GetIndex(). A u16 is never -1, so
//    "liPartIndex != KI_PROP_INDEX_NOT_FOUND" (:1042 / :1079) is dead in the shipped build. The
//    console emits it anyway (and falls through into the range assert when it trips, which is why
//    the two share one emission), so both are reproduced. Not tidied away.
//
// F. THE EVENT CARRIES THE **RAW** VELOCITIES; THE INSTANCE GETS THE **CLAMPED** ONES.
//    lUpdatePropEvent.mLinearVelocity / .mAngularVelocity are stored at 0x82632DC4/0x82632DD4,
//    BEFORE ClampAcceleration runs; the instance setters afterwards read the sp+0x100/sp+0x110
//    slots ClampAcceleration writes through its two Vector3& out-params. On the remove path
//    ClampAcceleration is not called and those slots still hold the raw values, so both consumers
//    agree there. Hoisting the event stores after the clamp would change what the world/sound
//    bridge sees.
//
// G. THE EXTRA-COM UNDO IS A BASIS ROTATION, AND IT REWRITES BOTH COPIES OF THE POSITION.
//    `pos - (xAxis*off.x + yAxis*off.y + zAxis*off.z)` -- the same rotation AddPropToSim
//    @0x82627714 applies in the forward direction under the same mu8Flags bit. The result is
//    stored to sp+0x1E0 (the Matrix44Affine handed to PropInstance::SetTransform) AND to sp+0x150
//    (the event's own mTransform.wAxis), which is one `lUpdatePropEvent.mTransform =
//    lUpdatedTransform` re-assignment -- the compiler's redundant re-store of the three unchanged
//    basis rows at 0x82632FDC..0x82632FEC is that assignment, not three separate writes.
//
//    ⚠️⚠️ WHY THE HOMED HELPER IS DELIBERATELY *NOT* CALLED HERE (round-1 G6 NIT 2, applied).
//    The DWARF names `rw::math::vpu::TransformVector` then `rw::math::vpu::operator-=` for exactly
//    this expression (dumpfile lines 1339/1340), and TransformVector IS homed, at
//    vendor/renderware/include/rw/math/vpu/matrix44affine_operation.h:41. It is still written
//    longhand below, ON PURPOSE, because the longhand is MORE faithful:
//      * the committed TransformVector forces `lvResult.w = 0.0f`;
//      * the console's chain at 0x82632FD8-0x82632FFC (`vmulfp128 v13,v126,v13` / two `vmaddfp128`
//        / `vsubfp128 v0,v122,v13`) is FULL 4-LANE and carries the basis rows' packed w scalars
//        (mStasis / mInertia / mTag) through into the result;
//        ⚠️ THOSE TWO ARE **VMX128** MULTIPLY-ADDS (`vmaddfp128 v13, v125, v12, v13` @0x82632FF4 and
//        `vmaddfp128 v13, v124, v0, v13` @0x82632FF8) and therefore read op1 = op2*op3 + op4 --
//        the OPPOSITE field order from the plain `vmaddfp` rule calibrated in ClampAcceleration's
//        banner above. Carrying the plain rule across gives row1*(row0*off.x) + off.y instead of
//        row0*off.x + row1*off.y, i.e. it would "correct" this correct decode into nonsense. The
//        same 128-vs-plain split is visible one screen down, in the 1/|v| Newton pair at
//        0x826331E0/0x826331E4, which is where the VMX128 order was re-derived.
//      * the tree's Vector3 operator* / operator- are full 4-lane too, so the longhand reproduces
//        the console lane for lane.
//    This is the OPPOSITE call to decode (B)'s "call the homed helper, do not copy it", and the
//    difference is exactly that InertiaUpdate is lane-identical while TransformVector is not.
//    A later sweep that reads the DWARF and "fixes" this to TransformVector will silently zero a
//    live lane. Do not.
//
// H. THE FOUR `BrnPropEntityID.h:278` OWNER TRIPWIRES ARE INLINED ACCESSORS, NOT HAND-WRITTEN
//    ASSERTS. In order: the explicit `PropEntityID(u32)` constructor (0x82632C34), the inlined
//    `GetPartIndex()` (0x82632DE0), and the two `GetEntityId().GetValue()` reads on the part /
//    prop slot (0x82632E8C / 0x82633358). The committed BrnPropEntityID.cpp already carries
//    AssertIsProp inside all three accessors, so calling them by name reproduces all four --
//    writing them out by hand would double them.
//
// I. `mUpdatedJointedProps` IS MERGED IN AND CLEARED AT THE **END**, `mUpdatedProps` IS CLEARED AT
//    THE **START**. `stw r27,0x688(this)` before the loop == mUpdatedProps.Clear(); the tail is
//    `Append(this+0x680, this+0x5E10)` then `stw 0, 0x5E18(this)` ==
//    mUpdatedProps.Append(mUpdatedJointedProps) + mUpdatedJointedProps.Clear(). The DWARF lists
//    Clear twice for exactly this reason.
//
// J. 1/dt IS `vrefp128` + TWO NEWTON REFINEMENT STEPS, x 1.0f. The DWARF spells the source
//    `VecFloat lvfOneOverTimeStep = GetVecFloat_One() / lvfTimeStep` (`rw::math::vpu::operator/`,
//    source :974). VecFloat is a broadcast lane quad in this tree with no operator/, so it is
//    written as an exact reciprocal splatted over the four lanes -- the same de-optimisation
//    vector3_operation.h's own banner documents for the SDK's rsqrt estimates.
//
// K. THE ZERO-PAGE CONSTANTS ARE NO LONGER ZERO (2026-08-18 round 3b) -- this item used to say
//    they were, and that the gap should be reported rather than fixed. It has now been fixed,
//    because the "no initialiser in the image" premise it rested on was measured false: the
//    initialisers are MSVC dynamic-initialiser thunks sitting outside every IDA function, which
//    is why every export-based scan reported readers only. Seated in BrnPropManager.cpp:
//        K_DEFAULT_GRAVITY            = (0, -9.8, 0)   [per-lane decode, not a splat]
//        KVF_GRAVITY_SCALE            = Splat(3.0f)
//        KVF_PROP_OUT_OF_WORLD_HEIGHT = Splat(-1000.0f)
//    So the InApplyForce this function posts is `K_DEFAULT_GRAVITY * (3 - 1)` == 19.6 m/s^2
//    downward on top of the simulation's own 1g -- a 3g prop fall, which is what the (scale - 1)
//    shape was always for -- and the out-of-world floor is a kilometre down instead of at Y == 0.
//    ⚠️ Worth keeping in mind for anyone reading old notes: with the placeholder zeroes the posted
//    force was not merely absent, it was NEGATIVE (0 - 1 == -1), i.e. props were being pushed up.
//    KVF_PROP_OUT_OF_WORLD_HEIGHT still carries its AUTHORED-NAME flag -- 0x82FB94C0's source name
//    is unrecovered, and that is a separate fact from its now-recovered value.
//
// -------------------------------------------------------------------------------------------------
// CALLEES -- 29 unique `bl` targets (26 of them non-helper), 60 `bl` lines; every one resolved
// -------------------------------------------------------------------------------------------------
//   0x825BB538 "CgsPh" (IDA-TRUNCATED)  = BaseEventQueue<OutUpdateRigidBody>::GetEvent (stride 192,
//                                   asserts at CgsBaseEventQueue.h:272/274/275) -- committed
//   0x822868E0 "BrnPhysics::Props::Prop" (IDA-truncated) = ResourcePtr<T>::operator-> with the
//                                   "Can not instance resource pointer - it has no main memory
//                                   resource\n" assert at CgsResourcePtr.h:544 -- committed
//                                   (round-2 fix: the earlier "- it is NULL" wording was wrong; the
//                                   committed spelling is CgsResourcePtr.h:201/:209 and the console
//                                   string is aCanNotInstance @0x820072A0)
//   0x825BCCB8 "CgsPhysics_" (truncated) = InputBuffer::GetUpdateRigidBodyQueue() NON-CONST -- LANDED
//   0x825BCD60 sub_825BCD60       = InputBuffer::GetApplyForceQueue() NON-CONST        -- LANDED
//   0x825E3C30 sub_825E3C30       = BaseEventQueue<InUpdateRigidBody>::AllocateEventSafe -- LANDED
//   0x82203EE8 sub_82203EE8       = StrStreamBase::operator<<(u64)  (it reads mePrintMode at +4,
//                                   hex-formats for modes 1..2 and resets HEXONCE to DECIMAL) --
//                                   committed; the `stw 2, gpDebugPrint+4` before it IS
//                                   `<< E_PRINTMODE_HEXONCE`
//   0x825E3CC8                    = BaseEventQueue<InApplyForce>::AddEvent -- committed
//   0x825E5DF8 / 0x825E61F0       = BaseEventQueue<UpdatePropEvent>::AddEvent / ::Append -- committed
//   0x825E3410                    = rw::physics::RigidBody::operator= -- committed
//   0x8260F540 / 0x8260F988       = PropManager::RemoveProp / RemovePart -- ✅ BODIED, in the
//                                   sibling round-2 part-file PropManager_wQ2_04.cpp (by NAME --
//                                   `PropManager::RemoveProp` / `PropManager::RemovePart`).
//                                   Mount that file with this one; do NOT stub them (a stub beside
//                                   the real body is an invisible LNK2005). Round-2 correction: the
//                                   earlier banner called this pair "the link hole this file
//                                   reports", which was false the day it was written.
//   0x82627F00                    = PropManager::ClampAcceleration -- bodied ABOVE, in this file
//   0x825DE798 / 0x825DE860       = PropPartInstance::SetPosition / SetLinearVelocity -- committed
//   0x825DE370 / 0x825DE6C8 / 0x825DE5F8 = PropInstance::SetTransform / SetLinearVelocity /
//                                   SetAngularVelocity -- committed
//   0x82277C50                    = PropPhysicsDataHeader::GetType -- committed
//   0x821F1F20 / 0x828226D8 / 0x8282BE40 = DebugInterface::DebugInterface / ::GetRender /
//                                   DebugRender::DrawAxis -- the debug arm. ⛔ DrawAxis is
//                                   DECLARATION-ONLY (CgsDebugRender.h:104) and ::GetRender resolves
//                                   only to the asserting stub at WorldLinkStubs.cpp:1605: THAT is
//                                   this file's link hole (pre-existing, see the banner).
//   0x82BBC4F0                    = rw::core::debug::detail::DebugCriticalSection::Leave -- its own
//                                   real symbol (the export's xrefs_from names it), NOT an alias for
//                                   ThreadSafeRelease. Round-2 correction: the earlier census
//                                   attributed this address to "the inlined
//                                   DebugManager::ThreadSafeRelease" and then listed Leave a second
//                                   time, counting one callee twice under two identities.
//                                   ThreadSafeRelease (CgsDebugManager.h:353) IS genuinely inlined
//                                   at that site -- it is Leave followed by the mpInstance assert --
//                                   and therefore has NO address of its own here.
//   plus CgsDev::Assert::{Begin,Fire,End}Assert and the three __save*/__rest* compiler helpers.
//
// ⚠️ TWO DWARF-NAMED HELPERS ARE SPELLED OUT OF COMMITTED ACCESSORS RATHER THAN GROWN:
//    * `RigidBody::SetLinearAcceleration` / `SetAngularAcceleration` (neither declared in the
//      committed rw/physics/rigidbody.h). The console emits the two as a bare
//      `vrlimi128 vD,vOld,1,0` + `stvx128` zeroing of mTorque (+0xA0) then mForce (+0x90), which is
//      byte-for-byte what the committed `ResetForces(Vector3(0))` does (mForce := arg,
//      mTorque := 0). ResetForces is used, with this note.
//    * `PropTypeData::ShouldAlwaysSmash()` -- spelled out of the committed IsSmashable() /
//      GetSmashThreshold() / GetMoveThreshold() accessors.
//    Both are recorded as optional, non-blocking header requests; neither is invented here.
// =================================================================================================
void PropManager::ReadUpdatedBodies(
    const CgsModule::EventQueue<CgsPhysics::PhysicsSimulationIO::OutUpdateRigidBody, 200>* lpUpdatedBodies,
    CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpSceneInput,
    CgsPhysics::PhysicsSimulationIO::InputBuffer* lpSimModuleInputBuffer,
    VecFloat lvfTimeStep )
{
    // `stw r27,0x688(r29)` before the loop -- mUpdatedProps.miLength = 0. FIRST, not last.
    mUpdatedProps.Clear();

    // [DIAG] NOT IN THE X360 BINARY. Wave-Q6 read-back census (opt in with BRN_PROP_DIAG),
    // rate-limited to ONE line per simulated second. scout.md §5: until wave Q6 this loop ran with
    // a non-empty queue every physics frame and threw the whole result away, because its only
    // consumer -- OutputUpdatedProps -- was an inert gate. These three counts are the first proof
    // the read-back is alive at all, and their SHAPE is the diagnosis:
    //   parts=0 with props>0  -> no part body ever reached the sim (scout.md §4.1);
    //   frozen climbing        -> bodies are being retired, i.e. cluster B's missing prop-vs-world
    //                             contacts are letting them free-fall past the out-of-world floor.
    // The clock is the SIMULATED one (lvfTimeStep), not a host clock: it stays meaningful under a
    // paused or stepped sim, and it costs no syscall.
    static const bool sbPropDiag = ( getenv( "BRN_PROP_DIAG" ) != 0 );
    s32               liDiagProps  = 0;
    s32               liDiagParts  = 0;
    s32               liDiagFrozen = 0;

    // :974  `vrefp128 v0,dt` + two Newton refinement steps, x 1.0f (v127 == vcsxwfp128 of
    // vspltisw 1). DWARF `VecFloat lvfOneOverTimeStep`, produced by rw::math::vpu::operator/ over
    // GetVecFloat_One(). De-optimised to an exact reciprocal, broadcast over the four lanes --
    // VecFloat is a broadcast lane quad in this tree.
    const f32 lfOneOverTimeStep = 1.0f / lvfTimeStep.x;
    const VecFloat lvfOneOverTimeStep = { lfOneOverTimeStep, lfOneOverTimeStep,
                                          lfOneOverTimeStep, lfOneOverTimeStep };

    // :964
    for ( s32 liIndex = 0; liIndex < lpUpdatedBodies->GetLength(); ++liIndex )
    {
        // :965
        const CgsPhysics::PhysicsSimulationIO::OutUpdateRigidBody* lpUpdateBodyEvent =
            &lpUpdatedBodies->GetEvent( liIndex );

        // :966
        const CgsPhysics::RigidBodyId lRigidBodyId = lpUpdateBodyEvent->mID;

        // `srdi r11,r25,32 ; srwi r11,r24,24 ; cmplwi r11,3 ; bne <next iteration>`.
        if ( lRigidBodyId.GetEntityIDOwner() != BrnWorld::E_ENTITYTYPE_PROP )
        {
            continue;
        }

        // :967
        UpdatePropEvent lUpdatePropEvent;

        // :1008..:1011  rw::physics::RigidBody::GetTransform() -- the mRi/mUp/mAt basis rows
        // (+0x40/+0x50/+0x60) with mCom (+0x10) as the translation row. The console materialises
        // the same value three times on the stack (a dead copy at sp+0x210, the event's own row at
        // sp+0x120 and the SetTransform argument at sp+0x1B0); one source local, copied.
        Matrix44Affine lUpdatedTransform       = lpUpdateBodyEvent->mRigidBody.GetTransform();
        Vector3        lUpdatedPosition        = lUpdatedTransform.Pos();
        Vector3        lUpdatedLinearVelocity  = lpUpdateBodyEvent->mRigidBody.GetLinearVelocity();
        Vector3        lUpdatedAngularVelocity = lpUpdateBodyEvent->mRigidBody.GetAngularVelocity();

        // :968  See decode (A): `lwz r11,0x9C(event)` is mRigidBody.mState (event+0x10 + 0x8C),
        // and `extrwi r10,r11,1,30` isolates FROZEN_BODY.
        bool lbFrozen =
            ( lpUpdateBodyEvent->mRigidBody.GetState() & rw::physics::FROZEN_BODY ) != 0;

        // Out-of-world floor. See decode (D) for the compare polarity.
        if ( lUpdatedPosition.y < KVF_PROP_OUT_OF_WORLD_HEIGHT.x )
        {
            if ( ( CgsDev::Message::gxMessageFilterFlags & 1 ) != 0 )
            {
                // The `stw 2, gpDebugPrint+4` between the two virtual sink calls is mePrintMode
                // := E_PRINTMODE_HEXONCE, i.e. the id is logged in hex exactly once. This is the
                // gpDebugPrint GLOBAL, which is why the DWARF spells it as four BARE
                // StrStreamBase::operator<< calls and not as a local StrStream -- see note (L).
                *CgsDev::Log::gpDebugPrint << "\t Warning!! prop fell out of the world: "
                                           << CgsDev::E_PRINTMODE_HEXONCE
                                           << static_cast<u64>( lRigidBodyId )
                                           << "\n";

                // [DIAG] NOT IN THE X360 BINARY -- a wave-Q6 SUFFIX on the console's own warning
                // (opt in with BRN_PROP_DIAG), first 16 only. scout.md §4.7: the floor constant
                // KVF_PROP_OUT_OF_WORLD_HEIGHT is not printed anywhere, yet it decides how long a
                // part falls before the freeze deletes it -- i.e. how much of the motion a drive
                // test can even observe. Printing the threshold NEXT TO the y that tripped it also
                // makes the memory-file's recurring failure ("a console constant read as a host
                // value", here Splat(-1000.0f) recovered from a dyn-init thunk) checkable in one
                // glance instead of by re-deriving the initialiser.
                {
                    static const bool sbPropDiag  = ( getenv( "BRN_PROP_DIAG" ) != 0 );
                    static u32        suDiagCount = 0;
                    if ( sbPropDiag && suDiagCount < 16u )
                    {
                        ++suDiagCount;
                        *CgsDev::Log::gpDebugPrint
                            << "[Q6-read] out-of-world floor="
                            << KVF_PROP_OUT_OF_WORLD_HEIGHT.x
                            << " pos.y=" << lUpdatedPosition.y
                            << "\n";
                    }
                }
            }
            lbFrozen = true;
        }

        // `if (raw) { if (!mbDisableFreezing) 1 else 0 } else 0` -- the debug switch suppresses
        // BOTH the freeze-driven and the fell-out-of-the-world removal.
        lbFrozen = lbFrozen && !mbDisableFreezing;

        lUpdatePropEvent.mbFrozen = lbFrozen;

        // [DIAG] see the census note at the top of this function. Counted here, after the
        // mbDisableFreezing fold, so the number matches what the event actually carries.
        if ( sbPropDiag && lbFrozen )
        {
            ++liDiagFrozen;
        }

        // :1035  The explicit PropEntityID(u32) ctor stores the word then fires AssertIsProp --
        // owner tripwire #1 of four, BrnPropEntityID.h:278. See decode (H).
        const PropEntityID lEntityId( static_cast<u32>( lRigidBodyId.GetEntityId() ) );

        lUpdatePropEvent.mEntityId     = lEntityId;
        lUpdatePropEvent.miPhysicsSlot = static_cast<s16>( lRigidBodyId.GetIndex() );
        lUpdatePropEvent.mTransform    = lUpdatedTransform;

        // :1017  The extra prop gravity: one InApplyForce per moving, non-removed prop.
        // `vmsum3fp128 v0,v123,v123` then `vcmpgtfp128. v0,v0,v127(1.0f)`.
        // ⚠️ Both globals read as ZERO in the shipped image -- see decode (K).
        if ( !lbFrozen )
        {
            if ( rw::math::vpu::MagnitudeSquared( lUpdatedLinearVelocity ) > 1.0f )
            {
                CgsPhysics::PhysicsSimulationIO::InApplyForce lApplyForceEvent;
                lApplyForceEvent.mID    = lRigidBodyId;
                lApplyForceEvent.mForce = K_DEFAULT_GRAVITY * ( KVF_GRAVITY_SCALE.x - 1.0f );

                // @0x82632D44 `bl sub_825BCD60` -> the NON-const accessor; @0x82632D4C the
                // ONE-ARG AddEvent(const T&), which has been committed all along. (Round 1 filed
                // this as needing a NEW AddEvent overload. It did not -- the C2663 was purely the
                // const-ness of the accessor. Recorded so the retired request is not re-filed.)
                lpSimModuleInputBuffer->GetApplyForceQueue()->AddEvent( lApplyForceEvent );
            }
        }

        // :1027 mbRenderCOM (+0x48). The console builds an automatic DebugInterface and draws the
        // gizmo on the event's own transform row (sp+0x120). Its destructor tests
        // mbIsAutomaticClass and releases the manager, matching the inlined ARTIST tail.
        if ( mbRenderCOM )
        {
            CgsDev::DebugInterface lInt;
            lInt.GetRender().DrawAxis(
                reinterpret_cast<const f32*>( &lUpdatePropEvent.mTransform ) );
        }

        // See decode (F): the event carries the RAW velocities, written before any clamping.
        lUpdatePropEvent.mLinearVelocity  = lUpdatedLinearVelocity;
        lUpdatePropEvent.mAngularVelocity = lUpdatedAngularVelocity;

        // :969  `clrlwi r11,r24,22` -- the id's low 10-bit part field. Owner tripwire #2 rides the
        // inlined GetPartIndex(). DWARF `bool lbIsPart`.
        if ( lEntityId.GetPartIndex() != 0 )
        {
            // ---- one shed PART of a smashed prop -------------------------------------------
            // [DIAG] see the census note at the top of this function.
            if ( sbPropDiag )
            {
                ++liDiagParts;
            }

            // :1041
            const s32 liPartIndex = static_cast<s32>( lRigidBodyId.GetIndex() );

            CGS_ASSERT( liPartIndex != KI_PROP_INDEX_NOT_FOUND,
                        "liPartIndex != KI_PROP_INDEX_NOT_FOUND" );                       // :1042
            CGS_ASSERT( liPartIndex >= 0
                        && liPartIndex < static_cast<int32_t>( KU_MAX_PHYSICAL_PROP_PARTS ),
                        "liPartIndex >= 0 && liPartIndex < static_cast<int32_t>( KU_MAX_PHYSICAL_PROP_PARTS )" ); // :1043

            // :1045  `slwi r11,r27,6` == index * 64 == the CONSOLE PropPartInstance stride; the
            // slot is reached by NAME, not by that constant (AGENTS.md gotcha 1).
            PropPartInstance* lpPart = &mpaPartInstances[ liPartIndex ];

            // Owner tripwire #3 rides GetEntityId().GetValue().
            CGS_ASSERT( static_cast<u32>( lRigidBodyId.GetEntityId() )
                            == lpPart->GetEntityId().GetValue(),
                        "lpUpdateBodyEvent->mID.GetEntityId() == lpPart->GetEntityId().GetValue()" ); // :1047

            lUpdatePropEvent.miTypeId = static_cast<s16>( lpPart->GetType() );

            if ( lbFrozen )
            {
                RemovePart( lEntityId, static_cast<u32>( liPartIndex ),
                            lpSceneInput, lpSimModuleInputBuffer );
            }
            else
            {
                // v1 = lpPart->mLinearVelocity (+0x10), v2 = lpPart->mAngularVelocity (+0x20) --
                // LAST frame's values; the two Vector3& out-params are updated in place.
                ClampAcceleration( lpPart->GetLinearVelocity(), lpPart->GetAngularVelocity(),
                                   lRigidBodyId, lpUpdateBodyEvent,
                                   lUpdatedLinearVelocity, lUpdatedAngularVelocity,
                                   lvfTimeStep, lvfOneOverTimeStep, lpSimModuleInputBuffer );
            }

            lpPart->SetPosition( lUpdatedPosition );
            lpPart->SetLinearVelocity( lUpdatedLinearVelocity );
            // ⚠️ MEASURED: a BARE `stvx128 v0, r31, 32` -- no IsValid tripwire, unlike its two
            // siblings above. BrnPropPartInstance.h's SetAngularVelocity is inline and
            // assert-free for exactly this reason; do not "restore" a tripwire here.
            lpPart->SetAngularVelocity( lUpdatedAngularVelocity );
        }
        else
        {
            // ---- a whole prop ----------------------------------------------------------------
            // [DIAG] see the census note at the top of this function.
            if ( sbPropDiag )
            {
                ++liDiagProps;
            }

            // :1078
            const s32 liPropIndex = static_cast<s32>( lRigidBodyId.GetIndex() );

            CGS_ASSERT( liPropIndex != KI_PROP_INDEX_NOT_FOUND,
                        "liPropIndex != KI_PROP_INDEX_NOT_FOUND" );                       // :1079
            CGS_ASSERT( liPropIndex >= 0
                        && liPropIndex < static_cast<int32_t>( KU_MAX_PHYSICAL_PROPS ),
                        "liPropIndex >= 0 && liPropIndex < static_cast<int32_t>( KU_MAX_PHYSICAL_PROPS )" ); // :1080

            // :1082  `mulli r11,r27,0x70` == index * 112 == the CONSOLE PropInstance stride;
            // reached by name (AGENTS.md gotcha 1).
            PropInstance* lpProp = &mpaPropInstances[ liPropIndex ];

            // `lbz r11,0x6F(prop) ; rlwinm r11,r11,0,30,30` == mu8Flags &
            // KU_HAS_EXTRA_COM_OFFSET_FLAG -- undo the shift AddPropToSim applied. Decode (G),
            // including why TransformVector is NOT called here.
            if ( lpProp->HasExtraComOffset() )
            {
                lUpdatedPosition = lUpdatedPosition
                                 - ( lUpdatedTransform.Right() * K_PROP_EXTRA_COM_OFFSET.x
                                   + lUpdatedTransform.Up()    * K_PROP_EXTRA_COM_OFFSET.y
                                   + lUpdatedTransform.At()    * K_PROP_EXTRA_COM_OFFSET.z );

                lUpdatedTransform.Pos()     = lUpdatedPosition;
                lUpdatePropEvent.mTransform = lUpdatedTransform;
            }

            const PropTypeData* lpType = mpPhysicsData->GetType( lpProp->GetTypeId() );

            // PropTypeData::ShouldAlwaysSmash() (DWARF). The asm is `lbz r11,0x5D(type)`
            // (muNumberOfParts != 0 == IsSmashable) AND `lfs 0x54 < lfs 0x50 + 0.1f`
            // (mfSmashThreshold below mfMoveThreshold + slack, i.e. the prop breaks before it can
            // ever be reported as moving). The float test is `fcmpu`+`blt` == an ORDERED `<`, so no
            // negated-predicate rewrite is needed (AGENTS.md gotcha 4). Spelled out of the
            // committed accessors because the DWARF's helper has no declaration in the tree; the
            // console +0x50/+0x54 are the header's host +0x5C/+0x60 and are reached by accessor.
            if ( lpType->IsSmashable()
                 && lpType->GetSmashThreshold()
                        < lpType->GetMoveThreshold() + KF_ALWAYS_SMASH_THRESHOLD_SLACK )
            {
                // :1096  Pin the body back to the prop instance's authored pose and stop it dead.
                // See decodes (B) and (C). @0x82633064 `bl sub_825BCCB8` (the NON-const accessor)
                // then @0x82633068 `bl sub_825E3C30` (AllocateEventSafe) -- both landed this round.
                CgsPhysics::PhysicsSimulationIO::InUpdateRigidBody* lpEvent =
                    lpSimModuleInputBuffer->GetUpdateRigidBodyQueue()->AllocateEventSafe();

                // ⚠️ The console does NOT null-check the result -- reproduced as-is. (The callee
                // CAN return NULL on a full queue; that is the console's own exposure, not an
                // omission here.)
                lpEvent->mID        = lRigidBodyId;
                lpEvent->mRigidBody = lpUpdateBodyEvent->mRigidBody;   // RigidBody::operator=

                const Vector3 lZero = { 0.0f, 0.0f, 0.0f, 0.0f };

                // `vrlimi128 vD,vOld,1,0` + `stvx128` into +0xA0 then +0x90 -- the DWARF's
                // SetAngularAcceleration(0) / SetLinearAcceleration(0) pair. The committed
                // rigidbody.h exposes exactly that pair as ResetForces (mForce := arg,
                // mTorque := 0); with a zero argument the two spellings are identical.
                lpEvent->mRigidBody.ResetForces( lZero );
                lpEvent->mRigidBody.SetLinearVelocity( lZero );    // +0x20 mVel
                lpEvent->mRigidBody.SetAngularVelocity( lZero );   // +0x30 mOmega

                // The four w-preserving row copies + the matrix->quaternion network.
                lpEvent->mRigidBody.SetTransform( lpProp->GetTransform() );

                // `lwz r8,0x5C(body)` == mUp.w == mInertia; the guard is the CALL SITE's, exactly
                // as Simulation::AddRigidBody emits it.
                if ( lpEvent->mRigidBody.GetInertia() != NULL )
                {
                    lpEvent->mRigidBody.InertiaUpdate( lpEvent->mRigidBody.GetInertia() );
                }

                // `b loc_82633490` -- no UpdatePropEvent, no instance write-back.
                continue;
            }

            // Owner tripwire #4 rides GetEntityId().GetValue().
            CGS_ASSERT( static_cast<u32>( lRigidBodyId.GetEntityId() )
                            == lpProp->GetEntityId().GetValue(),
                        "lpUpdateBodyEvent->mID.GetEntityId() == lpProp->GetEntityId().GetValue()" ); // :1109

            // `lbz r11,0x6E(prop) ; cmplwi r11,1 ; blt / bne` -- a three-way on the movement
            // state: 0 promotes to 1 only once the body actually has velocity, 1 promotes to 2
            // unconditionally, 2 stays. The IsZero test reads the PRE-clamp velocity (decode F).
            if ( lpProp->GetMovementState() == E_PROP_MOVESTATE_STATIONARY )
            {
                if ( !rw::math::vpu::IsZero( lUpdatedLinearVelocity, KF_PROP_MOVED_EPSILON ) )
                {
                    lpProp->SetMovementState( E_PROP_MOVESTATE_JUST_MOVED );
                }
            }
            else if ( lpProp->GetMovementState() == E_PROP_MOVESTATE_JUST_MOVED )
            {
                lpProp->SetMovementState( E_PROP_MOVESTATE_MOVING );
            }

            lUpdatePropEvent.miTypeId = static_cast<s16>( lpProp->GetTypeId() );

            if ( lbFrozen )
            {
                RemoveProp( lEntityId, static_cast<u32>( liPropIndex ),
                            lpSceneInput, lpSimModuleInputBuffer );
            }
            else
            {
                // [DIAG] NOT IN THE X360 BINARY -- 2026-09-02 props-at-speed: the raw sim
                // velocity BEFORE the acceleration clamp, next to the stored previous one and
                // the step, so a boot-drive can see who changes a flung prop's velocity (the
                // sim, or the clamp's re-post). Whole props only, BRN_PROP_DIAG, first-N.
                // DELETE-WHEN the high-speed prop reaction is confirmed on screen.
                const Vector3 lDiagSimLinearVelocity = lUpdatedLinearVelocity;

                // v1 = lpProp->mLinearVelocity (+0x40), v2 = lpProp->mAngularVelocity (+0x50).
                ClampAcceleration( lpProp->GetLinearVelocity(), lpProp->GetAngularVelocity(),
                                   lRigidBodyId, lpUpdateBodyEvent,
                                   lUpdatedLinearVelocity, lUpdatedAngularVelocity,
                                   lvfTimeStep, lvfOneOverTimeStep, lpSimModuleInputBuffer );

                {
                    // 200 -> 6000, 2026-09-06 (props lane, bug #2). 200 lines is ~40 physics
                    // frames with five props in the queue -- 0.7 s, far too short a window to
                    // measure how far a hit prop actually travels. Still first-N, still opt-in.
                    static s32 siDiagClampLinesLeft = 6000;
                    if ( sbPropDiag && siDiagClampLinesLeft > 0 && CgsDev::Log::gpDebugPrint != 0 )
                    {
                        --siDiagClampLinesLeft;
                        const Vector3 lPrev = lpProp->GetLinearVelocity();
                        const Vector3 lPos  = lUpdatedTransform.Pos();
                        *CgsDev::Log::gpDebugPrint
                            << "[Q6-clamp] prop " << lEntityId.GetValue()
                            << " pos (" << lPos.x << "," << lPos.y << "," << lPos.z << ")"
                            << " sim v=(" << lDiagSimLinearVelocity.x << "," << lDiagSimLinearVelocity.y
                            << "," << lDiagSimLinearVelocity.z << ")"
                            << " prev v=(" << lPrev.x << "," << lPrev.y << "," << lPrev.z << ")"
                            << " out v=(" << lUpdatedLinearVelocity.x << "," << lUpdatedLinearVelocity.y
                            << "," << lUpdatedLinearVelocity.z << ")"
                            << " dt=" << lvfTimeStep.x
                            << " moveState=" << static_cast<s32>( lpProp->GetMovementState() )
                            << " w=(" << lUpdatedAngularVelocity.x << "," << lUpdatedAngularVelocity.y
                            << "," << lUpdatedAngularVelocity.z << ")"
                            << " up=(" << lUpdatedTransform.Up().x << "," << lUpdatedTransform.Up().y
                            << "," << lUpdatedTransform.Up().z << ")"
                            << "\n";
                    }
                }
            }

            lpProp->SetTransform( lUpdatedTransform );
            lpProp->SetLinearVelocity( lUpdatedLinearVelocity );
            lpProp->SetAngularVelocity( lUpdatedAngularVelocity );
        }

        mUpdatedProps.AddEvent( lUpdatePropEvent );
    }

    // Tail: fold the jointed (leaning / tilting) props' own queue in and empty it. Decode (I).
    mUpdatedProps.Append( mUpdatedJointedProps );
    mUpdatedJointedProps.Clear();

    // [DIAG] NOT IN THE X360 BINARY. The once-per-simulated-second census line -- see the note at
    // the top of this function for what its shape means. Emitted AFTER the jointed fold so
    // `queued` is the exact length OutputUpdatedProps is about to publish, which is what the
    // matching one-shot `[Q6-out] first publish` should agree with on the frame it fires.
    if ( sbPropDiag && CgsDev::Log::gpDebugPrint != 0 )
    {
        static f32 sfDiagAccumulatedTime = 0.0f;
        sfDiagAccumulatedTime += lvfTimeStep.x;
        if ( sfDiagAccumulatedTime >= 1.0f )
        {
            sfDiagAccumulatedTime = 0.0f;
            *CgsDev::Log::gpDebugPrint
                << "[Q6-read] props=" << liDiagProps
                << " parts=" << liDiagParts
                << " frozen=" << liDiagFrozen
                << " queued=" << mUpdatedProps.GetLength()
                << "\n";
        }
    }
}

}
}

// ============================================================================
// FOLDED FROM PropManager_wQ2_03.cpp (wave Q2) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================
// =================================================================================================
// GameSource/Physics/PropManager/PropManager_wQ2_03.cpp
//
// Partfile of the TU GameSource/Unity/../Physics/PropManager/BrnPropManager.cpp
// (breakable-props wave Q ROUND 2, implementer 03, 2026-08-18). Folds back into
// BrnPropManager.cpp.
//
// THE THREE FUNCTIONS THIS FILE OWNS -- the two contact-generation legs
// BeginPropWorldContactGeneration dispatches on, and the queue drain EndPropWorldContactGeneration
// tail-calls:
//
//   * PropManager::DoPartWorldContactGeneration         @0x82611B70 (349 insns) -- ⭐ LANDED 2026-08-19
//   * PropManager::DoPropInstanceWorldContactGeneration @0x826120E8 (342 insns) -- ⭐ LANDED 2026-08-19
//   * PropManager::AddContactResultsToQueue             @0x82612F08 (184 insns) -- ⭐ LANDED 2026-08-18
//
// ⭐⭐ 2026-08-19 (wave Q6, cluster B "prop-vs-world contact generation"): THE TWO Do* LEGS ARE
// NO LONGER PARKED. Both blocking declarations landed this wave --
//   A. CgsSceneManager::CgsCollision::PrimitivePairListBuilder::AddPrimitive(const
//      rw::collision::Volume*, Matrix44Affine, f32, u16)   @0x82814AB8
//      -> CgsPrimitivePairListBuilder.h, above the Sphere overload (the DWARF's own order)
//   B. CgsSceneManager::CgsCollision::BaseCollisionGenerator::
//      CollidePrimitiveListAgainstTriangleList(...)         @0x828141D8
//      -> CgsCollisionGenerator.h, beside CollidePrimitivePairList
// Both bodies below were RE-DERIVED against the raw `assembly` arrays this wave (see the
// per-function decode blocks) rather than pasted from the park, and the park's two file-local
// constants were reconciled: KVF_MAX_CONTACT_GEN_PADDING is GONE -- its recovered name
// KVF_MAX_PROP_PADDING now lives in BrnPropManager.h (HEADER REQUEST D, landed wave Q4) and both
// bodies reach for it there.
//
// Instruction counts are (end-start)/4 over the measured function boundaries, stated
// END-EXCLUSIVE so the arithmetic and the addresses agree (round-2 NIT: the earlier line mixed the
// two conventions and one end address was simply wrong):
//     0x82611B70..0x826120E4   -> 349   (last instruction 0x826120E0 `b __restgprlr_16`)
//     0x826120E8..0x82612640   -> 342   (last instruction 0x8261263C `b __restgprlr_16`;
//                                        0x82612630 is `li r0,-0xA0`, three insns earlier)
//     0x82612F08..0x826131E8   -> 184
// For the first two the export's own `assembly` listing has exactly that many non-blank lines,
// which I also counted (349 / 342); the third has no per-address export and its boundary comes
// from ida_funcs.get_func on the .i64.
//
// -------------------------------------------------------------------------------------------------
// ⭐ HEADER REQUESTS A AND B -- BOTH LANDED 2026-08-19 (wave Q6). THE Do* LEGS ARE IN THIS FILE.
// -------------------------------------------------------------------------------------------------
// The round-2 diagnosis stands re-verified: `selfcheck.py
// scratchpad/waveQ2/probe_wQ2_03b/probe_bodies.cpp` used to return STATUS=fail with EXACTLY TWO
// DISTINCT diagnostics, one per missing declaration and each reported once per body --
//
//   error C2660: "CgsSceneManager::CgsCollision::PrimitivePairListBuilder::AddPrimitive":
//                function does not take 4 arguments
//                  ... "(rw::collision::Volume *, const Matrix44Affine, float, u16)"
//   error C2039: "CollidePrimitiveListAgainstTriangleList" is not a member of
//                "CgsSceneManager::CgsCollision::CollisionGenerator"
//
// -- and both are gone, because both declarations landed this wave.
//
// ---- HEADER REQUEST A -- LANDED ------------------------------------------------------------------
//   CgsPrimitivePairListBuilder.h, inside `struct PrimitivePairListBuilder`, public, ABOVE the
//   committed AddPrimitive(Sphere*, f32, u16) -- the DWARF's own declaration order (source :77):
//     void AddPrimitive(const ::rw::collision::Volume* lpVolume, Matrix44Affine lTransform,
//                       f32 lfPadding, u16 lu16PrimitiveTag);                    // @0x82814AB8
//   Register map re-measured this wave (headless IDA 9.3 on a PRIVATE .i64 copy; IDA leaves the
//   symbol `sub_82814AB8`, the function is 0x82814AB8..0x82814CE8, 140 insns):
//     r3 = the builder; r4 = the volume; r5 = the transform (four 16-byte rows);
//     f1 = the padding; r7 = the tag.
//   ⚠️ GOTCHA 3 IS WHY r6 IS SKIPPED: the f32 rides f1 and consumes its GPR slot, so the u16 tag
//   lands in r7. r6 is never read -- a signature derived from GPRs alone would invent a dead
//   fifth argument.
//   ⛔ ITS BODY IS A LINK HOLE AND IS **NOT** THIS WAVE'S (reported to the conductor, with the
//   whole decode on the declaration): it needs three sibling overloads that are unnamed in the
//   export (measured boundaries this wave -- sub_82814570 = AddPrimitive(Box*) 35 insns,
//   sub_82814600 = AddPrimitive(Capsule*) 29, sub_82814678 = AddPrimitive(Cylinder*) 36),
//   CgsGeometric::Box::Set @0x825E6918 (269, also bodyless), and two CgsGeometric primitive types
//   that do not exist in the tree at all (Capsule 32 bytes, Cylinder 80 bytes).
//
// ---- HEADER REQUEST B -- LANDED, **AND BODIED** --------------------------------------------------
//   CgsCollisionGenerator.h, inside `struct BaseCollisionGenerator`, beside CollidePrimitivePairList:
//     u16 CollidePrimitiveListAgainstTriangleList(const PrimitivePairList*, const TriangleList*,
//                                                 u16, u32, u16, bool);          // @0x828141D8
//   ⭐ The round-2 NIT asked whoever landed it to pick u16-with-a-note or s32-for-family-
//   consistency, but not to leave two undocumented spellings. RESOLVED: the DWARF's u16 is kept
//   (source :257) and the register truth carries the note on the declaration -- 0x8281428C
//   `mr r29,r3`, 0x82814298 `clrlwi r10,r29,16` for the LOCAL use only, 0x82814324 `mr r3,r29`
//   with no clrlwi on the return path. Every call site drops the result.
//   Its BODY landed with it, in CgsCollisionGenerator.cpp (86 insns, all callees present).
//   Seven measured call sites: DeformableObject::DoBodyPartWorldContactGeneration x2,
//   ::DoDetachedWheelWorldContactGeneration, VehicleManager::DoTrafficCarWorldContactGeneration
//   x2, and this pair (0x826120A8 / 0x82612604).
//   ⚠️ IT CANNOT BE DROPPED as "the dead arm": the selector is a RUNTIME byte read
//   (`lbz byte_82F2A39C`), not a compile-time constant -- the console emits BOTH arms.
//
// ---- HEADER REQUEST C (source-shape only, not a compile blocker) --------------------------------
//   `CgsSceneManager::CgsCollision::TriangleList::SetTriangleBuffer(const Triangle4*, s32)`
//   (DWARF CgsTriangleList.h:21) does not exist. The console inlines it to two member stores plus
//   its out-of-line CheckAlignment(), which is why `bl CheckAlignment` appears at 0x82612024 with
//   no `bl SetTriangleBuffer`. The parked bodies open-code the two stores, exactly as the
//   committed BrnVehicleManagerContactGeneration.cpp:591-593 / :619-621 already do. When it lands,
//   all four sites collapse onto it.
//
// ---- HEADER REQUESTS E / F / G (source-shape + names; NONE of them blocks anything) -------------
//   The DecFIGS scope for AddContactResultsToQueue (dwarfdump BrnPropManager.cpp:2975) names three
//   things the tree cannot spell today. All three are behaviour-neutral, and the body below uses
//   the SAME workaround the committed twin BrnVehicleManagerContactGeneration.cpp:684-765 already
//   ships, so this is not a new fork:
//     E. `CgsSceneManager::CgsCollision::CollisionResultList::GetPrimitiveTestResult(u16)` --
//        DWARF-named callee; does not exist. Its bounds tripwire ("lu16Index < mu16NumResults",
//        CgsCollisionResultList.h:148) IS reproduced explicitly below, and the 80-byte record walk
//        is done through a typed pointer. Home: CgsCollisionResult.h beside GetResult.
//        ⚠️ physfix.owner.md §3.3 lists this as a MISSING declaration that BLOCKS this function.
//        It does not block it -- see the "two wrong numbers" block below.
//     F. `CgsSceneManager::VolumeInstanceId::Set(...)` -- DWARF-named TWICE (once per side); does
//        not exist. Its argument list is NOT recoverable from this body (the console inlines it to
//        `extldi/or/std`), so it is NOT invented: the two muId words are assembled inline with the
//        console's own shifts and masks, exactly as the vehicle twin does at :750-753.
//     G. PARAMETER NAMES. The DWARF spells this function's parameters
//        `lpPotentialContactInterface, lpContactGenerator, lWorldEntityId`; the committed
//        AddContactResultsToQueue declaration in BrnPropManager.h spells the first two
//        `lpContactInterface, lpCollisionGenerator`.
//        This file matches the HEADER (a definition must), and the delta is filed rather than
//        silently diverged. Name-only: no type, order or count changes, so no caller moves.
//        (physfix.owner.md §3 says this signature was "left exactly as declared" because there is
//        no export to check registers against -- the DWARF scope line settles the names.)
//
// ---- HEADER REQUEST D (home for two recovered constants) -- CLOSED 2026-08-19 (wave Q6) ---------
//   The parked bodies carried two file-local AUTHORED-NAME constants over MEASURED values whose
//   real home is the KVF_* block in BrnPropManager.h/.cpp:
//     KVF_MAX_CONTACT_GEN_PADDING == Splat(0.3f)   and   KB_USE_CONTACT_GEN_STREAM == true.
//   ⭐ THE FIRST ONE IS HOMED **AND ITS NAME IS RECOVERED**: it is `extern const VecFloat
//   KVF_MAX_PROP_PADDING` in BrnPropManager.h (defined `= { 0.3f, 0.3f, 0.3f, 0.3f }` in
//   BrnPropManager.cpp with this file's own thunk/rodata provenance carried across).
//   ⭐ AND THE LANDING HONOURED IT: the two bodies below reach for the HEADER's constant under
//   that recovered name, and the file-local `KVF_MAX_CONTACT_GEN_PADDING` copy the parks carried
//   is GONE. That authored spelling no longer exists anywhere in the tree.
//   (Wave Q4 matched 0x82FB94F0's initialiser-table slot 0x82CD19AC to the DWARF's own source-order
//   file scope, where BrnPropManager.cpp:54 reads `const VecFloat KVF_MAX_PROP_PADDING;`.)
//   ⭐ THE SECOND ONE IS NOW MEASURED TOO, and its request is DISCHARGED rather than dropped. The
//   park said "KB_USE_CONTACT_GEN_STREAM has no address and no thunk cited anywhere, so there is
//   nothing for the header owner to re-measure". Both halves of that are now false: the address IS
//   cited (byte_82F2A39C, the `lbz` at 0x826125D4 / 0x82612078), and it was RE-MEASURED
//   INDEPENDENTLY this wave on a private .i64 copy (scratchpad/waveQ6/ida_worldc/out.json) --
//   the 32 bytes at 0x82F2A390 read
//       41D80000 41F00000 3CA3D70A 01000000 7F7FFFFF FFFFFFFF FFFFFFFF FFFFFFFF
//   so 0x82F2A39C == 0x01, inside an ordinary initialised .data run, not a zero page.
//   It STAYS FILE-LOCAL below (with that evidence attached) because it is a CgsCollision-side
//   switch with no DWARF name and no PropManager home -- seating an authored name in
//   BrnPropManager.h would be a worse fork than a documented file-local. The open request is
//   therefore narrowed, not closed: find its real home, do not re-measure its value.
//   ⭐ SCOPE OF THE CLAIM (round-2 NIT), now closed: this banner correctly warned that only the
//   0x82FB94F0 thunk had been walked end to end and that every other KVF_*/K_* pair must be
//   re-MEASURED through its own thunk before being seated. Round 3b did exactly that -- all 22
//   constants of BrnPropManager.h were walked individually, including the two Vector3-shaped
//   thunks (per-lane, not splats) and the two self-product _SQ thunks (which carry no rodata word
//   of their own). None was seated off a sibling's pattern.
//   ⭐ AND THE SENTENCE THIS FILE FLAGGED AS WRONG IS RETIRED AT SOURCE: "the export set contains
//   only readers, so the initial values are UNRECOVERED" is gone from BrnPropManager.h /
//   BrnPropManager.cpp / PropManager_wQ2_01 / _04 / _05 and from physfix.owner.md §5 N5. This
//   file's measurement is what started that; the recipe it wrote down is what finished it.
//
// -------------------------------------------------------------------------------------------------
// ⭐⭐ 0x82FB94F0 IS Splat(0.3f), **NOT** ZERO -- INDEPENDENTLY RE-MEASURED THIS ROUND
// -------------------------------------------------------------------------------------------------
// BrnPropManager.h's tuning-globals block and scratchpad/waveQ2/physfix.owner.md §5 N5 BOTH USED TO
// SAY (both retired 2026-08-18 round 3b, on the strength of this measurement) that every 0x82FB9xxx
// VecFloat "reads as all-zero on disk ... the export set contains only readers ... do not invent
// values". The first clause is true; the second is FALSE, and the difference is live behaviour.
// MEASURED by me, headless IDA 9.3 on the .i64 (scratchpad/waveQ2/probe_wQ2_03b/p1.txt):
//   * 0x82FB94F0 does read 16 zero bytes in the static image -- confirmed.
//   * A DYNAMIC-INITIALISER THUNK writes it. 0x82C5E750..0x82C5E774, nine instructions, not inside
//     any IDA function (which is exactly why an xrefs-over-exports scan reports "readers only"):
//         lis r11, flt_82004740@ha ; lfs f0, flt_82004740@l(r11) ; stfs f0,-0x10(r1)
//         lvlx v0,r0,r10 ; vspltw v0,v0,0 ; stvx128 v0,r0,unk_82FB94F0 ; blr
//     i.e. `unk_82FB94F0 = Splat(flt_82004740)`.
//   * flt_82004740 reads `3e 99 99 9a` big-endian == 0.30000001192092896f (raw bytes dumped).
//   * The thunk is reached: 0x82C5E750 appears in the MSVC dynamic-initialiser pointer run, at
//     0x82CD19AC (0x82CD1990 reads `82c5de68 82c5de90 82c5df40 82c5dfd8 82c5e6c0 82c5e6e8 82c5e710
//     82c5e750`; raw bytes dumped, not inferred from a name). ⚠️ The run CONTINUES past it --
//     0x82CD19B0 -> 82c5e778 -- so this thunk is one entry of the startup initialiser chain, not
//     its end. (Round-2 NIT: the earlier wording called 0x82C5E750 "the last dword of that run",
//     which was only true of the 32 bytes quoted; the reachability argument is unaffected.)
//  WHY IT MATTERS: the clamp is `Min(padding, that)`. At zero, every prop/part collision primitive
//  is posted with NO swept padding at all -- the same placeholder-zero failure mode the shadow
//  campaign lost a day to. At 0.3 it is a 30 cm cap on the per-frame swept expansion.
//  ⭐ ACTED ON 2026-08-18 round 3b: the constant now lives in BrnPropManager.h/.cpp at Splat(0.3f),
//  and the same walk found that FOURTEEN more of this subsystem's tuning globals were sitting at
//  placeholder zero for the same reason -- including KVF_GRAVITY_SCALE, whose zero made the prop
//  extra-gravity force NEGATIVE (the console posts `gravity * (scale - 1)`), i.e. smashed props
//  were being pushed upward. This file's 0x82FB94F0 measurement is what exposed all of them.
//
// -------------------------------------------------------------------------------------------------
// ⚠️ TWO WRONG NUMBERS IN THE COMMITTED HEADER (reported, NOT edited -- this file owns no header)
// -------------------------------------------------------------------------------------------------
//   1. The AddContactResultsToQueue declaration comment in BrnPropManager.h says its span is
//      "82612F08-826131E8, 736 insns".
//      736 is the BYTE length (0x2E0). The function is 184 instructions. Same slip appears in
//      scratchpad/waveQ2/physfix.owner.md's table ("~736").
//   2. physfix.owner.md §3.3 lists `CollisionResultList::GetPrimitiveTestResult` as a MISSING
//      declaration that AddContactResultsToQueue needs. It does not need it: the console walks the
//      records at the raw 80-byte stride off `mpResults` and bakes the bound assert itself, which
//      is exactly what the committed twin BrnVehicleManagerContactGeneration.cpp:713-722 already
//      does. That entry did not block this function.
//
// -------------------------------------------------------------------------------------------------
// LINK-LEVEL FACTS (gate-green != link-green) -- RE-GREPPED 2026-08-19 (wave Q6)
// -------------------------------------------------------------------------------------------------
//   * NO gate and NO stub exists for ANY of this file's three functions. Re-grepped over the whole
//     of b5-decomp/src, the stub TUs included: the only hits are the header declarations and
//     comments. This file is the SOLE definition of all three.
//   * ⭐ THIS FILE IS MOUNTED (tools/build/build_game_exe.bat:1810), so the two Do* bodies landing
//     here go straight into the link. Their callees, checked ONE BY ONE this wave:
//       BODIED IN THE TREE -- PropManager::HasProp/HasPartJustBeenRemoved (PropManager_wQ2_04.cpp),
//       ResourcePtr<PropPhysicsDataHeader>::operator-> (BrnPropQueueFacades.cpp:87),
//       PropPhysicsDataHeader::GetType, PropTypeData/PropPartTypeData accessors (header inlines),
//       PropInstance/PropPartInstance velocity accessors (header inlines),
//       PrimitivePairListBuilder::Prepare (CgsPrimitivePairListBuilder.cpp:40),
//       TriangleCacheInterface::GetCache / GetNumCachedTriangleBatches
//       (CgsSceneManagerModuleIO.cpp), TriangleList::CheckAlignment / ValidateTriangles
//       (CgsTriangleList.cpp), Triangle4::AssertIsValid, PropEntityID::GetValue,
//       CgsDev::StrStream + the Begin/Fire/EndAssert trio, rw::math::vpu::{Magnitude, Splat, Min,
//       GetComponent, operator*}, and -- NEW THIS WAVE --
//       BaseCollisionGenerator::CollidePrimitiveListAgainstTriangleList
//       (CgsCollisionGenerator.cpp, landed with its declaration).
//     ✅ **BOTH LINK HOLES ARE CLOSED** -- re-verified 2026-08-19, wave Q6 round 2 (this block
//        previously read "TWO LINK HOLES REMAIN"; that is history now):
//       1. PrimitivePairListBuilder::AddPrimitive(const rw::collision::Volume*, Matrix44Affine,
//          f32, u16) @0x82814AB8 -- REAL BODY, CgsPrimitivePairListBuilder.cpp:197 (MOUNTED,
//          bat:867). The five-arm jump table (sphere/capsule/box/cylinder + the console's own
//          TRIANGLE refusal) was read out of the image; a prop's collision volumes DO become
//          collision primitives now, so the pair list this file posts is no longer empty.
//       2. BaseCollisionGenerator::AddPrimitiveListWithTriangleListToStream @0x82811D40 -- REAL
//          BODY, CgsCollisionGenerator.cpp:422 (MOUNTED, bat:1018), with the Create/Run halves
//          @0x82811DD0 / @0x82811F58 beside it (:482 / :540) and its loud gate deleted from
//          CgsCollisionGenerator_StreamStubs.cpp. The two things that had blocked it for three
//          waves also landed: the descriptor type (JobDescription/
//          CgsPrimitiveListWithTriangleListStreamJobDesc.h) and the enum member
//          E_COLLISIONJOB_PRIMITIVE_LIST_WITH_TRIANGLE_LIST_STREAM = 12, whose worker
//          ContactGeneratorJob::ExecutePrimitiveListWithTriangleListStream @0x82926650 is now a
//          real body too -- so a command posted from this leg is actually drained.
//   * ⭐ NO GATE SURVIVES ON THIS PATH. This entry used to call
//     ContactGeneratorJob::ExecutePrimitiveListWithTriangleList @0x82925908 (849) -- the
//     primitive-vs-triangle NARROW PHASE the type-12 stream arm delegates to -- the one surviving
//     runtime gate, and to expect "Warning!! prop fell out of the world" to survive because of it.
//     It has been REAL since wave Q6 round 3 (2026-08-19), in ContactGeneratorJob.cpp, and the two
//     workers it delegates to landed with it: ::BuildGPInstance @0x829222A0 and
//     ::CollideGPInstances @0x829253C8, in the sibling partfile ContactGeneratorJob_wQ6_01.cpp.
//   * ✅ RESOLVED, was "INHERITED, still open": PropManager_wQ_03.cpp's UpdateTriangleCache and
//     PropManager_wQ2_02.cpp's Begin/End no longer have gate bodies -- all three gates are gone,
//     and both partfiles are MOUNTED as a pair right after this one (wQ_03 alone is an LNK2019).
//     Mount + retire happened in one change, as that note asked.
// =================================================================================================


// ⚠️ Do NOT add vendor/renderware/collision/AABBox.hpp here -- it drags in the second
//    rw::math::vpu::Vector3 and hard-C2011s against BrnCommonTypes.h (physfix.owner.md REQUEST 1b).
//    volume_debug_access.h only forward-declares AABBox, which is all these bodies need.

namespace BrnPhysics
{
namespace Props
{

namespace vpu = ::rw::math::vpu;

// =================================================================================================
// FILE-LOCAL CONSTANTS FOR THE TWO CONTACT-GENERATION LEGS
// =================================================================================================
// byte_82F2A39C -- the STREAM-vs-SYNCHRONOUS selector both legs read (`lbz` at 0x826125D4 in the
// prop leg and 0x82612078 in the part leg). AUTHORED NAME (no symbol survives), MEASURED VALUE,
// RE-MEASURED INDEPENDENTLY 2026-08-19 on a private copy of the .i64
// (scratchpad/waveQ6/ida_worldc/out.json): the 32 bytes at 0x82F2A390 read
//     41D80000 41F00000 3CA3D70A 01000000 7F7FFFFF FFFFFFFF FFFFFFFF FFFFFFFF
// so the byte at 0x82F2A39C is 0x01 -- one byte inside a plain initialised-data run (its
// neighbours are 27.0f, 30.0f, 0.02f and FLT_MAX), NOT a zero page. So on the shipped console
// build the STREAM arm is the live one.
// ⚠️ THE 0x82FB9xxx "PLACEHOLDER ZERO" TRAP (gotcha 13) DOES NOT APPLY: that trap is
// zero-in-the-static-image plus a dynamic-initialiser thunk that writes the real value at startup.
// This byte is already non-zero in the image, so there is nothing for a thunk to supply. (The
// wave-Q round-2 measurement additionally reported a whole-export-set xref scan returning exactly
// two references, both the `lbz` reads in this pair of functions -- attributed, not re-run here.
// What THIS wave re-measured is the value.)
// ⚠️ THE SYNCHRONOUS ARM STILL CANNOT BE DROPPED: the read is a RUNTIME load, not a folded
// constant, which is why the console emits BOTH arms and why both are reproduced below.
// ⚠️ HOME NOT RECOVERED -- see HEADER REQUEST D in the banner. It is a CgsCollision-side switch
// with no DWARF name, so it stays file-local here rather than being seated under an authored name
// in BrnPropManager.h.
static const bool KB_USE_CONTACT_GEN_STREAM = true;   // byte_82F2A39C == 0x01, measured

// (fold: an identical definition of KI_PROP_CACHE_START_INDEX was dropped here -- this TU defines it once, above)
// (fold: an identical definition of KI_PROP_PART_CACHE_START_INDEX was dropped here -- this TU defines it once, above)

// Console `li r6, 0x40` at all four collide call sites of this pair.
static const u16 KU16_COLLIDE_MAX_RESULTS = 64;

// =================================================================================================
// BrnPhysics::Props::PropManager::AddContactResultsToQueue @0x82612F08  (184 asm insns)
//
// ⚠️ EXPORT HOLE, NOT A MISSING FUNCTION. 0x82612F08 has no .ida-exports per-address JSON and no
// identity.json row, so `work show` and the dossier tooling report it absent. It is present in the
// IDB: `ida_funcs.get_func(0x82612F08)` returns 0x82612F08..0x826131E8 named
// `BrnPhysics::Props::PropManager::AddContactResultsToQueue`, and I disassembled all 184
// instructions with headless IDA 9.3 this round (raw dump:
// scratchpad/waveQ2/probe_wQ2_03b/p1.txt). Every line below is decoded from that dump. This is the
// same situation, and the same resolution, the tree already recorded for VehicleManager::
// IsRaceCarHidden @0x825C2EA0 ("MISSING JSON != MISSING FUNCTION",
// BrnVehicleManagerContactGeneration.cpp:651-655).
//
// ---- THE ICF QUESTION, ANSWERED: IT IS **NOT** AN ICF FOLD OF THE VEHICLE TWIN ------------------
// The task asked whether this is an identical-COMDAT-folding alias of the ledger-done
// BrnPhysics::Vehicle::VehicleManager::AddContactResultsToQueue @0x825EB350
// (BrnVehicleManagerContactGeneration.cpp:684). It is not, and the disproof is structural, not a
// judgement call -- an ICF fold is ONE body at ONE address with several names, and these are two
// distinct address ranges (0x82612F08..0x826131E8, 184 insns; 0x825EB350..0x825EB6C8, 222 insns).
// Eight measured behavioural differences, any one of which is sufficient:
//   1. Outer bound. Vehicle walks [0, miFirstPartContactGenEntry). Prop walks
//      [0, GetNumUsedResultLists()) and opens with an assert that
//      miNumPropsAddedToContactGen == that count (:2813) -- an assert with no Vehicle counterpart.
//   2. Entity words. Vehicle takes BOTH from a ContactGenList entry. Prop takes side A from
//      maPropsAddedToContactGen[liResultsList] and side B from the lWorldEntityId PARAMETER
//      (which the Vehicle signature does not even have).
//   3. Normal. Vehicle SIGN-FLIPS the source normal (vxor against a 0x80000000 splat) and
//      TAG-SELECTS between mPrimitive0Normal and mPrimitive1Normal on mu16UserTagB. Prop does
//      neither: `lvx128 v127, r0, r30` (record +0x00) is stored to the contact's mNormal
//      unmodified. There is no vxor and no vcmp anywhere in the 184 instructions.
//   4. Volume-instance low word. Vehicle uses (entry volume-instance base + primitive index).
//      Prop uses the primitive index TRUNCATED TO 8 BITS (`clrlwi r9,r9,24`).
//   5. AddEvent overload. Vehicle calls sub_825E73D0 == AddEvent(u32 queueID, contact). Prop
//      calls PotentialContactInterface::AddEvent(contact) -- the single-argument overload, r3/r4
//      only, no queue id in sight.
//   6. Prop carries a debug ring-buffer write (mpDebugWorldContacts / miNumDebugWorldContacts)
//      that Vehicle has no trace of.
//   7. Prop carries the E_ENTITYTYPE_PROP owner tripwire (BrnPropEntityID.h:278) per record.
//   8. Vehicle carries the "Bad Part index: " tripwire (:1317) that Prop has no trace of.
// So this is a real, separately-compiled sibling and it is reconstructed here on its own asm. The
// committed Vehicle body is used only as the IDIOM precedent for the shared shapes (the 80-byte
// record walk, the by-value result list, the PotentialContact field mapping).
//
// ---- DWARF GROUNDING (DecFIGS dwarfdump GameSource/Physics/PropManager/BrnPropManager.cpp:2975)
// The scope's declaration line and every local it names line up with the asm one for one:
//     int32_t liResultsList @2810 - uint16_t lu16Result @2811 - CollisionResultList lResultList
//     @2818 - const PrimitiveTestResult& lResult @2825 - PotentialContact lContact @2829
// and its callee list is
//     BaseCollisionGenerator::GetResultList - CollisionResultList::GetPrimitiveTestResult -
//     BrnWorld::PropEntityID::operator CgsSceneManager::EntityId - rw::math::vpu::Vector3::
//     operator= (x2) - PhysicsModuleIO::PotentialContactInterface::AddEvent -
//     CgsSceneManager::VolumeInstanceId::Set (x2) -
//     CgsModule::BaseEventQueue<PotentialContact>::AddEventSafe.
// Note what is ABSENT from that list: no AddEvent(u32,...), no negation helper, no tag compare --
// three more independent confirmations of the ICF verdict below.
// ⚠️ TWO HONEST DELTAS between the DecFIGS scope and the shipped ARTIST body, stated not hidden:
//   * The scope ENDS with a `{ CgsDev::StrStream; operator<< }` block -- one more assert with a
//     BUILT message. The X360 ARTIST body emits NO StrStream call at all (I scanned all 184
//     instructions: no `bl` to StrStream, StrStreamBase or BasePriorityQueue::Clear). The two
//     builds differ here; the ARTIST body is what is reconstructed, and the missing assert is
//     recorded rather than invented.
//   * The scope names `PropEntityID::operator CgsSceneManager::EntityId` where the body below
//     calls `GetValue()`. Both are the SAME two console instructions -- the owner tripwire then
//     `lwz` the packed word -- and GetValue()'s committed body carries AssertIsProp()
//     (BrnPropEntityID.cpp:50). The conversion operator is NOT used because this tree's
//     `PropEntityID::operator EntityId()` returns the BrnCommonTypes `EntityId {u32 muValue}`,
//     which is a DIFFERENT type from the `CgsSceneManager::EntityId` the DWARF names and the one
//     this function's third parameter has. That two-EntityId fork is pre-existing and is not
//     this file's to resolve.
//
// ---- REGISTER MAP, read off the prologue 0x82612F2C..0x82612F44 ---------------------------------
//     r3 = this (r31) - r4 = lpContactInterface (r21) - r5 = lpCollisionGenerator (r25)
//     r6 = lWorldEntityId (spilled to arg_2C, reloaded per result list)
// Corroborated by the caller: EndPropWorldContactGeneration @0x82628E8C does
// `mr r6,r26 ; mr r5,r30 ; mr r4,r27 ; mr r3,r31 ; bl` with r27 = its own lpContactInterface,
// r30 = its lpCollisionGenerator and r26 = its lWorldEntityId. No float or vector parameter is
// involved, so gotcha 3 does not apply and all three slots are ordinary GPRs.
//
// ---- DECODE, address by address -----------------------------------------------------------------
//   0x82612F38  addis r29,r25,1 ; addi r29,r29,0x23BC  -> r29 = generator + 0x123BC, the address
//        of mu16NumUsedResultLists, HOISTED and re-read through for the whole function.
//        ⚠️ 0x123BC IS A CONSOLE OFFSET (gotcha 1): the host BaseCollisionGenerator is wider
//        (IOBuffer base + 8-byte pointers in mapCollisionResultLists[200]). Reached by accessor.
//   0x82612F48..0x82612F78  `lwz 0x6570` (miNumPropsAddedToContactGen, s32) vs `lhz 0(r29)`
//        (a u16) under a SIGNED `cmpw` -> assert :2813 (li r5,0xAFD), NON-GATING (falls through).
//        String read in full from the image: "miNumPropsAddedToContactGen ==
//        lpContactGenerator->GetNumUsedResultLists()".
//   0x82612F7C..0x82612F88  `cmpwi r10,0 ; ble` -> the whole function is skipped for an empty
//        generator. The bound is RE-READ at 0x826131B4 at the bottom of every outer iteration,
//        so it is live, not hoisted into a local.
//   0x82612F90  addi r26,r31,0x64BC  -> &maPropsAddedToContactGen[0]; `addi r26,r26,4` at
//        0x826131BC walks it in lockstep with the outer counter. Reached by index here.
//   0x82612FCC..0x82612FEC  `clrlwi r30,r20,16` then the bounds assert
//        "luIndex < mu16NumUsedResultLists" (CgsCollisionGenerator.h:303, li r5,0x12F) --
//        that is BaseCollisionGenerator::GetResultList's OWN tripwire, inlined; reproduced by
//        calling the accessor, whose committed body (CgsCollisionGenerator.cpp:130) carries it.
//   0x82612FF0..0x8261301C  `addi r10,r30,0x4820 ; slwi 2 ; lwzx r10,r10,r25` == the pointer in
//        mapCollisionResultLists[index] (console generator+0x12080), then FOUR word copies of the
//        16-byte list header onto the stack -- i.e. GetResultList returns the list BY VALUE.
//   0x82613020..0x82613028  `lhz` at list+0xC == mu16NumResults; zero -> next result list.
//   0x82613034  `lwz` at list+0x00 == mpResults, the record base.
//   0x82613038  `extldi r24,r11,64,32` == (u64)lWorldEntityId << 32. Computed ONCE per result
//        list, which is why the parameter is re-read from its spill slot here and not per record.
//   0x8261303C..0x82613058  assert "lu16Index < mu16NumResults" (CgsCollisionResultList.h:148,
//        li r5,0x94) -- the per-record cursor bound, at the TOP of the inner body.
//   0x8261305C..0x82613068  `r29*5 <<4` == index * 80 -> THE RECORD STRIDE IS 80, i.e. these are
//        PrimitiveTestResults (meResultType == 0), NOT the 112-byte CollisionResult that
//        CollisionResultList::GetResult indexes. Walked through a typed pointer here, exactly as
//        the committed vehicle twin does; no GetPrimitiveTestResult accessor is required.
//   0x8261306C..0x826130D0  `lbz 0(r26) ; cmplwi 3` + assert
//        "mEntityId.GetOwner() == E_ENTITYTYPE_PROP" (BrnPropEntityID.h:278, li r5,0x116).
//        That is PropEntityID::GetValue()'s own inlined AssertIsProp, re-run per record, so it is
//        reproduced by calling GetValue() inside the inner loop rather than spelled out.
//   0x82613078..0x826130B4  the record -> contact copy. Emission order is not source order; the
//        MAPPING is what is measured, and it is exact:
//            contact +0x00 mPointOnA           <- record +0x20 mPrimitive0Contact  (v126)
//            contact +0x10 mPointOnB           <- record +0x30 mPrimitive1Contact  (v125)
//            contact +0x20 mNormal             <- record +0x00 mPrimitive0Normal   (v127)
//            contact +0x40 muPolyTagA          <- record +0x40 muPrimitive0Tag
//            contact +0x44 muPolyTagB          <- record +0x44 muPrimitive1Tag
//            contact +0x48 mu16PrimitiveIndexA <- record +0x48 muPrimitive0Index   (RAW u16)
//            contact +0x4A mu16PrimitiveIndexB <- record +0x4A muPrimitive1Index   (RAW u16)
//        ⚠️ record +0x10 mPrimitive1Normal IS NEVER READ by this function. Measured, stated.
//   0x826130D4..0x82613100  the two 64-bit volume-instance ids:
//            +0x30 muVolumeInstanceIdA = ((u64)maPropsAddedToContactGen[i] << 32)
//                                        | (muPrimitive0Index & 0xFF)
//            +0x38 muVolumeInstanceIdB = ((u64)lWorldEntityId          << 32)
//                                        | (muPrimitive1Index & 0xFF)
//        The `clrlwi r9,r9,24` / `clrlwi r10,r10,24` are the 8-bit truncations; they are in the
//        asm and are reproduced, not tidied away. The <<32 placement matches CgsVolumeInstanceId.h's
//        documented layout (entity word in the HIGH dword, volume index in the low).
//   0x82613104  bl PotentialContactInterface::AddEvent  -- r3, r4 ONLY. The single-argument
//        overload @0x825E72F0, not the (u32, contact) one the vehicle path uses.
//   0x82613108..0x82613198  the debug ring. `lwz 0x64B0` null-guard, then THREE stores at
//        `base + (count % 32) * 48`:  v127(record+0x00) -> +0x20 mNormal,
//        v126(record+0x20) -> +0x00 mPoint0, v125(record+0x30) -> +0x10 mPoint1, then
//        `lwz 0x64B4 ; addi 1 ; stw 0x64B4`. The modulo is the signed
//        `srawi 5 / addze / slwi 5 / subf` idiom == `count % KI_MAX_DEBUG_WORLD_CONTACTS`.
//        ⚠️ The console recomputes the base+index THREE TIMES (it reloads mpDebugWorldContacts and
//        miNumDebugWorldContacts before each store). That is the compiler's aliasing conservatism
//        across the intervening AddEvent, not three different indices -- the counter is not
//        touched between them. Written as one reference + three field stores.
//        ⚠️ The counter is NOT wrapped, only the index is; End() zeroes it each frame
//        (`stw r28,0x64B4` at 0x82628E90, already committed).
//   0x8261319C..0x826131C4  `(u16)(++lu16Result)` inner bound, then the outer `++liResultsList`
//        with the re-read bound.
//
// ⚠️ NOT REPRODUCED, deliberately, and it is not a divergence: the console never calls
// PotentialContact::Construct here (no `bl`, no zero-fill of the record). The stack record is left
// default-initialised and every one of its nine members is assigned below, so nothing carries
// console stack garbage -- but mu16TestIndex/muPad have no counterpart on this struct at all, so
// there is nothing left uninitialised either. Same shape as the committed vehicle twin.
//
// Every console offset above appears in a COMMENT only; every member is reached by name.
// =================================================================================================
void PropManager::AddContactResultsToQueue(
    BrnPhysics::PhysicsModuleIO::PotentialContactInterface* lpContactInterface,
    CgsSceneManager::CgsCollision::CollisionGenerator*      lpCollisionGenerator,
    CgsSceneManager::EntityId                               lWorldEntityId )
{
    typedef CgsSceneManager::CgsCollision::CollisionResultList CollisionResultList;
    typedef CgsSceneManager::CgsCollision::PrimitiveTestResult PrimitiveTestResult;
    typedef CgsSceneManager::SceneManagerIO::PotentialContact  PotentialContact;

    // :2813 -- non-gating. s32 member against a u16 accessor under the console's SIGNED compare;
    // the cast keeps that signedness rather than letting the comparison go unsigned.
    CGS_ASSERT( miNumPropsAddedToContactGen
                    == static_cast<s32>( lpCollisionGenerator->GetNumUsedResultLists() ),
                "miNumPropsAddedToContactGen == lpContactGenerator->GetNumUsedResultLists()" );

    // The bound is re-read every iteration (lhz through r29 at 0x82612F7C and 0x826131B4).
    for ( s32 liResultsList = 0;
          liResultsList < static_cast<s32>( lpCollisionGenerator->GetNumUsedResultLists() );
          ++liResultsList )
    {
        // Carries the console's own "luIndex < mu16NumUsedResultLists" tripwire
        // (CgsCollisionGenerator.h:303) and returns the 16-byte header BY VALUE, as @0x825B2AE0.
        const CollisionResultList lResultList =
            lpCollisionGenerator->GetResultList( static_cast<u16>( liResultsList ) );

        const u16 lu16NumResults = lResultList.mu16NumResults;
        if ( lu16NumResults == 0 )
        {
            continue;                                                   // 0x82613028 beq
        }

        // 0x82613038 -- hoisted out of the record loop by the console, once per result list.
        const u64 lu64WorldEntityWord = static_cast<u64>( static_cast<u32>( lWorldEntityId ) ) << 32;

        // meResultType == 0 lists carry 80-byte PrimitiveTestResults; the console walks them at
        // that raw stride off mpResults (`index*80`), NOT through the 112-stride GetResult.
        // Identical to the committed twin, BrnVehicleManagerContactGeneration.cpp:713-715.
        const PrimitiveTestResult* lpaResults =
            reinterpret_cast<const PrimitiveTestResult*>( lResultList.mpResults );

        for ( u16 lu16Result = 0; lu16Result < lu16NumResults; ++lu16Result )
        {
            // The per-record cursor bound (CgsCollisionResultList.h:148), at the top of the body.
            CGS_ASSERT( lu16Result < lResultList.mu16NumResults, "lu16Index < mu16NumResults" );
            const PrimitiveTestResult& lrResult = lpaResults[lu16Result];

            // 0x8261306C..0x826130D0 -- GetValue() carries the E_ENTITYTYPE_PROP owner tripwire
            // (BrnPropEntityID.h:278) the console re-runs per record.
            const u32 luPropEntityWord =
                maPropsAddedToContactGen[liResultsList].GetValue();

            PotentialContact lContact;

            // Full 16-byte lane copies (lvx/stvx on the console): Vector3Plus source, Vector3
            // destination, w lane carried verbatim -- the committed twin's spelling.
            lContact.mPointOnA = Vector3{ lrResult.mPrimitive0Contact.x,
                                          lrResult.mPrimitive0Contact.y,
                                          lrResult.mPrimitive0Contact.z,
                                          lrResult.mPrimitive0Contact.w };
            lContact.mPointOnB = Vector3{ lrResult.mPrimitive1Contact.x,
                                          lrResult.mPrimitive1Contact.y,
                                          lrResult.mPrimitive1Contact.z,
                                          lrResult.mPrimitive1Contact.w };

            // ⚠️ UNMODIFIED -- no sign flip and no tag selection on this path. See difference (3)
            // in the ICF block above; the vehicle twin's negation must NOT be copied here.
            lContact.mNormal = lrResult.mPrimitive0Normal;

            // The 8-bit truncations are the console's (`clrlwi rN,rN,24`), not a tidy-up.
            lContact.muVolumeInstanceIdA.muId =
                ( static_cast<u64>( luPropEntityWord ) << 32 )
                | static_cast<u64>( lrResult.muPrimitive0Index & 0x00FFu );
            lContact.muVolumeInstanceIdB.muId =
                lu64WorldEntityWord
                | static_cast<u64>( lrResult.muPrimitive1Index & 0x00FFu );

            lContact.muPolyTagA          = lrResult.muPrimitive0Tag;
            lContact.muPolyTagB          = lrResult.muPrimitive1Tag;
            lContact.mu16PrimitiveIndexA = lrResult.muPrimitive0Index;   // RAW u16, not truncated
            lContact.mu16PrimitiveIndexB = lrResult.muPrimitive1Index;

            // 0x82613104 -- the SINGLE-argument overload (r3/r4 only).
            lpContactInterface->AddEvent( lContact );

            // 0x82613108 -- the debug ring, null-guarded. Store order in the asm is
            // mNormal / mPoint0 / mPoint1; written in struct order here (three independent field
            // stores to the same record, no aliasing between them).
            if ( mpDebugWorldContacts != NULL )
            {
                DebugWorldContactInfo& lrDebugContact =
                    mpDebugWorldContacts[miNumDebugWorldContacts % KI_MAX_DEBUG_WORLD_CONTACTS];

                lrDebugContact.mPoint0 = Vector3{ lrResult.mPrimitive0Contact.x,
                                                  lrResult.mPrimitive0Contact.y,
                                                  lrResult.mPrimitive0Contact.z,
                                                  lrResult.mPrimitive0Contact.w };
                lrDebugContact.mPoint1 = Vector3{ lrResult.mPrimitive1Contact.x,
                                                  lrResult.mPrimitive1Contact.y,
                                                  lrResult.mPrimitive1Contact.z,
                                                  lrResult.mPrimitive1Contact.w };
                lrDebugContact.mNormal = lrResult.mPrimitive0Normal;

                ++miNumDebugWorldContacts;                              // 0x82613190, unwrapped
            }
        }
    }
}

// =================================================================================================
// BrnPhysics::Props::PropManager::DoPartWorldContactGeneration @0x82611B70  (349 asm insns)
//
// ⭐ LANDED 2026-08-19 (wave Q6). One leg of BeginPropWorldContactGeneration's per-UpdatePropEvent
// dispatch -- the arm taken when the event's PropEntityID carries a NON-ZERO part index, i.e. a
// SHED PANEL of a smashed prop. It builds a primitive-pair list out of that part's collision
// volumes, pairs it with the world triangles cached for the part's triangle-cache slot, and posts
// one collision job.
//
// GROUNDING: the RAW `assembly` array of .ida-exports/BURNOUT_X360_ARTIST.XEX/0x82611B70.json,
// re-read line by line this wave (scratchpad/waveQ6/asm_82611B70.txt). The Hex-Rays pseudocode in
// that same export was NOT consulted -- its `a5, a6` prototype drops the vector parameter entirely.
// 349 == (0x826120E0 - 0x82611B70)/4 + 1, and the export listing has exactly 349 lines.
//
// ---- CONSOLE ARGUMENT MAP, read off the prologue 0x82611B88..0x82611BA4 ------------------------
//     r3 = this (r19) · r4 = lpContactGenerator (r17) · r5 = lpTriCache (r18)
//     r6 = &lUpdatePartEvent (r22) · r7 = &liNumJobsAdded (r16) · r8 = lpMalloc (r26)
//     v1 = lvfTimeStep (saved to v127 at 0x82611B8C)
//   ⚠️ A VECTOR ARG CONSUMES NO GPR SLOT, so lvfTimeStep's POSITION in the parameter list is
//   DWARF-attested only (BrnPropManager.h carries the same note on the declaration); the asm alone
//   cannot order it.
//
// ---- DECODE, address by address ----------------------------------------------------------------
//   0x82611BA8  cmplwi r17,0 / bne        assert "lpContactGenerator != NULL", li r5,0x991 ==
//                                         BrnPropManager.cpp:2449. NON-GATING (falls through).
//   0x82611BD0  lhz 0x64(r22) / extsh     liPartIndex = event.miPhysicsSlot -- the member is s16
//                                         and is SIGN-extended into an s32 local.
//   0x82611BD8  cmpwi 0 / bge             assert "liPartIndex >= 0"          li r5,0x9A2 == :2466
//   0x82611BFC  cmpwi 0x1E / blt          assert "liPartIndex < ...", li r5,0x9A3 == :2467.
//                                         0x1E == 30 == KU_MAX_PHYSICAL_PROP_PARTS.
//   0x82611C30  bl HasPartJustBeenRemoved(event.mEntityId @0x60, liPartIndex)
//   0x82611C3C  clrlwi/bne -> 0x826120D0  TRUE => return (branch straight to the epilogue).
//   0x82611C44..0x82611D2C                the SECOND, message-CARRYING range check, which fires
//                                         only when liPartIndex is outside [0,30):
//                                           "Updated part was removed: Part ID = " << u32 id
//                                           << ", Returned part index = " << s32 index
//                                         li r5,0x9AE == :2478, exactly the DWARF's
//                                         `CgsDev::StrStream lStrStream @ BrnPropManager.cpp:2478`.
//                                         ⚠️ It also re-runs the mEntityId.GetOwner() ==
//                                         E_ENTITYTYPE_PROP tripwire (BrnPropEntityID.h:278,
//                                         li r5,0x116) -- that is PropEntityID::GetValue()'s OWN
//                                         inlined assert, not a separate source line, so it is
//                                         reproduced by CALLING GetValue() rather than spelled out.
//   0x82611D30  lwz 0x8C(r19) + slwi r29,6   lpPart = &mpaPartInstances[liPartIndex]
//                                         (console stride 64 == the CONSOLE sizeof(PropPartInstance);
//                                          the host indexes the typed array -- gotcha 1).
//   0x82611D40  addi r20, r29, 0x2B       liCacheSlotIndex = 43 + liPartIndex.
//   0x82611D44/48  lwz 0x34 / lbz 0x38    luTypeId = lpPart->GetType(), luPartId = GetPartId();
//                                         both loaded BEFORE the operator-> call clobbers r3.
//   0x82611D4C  bl ResourcePtr<PropPhysicsDataHeader>::operator-> on this+0x54 (0x822868E0,
//               identified in the tree at BrnPropQueueFacades.cpp:87), then GetType(luTypeId).
//   0x82611D60..0x82611D78  `slwi r9,r11,1 ; add r11,r11,r9 ; slwi r11,r11,4` == luPartId * 48,
//               added to `lwz 0x40(propType)` == PropTypeData::maParts.
//               ⚠️ 48 IS THE CONSOLE STRIDE; the HOST PropPartTypeData is 64 bytes, which is why
//               this indexes GetParts()[luPartId] instead of doing the console's byte arithmetic.
//   0x82611D7C  lbz 0x2C(partType)        lpPartType->GetNumberOfVolumes()
//   0x82611D80  bl PrimitivePairListBuilder::Prepare(lpMalloc, that count)
//
//   ---- THE PADDING (0x82611D84..0x82611E5C) ----------------------------------------------------
//   0x82611D98  lfs 0x28(partType)        the bounding radius, splatted (`vspltw ,0`).
//   0x82611DA8  lvx r31+0x10 / r31+0x20   lpPart->GetLinearVelocity() / GetAngularVelocity()
//   0x82611DB4  vmsum3fp128 x2            the two 3-lane MagnitudeSquared
//   0x82611DD4..0x82611E4C                vrsqrtefp + TWO Newton-Raphson refinements, then
//                                         lenSq * rsqrt == the magnitude, with a
//                                         `vcmpeqfp(0,lenSq)` + `vsel` zero-length guard.
//     ⚠️⚠️ THE VMX OPERAND-ORDER RULE THAT MAKES THIS READABLE (gotcha 9; a previous wave got it
//     backwards): IDA prints the VA-form as (vD, vA, vB, vC) while the arithmetic is vA*vC (+/-)
//     vB. So `vnmsubfp v7,v0,v11,v5` == v11 - v0*v5 == 1 - a*x^2 and `vmaddfp v9,v3,v9,v7` ==
//     v3*v7 + v9 == the NR step x + 0.5x(1 - a x^2). Read the other way the block is nonsense.
//     The 1.0f / 0.5f are `vcfsx v0,0` / `vcfsx v0,1` over `vspltisw v0,1` -- MATERIALISED, not
//     loaded from rodata.
//     HOST SPELLING: rw::math::vpu::Magnitude (an exact std::sqrt of MagnitudeSquared), which the
//     DWARF names TWICE in this scope -- so this is a de-optimisation, not a rewrite. The console's
//     vsel guard exists because rsqrte(0) is +inf and 0*inf is NaN; std::sqrt(0) is 0, so the host
//     needs no guard and none is added.
//   0x82611E50  vmaddfp v0, v13, v0, v5   == v13*v5 + v0 == |angular| * radius + |linear|
//   0x82611E54  vmulfp128 v0, v0, v127    *= lvfTimeStep
//   0x82611E58  vminfp v0, v0, v12        Min(that, unk_82FB94F0) == KVF_MAX_PROP_PADDING
//   0x82611E64  lfs f31, <that slot>      the f32 lane-0 read the AddPrimitive calls pass in f1.
//
//   ---- THE VOLUME LOOP (0x82611E80..0x82611F58) ------------------------------------------------
//   0x82611E84  lwz 0x24(partType) + vol*96   lpPartType->GetCollisionVolume(lu8Vol). 96 ==
//               sizeof(rw::collision::Volume), already static_asserted in BrnPhysicsPropTypeData.h,
//               so the typed index is exact and 96 does NOT widen.
//   0x82611E88..0x82611F40  a full Matrix44Affine product, broadcast+FMA:
//               out.row_i = vol.row_i.x*ev.row0 + vol.row_i.y*ev.row1 + vol.row_i.z*ev.row2
//                           (+ ev.row3 for the position row)
//               == `lpPartVolume->GetRelativeTransform() * lUpdatePartEvent.mTransform`. The
//               volume's four rows come from volume+0x00..+0x30, which IS its embedded transform
//               (volume_debug_access.h already returns it). The DWARF confirms
//               `rw::math::vpu::operator*` here.
//   0x82611E74  clrlwi r29,r29,16         the primitive tag is (u16)liPartIndex.
//   0x82611F44  bl sub_82814AB8           AddPrimitive(volume, transform, padding, tag)
//   0x82611F4C  lbz 0x2C(partType)        THE LOOP BOUND IS RE-READ EVERY ITERATION, and the
//                                         counter is an 8-bit `clrlwi ,24` compared UNSIGNED
//                                         (`cmplw`) -- the DWARF's `uint8_t lu8Vol @2502`.
//
//   ---- THE TRIANGLE LIST (0x82611F5C..0x82612058) ----------------------------------------------
//   0x82611F5C  assert "mpTriangleCacheManager != NULL" (CgsSceneManagerModuleIO.h:1286, 0x506) --
//               the INLINED body of TriangleCacheInterface::GetCache. The DWARF spells that method
//               GetCachedTriangles; the tree committed the same @0x82277810 method as GetCache --
//               same method, no fork. That is why three baked tripwires appear here with no `bl`.
//   0x82611F90..0x82611FE4  slot*48 into the manager's per-slot table (+0x04), `lwz 0x24` == the
//               batch base index, `mulli 0xE0` == * sizeof(Triangle4) off mpaTriangleCache (+0x00),
//               with the manager's own "mpaTriangleCache != NULL" (:153) tripwire between --
//               i.e. exactly the committed TriangleCacheManager::GetTrianglesForCachedObject.
//   0x82611FE8  assert :1295 (0x50F)      the second inlined accessor ==
//               GetNumCachedTriangleBatches, whose count is the per-slot `lwz 0x28`.
//   0x8261201C  two stw + bl CheckAlignment == TriangleList::SetTriangleBuffer INLINED. That method
//               (DWARF CgsTriangleList.h:21) does not exist in the tree, which is why the two
//               member stores are open-coded -- exactly as the committed sibling
//               BrnVehicleManagerContactGeneration.cpp:591-593 already does. HEADER REQUEST C.
//   0x82612028..0x82612058  the Triangle4::AssertIsValid walk at a 0xE0 stride with the count
//               re-read each iteration == TriangleList::ValidateTriangles (committed).
//
//   ---- THE POST (0x8261205C..0x826120CC) -------------------------------------------------------
//   0x82612084  slwi r11,(miNumPropsAddedToContactGen + 0x192F),2 ; stwx r9,r11,r19
//               0x192F*4 == 0x64BC == offsetof(maPropsAddedToContactGen), so this is the array
//               write followed by the ++. NO BOUNDS CHECK in the console (the array is 45 long);
//               none is added.
//   0x82612078  lbz byte_82F2A39C / bne   the stream-vs-synchronous selector.
//   0x826120A8  synchronous arm  CollidePrimitiveListAgainstTriangleList(prims, tris,
//                                  r6=0x40 max, r7=0 tagA, r8=0 tagB, r9=1 useOptimisedBoxTests)
//   0x826120C0  stream arm       AddPrimitiveListWithTriangleListToStream(prims, tris, 0x40,
//                                  r7=1 useOptimisedBoxTests, r8=0 tagA, r9=0 tagB,
//                                  r10 = mpPrimitiveWithTriangleStream (lwz 0xA0))
//     ⚠️ THE TWO CALLEES TAKE THE BOOL AND THE TAGS IN A DIFFERENT ORDER. Both orders are the
//     DWARF's own and both are register-confirmed. Do NOT "tidy" them to match.
//   0x826120C4  ++liNumJobsAdded through the reference.
//
// Every console offset above appears in a COMMENT only; every member is reached by name.
// =================================================================================================
void PropManager::DoPartWorldContactGeneration(
    CgsSceneManager::CgsCollision::CollisionGenerator*             lpContactGenerator,
    const CgsSceneManager::SceneManagerIO::TriangleCacheInterface* lpTriCache,
    const UpdatePropEvent&                                         lUpdatePartEvent,
    s32&                                                           liNumJobsAdded,
    CgsMemory::LinearMalloc*                                       lpMalloc,
    VecFloat                                                       lvfTimeStep )
{
    typedef CgsSceneManager::CgsCollision::PrimitivePairListBuilder PrimitivePairListBuilder;
    typedef CgsSceneManager::CgsCollision::TriangleList             TriangleList;

    // BrnPropManager.cpp:2449 -- non-gating tripwire (the asm falls through).
    CGS_ASSERT( lpContactGenerator != NULL, "lpContactGenerator != NULL" );

    // lhz +0x64 / extsh: the event's slot member is s16, SIGN-extended into an s32 local.
    const s32 liPartIndex = lUpdatePartEvent.miPhysicsSlot;

    CGS_ASSERT( liPartIndex >= 0, "liPartIndex >= 0" );                                   // :2466
    CGS_ASSERT( liPartIndex < static_cast<s32>( KU_MAX_PHYSICAL_PROP_PARTS ),
                "liPartIndex < static_cast< int32_t > ( KU_MAX_PHYSICAL_PROP_PARTS )" );  // :2467

    if ( HasPartJustBeenRemoved( lUpdatePartEvent.mEntityId, liPartIndex ) )
    {
        return;                                             // 0x82611C3C bne -> the epilogue
    }

    // :2478. The console streams this into the SHARED global sink CgsDev::Assert::gpcMessageBuffer;
    // that global is not declared anywhere in this tree, so the message is built on a stack buffer
    // of the same KI_MESSAGEBUFFERSIZE -- the committed idiom (CgsID.cpp:73-84,
    // PropManager_wQ2_02.cpp:317). Message content, stream order and the fired line are unchanged.
    if ( liPartIndex < 0 || liPartIndex >= static_cast<s32>( KU_MAX_PHYSICAL_PROP_PARTS ) )
    {
        char lacMessage[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
        CgsDev::StrStream lStrStream( lacMessage, CgsDev::Assert::KI_MESSAGEBUFFERSIZE );
        lStrStream << "Updated part was removed: Part ID = ";
        lStrStream << lUpdatePartEvent.mEntityId.GetValue();   // carries the owner tripwire :278
        lStrStream << ", Returned part index = ";
        lStrStream << liPartIndex;
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert( lacMessage, __FILE__, __LINE__ );
        CgsDev::Assert::EndAssert();
    }

    PropPartInstance* lpPart = &mpaPartInstances[liPartIndex];          // console stride 64
    const s32 liCacheSlotIndex = KI_PROP_PART_CACHE_START_INDEX + liPartIndex;

    const u32 luTypeId = lpPart->GetType();                             // +0x34
    const u32 luPartId = lpPart->GetPartId();                           // +0x38

    const PropTypeData*     lpPropType = mpPhysicsData->GetType( luTypeId );
    const PropPartTypeData* lpPartType = &lpPropType->GetParts()[luPartId];   // console stride 48

    PrimitivePairListBuilder lPrimPairList;
    lPrimPairList.Prepare( lpMalloc, lpPartType->GetNumberOfVolumes() );      // lbz 0x2C

    // |linear| + |angular| * boundingRadius, scaled by the timestep and capped. See the banner for
    // the vmaddfp/vnmsubfp operand-order rule that makes this the right reading of the console's
    // rsqrt+2NR+vsel block.
    VecFloat lvfPadding = vpu::Splat( vpu::Magnitude( lpPart->GetLinearVelocity() ) );
    lvfPadding = lvfPadding
               + vpu::Splat( vpu::Magnitude( lpPart->GetAngularVelocity() ) )
                     * vpu::Splat( lpPartType->GetBoundingRadius() );         // lfs 0x28
    lvfPadding = lvfPadding * lvfTimeStep;
    lvfPadding = vpu::Min( lvfPadding, KVF_MAX_PROP_PADDING );                // vminfp

    // The bound is RE-READ every iteration (lbz 0x2C at both 0x82611DA0 and 0x82611F4C) and the
    // counter is an 8-bit value compared UNSIGNED -- the DWARF's `uint8_t lu8Vol`.
    for ( u8 lu8Vol = 0; lu8Vol < lpPartType->GetNumberOfVolumes(); ++lu8Vol )
    {
        ::rw::collision::Volume* lpPartVolume = lpPartType->GetCollisionVolume( lu8Vol );

        const Matrix44Affine lPartTransform =
            lpPartVolume->GetRelativeTransform() * lUpdatePartEvent.mTransform;

        lPrimPairList.AddPrimitive( lpPartVolume, lPartTransform,
                                    vpu::GetComponent( lvfPadding, 0 ),   // VecFloat -> f32, in f1
                                    static_cast<u16>( liPartIndex ) );    // clrlwi r29,r29,16
    }

    // The DWARF spells the first accessor GetCachedTriangles; the tree committed the same
    // @0x82277810 method as GetCache. Same method, no fork.
    const CgsGeometric::Triangle4* lp4Tris = lpTriCache->GetCache( liCacheSlotIndex );
    const s32 liNum4Tris = lpTriCache->GetNumCachedTriangleBatches( liCacheSlotIndex );

    // HEADER REQUEST C: the console called TriangleList::SetTriangleBuffer(lp4Tris, liNum4Tris) and
    // the compiler inlined it to these two member stores plus its out-of-line CheckAlignment().
    TriangleList lListOfCachedTris;
    lListOfCachedTris.mpTriangles    = const_cast<CgsGeometric::Triangle4*>( lp4Tris );
    lListOfCachedTris.miNumTriangles = liNum4Tris;
    lListOfCachedTris.CheckAlignment();
    lListOfCachedTris.ValidateTriangles();

    // 0x82612084: maPropsAddedToContactGen[miNumPropsAddedToContactGen] = the event's id.
    // The console does NOT bounds-check the 45-entry array; none is added.
    maPropsAddedToContactGen[miNumPropsAddedToContactGen] = lUpdatePartEvent.mEntityId;
    ++miNumPropsAddedToContactGen;

    if ( !KB_USE_CONTACT_GEN_STREAM )
    {
        // 0x826120A8 -- r7 = 0 tag A, r8 = 0 tag B, r9 = 1 the bool. Return value dropped.
        lpContactGenerator->CollidePrimitiveListAgainstTriangleList(
            &lPrimPairList, &lListOfCachedTris, KU16_COLLIDE_MAX_RESULTS, 0u, 0u, true );
    }
    else
    {
        // 0x826120C0 -- NOTE the different order: r7 = 1 the bool, r8 = 0 tag A, r9 = 0 tag B,
        // r10 = mpPrimitiveWithTriangleStream (lwz 0xA0). Return value dropped.
        lpContactGenerator->AddPrimitiveListWithTriangleListToStream(
            &lPrimPairList, &lListOfCachedTris, KU16_COLLIDE_MAX_RESULTS, true, 0u, 0u,
            mpPrimitiveWithTriangleStream );
    }

    ++liNumJobsAdded;                                       // 0x826120C4 through the reference
}

// =================================================================================================
// BrnPhysics::Props::PropManager::DoPropInstanceWorldContactGeneration @0x826120E8  (342 insns)
//
// ⭐ LANDED 2026-08-19 (wave Q6). The other leg -- the arm taken when the event's PropEntityID
// carries a ZERO part index, i.e. a WHOLE prop.
//
// GROUNDING: the RAW `assembly` array of .ida-exports/BURNOUT_X360_ARTIST.XEX/0x826120E8.json,
// re-read this wave (scratchpad/waveQ6/asm_826120E8.txt); the Hex-Rays pseudocode was NOT
// consulted. 342 == (0x8261263C - 0x826120E8)/4 + 1, and the listing has exactly 342 lines.
//
// ---- IT IS THE SAME FUNCTION AS ITS DoPart TWIN, WITH THE PROP ARM SUBSTITUTED -----------------
// MEASURED, not assumed: strip the addresses from both export listings and diff them -- the two
// bodies are instruction-for-instruction the same shape, and every difference is one of the eleven
// below (plus register allocation and branch targets). That is why the two bodies read as
// near-copies here: the console's two bodies are too.
//
//   #  DoPart @0x82611B70                        DoPropInstance @0x826120E8
//   1  lhz 0x64(event) / extsh -> liPartIndex    same, -> liPropIndex
//   2  asserts :2449 :2466 :2467 :2478           asserts :2574 :2588 :2589 :2600
//                                                (li r5 0x991/0x9A2/0x9A3/0x9AE vs
//                                                       0xA0E/0xA1C/0xA1D/0xA28)
//   3  bound `cmpwi 0x1E` == 30                  bound `cmpwi 0xF` == 15
//      (KU_MAX_PHYSICAL_PROP_PARTS)              (KU_MAX_PHYSICAL_PROPS)
//   4  bl HasPartJustBeenRemoved                 bl HasPropJustBeenRemoved
//   5  "Updated part was removed: Part ID = "    "Updated prop was removed: Prop ID = "
//      ", Returned part index = "                ", Returned prop index = "
//   6  lwz 0x8C(this) + `slwi 6` (stride 64)     lwz 0x7C(this) + `mulli 0x70` (stride 112)
//      == mpaPartInstances                       == mpaPropInstances
//   7  `addi r20, idx, 0x2B` == slot 43 + i      `addi r21, idx, 0x1C` == slot 28 + i
//   8  lwz 0x34 / lbz 0x38 == GetType/GetPartId, lwz 0x64 == GetTypeId ONLY -- no part id, and NO
//      then `lwz 0x40(type)` + partId*48         maParts indexing at all
//   9  volume count `lbz 0x2C(partType)`         volume count `lbz 0x5E(propType)`
//      radius       `lfs 0x28(partType)`         radius       `lfs 0x44(propType)`
//      volumes      `lwz 0x24(partType)`         volumes      `lwz 0x3C(propType)`
//  10  velocities   lvx part+0x10 / part+0x20    velocities   lvx prop+0x40 / prop+0x50
//  11  DWARF locals lPartTransform @2505         DWARF locals lpRelativeTransform @2625 (a POINTER)
//                                                + lVolumeMatrix @2626
//
// EVERYTHING ELSE IS IDENTICAL, and was re-checked line by line this wave: the null-generator
// assert, the double range check, the ResourcePtr operator-> + PropPhysicsDataHeader::GetType pair,
// PrimitivePairListBuilder::Prepare, the whole rsqrt+2NR+vsel Magnitude pair, the `vminfp` against
// unk_82FB94F0, the per-volume Matrix44Affine product and AddPrimitive, the triangle-cache fetch
// with its three inlined asserts, the Triangle4::AssertIsValid walk, the
// maPropsAddedToContactGen post, the `lbz byte_82F2A39C` selector with its two arms IN THE SAME
// ARGUMENT ORDERS, and the `++liNumJobsAdded` through the reference. See the DoPart banner above
// for the shared decode; only the deltas are annotated inline below.
//
// ---- CONSOLE ARGUMENT MAP, read off the prologue 0x82612100..0x8261211C -----------------------
//     r3 = this (r19) · r4 = lpContactGenerator (r17) · r5 = lpTriCache (r18)
//     r6 = &lUpdatePropEvent (r22) · r7 = &liNumJobsAdded (r16) · r8 = lpMalloc (r27)
//     v1 = lvfTimeStep (saved to v127) -- again, a vector arg consumes NO GPR slot, so its
//     POSITION is DWARF-attested, not register-attested.
//
// ---- ONE SOURCE-SHAPE NOTE, STATED NOT HIDDEN ------------------------------------------------
// The DecFIGS scope names `Matrix44Affine* lpRelativeTransform @2625` -- a POINTER, because the
// SDK's own Volume::GetRelativeTransform returns `Matrix44Affine*`. This tree's accessor
// (volume_debug_access.h) deliberately returns a REFERENCE, so the local is a reference here.
// Same object, same product; recorded so nobody reads the difference as a recovered semantic.
// =================================================================================================
void PropManager::DoPropInstanceWorldContactGeneration(
    CgsSceneManager::CgsCollision::CollisionGenerator*             lpContactGenerator,
    const CgsSceneManager::SceneManagerIO::TriangleCacheInterface* lpTriCache,
    const UpdatePropEvent&                                         lUpdatePropEvent,
    s32&                                                           liNumJobsAdded,
    CgsMemory::LinearMalloc*                                       lpMalloc,
    VecFloat                                                       lvfTimeStep )
{
    typedef CgsSceneManager::CgsCollision::PrimitivePairListBuilder PrimitivePairListBuilder;
    typedef CgsSceneManager::CgsCollision::TriangleList             TriangleList;

    // BrnPropManager.cpp:2574 -- non-gating tripwire (the asm falls through).
    CGS_ASSERT( lpContactGenerator != NULL, "lpContactGenerator != NULL" );

    // lhz +0x64 / extsh: the event's slot member is s16, SIGN-extended into an s32 local.
    const s32 liPropIndex = lUpdatePropEvent.miPhysicsSlot;                       // DWARF :2576

    CGS_ASSERT( liPropIndex >= 0, "liPropIndex >= 0" );                           // :2588
    CGS_ASSERT( liPropIndex < static_cast<s32>( KU_MAX_PHYSICAL_PROPS ),
                "liPropIndex < static_cast< int32_t > ( KU_MAX_PHYSICAL_PROPS )" ); // :2589

    if ( HasPropJustBeenRemoved( lUpdatePropEvent.mEntityId, liPropIndex ) )
    {
        return;                                            // 0x826121B4 bne -> the epilogue
    }

    // :2600 -- same shared-message-buffer treatment as the DoPart twin.
    if ( liPropIndex < 0 || liPropIndex >= static_cast<s32>( KU_MAX_PHYSICAL_PROPS ) )
    {
        char lacMessage[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
        CgsDev::StrStream lStrStream( lacMessage, CgsDev::Assert::KI_MESSAGEBUFFERSIZE );
        lStrStream << "Updated prop was removed: Prop ID = ";
        lStrStream << lUpdatePropEvent.mEntityId.GetValue();  // carries the owner tripwire :278
        lStrStream << ", Returned prop index = ";
        lStrStream << liPropIndex;
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert( lacMessage, __FILE__, __LINE__ );
        CgsDev::Assert::EndAssert();
    }

    // console `mulli r11, liPropIndex, 0x70` -- stride 112 is a CONSOLE size; the host indexes the
    // typed array and lets sizeof do the striding (gotcha 1).
    PropInstance* lpProp = &mpaPropInstances[liPropIndex];                        // DWARF :2577
    const s32 liCacheSlotIndex = KI_PROP_CACHE_START_INDEX + liPropIndex;

    // `lwz 0x64(lpProp)` is muTypeId, loaded before the ResourcePtr operator-> call clobbers r3;
    // that is emission order, not a second statement.
    const PropTypeData* lpPropType = mpPhysicsData->GetType( lpProp->GetTypeId() );

    PrimitivePairListBuilder lPrimPairList;                                       // DWARF :2613
    lPrimPairList.Prepare( lpMalloc, lpPropType->GetNumberOfVolumes() );          // lbz 0x5E

    VecFloat lvfPadding = vpu::Splat( vpu::Magnitude( lpProp->GetLinearVelocity() ) );
    lvfPadding = lvfPadding
               + vpu::Splat( vpu::Magnitude( lpProp->GetAngularVelocity() ) )
                     * vpu::Splat( lpPropType->GetBoundingRadius() );             // lfs 0x44
    lvfPadding = lvfPadding * lvfTimeStep;
    lvfPadding = vpu::Min( lvfPadding, KVF_MAX_PROP_PADDING );                    // vminfp

    // The bound is RE-READ every iteration (lbz 0x5E at both 0x82612328 and 0x826124A8).
    for ( u8 lu8Vol = 0; lu8Vol < lpPropType->GetNumberOfVolumes(); ++lu8Vol )
    {
        // console `lwz 0x3C(propType)` + `vol*96`; 96 == sizeof(rw::collision::Volume), already
        // static_assert'd in BrnPhysicsPropTypeData.h, so the typed index is exact.
        ::rw::collision::Volume* lpVolume = lpPropType->GetCollisionVolume( lu8Vol ); // DWARF :2624

        const Matrix44Affine& lrRelativeTransform = lpVolume->GetRelativeTransform();

        const Matrix44Affine lVolumeMatrix =
            lrRelativeTransform * lUpdatePropEvent.mTransform;                    // DWARF :2626

        lPrimPairList.AddPrimitive( lpVolume, lVolumeMatrix,
                                    vpu::GetComponent( lvfPadding, 0 ),  // VecFloat -> f32, in f1
                                    static_cast<u16>( liPropIndex ) );   // clrlwi r26,r26,16
    }

    const CgsGeometric::Triangle4* lp4Tris = lpTriCache->GetCache( liCacheSlotIndex );  // DWARF :2579
    const s32 liNum4Tris = lpTriCache->GetNumCachedTriangleBatches( liCacheSlotIndex ); // DWARF :2578

    // HEADER REQUEST C -- the inlined TriangleList::SetTriangleBuffer, open-coded.
    TriangleList lListOfCachedTris;                                               // DWARF :2580
    lListOfCachedTris.mpTriangles    = const_cast<CgsGeometric::Triangle4*>( lp4Tris );
    lListOfCachedTris.miNumTriangles = liNum4Tris;
    lListOfCachedTris.CheckAlignment();
    lListOfCachedTris.ValidateTriangles();

    // 0x826125E0: maPropsAddedToContactGen[miNumPropsAddedToContactGen] = the event's id.
    // The console does NOT bounds-check the 45-entry array; none is added.
    maPropsAddedToContactGen[miNumPropsAddedToContactGen] = lUpdatePropEvent.mEntityId;
    ++miNumPropsAddedToContactGen;

    if ( !KB_USE_CONTACT_GEN_STREAM )
    {
        // 0x82612604 -- r7 = 0 tag A, r8 = 0 tag B, r9 = 1 the bool. Return value dropped.
        lpContactGenerator->CollidePrimitiveListAgainstTriangleList(
            &lPrimPairList, &lListOfCachedTris, KU16_COLLIDE_MAX_RESULTS, 0u, 0u, true );
    }
    else
    {
        // 0x8261261C -- NOTE the different order: r7 = 1 the bool, r8 = 0 tag A, r9 = 0 tag B,
        // r10 = mpPrimitiveWithTriangleStream (lwz 0xA0). Return value dropped.
        lpContactGenerator->AddPrimitiveListWithTriangleListToStream(
            &lPrimPairList, &lListOfCachedTris, KU16_COLLIDE_MAX_RESULTS, true, 0u, 0u,
            mpPrimitiveWithTriangleStream );
    }

    ++liNumJobsAdded;                                       // 0x82612620 through the reference
}

}
}

// ============================================================================
// FOLDED FROM PropManager_wQ2_02.cpp (wave Q2) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================
// =================================================================================================
// GameSource/Physics/PropManager/PropManager_wQ2_02.cpp
//
// Partfile of the TU GameSource/Unity/../Physics/PropManager/BrnPropManager.cpp
// (breakable-props wave Q ROUND 2, lander 02, 2026-08-18). Folds back into BrnPropManager.cpp.
//
// THREE FUNCTIONS, all of them the prop-vs-world triangle-collision leg:
//
//   * PropManager::GetTriangleCacheSlotAndRadius  @0x826115C0 (247 insns) -- FRESH this round.
//         Called by the already-landed PropManager_wQ_03.cpp::UpdateTriangleCache, which was
//         gate-green but not link-green without it.
//   * PropManager::BeginPropWorldContactGeneration @0x82628CB0 (89 insns) -- round-1 body,
//         parked at scratchpad/waveQ/parked/PropManager_03_BeginPropWorldContactGeneration.cpp
//         on two missing BaseCollisionGenerator declarations. THEY LANDED (round-2 collgen
//         owner): CgsCollisionGenerator.h:240 / :241-242. Body brought in-tree here.
//   * PropManager::EndPropWorldContactGeneration   @0x82628E18 (37 insns) -- round-1 body,
//         parked on BaseCollisionGenerator::GetNumUsedResultLists(). THAT LANDED TOO
//         (CgsCollisionGenerator.h:114, a public inline). Body brought in-tree here.
//
// LINK NOTE. Each of the three functions below is the SOLE definition of its symbol: the conductor
//    gates that once shadowed Begin/End -- and the one that shadowed PropManager_wQ_03.cpp's
//    UpdateTriangleCache -- are gone, and this file is mounted. It mounts as a PAIR with wQ_03,
//    whose UpdateTriangleCache calls this file's GetTriangleCacheSlotAndRadius and would take an
//    LNK2019 without it.
//
// GROUNDING. Every body below was re-derived this round from the RAW `assembly` array of
// .ida-exports/BURNOUT_X360_ARTIST.XEX/<addr>.json (Hex-Rays pseudocode NOT consulted; its
// prototype for 0x826115C0 is the usual variadic-arg mislabel). Instruction counts stated
// anywhere in this file are counts of the lines in that export listing, which I counted:
// 247 / 89 / 37 (prologue + body + epilogue; the round-1 End banner's "25" was a body-only
// count -- a different measure, not a contradiction).
// Rodata was read out of IDA Files/BURNOUT_X360_ARTIST.XEX.i64 with headless IDA 9.3 this
// round (scratchpad/waveQ2/probe_wq2lander/ida/dump.txt) rather than trusted from a banner:
//     flt_82001CC0 = 00 00 00 00 = 0.0f      flt_820138DC = 42 48 00 00 = 50.0f
//     flt_82004014 = 3D CC CC CD = 0.1f
// and every truncated assert string was read in full from the same probe.
//
// ⚠️ NO CONSOLE OFFSET IS USED AS A HOST OFFSET anywhere below (AGENTS.md gotcha 1). Console
// immediates appear only in comments; every member/array is reached by name, every record is
// indexed by its host type. The three places this actually bites are called out inline:
// PropInstance stride 112 vs PropPartInstance stride 64 (console) and PropPartTypeData
// stride 48 console / 64 host -- the last one is why the part-type lookup indexes
// GetParts()[luPartId] instead of doing the console's `base + luPartId*48`.
// =================================================================================================


namespace BrnPhysics
{
namespace Props
{

// (fold: an identical definition of KI_PROP_CACHE_START_INDEX was dropped here -- this TU defines it once, above)
// (fold: an identical definition of KI_PROP_PART_CACHE_START_INDEX was dropped here -- this TU defines it once, above)

// The upper sanity bound the :2259 radius tripwire compares against.
// ⚠️ AUTHORED NAME -- the console has only the rodata float, and the DWARF's
// BrnTriangleCacheConstants.h:44 KF_DEFAULT_PROP_CACHE_SPHERE_SIZE prints NO value, so this is
// NOT that constant being recovered and the two must not be conflated. What is MEASURED is the
// value: flt_820138DC reads 42 48 00 00 big-endian == 50.0f (headless IDA 9.3 on the .i64, this
// round). Everything else about the name is this file's invention and is flagged as such.
static const f32 KF_MAX_PROP_CACHE_SPHERE_RADIUS = 50.0f;   // flt_820138DC, measured

// =================================================================================================
// BrnPhysics::Props::PropManager::GetTriangleCacheSlotAndRadius @ 0x826115C0  (247 asm insns)
//
// Resolve one live prop-or-part handle to (a) its fixed triangle-cache slot and (b) the radius of
// the collision sphere the scene should cache triangles inside. Returns false -- leaving both
// out-params untouched -- when the entity was removed earlier this frame. Its one caller,
// UpdateTriangleCache @0x826119A0, zeroes both out-params before every call and tests the bool
// with a `beq` early-out (0x82611A68; the `clrlwi r11,r3,24` / `cmplwi cr6,r11,0` pair that feeds
// it is 0x82611A60 / 0x82611A64).
//
// ---- REGISTER MAP, read off the prologue (0x826115CC..0x826115E8) -------------------------------
//     r3 = this (r28) - r4 = lPropEntityId (r31) - r5 = liPhysicalPropIndex (r30)
//     r6 = &liOutCacheSlotIndex (r24) - r7 = &lfOutCacheSphereRadius (r25)
// No float or vector parameter is involved, so gotcha 3 (an f32 riding f1 and SKIPPING its GPR
// slot) does not apply here -- all five slots are ordinary GPRs and the DWARF class declaration
// (dwarfdump BrnPropManager.h:199, dumpfile line 232) agrees register-for-register. ⚠️ Round-2 fix:
// this used to cite ":232", which is the DUMPFILE line -- everywhere else in this file ".h:NNN"
// means a SOURCE line, and this function's own declaration comment in BrnPropManager.h already
// cites :199 for it.
//
// ---- THE BRANCH, and the one honest divergence --------------------------------------------------
//   0x826115D4  clrlwi r11, r31, 22          == lPropEntityId & 0x3FF == the part index
//   0x826115DC..0x826115EC  subfic/subfe/clrlwi 31   the standard (x != 0) boolean idiom
//   0x826115F4/0x82611618   cmplwi / beq -> loc_826117FC (the PROP arm)
// ⚠️ DIVERGENCE, STATED NOT HIDDEN: the console emits that mask INLINE, with NO `bl` between
// 0x826115D0 and 0x826115F4 -- i.e. no owner tripwire at all. The tree's only accessor for that
// field is the OUT-OF-LINE PropEntityID::GetPartIndex() (BrnPropEntityID.cpp:43), whose first
// statement is AssertIsProp(). Using it therefore adds one call plus one owner tripwire per live
// prop per frame that the shipped image does not have. It is VALUE-IDENTICAL (both are
// muValue & KU_PART_INDEX_MASK) and it is the existing tree precedent
// (BrnPropEntityModule_Render.cpp:762), so it is used rather than hand-inlining a mask over the
// member -- but the divergence is recorded here so a later sweep does not mistake the extra
// assert for a recovered console tripwire. The DWARF declares the accessor the original source
// used: `bool IsPart() const` (dwarfdump SharedClasses/Physics/Props/BrnPropEntityID.h:74, dumpfile
// line 27 -- the SOURCE line is 74; GetPartIndex is :71), and
// a baked assert string confirms IsPart() == (partIndex != 0): PropZoneManager::GetProp bakes
// "!lEntityId.IsPart()" against BrnPropZoneManager.h:534 (`li r5, 0x216`), and the tree already
// reproduces it as `GetPartIndex() == 0` at BrnPropZoneManager.cpp:423. (⚠️ The round-1 NIT
// cited that as "BrnPropZoneManager.cpp:405"; re-checked this round -- :405 is a comment line,
// the assert is at :423 and the baked file is the .h, not the .cpp.)
// When PropEntityID grows that non-asserting accessor, switch both
// this dispatch and BeginPropWorldContactGeneration's below to it. Filed as a header request.
//
// ---- THE PART ARM (partIndex != 0), r30 == liPartIndex ------------------------------------------
//   0x8261161C..0x82611640  `cmpwi r30,-1 / bne` -> assert :2202, then `b` STRAIGHT INTO the
//        second assert. That fall-through is the compiler's, not a second source statement: -1
//        also fails `>= 0`, so both messages fire for the same value. Two source asserts.
//   0x82611644..0x8261166C  `cmpwi 0 / blt` + `cmpwi 0x1E / blt` -> assert :2205. 0x1E == 30 ==
//        KU_MAX_PHYSICAL_PROP_PARTS, and the assert STRING names that constant verbatim
//        ("liPartIndex >= 0 && liPartIndex < static_cast<int32_t>( KU_MAX_PHYSICAL_PROP_PARTS )",
//        read in full from the image this round).
//   0x8261167C  bl HasPartJustBeenRemoved(this, lPropEntityId, liPartIndex)
//   0x82611680..0x82611694  clrlwi 24 / beq -> `li r3,0` + epilogue. THE ONLY early return.
//   0x82611698..0x8261172C  the inlined CgsContainers::BitArray<30>::IsBitSet bounds assert
//        (its own file: CgsBitArray.h:203, message "invalid index : " << i << " < " << 30).
//        ⚠️ NOT REPRODUCED, deliberately: that tripwire is IsBitSet's body, not this function's
//        source, and the tree's BitArray header states in its own banner (CgsBitArray.h:15-17)
//        that it deliberately carries no assert-system dependency. Its bound (30) is identical
//        to the :2205 assert emitted just above it, so nothing is lost. Named here so the
//        gap between 0x82611698 and 0x82611730 is not read as a missing branch.
//   0x82611730..0x82611750  the bit math itself: `srwi r11,r30,6` (index/64), `addi r11,0x12`
//        (+18 dwords == this+0x90 == mUsedParts), `ldx` a 64-bit field, `sld 1,(index&63)`.
//        The 18 is a CONSOLE dword offset and is NOT reproduced -- mUsedParts is reached by name.
//   0x82611764..0x82611784  assert :2212 "mUsedParts.IsBitSet( liPartIndex )".
//   0x82611788..0x82611790  `lwz 0x8C` (mpaPartInstances) + `slwi r30,6` -> CONSOLE stride 64.
//        Reproduced as mpaPartInstances[liPartIndex]; the host sizeof does the striding.
//   0x82611794..0x826117B4  assert :2217 "lpPart != NULL".
//   0x826117B8..0x826117CC  `addi r3,r28,0x54` -> ResourcePtr<PropPhysicsDataHeader>::operator->
//        (0x822868E0; IDA truncates the symbol to "BrnPhysics::Props::Prop" -- I dumped the body
//        this round and it is the ResourcePtr null tripwire "Can not instance resource pointer",
//        baked CgsResourcePtr.h:544, which pins the identity), then GetType(luTypeId).
//        luTypeId is `lwz 0x34(lpPart)` and luPartId is `lbz 0x38(lpPart)`, both loaded BEFORE
//        the call because the call clobbers r3.
//   0x826117D4..0x826117EC  `lwz 0x40(type)` == PropTypeData::maParts, then `luPartId*48` and
//        `lfs 0x28`. ⚠️ 48 and 0x28 are CONSOLE numbers: the host PropPartTypeData is 64 bytes
//        with mfSphereRadius at +0x30 (BrnPhysicsPropTypeData.h:281-283 static_asserts both).
//        Written as GetParts()[luPartId].GetBoundingRadius() so the host layout drives it.
//   0x826117D8/0x826117F4  `addi r8, r30, 0x2B` -> liOutCacheSlotIndex = 43 + liPartIndex.
//
// ---- THE PROP ARM (partIndex == 0), r30 == liPropIndex ------------------------------------------
//   Same shape with 0xF == 15 == KU_MAX_PHYSICAL_PROPS (asserts :2235 / :2238), the
//   HasPropJustBeenRemoved early-out branching to the SAME `li r3,0` epilogue (0x82611868 ->
//   loc_8261168C), `mulli r11, r30, 0x70` == console stride 112 for PropInstance, assert :2248,
//   `lwz 0x64(lpProp)` == muTypeId, `lfs 0x44(type)` == PropTypeData::mfSphereRadius (host +0x50),
//   and `addi r11, r30, 0x1C` -> liOutCacheSlotIndex = 28 + liPropIndex.
//   ⚠️ MEASURED ASYMMETRY, not an omission: the prop arm has NO mUsedProps bit assert. There is
//   no bit test of any kind between 0x8261186C and 0x8261189C -- the part arm's IsBitSet has no
//   twin here.
//
// ---- THE COMMON TAIL ----------------------------------------------------------------------------
//   0x826118C0  `lwz r11, 0(r24)` -- the slot is RE-READ back through the out-reference rather
//        than reused from the register, which is why the assert below reads liOutCacheSlotIndex.
//   0x826118C4..0x826118EC  `cmpwi 0x1C / blt` + `cmpwi 0x49 / blt` -> assert :2258. 0x49 == 73
//        == 28 + (15 + 30), and the assert string (read in full this round) is literally
//        "liOutCacheSlotIndex >= KI_PROP_CACHE_START_INDEX && liOutCacheSlotIndex <
//         KI_PROP_CACHE_START_INDEX + static_cast<int32_t>( KU_MAX_PHYSICAL_PROPS +
//         KU_MAX_PHYSICAL_PROP_PARTS )" -- which is where the two cache constants above are named.
//   0x826118F0..0x82611910  the radius window. ⚠️ NaN POLARITY (gotcha 4), re-derived:
//        `lfs f13, flt_82001CC0(0.0f) ; fcmpu ; ble -> ASSERT` then
//        `lfs f13, flt_820138DC(50.0f) ; fcmpu ; ble -> SKIP`.
//        `ble` is false on an unordered compare, so a NaN radius falls through BOTH tests and
//        asserts. The C form `!( r > 0.0f && r <= 50.0f )` reproduces that exactly (NaN makes the
//        first conjunct false, so the assert fires), and it also reproduces the boundary cases:
//        r == 0.0f asserts (the console's `ble` takes the equal case), r == 50.0f does not.
//        Do NOT rewrite either half as `>= ` / `< ` -- that flips the NaN behaviour.
//   0x82611914..0x8261198C  the assert message is built with a CgsDev::StrStream, which is why
//        the DWARF names `CgsDev::StrStream lStrStream @2259` as a local of THIS function:
//        "Sphere radius: " << lfOutCacheSphereRadius << "\n", fired at :2259.
//   0x82611990  `li r3, 1` -- the single success return.
//
// ---- SOURCE SHAPE: one deliberate re-ordering, stated ------------------------------------------
// The DecFIGS scope for this function (dwarfdump .../BrnPropManager.cpp:2403) names its locals
// with their DECLARATION lines: liPartIndex @2198, lpPart @2200, luTypeId @2220, luPartId @2221,
// lpPartType @2222 / liPropIndex @2231, lpProp @2233, lpPropType @2251 / lStrStream @2259. Note
// lpPart @2200 and lpProp @2233 are declared AHEAD of their arms' asserts (@2202/@2205 and
// @2235/@2238) while the asm computes both addresses AFTER them -- i.e. the original declared the
// two pointers uninitialised at the top of each block and assigned them later. This body declares
// each at its point of assignment instead. That is a source-shape difference only: the assignment
// is a pure address computation with no side effects, and nothing between the two points reads
// the pointer. Written this way rather than emitting an `= NULL` store the console never makes.
// =================================================================================================
bool PropManager::GetTriangleCacheSlotAndRadius( PropEntityID lPropEntityId,
                                                 s32          liPhysicalPropIndex,
                                                 s32&         liOutCacheSlotIndex,
                                                 f32&         lfOutCacheSphereRadius )
{
    // 0x826115D4 -- see the divergence note in the banner: the console open-codes this mask.
    if ( lPropEntityId.GetPartIndex() != 0u )
    {
        s32 liPartIndex = liPhysicalPropIndex;                                  // DWARF :2198

        // :2202 / :2205 -- both non-gating (the asm falls through into the body either way).
        CGS_ASSERT( liPartIndex != KI_PROP_INDEX_NOT_FOUND,
                    "liPartIndex != KI_PROP_INDEX_NOT_FOUND" );
        CGS_ASSERT( liPartIndex >= 0
                        && liPartIndex < static_cast<s32>( KU_MAX_PHYSICAL_PROP_PARTS ),
                    "liPartIndex >= 0 && liPartIndex < static_cast<int32_t>( KU_MAX_PHYSICAL_PROP_PARTS )" );

        if ( HasPartJustBeenRemoved( lPropEntityId, liPartIndex ) )
        {
            return false;                                                       // 0x8261168C
        }

        // :2212. mUsedParts is the BitArray<30> at console this+0x90; reached by name.
        CGS_ASSERT( mUsedParts.IsBitSet( static_cast<u32>( liPartIndex ) ),
                    "mUsedParts.IsBitSet( liPartIndex )" );

        PropPartInstance* lpPart = &mpaPartInstances[liPartIndex];              // DWARF :2200
        CGS_ASSERT( lpPart != NULL, "lpPart != NULL" );                        // :2217

        const u32 luTypeId = lpPart->GetType();                                 // DWARF :2220 (lwz 0x34)
        const u32 luPartId = lpPart->GetPartId();                               // DWARF :2221 (lbz 0x38)

        // DWARF :2222. The console folds the whole chain -- ResourcePtr::operator->, GetType,
        // maParts, the per-part stride -- into one expression; so does this. Note the DWARF
        // names NO PropTypeData local in this arm, only lpPartType, which is what pins the
        // fold as a single statement rather than two.
        const PropPartTypeData* lpPartType =
            &mpPhysicsData->GetType( luTypeId )->GetParts()[luPartId];

        lfOutCacheSphereRadius = lpPartType->GetBoundingRadius();               // stfs 0(r25)
        liOutCacheSlotIndex    = KI_PROP_PART_CACHE_START_INDEX + liPartIndex;  // addi r30, 0x2B
    }
    else
    {
        s32 liPropIndex = liPhysicalPropIndex;                                  // DWARF :2231

        // :2235 / :2238 -- both non-gating.
        CGS_ASSERT( liPropIndex != KI_PROP_INDEX_NOT_FOUND,
                    "liPropIndex != KI_PROP_INDEX_NOT_FOUND" );
        CGS_ASSERT( liPropIndex >= 0
                        && liPropIndex < static_cast<s32>( KU_MAX_PHYSICAL_PROPS ),
                    "liPropIndex >= 0 && liPropIndex < static_cast<int32_t>( KU_MAX_PHYSICAL_PROPS )" );

        if ( HasPropJustBeenRemoved( lPropEntityId, liPropIndex ) )
        {
            return false;                                                       // shares 0x8261168C
        }

        PropInstance* lpProp = &mpaPropInstances[liPropIndex];                  // DWARF :2233
        CGS_ASSERT( lpProp != NULL, "lpProp != NULL" );                        // :2248

        // DWARF :2251. `lwz 0x64(lpProp)` is muTypeId; the type id is loaded before the
        // operator-> call clobbers r3, which is emission order, not a second statement.
        const PropTypeData* lpPropType = mpPhysicsData->GetType( lpProp->GetTypeId() );

        lfOutCacheSphereRadius = lpPropType->GetBoundingRadius();               // lfs 0x44 -> stfs
        liOutCacheSlotIndex    = KI_PROP_CACHE_START_INDEX + liPropIndex;       // addi r30, 0x1C
    }

    // :2258 -- non-gating. The console re-reads the slot back through the out-reference.
    CGS_ASSERT( liOutCacheSlotIndex >= KI_PROP_CACHE_START_INDEX
                    && liOutCacheSlotIndex < KI_PROP_CACHE_START_INDEX
                           + static_cast<s32>( KU_MAX_PHYSICAL_PROPS
                                               + KU_MAX_PHYSICAL_PROP_PARTS ),
                "liOutCacheSlotIndex >= KI_PROP_CACHE_START_INDEX && liOutCacheSlotIndex < "
                "KI_PROP_CACHE_START_INDEX + static_cast<int32_t>( KU_MAX_PHYSICAL_PROPS + "
                "KU_MAX_PHYSICAL_PROP_PARTS )" );

    // :2259 -- non-gating, and the one assert in this function whose message is BUILT, not baked.
    // See the NaN-polarity note in the banner before touching the condition.
    if ( !( lfOutCacheSphereRadius > 0.0f
            && lfOutCacheSphereRadius <= KF_MAX_PROP_CACHE_SPHERE_RADIUS ) )
    {
        // ⚠️ The console streams into the SHARED global sink CgsDev::Assert::gpcMessageBuffer
        // (`lwz r31, gpcMessageBuffer ; stb 0,0(r31)` then the StrStream vtable off_82000D08).
        // That global is not declared anywhere in this tree, so the message is built on a stack
        // buffer of the same size instead -- the committed idiom, see BrnArbStateCarSelect.cpp:218.
        // Message content, stream order and the fired line are unchanged.
        char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
        CgsDev::StrStream lStrStream( lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE );
        lStrStream << "Sphere radius: " << lfOutCacheSphereRadius << "\n";

        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert( lacMessageBuffer, __FILE__, __LINE__ );  // console: BrnPropManager.cpp:2259
        CgsDev::Assert::EndAssert();
    }

    return true;                                                                // 0x82611990 li r3,1
}

// =================================================================================================
// BrnPhysics::Props::PropManager::BeginPropWorldContactGeneration @ 0x82628CB0  (89 asm insns)
//
// ROUND-2 LANDING of the round-1 parked body
// (scratchpad/waveQ/parked/PropManager_03_BeginPropWorldContactGeneration.cpp). The two
// declarations it was parked on now exist -- BaseCollisionGenerator::
// CreateCollidePrimitiveListWithTriangleListStream (CgsCollisionGenerator.h:240, X360 0x82811DD0,
// exported only as `sub_82811DD0`) and ::RunCollidePrimitiveListWithTriangleListStream
// (:241-242, X360 0x82811F58, IDA symbol truncated to "...RunCollidePrimitiveListWit").
// ⚠️ The round-1 park banner cited them as DWARF CgsCollisionGenerator.h:271/:275 and that is
// CORRECT (the wave spec's :385/:388 were the dumpfile's own line numbers). The round-2 collgen
// owner landed both at those signatures. Their BODIES are still parked (that owner's §4), so
// this file calls two declared-but-unbodied symbols -- reported, `cl /c` cannot see it.
//
// ⛔⛔ THE COMPLETE LINK-HOLE LIST THIS BODY INTRODUCES (AGENTS gotcha 12; RE-GREPPED 2026-08-19,
//     wave Q6 -- and it SHRANK by half):
//       ✅ PropManager::DoPartWorldContactGeneration          @0x82611B70 -- NO LONGER A HOLE.
//         Landed 2026-08-19 in the already-mounted PropManager_wQ2_03.cpp.
//       ✅ PropManager::DoPropInstanceWorldContactGeneration   @0x826120E8 -- NO LONGER A HOLE.
//         Landed 2026-08-19 in the same file.
//       ✅ BaseCollisionGenerator::CreateCollidePrimitiveListWithTriangleListStream
//         (X360 0x82811DD0, 98 insns) and ::RunCollidePrimitiveListWithTriangleListStream
//         (X360 0x82811F58, 80 insns) -- NO LONGER HOLES. Landed 2026-08-19 (wave Q6, cluster
//         pstream) in the already-mounted CgsCollisionGenerator.cpp, together with the family's
//         poster ::AddPrimitiveListWithTriangleListToStream @0x82811D40 (35, whose loud gate in
//         CgsCollisionGenerator_StreamStubs.cpp was deleted in the same change) and the two things
//         that had blocked them for three waves: the descriptor type
//         (JobDescription/CgsPrimitiveListWithTriangleListStreamJobDesc.h) and the enum member
//         E_COLLISIONJOB_PRIMITIVE_LIST_WITH_TRIANGLE_LIST_STREAM = 12. The type-12 worker
//         (ContactGeneratorJob::ExecutePrimitiveListWithTriangleListStream @0x82926650, 100)
//         landed with them, so a command posted from this file is actually drained.
//         ⭐ NOTHING IN THIS LEG IS GATED ANY MORE. This block used to name
//         ContactGeneratorJob::ExecutePrimitiveListWithTriangleList @0x82925908 (849) -- the
//         primitive-vs-triangle narrow phase the stream arm delegates to -- as the runtime
//         residual of the whole leg. It has been REAL since wave Q6 round 3 (2026-08-19), in
//         ContactGeneratorJob.cpp, and the two workers it delegates to landed with it:
//         ::BuildGPInstance @0x829222A0 and ::CollideGPInstances @0x829253C8, in the sibling
//         partfile ContactGeneratorJob_wQ6_01.cpp. Prop commands now produce real contacts.
//     VERIFIED-BODIED and therefore NOT holes (checked, so the list above is exhaustive):
//     BaseCollisionGenerator::Prepare(void*,s32) and ::Finish (CgsCollisionGenerator.cpp:68/:86),
//     SimpleDataStreamProducer::Begin (CgsSimpleDataStreamProducer_Begin.cpp) and ::End
//     (CgsSimpleDataStreamProducer.cpp:134), DataStreamCommandPoster::Begin/End,
//     BaseEventQueue<UpdatePropEvent>::GetEvent, PropEntityID::GetPartIndex.
//
// ---- REGISTER MAP, read off the prologue --------------------------------------------------------
//     r3 = this (r31) - r4 = lpTriangleCacheInterface (r29) - r5 = lpCollisionGenerator (r30)
//     r6 = lpLinearMalloc (r28) - v1 = lvfTimeStep
// ⚠️ gotcha 3's cousin: the VecFloat rides a VMX register and consumes NO GPR slot, so r6 really
// is the FOURTH parameter, not the third. The console saves v1 into v127 at 0x82628CC8 and
// restores it into v1 at 0x82628D64 before every dispatch, which is what makes that visible.
// The parameter ORDER itself is DWARF-attested (dwarfdump BrnPropManager.h), not inferred from
// the asm -- a vector arg skips no GPR, so the asm alone cannot order it.
//
// ---- DECODE -------------------------------------------------------------------------------------
//   0x82628CD0..0x82628CE8  `addis r4,r30,1 ; addi r4,r4,0x2400` (r30 + 0x12400), `lis r5,0x20`
//        (0x200000), `mr r3,r30`, `bl BaseCollisionGenerator::Prepare`.
//        ⚠️ r3 == r30 == the THIRD ARGUMENT, not `this` -- PropManager does not derive from the
//        generator. Those two console literals are exactly CollisionGenerator::
//        mau8CollisionResultsMemory and KI_RESULTS_MEMORY_SIZE, i.e. this is the committed
//        derived no-arg override CollisionGenerator::Prepare() (CgsCollisionGenerator.h:400,
//        `return BaseCollisionGenerator::Prepare(mau8CollisionResultsMemory,
//        KI_RESULTS_MEMORY_SIZE);`) INLINED. Written as that one call so the two console numbers
//        stay comments instead of becoming host offsets (gotcha 1). The bool result is dropped
//        by the console; dropped here too.
//   0x82628CFC  stw r26(0), 0x9C(r31)        miNumJobsAdded = 0
//   0x82628CF0/0x82628D00/0x82628D08  li r4,0x64 (100) -> the Create call -> stw r3, 0xA0(r31)
//   0x82628D04..0x82628D2C  assert :2346 (li r5,0x92A) "mpPrimitiveWithTriangleStream".
//        NON-GATING: the asm falls straight through into the loop.
//   0x82628D34  stw r26, 0x6570(r31)         miNumPropsAddedToContactGen = 0
//   0x82628D30/0x82628DA0  `lwz r11, 0x688(r31)` -- mUpdatedProps.GetLength() is read at the top
//        AND re-read at the bottom of every iteration, so the bound is live, not hoisted.
//   0x82628D4C  bl BrnPhysics::Props::UpdatePro   (IDA-truncated) ==
//        BaseEventQueue<UpdatePropEvent>::GetEvent(i) on this+0x680.
//   0x82628D54  lbz 0x68(event) / bne        skip the event when mbFrozen
//   0x82628D60..0x82628D90  lwz 0x60 (mEntityId), `clrlwi r11,r11,22`, the (x != 0) idiom, beq
//        -> the same bare-mask divergence documented at length on GetTriangleCacheSlotAndRadius
//        above. Same value, same extra tripwire, same open header request.
//   0x82628D94 / 0x82628D9C  DoPartWorldContactGeneration / DoPropInstanceWorldContactGeneration
//        with r3=this, r4=generator, r5=triangle cache, r6=the event, r7=r27 (== this+0x9C,
//        i.e. miNumJobsAdded BY REFERENCE -- `addi r27,r31,0x9C` at 0x82628CF8), r8=malloc,
//        v1=lvfTimeStep.
//   0x82628DB0..0x82628DF4  SimpleDataStreamProducer::Begin() emitted INLINE: six lwz/stw pairs
//        copying producer +0x20..+0x34 down to +0x00..+0x14 (mShared <- the private geometry),
//        `stb 1,0x100` (mbIsStreaming = true), `stw producer+0x80, 0x18` (mShared.mpPoster =
//        &mCommandPoster), then `bl DataStreamCommandPoster::Begin` on producer+0x80. That is
//        instruction-for-instruction the committed CgsSimpleDataStreamProducer_Begin.cpp body,
//        so it is written as that one call.
//   0x82628E00  bl RunCollidePrimitiveListWithTriangleListStream(generator, the producer) -- a
//        the LAST CALL BEFORE THE EPILOGUE (tail POSITION, not a tail call: it is an ordinary `bl`
//        followed by 0x82628E04 `addi r1,r1,0xA0` / 0x82628E08 `li r0,-0x50` / 0x82628E0C
//        `lvx128 v127,r1,r0` / 0x82628E10 `b __restgprlr_25`).
//        ⚠️ It returns EA::Jobs::Job* in r3 and r3 survives that epilogue, but
//        the DWARF/header signature of this function is `void` and there is NO store of the
//        result to any PropManager member anywhere in the 89 instructions. The value is
//        genuinely dropped. Stated, not tidied.
// =================================================================================================
void PropManager::BeginPropWorldContactGeneration(
    const CgsSceneManager::SceneManagerIO::TriangleCacheInterface* lpTriangleCacheInterface,
    CgsSceneManager::CgsCollision::CollisionGenerator*             lpCollisionGenerator,
    CgsMemory::LinearMalloc*                                       lpLinearMalloc,
    VecFloat                                                       lvfTimeStep )
{
    // 0x82628CE8 -- the derived no-arg override feeding its own embedded arena
    // (console: generator + 0x12400, size 0x200000). Result dropped by the console.
    lpCollisionGenerator->Prepare();

    miNumJobsAdded = 0;                                                     // stw 0x9C

    mpPrimitiveWithTriangleStream =
        lpCollisionGenerator->CreateCollidePrimitiveListWithTriangleListStream( 100 );  // li r4,0x64
    CGS_ASSERT( mpPrimitiveWithTriangleStream != NULL,
                "mpPrimitiveWithTriangleStream" );                          // :2346, non-gating

    miNumPropsAddedToContactGen = 0;                                        // stw 0x6570

    // [DIAG] NOT IN THE X360 BINARY -- the two counters below and the one-shot print after the
    // loop. They are plain s32 locals bumped in the two dispatch arms (one add each, no branch,
    // no store outside this frame), so the shipped path is unchanged whether the probe is on or
    // off; only the PRINT is gated. See the block after the loop for what the line answers.
    s32 liDiagProps = 0;
    s32 liDiagParts = 0;

    // The bound is re-read every iteration (lwz 0x688 at both 0x82628D30 and 0x82628DA0).
    for ( s32 liEvent = 0; liEvent < mUpdatedProps.GetLength(); ++liEvent )
    {
        const UpdatePropEvent& lrEvent = mUpdatedProps.GetEvent( liEvent );

        if ( lrEvent.mbFrozen )
        {
            continue;                                                       // lbz 0x68 / bne
        }

        // 0x82628D6C -- bare `clrlwi r11,r11,22`; see the divergence note above.
        if ( lrEvent.mEntityId.GetPartIndex() != 0u )
        {
            ++liDiagParts;                                                  // [DIAG]
            DoPartWorldContactGeneration( lpCollisionGenerator, lpTriangleCacheInterface,
                                          lrEvent, miNumJobsAdded, lpLinearMalloc, lvfTimeStep );
        }
        else
        {
            ++liDiagProps;                                                  // [DIAG]
            DoPropInstanceWorldContactGeneration( lpCollisionGenerator, lpTriangleCacheInterface,
                                                  lrEvent, miNumJobsAdded, lpLinearMalloc,
                                                  lvfTimeStep );
        }
    }

    // =============================================================================================
    // [DIAG] NOT IN THE X360 BINARY -- ONE-SHOT, behind BRN_PROP_DIAG.
    //
    // This is the line that says cluster B is ALIVE: the wave's user-visible complaint is that
    // smashed parts do not visibly move, and today's boot log shows the second half of that story
    // -- six "Warning!! prop fell out of the world" prints, because prop and part rigid bodies are
    // created, integrated, and then FREE-FALL, since this whole function was an inert
    // BRN_CONDUCTOR_GATE and no prop-vs-world contact was ever generated.
    // ⚠️ READ IT AS A PAIR WITH THAT WARNING. `props`/`parts` non-zero here and the fall-out
    // warning still firing used to be explained by the narrow phase being gated. IT IS NOT ANY
    // MORE: ContactGeneratorJob::ExecutePrimitiveListWithTriangleList @0x82925908 (849), the
    // primitive-vs-triangle NARROW PHASE the type-12 stream arm delegates to, has been REAL since
    // wave Q6 round 3 (2026-08-19) in GameShared/Jobs/ContactGenerator/ContactGeneratorJob.cpp,
    // and its own closure landed with it -- ::BuildGPInstance @0x829222A0 and
    // ::CollideGPInstances @0x829253C8, in the sibling partfile ContactGeneratorJob_wQ6_01.cpp.
    // So counts here plus a surviving fall-out warning now means something further down.
    // ✅ NOT the explanation any more (both landed 2026-08-19, wave Q6 round 2):
    // BaseCollisionGenerator::AddPrimitiveListWithTriangleListToStream is a REAL BODY in the
    // mounted CgsCollisionGenerator.cpp, and PrimitivePairListBuilder::AddPrimitive(const
    // rw::collision::Volume*) @0x82814AB8 is a real body in the mounted
    // CgsPrimitivePairListBuilder.cpp (bat:867) -- the pair list it posts is no longer empty.
    // `parts` staying 0 while `props` climbs is scout.md's
    // honest unknown 1 firing: no PART rigid body has ever reached the simulation.
    //
    // The latch is evaluated ONCE -- a getenv per frame would be a syscall on the hot path -- and
    // the print is one-shot, so it costs one predicted branch per frame forever after.
    // =============================================================================================
    {
        static const bool sbPropDiag  = ( getenv( "BRN_PROP_DIAG" ) != 0 );
        static bool       sbFirstPass = true;
        if ( sbPropDiag && sbFirstPass && CgsDev::Log::gpDebugPrint != 0 )
        {
            sbFirstPass = false;
            *CgsDev::Log::gpDebugPrint
                << "[Q6-worldc] first prop-vs-world contact pass: " << liDiagProps
                << " props, " << liDiagParts << " parts\n";
        }
    }

    // 0x82628DB0..0x82628DF4 -- SimpleDataStreamProducer::Begin() inlined by the console.
    mpPrimitiveWithTriangleStream->Begin();

    // 0x82628E00 -- the last call before the epilogue (tail POSITION, an ordinary `bl`); the
    // returned EA::Jobs::Job* is dropped (no store for it exists in the 89 instructions).
    lpCollisionGenerator->RunCollidePrimitiveListWithTriangleListStream(
        mpPrimitiveWithTriangleStream );
}

// =================================================================================================
// BrnPhysics::Props::PropManager::EndPropWorldContactGeneration @ 0x82628E18  (37 asm insns)
//
// ROUND-2 LANDING of the round-1 parked body
// (scratchpad/waveQ/parked/PropManager_03_EndPropWorldContactGeneration.cpp).
// ⚠️ THE ROUND-1 PARK BANNER'S DIAGNOSIS WAS WRONG AND IS CORRECTED HERE, per the round-2
// collgen owner's re-derivation: it said BaseCollisionGenerator::GetNumUsedResultLists() "does
// not exist" and wondered whether the console was reaching a private member. It does exist --
// the DWARF declares it public at CgsCollisionGenerator.h:298; it simply has no out-of-line
// export because it is a one-line header inline that the compiler folded into this one call
// site. The `lhzx` below IS that inlined accessor. So the park's DIAGNOSIS was wrong while its
// requested FIX (land it as a public header inline) was right, and that is what landed:
// CgsCollisionGenerator.h:114, over the already-correctly-typed member. No friendship, no
// offset poke.
//
// ---- REGISTER MAP, read off the prologue --------------------------------------------------------
//     r3 = this (r31) - r4 = lpContactInterface (r27) - r5 = lpCollisionGenerator (r30)
//     r6 = lWorldEntityId (r26)
//
// ---- DECODE, in emission order ------------------------------------------------------------------
//   0x82628E38  bl BaseCollisionGenerator::Finish   r3 == r30, i.e. the GENERATOR, not `this`.
//   0x82628E3C  lwz r29, 0xA0(r31)                  mpPrimitiveWithTriangleStream
//   0x82628E40/0x82628E44  addi r3,r29,0x80 ; bl DataStreamCommandPoster::End
//   0x82628E54  stb r28(0), 0x100(r29)              mbIsStreaming = false
//        ^ those two, in that order, are exactly the committed
//          CgsMemory::SimpleDataStreamProducer::End() (`mCommandPoster.End(); mbIsStreaming =
//          false;`), so the pair is written as that one call.
//   0x82628E58  stw r28, 0xA0(r31)                  mpPrimitiveWithTriangleStream = NULL
//        ^ this happens BEFORE the assert below, not after. Order preserved.
//   0x82628E48..0x82628E5C  `lis r11,1 ; ori r11,r11,0x23BC ; lhzx r11, r30, r11` -- a HALFWORD
//        read at console generator+0x123BC == mu16NumUsedResultLists.
//        ⚠️ 0x123BC IS A CONSOLE OFFSET and must NOT be reproduced (gotcha 1): the host
//        BaseCollisionGenerator is wider (IOBuffer base + 8-byte CollisionResultList* pointers
//        in mapCollisionResultLists[200]), so that byte offset lands somewhere else entirely.
//        Reached through the accessor instead.
//   0x82628E60..0x82628E88  `lwz r10, 0x9C(r31)` (miNumJobsAdded, an s32) compared to that u16
//        under a SIGNED `cmpw`, assert :2416 (li r5,0x970), NON-GATING. Reproduced as an s32
//        comparison with an explicit widening cast so the console's signedness survives rather
//        than the compare silently becoming unsigned.
//   0x82628E90  stw r28, 0x64B4(r31)                miNumDebugWorldContacts = 0
//   0x82628EA0  bl AddContactResultsToQueue         the last call before the epilogue (tail
//        POSITION, not a tail call -- `bl`, then 0x82628EA4 `addi r1,r1,0x90` / 0x82628EA8
//        `b __restgprlr_26`): r3=this, r4=r27, r5=r30, r6=r26
//
// ⚠️ AddContactResultsToQueue @0x82612F08 has NO per-address JSON export and no identity.json
// row (an export hole, not a missing function: two independent headless-IDA reads of the .i64 put
// BrnPhysics::Props::PropManager::AddContactResultsToQueue at 82612F08..826131E8). That EXPORT hole
// is real and is worth keeping written down.
// ✅ IT IS NOT A LINK HOLE. Round-2 correction (the earlier banner said it "is NOT bodied anywhere
// in the tree", which was false the day it was written): the body landed this same wave in the
// sibling part-file b5-decomp/src/GameSource/Physics/PropManager/PropManager_wQ2_03.cpp, as
// `PropManager::AddContactResultsToQueue` -- the only function that file defines. (Cited by name:
// the :348/:454 line pair this banner used to carry is exactly the kind of citation that goes
// stale on the next edit.) Mount wQ2_03 alongside this file; do NOT write
// a second definition or a trap stub for it.
// =================================================================================================
void PropManager::EndPropWorldContactGeneration(
    BrnPhysics::PhysicsModuleIO::PotentialContactInterface* lpContactInterface,
    CgsSceneManager::CgsCollision::CollisionGenerator*      lpCollisionGenerator,
    CgsSceneManager::EntityId                               lWorldEntityId )
{
    lpCollisionGenerator->Finish();                                     // 0x82628E38 (r3 == r30)

    // 0x82628E40..0x82628E54 -- SimpleDataStreamProducer::End() inlined by the console.
    mpPrimitiveWithTriangleStream->End();
    mpPrimitiveWithTriangleStream = NULL;                               // stw 0xA0, BEFORE the assert

    // :2416 -- non-gating. Halfword read, SIGNED compare against the s32 job counter.
    CGS_ASSERT( miNumJobsAdded == static_cast<s32>( lpCollisionGenerator->GetNumUsedResultLists() ),
                "miNumJobsAdded == lpContactGenerator->GetNumUsedResultLists()" );

    miNumDebugWorldContacts = 0;                                        // stw 0x64B4

    // 0x82628EA0 -- the last call before the epilogue (tail POSITION, an ordinary `bl`).
    AddContactResultsToQueue( lpContactInterface, lpCollisionGenerator, lWorldEntityId );
}

}
}

// ============================================================================
// FOLDED FROM PropManager_wQ_03.cpp (wave Q) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================
// =================================================================================================
// GameSource/Physics/PropManager/PropManager_wQ_03.cpp
//
// Partfile of the TU GameSource/Unity/../Physics/PropManager/BrnPropManager.cpp
// (breakable-props keystone wave Q, group 3, 2026-08-18). Folds back into BrnPropManager.cpp.
//
// GROUP 3 WAS THREE FUNCTIONS. ONE LANDS HERE; THE OTHER TWO ARE NO LONGER PARKED --
// ⭐ STALE-BANNER CORRECTION 2026-08-19 (wave Q6): the two "PARKED" entries below were already
// wrong when this file was last read. Both bodies were brought in-tree on 2026-08-18 and live in
// the sibling part-file PropManager_wQ2_02.cpp; the declarations they were parked on
// (BaseCollisionGenerator::{Create,Run}CollidePrimitiveListWithTriangleListStream and
// ::GetNumUsedResultLists) all exist in CgsCollisionGenerator.h.
//
//   * PropManager::UpdateTriangleCache            @0x826119A0  -- BODIED BELOW.
//   * PropManager::BeginPropWorldContactGeneration @0x82628CB0 -- REAL, PropManager_wQ2_02.cpp
//   * PropManager::EndPropWorldContactGeneration   @0x82628E18 -- REAL, PropManager_wQ2_02.cpp
//
// ODR: all three of these functions once had a one-shot conductor-gate twin. All three gates were
//    retired in the same change that mounted the real bodies, so each symbol now has exactly one
//    definition. Run coverage_check against the whole GameSource/Physics directory, not just
//    GameSource/Physics/PropManager, or a cross-directory duplicate would stay invisible.
//
// ⭐⭐ THIS PARTFILE IS LINK-READY AS OF 2026-08-19 (wave Q6, cluster pstream). Its only two
//    non-vendor external callees are InputBuffer_Update::GetInSceneUpdateInterface (real, mounted
//    at build_game_exe.bat:658) and PropManager::GetTriangleCacheSlotAndRadius -- whose only
//    definition lives in the sibling PropManager_wQ2_02.cpp, which is ALSO still unmounted. So the
//    two partfiles MOUNT AS A PAIR: adding this one alone takes an LNK2019. Measured, not
//    reasoned: scratchpad/waveQ6/probe_pstream/obj/PropManager_wQ_03.sym.txt.
//
// ⛔⛔ THE :522-527 GATE'S OWN TEXT IS WRONG, AND IT IS WRONG IN THE HELPFUL DIRECTION.
//    It reads: "props own ZERO triangle-cache slots today (usedSlots==28==8+20)", i.e. "gating
//    this costs nothing". RE-MEASURED 2026-08-19 three independent ways. TWO of the three refute
//    it (1 and 2 below); the THIRD does not -- it was written backwards and is corrected in place
//    rather than deleted, because what it really measures is a live defect (see 3):
//      1. PROPS POST CACHE ADDS. PropManager::ProcessAddPropInstanceEvents builds an
//         InEventAddToCache and appends it to InSceneUpdateInterface::mAddToCacheQueue
//         (PropManager_wQ2_06.cpp:557), and PropManager::CreatePart does the same per part
//         (PropManager_wQ2_04.cpp:357-361). Both TUs are MOUNTED.
//      2. PROPS POST CACHE REMOVES. RemoveProp / RemovePart append InEventRemoveFromCache
//         (PropManager_wQ2_04.cpp:554 and :635). Also mounted.
//      3. THE BOOT LOG PROVES THE REMOVE LEG REACHES THE CACHE MANAGER AT RUNTIME:
//         build/game/BrnGame.log:11541 / :11558 / :11575 / :11592 are four
//         "Trying to remove unused triangle cache slot" asserts from
//         CgsTriangleCacheManager_Events.cpp:201 -- an assert that can only fire for a slot the
//         remove queue names, and callstack line 11545 names
//         TriangleCacheManager::ProcessRemoveFromCacheEvents.
//         ⚠️⚠️ CORRECTED wave Q6 round 1 (worldc #2). This item USED TO END with "A subsystem
//         that owned zero slots could not produce it." THAT WAS LOGICALLY BACKWARDS and is
//         deleted. Read the asserting code: CgsTriangleCacheManager_Events.cpp:169 guards the
//         whole block with `if (!mUsedCacheSlots.IsBitSet(liCacheSlot))`, and :199-201 fire only
//         when the slot is ALSO not added-this-frame and not already-removed. So the assert
//         fires PRECISELY BECAUSE the used-bitmap does NOT hold that slot -- it is evidence FOR
//         the gate text's "zero slots", not against it. What it actually proves is the two
//         things below, and they are worth more than the refutation it was mis-sold as:
//           (a) props REACH the cache manager's remove leg at runtime (the queue plumbing is
//               live end to end), which is all this item is entitled to claim; and
//           (b) 🔴 THE PROP *ADD* LEG IS NOT SEATING SLOTS. The remove arrives for a slot the
//               used-bitmap never took. That is a live lead on this cluster's own goal:
//               DoPart/DoPropInstanceWorldContactGeneration read the same slots back through
//               `lpTriCache->GetCache(slot)`, so they will be handed an EMPTY triangle set --
//               i.e. parts keep falling out of the world -- even after every gate in this
//               cluster is retired. FOLLOW-UP FOR THE CONDUCTOR: prop InEventAddToCache IS
//               queued (PropManager_wQ2_06.cpp:557, PropManager_wQ2_04.cpp:357-361) but the
//               slot is not marked used by the time the remove arrives -- TRACE
//               ProcessAddToCacheEvents before declaring cluster B runtime-complete.
//         Refutations 1 and 2 stand on their own and are enough to justify deleting the stale
//         `usedSlots==28` sentence; this item is NOT part of that justification.
//    So the slot range props own is KI_PROP_CACHE_START_INDEX..+KU_MAX_PHYSICAL_PROPS +
//    KU_MAX_PHYSICAL_PROP_PARTS == 28..72 (the two constants are DWARF-attested WITH their values;
//    GetTriangleCacheSlotAndRadius's own :2258 assert names exactly that window), and gating this
//    function means those 45 slots never follow their prop. That is not free -- it is precisely the
//    input DoProp/DoPartWorldContactGeneration read back through
//    TriangleCacheInterface::GetCache(slot), so a gated UpdateTriangleCache makes prop-vs-world
//    contact generation collide against a STALE OR EMPTY triangle set even once its own gate is
//    retired. ⚠️ The literal number `usedSlots==28` is NOT re-measurable statically (it is a
//    runtime count inside the cache manager); it is not inherited here and must not be re-quoted.
//    RECOMMENDATION TO THE CONDUCTOR: when this gate is retired, DELETE the sentence -- do not
//    reword it and do not carry the number forward.
//
// Grounding: .ida-exports/BURNOUT_X360_ARTIST.XEX/0x826119A0.json (RAW `assembly`, 116 insns);
// the Hex-Rays pseudocode in the same export was used only to confirm the stack-slot map.
// Nothing below is invented; every claim in a comment is either an asm line quoted verbatim or
// is labelled INFERENCE.
// =================================================================================================


namespace BrnPhysics
{
namespace Props
{

// The cache sphere is padded before it is published. MEASURED: the immediate is the rodata float
// flt_82004014, whose bytes are 3D CC CC CD == 0.1f big-endian (read out of the .i64 with headless
// IDA 9.3). NAMED rather than left as a bare literal in round 2 (2026-08-18) to match the two
// sibling producers of the same constant -- BrnDetachedWheelManager.cpp:76 and
// BrnDetachedPartManager.cpp:269 both spell it KF_TRIANGLE_CACHE_PADDING. ⚠️ Each of those is a
// FUNCTION-LOCAL / FILE-LOCAL definition in its own .cpp -- there is no shared header home for
// this constant anywhere in the tree, so it is redeclared here in the same local form rather than
// a new shared header being invented for it. If a real home ever appears, all three collapse.
static const f32 KF_TRIANGLE_CACHE_PADDING = 0.1f;   // flt_82004014, measured

// =================================================================================================
// BrnPhysics::Props::PropManager::UpdateTriangleCache @ 0x826119A0  (116 asm insns)
//
// Arm 2 of PhysicsModule::UpdateCachedPositions @0x8259C370: per live (non-frozen) prop, post one
// InEventUpdateCachedPosition so the scene's triangle cache follows that prop's collision sphere.
//
// ---- MEASURED, line by line (all offsets are CONSOLE offsets quoted in comments only) ----------
//
//   0x826119C4  cmplwi cr6, r24, 0 / bne             the null tripwire on the parameter is
//   0x826119DC  "lpSceneInputBuffer_Update != NULL"  NON-GATING: the asm falls straight through
//   0x826119D8  li r5, 0x8EB  == BrnPropManager.cpp:2283
//
//   0x826119EC  lwz r11, 0x688(r26)                  mUpdatedProps.GetLength() -- and it is
//   0x82611B4C  lwz r11, 0x688(r26)                  RE-READ at the bottom of every iteration,
//                                                    so the bound is live, not hoisted.
//   0x82611A28  bl BrnPhysics::Props::UpdatePro      (IDA-truncated) == BaseEventQueue<
//                                                    UpdatePropEvent>::GetEvent(i) on this+0x680.
//   0x82611A30  lbz r11, 0x68(r31) / bne             skip the event when mbFrozen.
//
//   0x82611A18  lfs f31, flt_82001CC0 (== 0.0f, the value this TU's committed Construct banner
//               already pins) -- hoisted out of the loop and re-stored EVERY iteration at
//   0x82611A50  stfs f31, lfRadius ; 0x82611A58 stw r30(0), liCacheSlot
//               i.e. both out-params are freshly zero-initialised before each query.
//
//   0x82611A5C  bl GetTriangleCacheSlotAndRadius with r3=this, r4=lwz 0x60 (mEntityId),
//               r5=extsh lhz 0x64 (miPhysicsSlot, SIGN-extended), r6=&liCacheSlot, r7=&lfRadius.
//   0x82611A60  clrlwi r3,r3,24 / beq                the bool return gates everything below.
//
//   0x82611A6C  addi r31, r31, 0x30                  &lEvent.mTransform.Pos() -- row 3 of the
//               Matrix44Affine at event+0x00, i.e. wAxis. (event+0x30 == offsetof row 3.)
//   0x82611A74..0x82611AC8  lvx128 + three vspltw(0,1,2) + vcmpeqfp. self-equality lanes ANDed
//               == rw::math::vpu::IsValid(Vector3) exactly (x, y, then z), then
//   0x82611AE0  li r5, 0x8FE == BrnPropManager.cpp:2302, assert
//               "RwMathVPU::IsValid( lEvent.mTransform.Pos() )". Also NON-GATING.
//
//   0x82611AFC  bl CgsSceneManager::SceneManagerIO::InputB (IDA-truncated) ==
//               InputBuffer_Update::GetInSceneUpdateInterface(), r3 = the parameter.
//   0x82611B18  add r3, r3, r29 where r29 = 0xC5290  == &mUpdateCachedPositionQueue.
//   0x82611B48  bl BaseEventQueue<InEventUpdateCachedPosition>::AddEvent.
//
// ---- THE PAYLOAD BUILD, exactly as emitted (frame: sp+0x60 a Vector3Plus temp, sp+0x70 the
//      32-byte event, whose mNewPositionAndRadius sits at sp+0x80) ---------------------------
//   0x82611B00  vspltisw v0, 0                       a zero vector
//   0x82611B1C  vrlimi128 v127, v0, 1, 0             ZERO THE W LANE of the loaded position
//   0x82611B20  stvx128 v0,  sp+0x80                 zero the event's Vector3Plus member
//   0x82611B24  lwz/stw  liCacheSlot -> sp+0x70      lEvent.miCacheSlot = liCacheSlot
//   0x82611B30  stvx128 v127, sp+0x60                temp = position (w == 0)
//   0x82611B08/0x82611B0C/0x82611B14  f0 = lfRadius + flt_82004014
//   0x82611B38  stfs f0, sp+0x6C                     temp.w = that sum  (the W LANE IS THE RADIUS)
//   0x82611B40  lvx128/stvx128 sp+0x60 -> sp+0x80    lEvent.mNewPositionAndRadius = temp
//
//   flt_82004014 == 0.1f. MEASURED, not rounded: Hex-Rays renders this same rodata slot as the
//   literal 0.1 in four other independent exports (0x82708D48, 0x82715A18, 0x828ABF48,
//   0x82623198), and the sibling DetachedWheelManager::UpdateTriangleCache @0x8260E9F8 pads its
//   own cache sphere by the same 0.1. It is a real cache-padding constant, NOT a rounding
//   artefact of the disassembly.
//
//   Two stores in that sequence are DEAD and are therefore not transcribed (each is overwritten
//   before anything can observe it, and no address is published in between): the w-lane zero at
//   0x82611B1C (overwritten by the radius at 0x82611B38) and the whole-vector zero at
//   0x82611B20 (overwritten by the copy at 0x82611B44). They are the register-level way the
//   compiler built a Vector3Plus from a Vector3 plus a scalar. Called out rather than smoothed.
//
// ---- ⚠️ ONE DIVERGENCE FROM THE SOURCE SHAPE, FLAGGED, NOT HIDDEN ------------------------------
//   The DecFIGS DWARF declares the producer this loop called:
//       CgsSceneManagerIO_SceneUpdate.h:478
//         void InSceneUpdateInterface::UpdateCachedObjectPosition(int32_t, Vector3, float32_t);
//       (and a VecFloat-tailed overload at :484)
//   That method DOES NOT EXIST in the tree yet -- the X360 inlined it here, which is why the
//   event build above is open-coded in this body. The queue it appends to is a PUBLIC member of
//   InSceneUpdateInterface (CgsSceneManagerIO_SceneUpdate.h:258 -- the struct has no `private:`
//   at all), so this body reaches it BY NAME, which is legal and is the same by-name discipline
//   CgsTriangleCacheManager_Events.cpp already uses on its two sibling queues.
//   ⚠️ NOTE FOR WHOEVER OWNS CgsSceneManagerIO_SceneUpdate.h: when the DWARF producer lands, the
//   four payload lines below collapse into one call and SHOULD be folded. The spec's claim that
//   "the queue is private" is FALSE as of this wave; the only thing missing is the producer.
//   (I do not own that header, so nothing there was touched.)
//   ⚠️ ROUND 2 (2026-08-18): REQUEST 4 IN scratchpad/waveQ/PropManager.spec.md STAYS OPEN even
//   though this body compiles. It is a SOURCE-SHAPE divergence, not a blocker, and the risk of
//   dropping the request is that a second, independent producer of InEventUpdateCachedPosition
//   then exists forever. When UpdateCachedObjectPosition(s32, Vector3, f32) lands on
//   InSceneUpdateInterface, collapse the four payload lines below to
//       lpSceneUpdateInterface->UpdateCachedObjectPosition(
//           liCacheSlot, lrEvent.mTransform.Pos(), lfRadius + KF_TRIANGLE_CACHE_PADDING);
//   and delete this divergence banner. The DWARF citations were also renumbered in round 2:
//   :478 / :484 are the real SOURCE lines (the round-1 spec cited :506 / :509, which are the
//   dumpFILE's own line numbers).
// =================================================================================================
void PropManager::UpdateTriangleCache(
    CgsSceneManager::SceneManagerIO::InputBuffer_Update* lpSceneInputBuffer_Update)
{
    // BrnPropManager.cpp:2283 -- non-gating tripwire (the asm falls through).
    CGS_ASSERT(lpSceneInputBuffer_Update != NULL, "lpSceneInputBuffer_Update != NULL");

    // The bound is re-read every iteration (lwz 0x688 at both 0x826119EC and 0x82611B4C).
    for (s32 liEvent = 0; liEvent < mUpdatedProps.GetLength(); ++liEvent)
    {
        const UpdatePropEvent& lrEvent = mUpdatedProps.GetEvent(liEvent);

        if (lrEvent.mbFrozen)
        {
            continue;                       // lbz 0x68 / bne
        }

        s32 liCacheSlot = 0;                // stw r30(0)  -> the s32& out-param
        f32 lfRadius    = 0.0f;             // stfs f31     -> the f32& out-param (flt_82001CC0)

        if (!GetTriangleCacheSlotAndRadius(lrEvent.mEntityId, lrEvent.miPhysicsSlot,
                                           liCacheSlot, lfRadius))
        {
            continue;                       // this prop owns no triangle-cache slot
        }

        // BrnPropManager.cpp:2302 -- non-gating tripwire.
        CGS_ASSERT(rw::math::vpu::IsValid(lrEvent.mTransform.Pos()),
                   "RwMathVPU::IsValid( lEvent.mTransform.Pos() )");

        CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpSceneUpdateInterface =
            lpSceneInputBuffer_Update->GetInSceneUpdateInterface();

        // --- the inlined InSceneUpdateInterface::UpdateCachedObjectPosition (see the banner) ---
        CgsSceneManager::TriangleCacheManagerIO::InEventUpdateCachedPosition lEvent;
        lEvent.miCacheSlot = liCacheSlot;
        lEvent.mNewPositionAndRadius.SetVector3(lrEvent.mTransform.Pos());
        lEvent.mNewPositionAndRadius.SetPlus(lfRadius + KF_TRIANGLE_CACHE_PADDING);

        lpSceneUpdateInterface->mUpdateCachedPositionQueue.AddEvent(lEvent);
    }
}

}
}

// ============================================================================
// FOLDED FROM PropManager_wQ2_04.cpp (wave Q2) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================
// GameSource/Physics/PropManager/PropManager_wQ2_04.cpp
//
// BrnPhysics::Props::PropManager -- breakable-props wave Q, ROUND 2 (2026-08-18), lander 04.
// Part-file of the TU GameSource/.../Physics/PropManager/BrnPropManager.cpp; the class banner,
// the member map and the tuning-global caveats live there and are NOT repeated here.
//
//     CreatePart() @ 0x826278D0   (382 instructions, 0x826278D0..0x82627EC4)
//     RemovePart() @ 0x8260F988   (117 instructions, 0x8260F988..0x8260FB58)
//     RemoveProp() @ 0x8260F540   (273 instructions, 0x8260F540..0x8260F980)
//
// All three counts were COUNTED this round from each export's own `assembly` text -- the JSON
// stores it as a single NEWLINE-DELIMITED STRING, not an array, so the cross-check is
// (last - first)/4 + 1 against the NON-BLANK LINE COUNT (round-2 NIT: "the array length" named a
// check that cannot be run as written): 382 / 117 / 273, agreeing both ways. They also agree with
// scratchpad/waveQ2/physfix.owner.md's table, which is a cross-check, not the source.
//
// These are the SPAWN and the two RETIRE legs of the breakable-prop lifetime:
//   * CreatePart turns one shed panel of a smashed prop into a live rigid body -- it claims a
//     part slot, seeds the PropPartInstance, registers the part's triangle-cache slot with the
//     scene, and posts one InAddRigidBody carrying the transform, the two velocities, the
//     inverse inertia (GetPartInertia x KVF_INERTIA_SCALE, reciprocated per axis), the inverse
//     mass and the four KF_PROP_* drag/limit scalars.
//   * RemovePart / RemoveProp are the reverse: evict the triangle-cache slot, free the slot bit,
//     (for a prop) release its joint, and post one InRemoveRigidBody.
//
// Callers, from the image's own xrefs_to, re-measured 2026-08-18 (CreatePart has ONE; RemovePart
// and RemoveProp have TWO each -- the earlier "each function has exactly one" was wrong and
// contradicted its own next line):
//   CreatePart  <- ProcessAddPartInstanceEvents    @0x826280F8  (PropManager_wQ_02.cpp)
//   RemovePart  <- ProcessRemovePartInstanceEvents @0x82627818  (PropManager_wQ_02.cpp)
//                  and ReadUpdatedBodies           @0x82632918  (PropManager_wQ2_01.cpp, the
//                                                   frozen-part arm)
//   RemoveProp  <- ProcessRemovePropInstanceEvents @0x82627778  (an EXPORT HOLE -- no per-address
//                                                   JSON -- and still un-bodied in the tree)
//                  and ReadUpdatedBodies           @0x82632918  (PropManager_wQ2_01.cpp, the
//                                                   frozen-prop arm)
//   ⚠️ So RemoveProp is REACHABLE FROM LANDED CODE TODAY: PropManager_wQ2_01.cpp calls both it
//   and RemovePart. Neither is dead code waiting on the un-bodied Process* leg.
//
// ==================================================================================================
// GOTCHA 1 (console literals are not host values) -- WHERE EACH ONE WENT
// ==================================================================================================
// Every console byte offset below appears in a COMMENT only. Specifically:
//   * `lwz 0x8C(this)` / `slwi idx,6`  == mpaPartInstances + idx*64 (CONSOLE stride) -> written as
//     mpaPartInstances[liPropPartIndex]; the host sizeof does the striding.
//   * `lwz 0x7C(this)` / `mulli idx,0x70` == mpaPropInstances + idx*112 (CONSOLE stride) -> same.
//   * `slwi partIdx,1 ; add ; slwi 4` == partIdx*48, the CONSOLE PropPartTypeData stride
//     (the host record is 64 -- BrnPhysicsPropTypeData.h:_AssertLayout pins both) -> written as
//     lpType->GetParts()[ li16PartIndex ].
//   * `addis r3,scene,0xC ; addi r3,r3,0x4930` == scene + 0xC4930 == &mAddToCacheQueue and
//     `+0xC77E0` == &mRemoveFromCacheQueue -- CONSOLE offsets into a struct whose host layout
//     differs (every embedded EventQueue header is 8 bytes wider on LLP64). Reached BY NAME.
//   * `(idx>>6 + 0x12)*8 + this` == &mUsedParts, `+0x10` == &mUsedProps, `+0xCE` ==
//     &mUsedPropJoints -- CONSOLE dword offsets. Reached BY NAME.
//   * the InAddRigidBody / InRemoveRigidBody / InEventAddToCache / InEventRemoveFromCache stack
//     records are filled BY MEMBER, never by offset; the offset map is quoted in the banners so
//     the field identification is auditable, and every one of those members is already pinned by
//     a committed static_assert (CgsPhysicsSimulationIO_Events.h:279-282 / :102-106,
//     rw/physics/inertia.h:_rw_physics_Inertia_AssertLayout).
// GOTCHA 3 (a float rides an FPR and SKIPS its GPR slot): no float or vector PARAMETER exists on
//   RemovePart/RemoveProp, so there is no site. CreatePart takes two Vector3s; they ride v1/v2 and
//   consume no GPR slot, which is exactly why r8 (liSlotIndex) follows r7 (&lTransform) with the
//   two vectors "in between" in the declaration -- the committed signature already encodes that
//   and the ProcessAddPartInstanceEvents call site matches it positionally.
// GOTCHA 4 (NaN polarity): exactly TWO compare-and-select sites exist, both in CreatePart, both
//   `fcmpu ; blt <skip> ; fmr` == `(a < b) ? a : b`. That is a MIN with the unordered case falling
//   into the `fmr` (so a NaN in the first operand yields the SECOND). Written as the explicit
//   ternary, not as any fpu::Min/Max helper -- see the SetInverseInertia note.
// ==================================================================================================
//
// ✅ NO LINK HOLES LEFT IN THIS FILE -- re-grepped 2026-08-19 (wave Q6 A2). This block used to
//    read "⛔ CALLEES DECLARED BUT NOT BODIED ANYWHERE IN THE TREE" and name two. BOTH have since
//    landed, and both are mounted; the banner had gone stale in the HELPFUL direction, which is
//    exactly the AGENTS.md gotcha-10 failure mode:
//      * BrnPhysics::Props::PropManager::GetPartInertia @0x82612AC8 -- REAL at
//        PropManager_wQ4_03.cpp:520 (with GetPropInertia at :482), mounted at
//        tools/build/build_game_exe.bat:1783. The "parked on the rw::collision::AABBox include
//        clash" claim is history, not state. The stale ⛔ that stood at its call site below has
//        been corrected too.
//      * BrnPhysics::Props::PropPartInstance::Construct -- REAL at
//        PropPhysics/BrnPropPartInstance.cpp:52 (bodied 2026-08-18, wave Q4), mounted at
//        build_game_exe.bat:1792.
//    Everything else this file calls has a real body -- each one is named at its call site.


namespace BrnPhysics
{
namespace Props
{

// (fold: an identical definition of KI_PROP_CACHE_START_INDEX was dropped here -- this TU defines it once, above)
// (fold: an identical definition of KI_PROP_PART_CACHE_START_INDEX was dropped here -- this TU defines it once, above)


// =================================================================================================
// BrnPhysics::Props::PropManager::CreatePart @ 0x826278D0   (382 instructions)
// DWARF: class decl BrnPropManager.h:347, body scope BrnPropManager.cpp:837.
//
// THE PART SPAWN. Its one caller, ProcessAddPartInstanceEvents @0x826280F8, hands it one
// AddPhysicalPartEvent's fields plus two ZERO Vector3s as the initial velocities.
//
// ---- REGISTER MAP, read off the prologue 0x826278F0..0x82627928 --------------------------------
//   r3  = this (r16)          r4 = lEntityId (r17)      r5 = luPropTypeIndex (r19)
//   r6  = li16PartIndex (r15) r7 = &lTransform (r14)
//        ⚠️ WHAT IS MEASURED IS THE POINTER, NOT ITS PROVENANCE (round-2 NIT): the DWARF declares
//        this parameter `const rw::math::vpu::Matrix44Affine&` (dwarfdump BrnPropManager.cpp:874),
//        while the committed header spells it by value -- the tree-wide convention, shared by
//        AddPropToSim (:434) and BreakJoint (:2037). Both spellings put the same pointer in r7 (the
//        by-value form adds a caller-side copy), so the asm cannot tell them apart; stated here so
//        the by-value spelling is not read back as a measurement.
//   r8  = liSlotIndex (r30)   r9 = lpSceneInput (spilled to arg_6C)
//   r10 = lpSimModuleInputBuffer (spilled to arg_74)
//   v1  = lLinearVelocity (v127)   v2 = lAngularVelocity (v126)
//
// ---- THE SOURCE LINES THE ASSERTS BAKE (all BrnPropManager.cpp unless stated) -------------------
//   :851 (0x353)  "lEntityId.IsPart()"
//   :856 (0x358)  "liPropPartIndex != -1"
//   :857 (0x359)  "!mUsedParts.IsBitSet( liPropPartIndex )"
//   :859 (0x35B)  StrStream "Too many physical parts: " << liPropPartIndex << "\n"
//   :874 (0x36A)  "luPropTypeIndex < KU_MAX_PROP_TYPES"
//   :876 (0x36C)  "li16PartIndex < lpType->muNumberOfParts"
//   :880 (0x370)  "luPropTypeIndex < KU_MAX_PROP_TYPES"      <-- the SAME message again
//   :881 (0x371)  "li16PartIndex < lpType->muNumberOfParts"  <-- and the same again
//   BrnPropEntityID.h:278 (0x116) "mEntityId.GetOwner() == E_ENTITYTYPE_PROP"
//   CgsBitArray.h:203 (0xCB) / :222 (0xDE) -- IsBitSet's and SetBit's own bound tripwires.
// The 874/876 pair and the 880/881 pair are FOUR DISTINCT SOURCE LINES with two distinct messages;
// the emission holds both pairs in full. Reproduced as four asserts -- do NOT tidy either pair
// away (same treatment as the twin :735/:737 asserts in PropManager_wQ_02.cpp).
//
// ---- liPropPartIndex IS liSlotIndex -----------------------------------------------------------
// The DWARF names `int32_t liPropPartIndex` at source :839 -- the first line of the body -- and
// r30 (the liSlotIndex parameter) is what every one of the asserts above tests and what indexes
// mUsedParts and mpaPartInstances, with no arithmetic anywhere between the prologue and its first
// use. So :839 is `int32_t liPropPartIndex = liSlotIndex;` (or the parameter under its DWARF name).
//
// ---- THE DWARF LOCALS, AND THE TWO THAT HAVE NO EMISSION --------------------------------------
// The DecFIGS scope names ten locals: liPropPartIndex @839, PropVolumeInstanceID
// lPropVolumeInstanceID @841, RigidBodyId lRigidBodyId @842, InAddRigidBody
// lSimulationAddBodyEvent @844, Vector3 lInverseInertia @845, const PropTypeData* lpType @847,
// PropPartTypeData* lpPartType @848, PropPartInstance* lpPropPart @849, Vector3 lInertia @914,
// CgsDev::StrStream lStrStream @859.
// ⚠️ `lPropVolumeInstanceID` @841 has NO counterpart in the emission and NOTHING IS SYNTHESISED
//    for it. Negative evidence, measured: the 382 instructions contain no 64-bit volume-instance
//    traffic at all -- the only scene-side call in the whole body is the InEventAddToCache append
//    at 0x82627CE4, there is no AddVolumeInstance / SetVolumeInstanceTransform / VolumeInstanceId
//    ::Set among the fifteen `bl` targets (xrefs_from), and no `std` outside the InAddRigidBody
//    record and the assert frames. It is dead in this build. This note exists so a later sweep that
//    re-reads the DWARF does not "restore" a statement the image disproves.
// ⚠️ `lInertia` is declared at :914 -- far below the rest -- which is why the GetPartInertia call
//    and the reciprocal fold sit at the END of the body rather than beside the other inertia
//    fields. The emission agrees (0x82627D88 onwards).
//
// ---- lpType / lpPartType are `const` here ------------------------------------------------------
// The DWARF spells lpPartType `PropPartTypeData*` (non-const). The tree's only route to it,
// PropTypeData::GetParts(), returns `const PropPartTypeData*`, and this body only READS it
// (GetBoundingRadius / GetMass, plus the const GetPartInertia parameter). Kept const rather than
// const_cast-ing to match a DWARF spelling nothing here needs.
// =================================================================================================
void PropManager::CreatePart( PropEntityID                                             lEntityId,
                              u32                                                      luPropTypeIndex,
                              s16                                                      li16PartIndex,
                              Matrix44Affine                                           lTransform,
                              Vector3                                                  lLinearVelocity,
                              Vector3                                                  lAngularVelocity,
                              s32                                                      liSlotIndex,
                              CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpSceneInput,
                              CgsPhysics::PhysicsSimulationIO::InputBuffer*            lpSimModuleInputBuffer )
{
    // :839 -- the DWARF's own name for the slot this part takes (see the banner).
    const s32 liPropPartIndex = liSlotIndex;

    // :851. 0x82627900 `clrlwi r11, r17, 22` == lEntityId & 0x3FF == the part-index field, then
    // the standard subfic/subfe/clrlwi-31 (x != 0) boolean idiom -> `IsPart()`.
    // ⚠️ DIVERGENCE, STATED NOT HIDDEN: the console open-codes that mask with NO `bl` between
    // 0x826278FC and 0x82627928 -- i.e. no owner tripwire. The tree's only accessor for the field,
    // PropEntityID::GetPartIndex(), is out-of-line and opens with AssertIsProp(), so calling it
    // here adds one call + one owner assert the console does not emit at this point. The VALUE is
    // identical and the owner is asserted twelve instructions later anyway (see the RigidBodyId
    // build below), so this is a note, not a bug. The DWARF-declared `bool IsPart() const`
    // (BrnPropEntityID.h:27) is what this line wants; it is not declared in this tree yet.
    CGS_ASSERT( lEntityId.GetPartIndex() != 0u, "lEntityId.IsPart()" );

    // :856. 0x82627964 `cmpwi cr6, r30, -1` -- SIGNED, against the class's own
    // KI_PROP_INDEX_NOT_FOUND == -1.
    CGS_ASSERT( liPropPartIndex != KI_PROP_INDEX_NOT_FOUND, "liPropPartIndex != -1" );

    // :857. mUsedParts is the BitArray<30> at console this+0x90; reached by name.
    // The console emits IsBitSet's OWN bound tripwire inline first (0x826279A8..0x82627A30:
    // StrStream "invalid index : " << liPropPartIndex << " < " << 30, fired at CgsBitArray.h:203).
    // CgsBitArray.h's banner (:15-17) states that tripwire is the CALLER's to emit because the
    // container header carries no assert-system dependency, and BrnPropManager_PropInstanceQueries
    // .cpp:59/:78 is the committed precedent for spelling it as a plain "invalid index" here.
    // Note the console's compare is `cmplwi ... 0x1E` -- UNSIGNED -- which is why a -1 index falls
    // straight through the :856 assert into this one (the emission literally branches from the
    // :856 assert into the bound-assert block at 0x8262799C).
    CGS_ASSERT( static_cast<u32>( liPropPartIndex ) < mUsedParts.GetCapacity(), "invalid index" );
    CGS_ASSERT( !mUsedParts.IsBitSet( static_cast<u32>( liPropPartIndex ) ),
                "!mUsedParts.IsBitSet( liPropPartIndex )" );

    // :858. SetBit's own bound tripwire first (0x82627A94..0x82627B1C: StrStream "Index: " <<
    // liPropPartIndex << ", Number of bits: " << 30, fired at CgsBitArray.h:222 -- a DIFFERENT
    // line from IsBitSet's :203 and from UnSetBit's :241), then the ld/or/std at
    // 0x82627B28..0x82627B30.
    CGS_ASSERT( static_cast<u32>( liPropPartIndex ) < mUsedParts.GetCapacity(), "Index" );
    mUsedParts.SetBit( static_cast<u32>( liPropPartIndex ) );

    // :859. ⚠️ MEASURED ORDER, not a transcription slip: the bit is SET at 0x82627B30 and the
    // ceiling is asserted afterwards (the `cmpwi cr6, r30, 0x1E` at 0x82627B24 is scheduled into
    // the middle of the bit math but its `blt` is at 0x82627B34, after the `stdx`). This compare is
    // SIGNED (`cmpwi`), unlike the container's unsigned one above. The DWARF's `CgsDev::StrStream
    // lStrStream @859` independently confirms the stream lives on this line.
    if ( !( liPropPartIndex < static_cast<s32>( KU_MAX_PHYSICAL_PROP_PARTS ) ) )
    {
        // The console builds the message in CgsDev::Assert::gpcMessageBuffer, a global this tree
        // does not declare; a stack buffer of the same size is the committed substitute
        // (PropManager_wQ2_02.cpp does the same in its cache-slot-range assert;
        // BrnArbStateCarSelect.cpp:218). Content, stream order and
        // the fired line are unchanged. `<< liPropPartIndex` binds the s32 overload -- the console
        // calls the "%d" formatter (0x821F0E50) here, not the "%u" one it uses for the bound
        // messages above.
        char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
        CgsDev::StrStream lStrStream( lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE );
        lStrStream << "Too many physical parts: " << liPropPartIndex << "\n";

        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert( lacMessageBuffer, __FILE__, __LINE__ );  // console: BrnPropManager.cpp:859
        CgsDev::Assert::EndAssert();
    }

    // :849. `lwz r11, 0x8C(r16)` + `slwi r10, r30, 6` == mpaPartInstances + idx*64 (CONSOLE
    // stride); the host sizeof strides the array.
    PropPartInstance* lpPropPart = &mpaPartInstances[ liPropPartIndex ];

    // 0x82627BD4..0x82627BE8 -- SIX stores into the slot, all plain and inline (no `bl`), in the
    // emission's own order:
    //     stb 0x39 = 0            mbUpdated        <- no setter exists on PropPartInstance
    //     stvx +0x10 = {0,0,0,0}  mLinearVelocity  <- SetLinearVelocity is a `bl`; this is not it
    //     stw  0x30 = r17         mEntityId        -> SetEntityId
    //     stvx +0x20 = {0,0,0,0}  mAngularVelocity <- ALSO Construct's store; this body never calls
    //                                                  SetAngularVelocity (there is no such `bl`)
    //     stw  0x34 = r19         muTypeId         -> SetType
    //     stb  0x38 = r15         mu8PartId        -> SetPartId (the low byte of li16PartIndex)
    // and NOTHING is stored to +0x00 (mPos) -- SetPosition writes it two instructions later.
    //
    // ⚠️ INFERENCE, FLAGGED: the three fields with no matching named setter (mbUpdated and the two
    // zeroed velocity lanes) are written here as PropPartInstance::Construct(). What is MEASURED
    // is the three plain stores; what is INFERRED is that they are that DWARF-declared method
    // inlined (the DecFIGS callee list for CreatePart names PropPartInstance::Construct, which is
    // what makes the inference more than a guess). It IS load-bearing for the link:
    // PropPartInstance::Construct (BrnPropPartInstance.h:37) is declaration-only in this tree, so
    // this call is an unresolved external and is reported as such rather than stubbed.
    //
    // ⚠️⚠️ CONTRACT FOR WHOEVER BODIES PropPartInstance::Construct (round-2 MUST_FIX -- the earlier
    // "even a Construct() that zeroed the whole record would be indistinguishable" argument covered
    // only a Construct that zeroes MORE, and the dangerous direction is one that zeroes LESS):
    // at this call site the console zeroes mLinearVelocity (+0x10), mAngularVelocity (+0x20) and
    // mbUpdated (+0x39), and does NOT write mPos (+0x00). NOTHING ELSE IN CreatePart WRITES +0x20 --
    // measured: the only store to it in all 382 instructions is `stvx128 v0, r31, r24` @0x82627BE0
    // with v0 == vspltisw 0 and r24 == 0x20 -- so a Construct() that omits the mAngularVelocity zero
    // leaves stale garbage from the previously recycled part slot where the console guarantees zero.
    // That IS observable. mLinearVelocity is re-written by SetLinearVelocity below, mPos by
    // SetPosition, so those two are the forgiving ones; +0x20 and +0x39 are not.
    lpPropPart->Construct();
    lpPropPart->SetEntityId( lEntityId );
    lpPropPart->SetType( luPropTypeIndex );
    lpPropPart->SetPartId( static_cast<u8>( li16PartIndex ) );

    // 0x82627BEC `lvx128 v1, r0, r25` with r25 == &lTransform + 0x30 == the affine's translation
    // row, then `bl PropPartInstance::SetPosition @0x825DE798`.
    lpPropPart->SetPosition( lTransform.Pos() );
    // 0x82627BF4 `vmr128 v1, v127` then `bl PropPartInstance::SetLinearVelocity @0x825DE860`.
    // ⚠️ MEASURED ASYMMETRY, stated exactly (round-2 MUST_FIX -- the earlier wording said "the
    // ANGULAR velocity is NOT written to the instance", which contradicts the zero store above):
    // the lAngularVelocity PARAMETER is never stored into the instance. mAngularVelocity (+0x20) IS
    // written, with ZERO, by Construct() above; lAngularVelocity itself (v126) survives untouched to
    // the InAddRigidBody record below and nowhere else -- there is no second `bl` and no `stvx` of
    // v126 into lpPropPart anywhere in the body (the only two stvx of v126 are the prologue spill
    // @0x826278E0 and the event record @0x82627D04).
    lpPropPart->SetLinearVelocity( lLinearVelocity );

    // :874. `cmplwi cr6, r19, 0x1F4` == 500 == KU_MAX_PROP_TYPES.
    CGS_ASSERT( luPropTypeIndex < KU_MAX_PROP_TYPES, "luPropTypeIndex < KU_MAX_PROP_TYPES" );

    // :847. 0x82627C28 `addi r3, r16, 0x54` -> CgsResource::ResourcePtr<T>::operator-> (0x822868E0,
    // identified by its baked assert "Can not instance resource pointer..." at CgsResourcePtr.h:544,
    // the NON-const overload's line), then `bl PropPhysicsDataHeader::GetType @0x82277C50` with
    // r4 = luPropTypeIndex. mpPhysicsData is the ResourcePtr at console this+0x54.
    const PropTypeData* lpType = mpPhysicsData->GetType( luPropTypeIndex );

    // :876. `extsh r31, r15` (li16PartIndex SIGN-extended) vs `lbz r11, 0x5D(r29)`
    // (muNumberOfParts, zero-extended), compared with `cmpw` -- a SIGNED compare of the two.
    CGS_ASSERT( static_cast<s32>( li16PartIndex ) < static_cast<s32>( lpType->GetNumberOfParts() ),
                "li16PartIndex < lpType->muNumberOfParts" );

    // :880 / :881. The same two conditions again, on two further source lines. Both are in the
    // emission in full (0x82627C6C..0x82627CAC) and both re-load `lbz 0x5D(r29)`.
    CGS_ASSERT( luPropTypeIndex < KU_MAX_PROP_TYPES, "luPropTypeIndex < KU_MAX_PROP_TYPES" );
    CGS_ASSERT( static_cast<s32>( li16PartIndex ) < static_cast<s32>( lpType->GetNumberOfParts() ),
                "li16PartIndex < lpType->muNumberOfParts" );

    // :848. `lwz r10, 0x40(r29)` == PropTypeData::maParts (console +0x40, host +0x48) and
    // partIdx*48 == the CONSOLE PropPartTypeData stride; indexed by name here.
    const PropPartTypeData* lpPartType = &lpType->GetParts()[ li16PartIndex ];

    // 0x82627CD4..0x82627CE4 -- register this part's triangle-cache slot with the scene.
    // `addi r11, r30, 0x2B` == KI_PROP_PART_CACHE_START_INDEX + liPropPartIndex, and
    // `lfs f0, 0x28(r31)` == PropPartTypeData::mfSphereRadius (console +0x28, host +0x30).
    // ⚠️ MEASURED: the radius is the RAW GetBoundingRadius() -- there is NO KF_TRIANGLE_CACHE_PADDING
    //    add here, unlike the two UpdateCachedObjectPosition producers
    //    (BrnDetachedWheelManager.cpp:260 / BrnDetachedPartManager.cpp:299). No `fadds` and no
    //    rodata float load between 0x82627CD8 and 0x82627CE4.
    // ⚠️ The DWARF names an `AddCachedObject` producer on InSceneUpdateInterface
    //    (CgsSceneManagerIO_SceneUpdate.h:472) that this tree does not declare; the console inlines
    //    it, so the emission is a bare `bl BaseEventQueue<InEventAddToCache>::AddEvent @0x825E4620`
    //    on the queue itself. Appending to the named queue member reproduces that call exactly and
    //    invents no API. The queue's own "Reached Max length" tripwire lives in AddEvent
    //    (CgsBaseEventQueue.h:313); the caller emits no separate producer assert -- measured, there
    //    is no length compare between 0x82627CD0 and the `bl`.
    {
        CgsSceneManager::TriangleCacheManagerIO::InEventAddToCache lAddToCacheEvent;
        lAddToCacheEvent.miCacheSlot         = KI_PROP_PART_CACHE_START_INDEX + liPropPartIndex;
        lAddToCacheEvent.mfCacheSphereRadius = lpPartType->GetBoundingRadius();

        lpSceneInput->mAddToCacheQueue.AddEvent( lAddToCacheEvent );
    }

    // ---- :844, the InAddRigidBody record ---------------------------------------------------------
    // OFFSET MAP, from the stack slots the emission fills (record base == var_180, the pointer
    // handed to AddEvent). Each console offset is matched to the member a committed static_assert
    // already pins, so nothing here is an offset cast:
    //   +0x00 mID                                   `std` 0x82627D84
    //   +0x10..+0x40 mRigidBody.mTransform          four lvx/stvx pairs 0x82627E68..0x82627E9C
    //   +0x50 mRigidBody.mVelocity                  `stvx128 v127` 0x82627CF4
    //   +0x60 mRigidBody.mAngularVelocity           `stvx128 v126` 0x82627D04
    //   +0x70 mRigidBody.mInertia.mInvTens          `stvx128 v0`   0x82627E44
    //   +0x80 mRigidBody.mInertia.mInvMass          `stfs`         0x82627D7C
    //   +0x84 mRigidBody.mInertia.mSpherical        `stfs`         0x82627E70
    //   +0x88 mRigidBody.mInertia.mMaxVelocity      `stfs`         0x82627D28
    //   +0x8C mRigidBody.mInertia.mMaxOmega         `stfs`         0x82627D30
    //   +0x90 mRigidBody.mInertia.mLinearDrag       `stfs`         0x82627D20
    //   +0x94 mRigidBody.mInertia.mAngularDrag      `stfs`         0x82627D18
    //   +0xA0 mRigidBody.mbSpy                      `stb 0`        0x82627D0C
    //   +0xB0 meState                               `stw 4`        0x82627CFC
    // (NewRigidBody sits at InAddRigidBody+0x10 and Inertia at NewRigidBody+0x60, so
    //  Inertia+0x18 == record+0x88 and so on -- CgsPhysicsSimulationIO_Events.h:279-282/:102-106
    //  and rw/physics/inertia.h:_rw_physics_Inertia_AssertLayout pin every one of them.)
    // Nothing writes +0x98/+0x9C (Inertia's tail padding) or the +0xA1..+0xAF gap -- so the record
    // ships with those bytes as stack garbage, exactly as it does on the console.
    CgsPhysics::PhysicsSimulationIO::InAddRigidBody lSimulationAddBodyEvent;

    lSimulationAddBodyEvent.mRigidBody.mVelocity        = lLinearVelocity;
    lSimulationAddBodyEvent.mRigidBody.mAngularVelocity = lAngularVelocity;

    // ⚠️⚠️ MEASURED CROSS-WIRING -- transcribed as it is, NOT "corrected". Two independent facts
    // meet here and both are solid:
    //   (a) the ADDRESS -> NAME map: PropDebugComponent::OnActivate @0x825E3628 hands
    //       flt_82F2A390 to RegisterVariable with the label "Max Angular Velocity" and
    //       flt_82F2A394 with "Max Linear Velocity" (re-read this round at 0x825E391C..0x825E3970),
    //       which is where KF_PROP_MAX_ANGULAR_VEL == 27.0f / KF_PROP_MAX_LINEAR_VEL == 30.0f come
    //       from (BrnPropManager.cpp:184-185).
    //   (b) the OFFSET -> MEMBER map: Inertia+0x18 is mMaxVelocity and +0x1C is mMaxOmega, by DWARF
    //       declaration order and by rw::physics::RigidBody::DynamicUpdate @0x82BC2B78, which
    //       clamps the ANGULAR magnitude first from `lfs 0x1C` (0x82BC2DF8) and the linear one
    //       second from `lfs 0x18` (0x82BC2EA8) -- I re-read both loads this round.
    // The emission puts flt_82F2A390 (the ANGULAR constant) into +0x18 (the LINEAR clamp) and
    // flt_82F2A394 (the LINEAR constant) into +0x1C (the ANGULAR clamp). AddPropToSim @0x826274D8
    // does exactly the same (0x82627598/0x826275A0 into its record's +0x88/+0x8C), so it is not a
    // one-off. Whether the shipped SOURCE crossed the two arguments or one of the two name maps
    // above is mislabelled is NOT decidable from the asm, and this file does not decide it: what is
    // reproduced is the stores. If a later wave settles it, fix it in ONE place with the evidence.
    lSimulationAddBodyEvent.mRigidBody.mInertia.SetMaxLinearVelocity(  KF_PROP_MAX_ANGULAR_VEL ); // -> +0x18
    lSimulationAddBodyEvent.mRigidBody.mInertia.SetMaxAngularVelocity( KF_PROP_MAX_LINEAR_VEL );  // -> +0x1C
    lSimulationAddBodyEvent.mRigidBody.mInertia.SetLinearDrag(  KF_PROP_LINEAR_DRAG );            // -> +0x20
    lSimulationAddBodyEvent.mRigidBody.mInertia.SetAngularDrag( KF_PROP_ANGULAR_DRAG );           // -> +0x24
    // (KF_PROP_RESTITUTION @0x82F2A398 is the fifth of that rodata run and is NOT read anywhere in
    //  these 382 instructions -- stated because the sibling AddPropToSim banner says "the five
    //  KF_PROP_* scalars"; here it is four.)

    lSimulationAddBodyEvent.mRigidBody.mbSpy = false;                       // `stb 0` 0x82627D0C
    // `stw r11(=4)` 0x82627CFC. 4 == rw::physics::ACTIVE_BODY (rigidbody.h:79).
    lSimulationAddBodyEvent.meState = rw::physics::ACTIVE_BODY;

    // :842. `sldi r9, r17, 32 ; clrlwi r11, r30, 16 ; or ; std` == (entityWord << 32) | (idx & 0xFFFF)
    // -- the packed CgsPhysics::RigidBodyId (high dword = the EntityId, low 16 bits = the index;
    // CgsRigidBody.h's banner and GetIndex() both say so).
    // ⚠️ HEADER REQUEST (round-2 NIT; do NOT change the expressions -- they are bit-exact):
    // `CgsPhysics::RigidBodyId::Set(EntityId, u16)` is DWARF-attested as an inlined callee of ALL
    // THREE bodies in this file, but CgsRigidBody.h has SetEntityId and GetIndex and no index
    // setter, so it could not be used. Until it lands, the packing is open-coded three times here
    // (this site plus RemovePart's and RemoveProp's); when it lands, collapse all three onto it --
    // CgsRigidBody.h:58-63 states that convention explicitly ("so the bit arithmetic lives in ONE
    // place each"). The owner tripwire at 0x82627CEC..
    // 0x82627D54 (`srwi r10,r17,24 ; cmplwi 3`, fired at BrnPropEntityID.h:278) is
    // PropEntityID::AssertIsProp() inlined -- reached here by calling GetValue(), whose committed
    // body (BrnPropEntityID.cpp:50) is exactly "AssertIsProp() then return the raw word".
    const CgsPhysics::RigidBodyId lRigidBodyId(
        ( static_cast<u64>( lEntityId.GetValue() ) << 32 )
        | static_cast<u64>( static_cast<u16>( liPropPartIndex ) ) );

    lSimulationAddBodyEvent.mID = lRigidBodyId;
    lSimulationAddBodyEvent.mRigidBody.mTransform = lTransform;

    // 0x82627D5C/0x82627D78 `lfs f0, 0x20(r31)` == PropPartTypeData::mfMass (console +0x20) then
    // `fdivs f0, f31, f0` with f31 == flt_82001C98 == 1.0f (byte-verified in the tree at
    // rw/physics/inertia.h:63).
    lSimulationAddBodyEvent.mRigidBody.mInertia.SetInverseMass( 1.0f / lpPartType->GetMass() );

    // :914 / :845. `bl PropManager::GetPartInertia @0x82612AC8` returns Vector3 through the hidden
    // pointer in r3 (r4 = this, r5 = lpPartType) -- a normal by-value return here.
    // ✅ CORRECTED 2026-08-19 (wave Q6 A2): the "⛔ GetPartInertia HAS NO BODY IN THE TREE ...
    //    Link-red, gate-green" note that stood here is STALE. It is REAL at
    //    PropManager_wQ4_03.cpp:520 and mounted (build_game_exe.bat:1783).
    const Vector3 lInertia = GetPartInertia( lpPartType );

    // 0x82627DA4..0x82627E30. Each axis is splatted, multiplied by the WHOLE KVF_INERTIA_SCALE
    // register (`lvx128 v0, flt_82FB9400` -> vmulfp128 x3), and only LANE 0 of each product is read
    // back (`stvx` then `lfs` at offset 0) before `fdivs f0, f31(1.0f), f0`. So the scalar taken
    // from the VecFloat is its lane 0, and the result is the per-axis reciprocal of the scaled
    // inertia. The vperm/vrlimi128 at 0x82627E28/0x82627E2C is just how the compiler reassembles
    // the three scalars into one register; lane 3 of the assembled vector is a copy of one of the
    // splats and is never read.
    // ⭐ KVF_INERTIA_SCALE == Splat(3.0f), RECOVERED 2026-08-18 round 3b (thunk 0x82C5E6C0,
    //    rodata flt_82004270 = 0x40400000; provenance on the declaration in BrnPropManager.h). The
    //    warning that stood here -- "reads all-zero on disk and has no recovered initialiser ...
    //    every division below is 1.0f/0.0f" -- rested on a claim that was measured false: the disk
    //    bytes are zero, but the initialiser is a dynamic-initialiser thunk outside every IDA
    //    function. The divisions are now by 3x the raw inertia.
    Vector3 lInverseInertia;
    lInverseInertia.x = 1.0f / ( lInertia.x * KVF_INERTIA_SCALE.x );
    lInverseInertia.y = 1.0f / ( lInertia.y * KVF_INERTIA_SCALE.x );
    lInverseInertia.z = 1.0f / ( lInertia.z * KVF_INERTIA_SCALE.x );

    // 0x82627E38..0x82627E70 -- mInvTens := lInverseInertia, then
    // mSpherical := 1.0f / min(x, min(y, z)), which is rw::physics::Inertia::SetInverseInertia
    // (inertia.h:117) exactly. GOTCHA 4: both selects are `fcmpu ; blt <skip> ; fmr`, i.e.
    // `(a < b) ? a : b` with the UNORDERED case taking the `fmr` -- the committed inline spells it
    // with the same ternaries, so the NaN behaviour matches lane for lane.
    // ⚠️ ONE MEASURED DIFFERENCE, harmless: the committed inline stores the intermediate min(x,y)
    //    to +0x14 before the second compare (the console does that at the site inertia.h's banner
    //    quotes); HERE the intermediate store is optimised away and only the final `stfs` at
    //    0x82627E70 lands. Same final value, one fewer dead store. Not a reason to hand-spell the
    //    two lines instead of calling the named setter.
    lSimulationAddBodyEvent.mRigidBody.mInertia.SetInverseInertia( lInverseInertia );

    // [DIAG] NOT IN THE X360 BINARY. Wave-Q6 pre-flight probe (opt in with BRN_PROP_DIAG), FIRST
    // EIGHT parts only. Its twin at PropManager_wQ_02.cpp proves the ADD EVENT reached the drain;
    // this one proves the drain turned it into an InAddRigidBody on the SIM queue. Together they
    // bracket scout.md §4.1 ("has a part rigid body ever reached the simulation?"): if the wQ_02
    // line prints and this one does not, the loss is inside CreatePart's assert chain above; if
    // both print and no part ever moves, the loss is downstream of the solver.
    // meState is printed as the literal ACTIVE_BODY only when it really is -- a state that is not
    // ACTIVE means the part is asleep and will never be integrated, which is the exact failure
    // this line has to be able to show.
    {
        static const bool sbPropDiag  = ( getenv( "BRN_PROP_DIAG" ) != 0 );
        static u32        suDiagCount = 0;
        if ( sbPropDiag && suDiagCount < 8u && CgsDev::Log::gpDebugPrint != 0 )
        {
            ++suDiagCount;
            *CgsDev::Log::gpDebugPrint
                << "[Q6-part] rigid body posted id=" << CgsDev::E_PRINTMODE_HEXONCE
                << static_cast<u64>( lRigidBodyId )
                << " state="
                << ( lSimulationAddBodyEvent.meState == rw::physics::ACTIVE_BODY
                         ? "ACTIVE"
                         : "NOT-ACTIVE" )
                << "\n";
        }
    }

    // 0x82627EA0 `bl CgsPhysics::PhysicsSimulationIO::InputBuffer::GetAddRigidBodyQueue @0x825BCE08`
    // (the write-lock-guarded producer overload -- its "Not locked for writing\n" assert bakes
    // CgsPhysicsSimulationModuleIO.h:1059 and it returns this+0x10), then
    // `bl BaseEventQueue<InAddRigidBody>::AddEvent @0x825A3000`.
    lpSimModuleInputBuffer->GetAddRigidBodyQueue()->AddEvent( lSimulationAddBodyEvent );
}


// =================================================================================================
// BrnPhysics::Props::PropManager::RemovePart @ 0x8260F988   (117 instructions)
// DWARF: class decl BrnPropManager.h:328, body scope BrnPropManager.cpp:803.
//
// The part half of the retire pair: drop the part's triangle-cache slot, free its slot bit, and
// post one InRemoveRigidBody. It does NOT touch the PropPartInstance record at all -- measured:
// there is no `lwz 0x8C(this)` and no store into mpaPartInstances anywhere in the 117 instructions.
//
// ---- REGISTER MAP, prologue 0x8260F994..0x8260F9B0 ---------------------------------------------
//   r3 = this (r26)   r4 = lEntityId (r25)   r5 = luPartIndex (r29)
//   r6 = lpSceneInput (used immediately, not saved)   r7 = lpSimModuleInputBuffer (r24)
//
// ---- THE SOURCE LINES THE ASSERTS BAKE ---------------------------------------------------------
//   CgsBitArray.h:203 (0xCB)   IsBitSet's bound tripwire, StrStream "invalid index : " << i << " < " << 30
//   BrnPropManager.cpp:811 (0x32B)  "mUsedParts.IsBitSet( luPartIndex )"
//   CgsBitArray.h:241 (0xF1)   UnSetBit's bound tripwire, "luIndex < NUMBITS"
//   BrnPropEntityID.h:278 (0x116)   "mEntityId.GetOwner() == E_ENTITYTYPE_PROP"
// Both container tripwires compare UNSIGNED against 0x1E == 30 == KU_MAX_PHYSICAL_PROP_PARTS,
// which is also mUsedParts' capacity.
//
// ---- THE ONLY DWARF LOCAL ----------------------------------------------------------------------
// The DecFIGS scope names exactly one: `InRemoveRigidBody lSimulationRemoveBodyEvent @815`. There
// is deliberately NO named RigidBodyId local here (unlike RemoveProp, whose scope does name one) --
// the emission composes the handle straight into the record's leading qword at 0x8260FB30..0x8260FB44.
// =================================================================================================
void PropManager::RemovePart( PropEntityID                                             lEntityId,
                              u32                                                      luPartIndex,
                              CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpSceneInput,
                              CgsPhysics::PhysicsSimulationIO::InputBuffer*            lpSimModuleInputBuffer )
{
    // 0x8260F99C..0x8260F9B8 -- FIRST thing the body does, before any assert.
    // `addi r11, r29, 0x2B` == KI_PROP_PART_CACHE_START_INDEX + luPartIndex, written into the
    // 4-byte stack record, then `bl BaseEventQueue<InEventRemoveFromCache>::AddEvent @0x825E48C0`
    // on the queue at scene + 0xC77E0 (== &mRemoveFromCacheQueue -- reached by name; see gotcha 1).
    // Same inlined-producer situation as CreatePart's add: the DWARF's RemoveCachedObject
    // (CgsSceneManagerIO_SceneUpdate.h:488) is not declared in this tree and the console emits the
    // queue append directly. No caller-side length tripwire (measured: no compare, no other `bl`).
    {
        CgsSceneManager::TriangleCacheManagerIO::InEventRemoveFromCache lRemoveFromCacheEvent;
        lRemoveFromCacheEvent.miCacheSlot =
            KI_PROP_PART_CACHE_START_INDEX + static_cast<s32>( luPartIndex );

        lpSceneInput->mRemoveFromCacheQueue.AddEvent( lRemoveFromCacheEvent );
    }

    // IsBitSet's own bound tripwire (CgsBitArray.h:203) -- see the CreatePart note for why it is
    // spelled here rather than in the container header.
    CGS_ASSERT( luPartIndex < mUsedParts.GetCapacity(), "invalid index" );
    // :811. 0x8260FA74..0x8260FAC8.
    CGS_ASSERT( mUsedParts.IsBitSet( luPartIndex ), "mUsedParts.IsBitSet( luPartIndex )" );

    // UnSetBit's bound tripwire (CgsBitArray.h:241), then the ld/andc/std at
    // 0x8260FAF4..0x8260FB08. The message is the binary's own and matches the committed spelling in
    // BrnPhysicalBodyPartPool_Remove.cpp:58.
    CGS_ASSERT( luPartIndex < mUsedParts.GetCapacity(), "luIndex < NUMBITS" );
    mUsedParts.UnSetBit( luPartIndex );

    // :815. `sldi r10, r25, 32 ; clrlwi r11, r29, 16 ; or ; std` -- same packing as CreatePart, and
    // the owner tripwire at 0x8260FAF8..0x8260FB2C is again AssertIsProp() inlined, reached through
    // GetValue().
    CgsPhysics::PhysicsSimulationIO::InRemoveRigidBody lSimulationRemoveBodyEvent;
    lSimulationRemoveBodyEvent.mID =
        ( static_cast<u64>( lEntityId.GetValue() ) << 32 )
        | static_cast<u64>( static_cast<u16>( luPartIndex ) );
    // ⚠️ The console's 16-byte stack record only ever writes the leading 8 bytes -- the
    // mbFailIfRigidBodyNotFound byte at +8 is UNINITIALISED stack on the X360 (measured: the only
    // store into var_60 is the `std` at 0x8260FB44). The host sets it FALSE == the tolerant remove,
    // which is both what ProcessRemoveRigidBodyQueue's not-found-tolerant arm does with a clear byte
    // and the committed treatment of the identical console quirk in
    // BrnPhysicalBodyPartPool_Remove.cpp:51.
    lSimulationRemoveBodyEvent.mbFailIfRigidBodyNotFound = false;

    // 0x8260FB48 `bl sub_825BCF58` == InputBuffer::GetRemoveRigidBodyQueue() (the write-lock
    // producer overload; its "Not locked for writing\n" assert bakes
    // CgsPhysicsSimulationModuleIO.h:1080 and it returns this + 0x196A0), then
    // `bl BaseEventQueue<InRemoveRigidBody>::AddEvent`.
    lpSimModuleInputBuffer->GetRemoveRigidBodyQueue()->AddEvent( lSimulationRemoveBodyEvent );
}


// =================================================================================================
// BrnPhysics::Props::PropManager::RemoveProp @ 0x8260F540   (273 instructions)
// DWARF: class decl BrnPropManager.h:320, body scope BrnPropManager.cpp:760.
//
// The whole-prop retire: the same cache-evict / bit-free / InRemoveRigidBody spine as RemovePart,
// plus two things a part does not have -- an "added this frame" diagnostic and the release of the
// prop's JOINT slot (the smash-gate / signpost hinge).
//
// ---- REGISTER MAP, prologue 0x8260F54C..0x8260F568 ---------------------------------------------
//   r3 = this (r16)   r4 = lEntityId (r25)   r5 = luPropIndex (r31)
//   r6 = lpSceneInput (used immediately)     r7 = lpSimModuleInputBuffer (spilled to arg_34)
//
// ---- THE SOURCE LINES THE ASSERTS BAKE ---------------------------------------------------------
//   CgsBitArray.h:203 (0xCB)        IsBitSet bound, "invalid index : " << i << " < " << 15  (twice:
//                                   once on luPropIndex, once on the joint index)
//   BrnPropManager.cpp:767 (0x2FF)  "mUsedProps.IsBitSet( luPropIndex )"
//   CgsBitArray.h:241 (0xF1)        UnSetBit bound, "luIndex < NUMBITS"                     (twice)
//   BrnPropEntityID.h:278 (0x116)   "mEntityId.GetOwner() == E_ENTITYTYPE_PROP"             (twice)
//   BrnPropManager.cpp:778 (0x30A)  the StrStream "Trying to remove prop added this frame: " message
//   BrnPropManager.cpp:783 (0x30F)  "mUsedPropJoints.IsBitSet( lpProp->GetJointIndex() )"
//   BrnPropInstance.h:326 (0x146)   "IsJointed()"  -- GetJointIndex's own tripwire, inlined
//
// ---- THE DWARF LOCALS --------------------------------------------------------------------------
//   InRemoveRigidBody lSimulationRemoveBodyEvent @771 - RigidBodyId lPropRigidBodyId @772 -
//   PropInstance* lpProp @776 - CgsDev::StrStream lStrStream @778.
// The declaration lines settle one ordering the emission scrambles: lPropRigidBodyId (:772) is
// declared BEFORE lpProp (:776), which is why the owner tripwire fires at 0x8260F6E8 -- ahead of
// the `lwz 0x7C(this)` that computes lpProp at 0x8260F704 -- even though the `or`/`std` that packs
// the handle is scheduled after it. Source order below follows the DWARF; the two statements have
// no interdependency, so this is a scheduling difference only.
// =================================================================================================
void PropManager::RemoveProp( PropEntityID                                             lEntityId,
                              u32                                                      luPropIndex,
                              CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpSceneInput,
                              CgsPhysics::PhysicsSimulationIO::InputBuffer*            lpSimModuleInputBuffer )
{
    // 0x8260F558..0x8260F570 -- first, exactly as in RemovePart but with the PROP slot base.
    // `addi r11, r31, 0x1C` == KI_PROP_CACHE_START_INDEX + luPropIndex.
    {
        CgsSceneManager::TriangleCacheManagerIO::InEventRemoveFromCache lRemoveFromCacheEvent;
        lRemoveFromCacheEvent.miCacheSlot =
            KI_PROP_CACHE_START_INDEX + static_cast<s32>( luPropIndex );

        lpSceneInput->mRemoveFromCacheQueue.AddEvent( lRemoveFromCacheEvent );
    }

    // IsBitSet's bound tripwire (CgsBitArray.h:203, bound 0xF == 15 == mUsedProps' capacity).
    CGS_ASSERT( luPropIndex < mUsedProps.GetCapacity(), "invalid index" );
    // :767. 0x8260F634..0x8260F690.
    CGS_ASSERT( mUsedProps.IsBitSet( luPropIndex ), "mUsedProps.IsBitSet( luPropIndex )" );

    // UnSetBit's bound tripwire (CgsBitArray.h:241), then ld/andc/std at 0x8260F6C0..0x8260F6D4.
    CGS_ASSERT( luPropIndex < mUsedProps.GetCapacity(), "luIndex < NUMBITS" );
    mUsedProps.UnSetBit( luPropIndex );

    // :771 / :772. Same packing and same inlined AssertIsProp() (fired at 0x8260F6EC) as the two
    // functions above.
    CgsPhysics::PhysicsSimulationIO::InRemoveRigidBody lSimulationRemoveBodyEvent;
    const CgsPhysics::RigidBodyId lPropRigidBodyId(
        ( static_cast<u64>( lEntityId.GetValue() ) << 32 )
        | static_cast<u64>( static_cast<u16>( luPropIndex ) ) );

    lSimulationRemoveBodyEvent.mID = lPropRigidBodyId;                 // `std` 0x8260F71C
    // Same console quirk / same committed treatment as in RemovePart above: the byte at +8 is never
    // written by the console (the only store into var_B0 is that one `std`).
    lSimulationRemoveBodyEvent.mbFailIfRigidBodyNotFound = false;

    // :776. `lwz r10, 0x7C(r16)` + `mulli r11, r31, 0x70` == mpaPropInstances + idx*112 (CONSOLE
    // stride); indexed by name here.
    PropInstance* lpProp = &mpaPropInstances[ luPropIndex ];

    // :778. `lbz r11, 0x6F(r27) ; clrlwi r11,r11,31` == mu8Flags & KU_ADDED_THIS_FRAME_FLAG ==
    // WasAddedThisFrame(). A DIAGNOSTIC ONLY -- the console does not skip anything on this path;
    // it falls straight through into the joint block whether or not the assert fires.
    if ( lpProp->WasAddedThisFrame() )
    {
        // Stream order, read off 0x8260F780..0x8260F7F0:
        //   << "Trying to remove prop added this frame: "   (the virtual char* sink)
        //   `stw r11(=2), stream+4`  == mePrintMode = E_PRINTMODE_HEXONCE, i.e. the inlined
        //        StrStreamBase::operator<<(PrintMode). Emitted BETWEEN the string and the id.
        //   << lpProp->GetEntityId().GetValue()            (`bl 0x821F0EC8`, the "%u"/"0x%X"
        //        u32 formatter; the owner tripwire at 0x8260F75C..0x8260F77C is GetValue's own
        //        AssertIsProp)
        //   << " Position: "
        //   << lpProp->GetTransform().Pos()                (`lvx128 v1, r27, 0x30` == the affine's
        //        translation row, then `bl 0x82203F70`)
        //   << "\n"
        //   FireAssert(gpcMessageBuffer, BrnPropManager.cpp, 778)
        //
        // ⚠️ ONE UNAVOIDABLE SPELLING CHANGE, FLAGGED. 0x82203F70 is
        //    `operator<<(StrStreamBase&, Vector3)` -- format string "(%f, %f, %f)", read out of
        //    off_82F31964 in its own body this round, and independently identified under that name
        //    by a committed file (BrnBehaviourGameplayExternal.h:375). That free operator has NO
        //    home anywhere in this tree (grepped src and vendor), and this lander may not add one.
        //    So the three lanes are pushed through StrStreamBase::AppendFormat with the console's
        //    OWN format string, which is exactly what that callee does internally -- byte-identical
        //    output, one call frame fewer. Replace this line with `<< lpProp->GetTransform().Pos()`
        //    the moment the operator is homed (reported as a header request).
        char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
        CgsDev::StrStream lStrStream( lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE );

        lStrStream << "Trying to remove prop added this frame: ";
        lStrStream << CgsDev::E_PRINTMODE_HEXONCE;
        lStrStream << lpProp->GetEntityId().GetValue();
        lStrStream << " Position: ";
        lStrStream.AppendFormat( "(%f, %f, %f)",
                                 lpProp->GetTransform().Pos().x,
                                 lpProp->GetTransform().Pos().y,
                                 lpProp->GetTransform().Pos().z );
        lStrStream << "\n";

        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert( lacMessageBuffer, __FILE__, __LINE__ );  // console: BrnPropManager.cpp:778
        CgsDev::Assert::EndAssert();
    }

    // 0x8260F808 `lbz r28, 0x6C(r27) ; cmplwi 0xFF ; beq -> the tail` == mu8JointIndex !=
    // KU_NOT_JOINTED == IsJointed(). Everything below runs only for a jointed prop.
    if ( lpProp->IsJointed() )
    {
        // mUsedPropJoints is the BitArray<15> at console this+0x670 (the emission reaches it as
        // `(idx>>6 + 0xCE)*8 + this` at 0x8260F8A8 and as `addi r30, r16, 0x670` at 0x8260F924 --
        // 0xCE*8 == 0x670, an arithmetic self-check on the member map). Reached by name.
        //
        // ⚠️ MEASURED RE-READ, transcribed: the console re-loads `lbz 0x6C(r27)` THREE separate
        //    times in this block (0x8260F808 for the IsJointed test, 0x8260F8F4 for GetJointIndex's
        //    own IsJointed tripwire, 0x8260F920 for the value it un-sets with), and it emits the
        //    BrnPropInstance.h:326 "IsJointed()" tripwire exactly ONCE (0x8260F900). GetJointIndex()
        //    is called FOUR times below (round-2 NIT: the earlier comment said "twice" and the code
        //    beneath it always had four). Two of the four are this codebase's own CALLER-SIDE
        //    CgsBitArray bound tripwires (CgsBitArray.h:203/:241), which the console open-codes from
        //    the already-loaded value instead of re-calling; the DWARF attests one inlined
        //    PropInstance::GetJointIndex in this body. So the reconstruction can emit that assert up
        //    to four times where the console emits it once -- all provably unreachable (const getter,
        //    the whole block is guarded by IsJointed()), which is why the spelling stands.
        //    GetJointIndex()'s committed body (BrnPropInstance.cpp:66) carries that same assert.
        CGS_ASSERT( static_cast<u32>( lpProp->GetJointIndex() ) < mUsedPropJoints.GetCapacity(),
                    "invalid index" );                                    // CgsBitArray.h:203
        // :783.
        CGS_ASSERT( mUsedPropJoints.IsBitSet( static_cast<u32>( lpProp->GetJointIndex() ) ),
                    "mUsedPropJoints.IsBitSet( lpProp->GetJointIndex() )" );

        CGS_ASSERT( static_cast<u32>( lpProp->GetJointIndex() ) < mUsedPropJoints.GetCapacity(),
                    "luIndex < NUMBITS" );                                // CgsBitArray.h:241
        // 0x8260F948..0x8260F960 -- `rlwinm r11, r31, 29,3,28` is (idx>>6)*8, the field index.
        mUsedPropJoints.UnSetBit( static_cast<u32>( lpProp->GetJointIndex() ) );

        // 0x8260F964 `stb r9(=0xFF), 0x6C(r27)` == mu8JointIndex = KU_NOT_JOINTED.
        lpProp->SetNotJointed();
    }

    // 0x8260F968..0x8260F978 -- same producer pair as RemovePart.
    lpSimModuleInputBuffer->GetRemoveRigidBodyQueue()->AddEvent( lSimulationRemoveBodyEvent );
}

}
}

// ============================================================================
// FOLDED FROM PropManager_wQ2_05.cpp (wave Q2) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================
// GameSource/Physics/PropManager/PropManager_wQ2_05.cpp
//
// BrnPhysics::Props::PropManager -- breakable-props wave Q, ROUND 2 (2026-08-18), lander 05.
// Part-file of the TU GameSource/.../Physics/PropManager/BrnPropManager.cpp; the class banner,
// the member map and the tuning-global caveats live there and are NOT repeated here.
//
//     AddPropToSim()       @ 0x826274D8   (167 instructions, 0x826274D8..0x82627770)
//     BreakJoint()         @ 0x82628A88   (137 instructions, 0x82628A88..0x82628CA8)
//     UpdateJointedProps() @ 0x82631260   (938 instructions, 0x82631260..0x82632104)
//
// All three counts were COUNTED here from each export's own `assembly` array,
// (last - first)/4 + 1:  (0x82627770-0x826274D8)/4+1 == 167,
// (0x82628CA8-0x82628A88)/4+1 == 137, (0x82632104-0x82631260)/4+1 == 938. They agree with
// scratchpad/waveQ2/physfix.owner.md's table, which is a cross-check, not the source.
//
// THESE THREE ARE THE JOINTED-PROP (lean / tilt / smash-gate) SPINE, and they call each other:
//   UpdateJointedProps  is the per-frame driver. Pass 1 walks mUsedPropJoints and, for every
//                       joint whose bit is also set in mBreakPropJoints, calls BreakJoint.
//                       Pass 2 walks mUsedPropJoints again and lerps maCurrentJointTransforms
//                       toward each jointed prop's live transform, re-queuing one
//                       UpdatePropEvent per prop that actually moved.
//   BreakJoint          cuts one joint: remove the kinematic body, free the joint slot, hand
//                       the freed prop a spin + the tangential velocity that spin implies at
//                       the prop's centre, and re-add it as a free dynamic body.
//   AddPropToSim        builds the InAddRigidBody for a whole prop (inertia, mass, drag and
//                       velocity limits, optional centre-of-mass shift) and posts it.
//
// Callers, from the image's own xrefs_to:
//   AddPropToSim        <- ProcessAddPropInstanceEvents @0x82632108 (PropManager_wQ_02.cpp)
//                          and BreakJoint @0x82628A88 (below)
//   BreakJoint          <- UpdateJointedProps @0x82631260 (below)
//   UpdateJointedProps  <- ProcessInputsPreScene @0x8263AF30 (the sole caller, from xrefs_to;
//                          bodied at PropManager_wQ2_08.cpp -- that file is a concurrent lander's
//                          and its line numbers move, so only the address is cited)
//
// ==================================================================================================
// GOTCHA 1 (console literals are not host values) -- WHERE EACH ONE WENT
// ==================================================================================================
// Every console byte offset below appears in a COMMENT only:
//   * `lwz 0x7C(this)` / `mulli idx,0x70` == mpaPropInstances + idx*112 (CONSOLE stride) -> written
//     as mpaPropInstances[liPropIndex]; the host sizeof does the striding.
//   * `(idx>>6 + 0xCF)*8 + this` == &mBreakPropJoints, `+0x10` == &mUsedProps,
//     `addi r,this,0x670` == &mUsedPropJoints, `addi r,this,0x2A0` == &mauPropIndexForJoint,
//     `addi r,this,0x2B0 + idx*64` == &maCurrentJointTransforms[idx],
//     `(idx+12)*16` == &maPropJointPositions[idx] (0xC0 + idx*16),
//     `(idx+27)*16` == &maLastJointRotation[idx]  (0x1B0 + idx*16),
//     `addi r3,this,0x5E10` == &mUpdatedJointedProps -- all CONSOLE offsets, all reached BY NAME.
//   * `addi r3,this,0x54` == &mpPhysicsData, taken into ResourcePtr<T>::operator-> -- by name.
//   * `(typeId+4)*4 + header` == mapPropTypes[typeId] (the +0x10 array base) -- reached through
//     the committed PropPhysicsDataHeader::GetType, which carries both of its bounds asserts.
//   * the InAddRigidBody / InRemoveRigidBody / UpdatePropEvent stack records are filled BY MEMBER,
//     never by offset; the offset maps are quoted in the banners so the field identification is
//     auditable, and every one of those members is already pinned by a committed static_assert
//     (CgsPhysicsSimulationIO_Events.h:280-283 and its NewRigidBody static_assert block,
//     rw/physics/inertia.h::_rw_physics_Inertia_AssertLayout, BrnPropInstance.h's offsetof block).
// GOTCHA 3 (a float rides an FPR and SKIPS its GPR slot): AddPropToSim takes no f32 parameter, but
//   it DOES take two Vector3s, which ride v1/v2 and consume no GPR slot -- which is exactly why its
//   r10 is lpSimModuleInputBuffer (the 7th declared parameter) and the two vectors declared after
//   it are in vector registers. The committed signature already encodes that, and BreakJoint's
//   call site at 0x82628C80..0x82628CA0 matches it positionally (r4..r10 + v1/v2).
// GOTCHA 4 (NaN polarity): two compare-and-select sites exist and both are inside
//   rw::physics::Inertia::SetInverseInertia's inlined min (AddPropToSim 0x826276A4..0x826276C0),
//   spelled `fcmpu ; blt <skip> ; fmr` == `(a < b) ? a : b` -- a MIN whose UNORDERED case takes the
//   `fmr` and yields the SECOND operand. Reached by calling the committed named setter, whose
//   inline uses exactly those ternaries, so the NaN behaviour matches.
//   ⚠️ THE TWO `vcmpgtfp` TESTS IN UpdateJointedProps DO **NOT** MATCH LANE-FOR-LANE, and saying
//   they did would be the wrong-comment defect gotcha 9 exists to stop. Both are the SDK's
//   `IsZero(v, tolerance)` inlined, and the console lowers it as
//       !( any lane of |v| > tolerance )        -- `vcmpgtfp` + the CR6 "none compared true" bit
//   whereas the committed host inline (vector3_operation.h:269) spells it
//       all lanes of |v| <= tolerance
//   The two agree on every ORDERED value and differ ONLY when a lane is NaN: the console then says
//   "is zero" (NaN > tol is false) and the host says "is not zero" (fabs(NaN) <= tol is false).
//   The bodies below call the committed IsZero, because IsZero is what the DWARF callee list
//   attests the source called and because forking a second copy of a vendor predicate is worse
//   than a documented NaN edge. Consequence, stated: a jointed prop whose transform has gone NaN
//   is SKIPPED by the console and LERPED (then caught by the :392 assert) here. Filed as a
//   vendor-home note, not silently absorbed.
// GOTCHA 9 (a wrong comment is a real defect): every instruction address quoted below was read off
//   the per-address export this round; the four rodata reads that mattered were BYTE-DUMPED with
//   headless IDA 9.3 rather than inferred (scratchpad/waveQ2/probe_wQ2_05/ida_basis.txt,
//   ida_fmt.txt). Where something is an inference it says INFERENCE on its own line.
//
// ==================================================================================================
// ⛔ CALLEES / GLOBALS THAT ARE DECLARED BUT HAVE NO DEFINITION IN THE TREE
//    (`cl /c` cannot see an unresolved external, so this file gating green does NOT mean the TU
//     links). Reported, not stubbed, not invented:
//      BrnPhysics::Props::PropManager::GetPropInertia @0x82612640 (declared in BrnPropManager.h's GetPropInertia/GetPartInertia block; its
//          round-1 body is parked at scratchpad/waveQ/parked/
//          PropManager_04_GetPropInertia_GetPartInertia.cpp, blocked on the rw::collision::AABBox
//          include clash -- physfix.owner.md §5.1 REQUEST 1b)
//      the FIVE tuning globals declared at the top of this file (see their banner) -- they need a
//          declaration in BrnPropManager.h and a definition in BrnPropManager.cpp, exactly like
//          their fifteen already-homed siblings.
//    Everything else these three bodies call has a real body -- each one is named at its call site.
// ==================================================================================================


namespace BrnPhysics
{
namespace Props
{

// =================================================================================================
// ⚠️⚠️ FIVE TUNING GLOBALS THAT HAVE NO DECLARATION ANYWHERE IN THE TREE YET -- DECLARED HERE,
// DEFINED NOWHERE, AND FILED AS A HEADER REQUEST. THIS FILE IS LINK-RED UNTIL THEY LAND.
//
// BrnPropManager.h already carries fifteen of this .cpp's tuning globals (the seven the debug
// component registers plus the eight the keystone wave added). These five are the ones ONLY these
// three bodies read, so nothing had reason to declare them before. This lander may not edit a
// header, so they are DECLARED here and reported; nothing is defined, so there is no chance of an
// ODR clash when BrnPropManager.h/.cpp grow them (a second identical `extern` declaration is legal
// and inert -- a second DEFINITION would not be, which is why there is none here).
//
// WHERE THE NAMES COME FROM -- the same method the header's other eight used, and it is not a
// guess: the DecFIGS global list for THIS EXACT .cpp
// (references/DecFIGS/dwarfdump/GameSource/Physics/PropManager/BrnPropManager.cpp, the
// `BrnPhysics::Props` namespace block) names every namespace-scope constant in the file WITH its
// source line and its const-ness, and each of the five addresses below plays exactly one role in
// the asm. Both halves are recorded per line.
//
//   DWARF :338 const VecFloat KVF_LEAN_PROP_LERP_SPEED
//   DWARF :339 const VecFloat KVF_LEAN_PROP_MIN_LERP
//   DWARF :340 const VecFloat KVF_LEAN_PROP_ORTHOGONAL_TOLERANCE
//        -- the three constants declared IMMEDIATELY BEFORE UpdateJointedProps' own body scope
//           (DWARF puts that at BrnPropManager.cpp:343), and UpdateJointedProps reads exactly
//           three zero-page VecFloats. The role split inside those three is what pins which is
//           which; see each line.
//   DWARF :2046 VecFloat KVF_BREAK_JOINT_LINEAR_VEL
//   DWARF :2047 VecFloat KVF_BREAK_JOINT_ANGULAR_VEL
//        -- declared immediately before BreakJoint's body scope (DWARF :2054), and BreakJoint
//           reads exactly two zero-page VecFloats: one scales an ANGULAR quantity, one scales a
//           LINEAR one. Not const in the DWARF, unlike the three above -- transcribed as declared.
//
// ⭐ STATUS 2026-08-18 (round 3b): THE HEADER REQUEST THAT STOOD HERE IS DONE, AND THE VALUES ARE
//    RECOVERED. Both halves are now closed and this block is the record of what happened, not a
//    request:
//      * DECLARATIONS: all five are declared in BrnPropManager.h and defined in BrnPropManager.cpp,
//        so the file-local `extern` re-declarations this file used to carry -- which were the other
//        half of the request -- are deleted. The per-constant ROLE evidence that used to hang off
//        those re-declarations moved WITH them onto the header declarations; nothing was dropped.
//      * VALUES: the note here claimed "their real values come from a dynamic initialiser that is
//        not in the ARTIST export set (only readers are)". The first clause was right, the second
//        was measured FALSE. The initialisers ARE in the image -- MSVC dynamic-initialiser thunks
//        that sit outside every IDA function, hence invisible to any scan built on the per-address
//        function exports. Measured values, provenance on each header declaration:
//            KVF_LEAN_PROP_LERP_SPEED           = Splat(0.1f)
//            KVF_LEAN_PROP_MIN_LERP             = Splat(0.01f)
//            KVF_LEAN_PROP_ORTHOGONAL_TOLERANCE = Splat(0.1f)
//            KVF_BREAK_JOINT_LINEAR_VEL         = Splat(1.0f)
//            KVF_BREAK_JOINT_ANGULAR_VEL        = Splat(1.0f)
//        The old "CONSEQUENCE" paragraph (a leaning prop's visual transform never catching up; a
//        broken sign post released with zero spin and zero throw) described what THIS TREE would
//        have shipped with the placeholder zeroes, never what the console does. It is retired with
//        the claim it rested on.
// =================================================================================================

// (The five file-local `extern` re-declarations that used to sit here are DELETED -- they were the
//  pre-header workaround, and BrnPropManager.h now declares all five. Their ROLE evidence moved
//  onto those header declarations verbatim.)


// =================================================================================================
// The three SDK basis vectors UpdateJointedProps' orthogonality tripwire subtracts.
//
// ⭐ MEASURED, NOT ASSUMED -- byte-dumped this round (scratchpad/waveQ2/probe_wQ2_05/ida_basis.txt):
//     0x82181500  rw::math::vpu::detail::gIVector   3f800000 00000000 00000000 00000000  (1,0,0,0)
//     0x82181510  (unnamed in the .i64)             00000000 3f800000 00000000 00000000  (0,1,0,0)
//     0x82181520  (unnamed in the .i64)             00000000 00000000 3f800000 00000000  (0,0,1,0)
//     0x82181530  (unnamed in the .i64)             00000000 00000000 00000000 3f800000  (0,0,0,1)
// i.e. the SDK's I/J/K/L basis run, of which the first three are read here. Only gIVector carries
// a symbol in the database; the other two are named by position in that run and by the role the
// asm gives them (each is subtracted from the Gram-matrix row that must equal it). They are
// file-local `static` (internal linkage) here because rw::math::vpu::detail has no home in this
// tree -- reported as a header request rather than left as three bare literals inside the assert.
// =================================================================================================
static const Vector3 K_RW_I_VECTOR = { 1.0f, 0.0f, 0.0f, 0.0f };   // X360 0x82181500
static const Vector3 K_RW_J_VECTOR = { 0.0f, 1.0f, 0.0f, 0.0f };   // X360 0x82181510
static const Vector3 K_RW_K_VECTOR = { 0.0f, 0.0f, 1.0f, 0.0f };   // X360 0x82181520


// =================================================================================================
// BrnPhysics::Props::PropManager::AddPropToSim @ 0x826274D8   (167 instructions)
// DWARF: class decl BrnPropManager.h:313, body scope BrnPropManager.cpp:568.
//
// Build and post the InAddRigidBody for ONE WHOLE PROP.
//
// ---- REGISTER MAP, prologue 0x826274F4..0x82627524 ---------------------------------------------
//   r3 = this (r30)          r4 = lEntityId (r31)        r5 = liPropIndex (r28)
//   r6 = &lTransform (r26)   r7 = lpType (r29)           r8 = lbStatic (r27)
//   r9 = lbAddExtraComOffset (r25)                       r10 = lpSimModuleInputBuffer (r24)
//   v1 = lLinearVelocity (saved to v127)   v2 = lAngularVelocity (saved to v126)
// The by-value Matrix44Affine rides a hidden reference in r6 and the two Vector3s ride v1/v2 --
// see the gotcha-3 note in the file banner.
//
// ---- THE InAddRigidBody STACK RECORD, offset map ------------------------------------------------
// Record base == var_130 (frame 0xA0), the pointer handed to AddEvent. Every offset below is a
// store this body actually makes, matched to the member a committed static_assert already pins:
//   +0x00 mID                                `std`        0x826275AC
//   +0x10..+0x40 mRigidBody.mTransform       four stvx    0x826276DC..0x8262770C  (+0x40 rewritten
//                                                          by the extra-COM arm at 0x82627740)
//   +0x50 mRigidBody.mVelocity               `stvx128 v127` 0x82627568
//   +0x60 mRigidBody.mAngularVelocity        `stvx128 v126` 0x82627574
//   +0x70 mRigidBody.mInertia.mInvTens       `stvx128 v0`   0x826276A8
//   +0x80 mRigidBody.mInertia.mInvMass       `stfs`         0x826275C0 (and again 0x826275DC)
//   +0x84 mRigidBody.mInertia.mSpherical     `stfs`         0x826276EC
//   +0x88 mRigidBody.mInertia.mMaxVelocity   `stfs`         0x8262759C
//   +0x8C mRigidBody.mInertia.mMaxOmega      `stfs`         0x826275A8
//   +0x90 mRigidBody.mInertia.mLinearDrag    `stfs`         0x82627594
//   +0x94 mRigidBody.mInertia.mAngularDrag   `stfs`         0x8262758C
//   +0xA0 mRigidBody.mbSpy                   `stb 1`        0x826275CC
//   +0xB0 meState                            `stw`          0x826275C4
// (NewRigidBody sits at InAddRigidBody+0x10 and Inertia at NewRigidBody+0x60, so Inertia+0x18 ==
//  record+0x88 and so on.) Nothing writes Inertia's +0x98/+0x9C tail padding or the +0xA1..+0xAF
//  gap, so the record ships with those bytes as stack garbage, exactly as on the console.
//
// ---- THE DWARF LOCALS --------------------------------------------------------------------------
//   RigidBodyId lRigidBodyId @570 - InAddRigidBody lSimulationAddBodyEvent @571 -
//   Vector3 lInverseInertia @572 - NewRigidBody* lpNewRigidBody @587 - float32_t lrRatio @617 -
//   Vector3 lInertia @628.
// ⚠️ lpNewRigidBody @587 is a `NonConstructedClassContainer<NewRigidBody>::GetObjectPointer()`
//    result -- the source reached the embedded record through that accessor and then wrote through
//    the pointer. This tree embeds `NewRigidBody mRigidBody;` BY VALUE (the container is modelled
//    as raw storage -- see that header's NewRigidBody / InAddRigidBody blocks) and GetObjectPointer has no home
//    anywhere (physfix.owner.md §3.3 lists it among the missing), so the members are reached
//    directly. Same record, same stores; one accessor fewer.
// ⚠️ lrRatio @617: INFERENCE, and the weaker of the two readings (round-2 NIT -- this used to be
//    stated as fact). The DWARF scopes `float32_t lrRatio` (BrnPropManager.cpp:617) inside a nested
//    block whose ONLY three callees are the per-axis reciprocal divides
//    (`operator/<VectorAxisX/Y/Z>`, dumpfile :508-:510), which argues lrRatio is the per-axis ratio
//    local of the inverse-inertia fold -- i.e. the `1.0f / (lInertia.axis * KVF_INERTIA_SCALE.x)`
//    divisions below -- rather than the spherical minimum, which Inertia::SetInverseInertia (named
//    separately at :477, OUTSIDE that block) derives and stores at Inertia+0x14. Either way it is
//    not a separate local in the emission, so nothing below changes.
// =================================================================================================
void PropManager::AddPropToSim( PropEntityID                                  lEntityId,
                                s32                                           liPropIndex,
                                Matrix44Affine                                lTransform,
                                const PropTypeData*                           lpType,
                                bool                                          lbStatic,
                                bool                                          lbAddExtraComOffset,
                                CgsPhysics::PhysicsSimulationIO::InputBuffer* lpSimModuleInputBuffer,
                                Vector3                                       lLinearVelocity,
                                Vector3                                       lAngularVelocity )
{
    // 0x82627508..0x82627548 -- `srwi r11, r31, 24 ; cmplwi cr6, r11, 3`, fired at
    // BrnPropEntityID.h:278 with "mEntityId.GetOwner() == E_ENTITYTYPE_PROP". That is
    // PropEntityID::AssertIsProp() inlined; the console HOISTED it to the prologue, ahead of every
    // other instruction in the body, but the only thing that consumes the entity word is the
    // RigidBodyId packing below, whose GetValue() carries exactly this assert
    // (BrnPropEntityID.cpp:48-52 -- "AssertIsProp() then return the raw word"). Reaching it through
    // GetValue() reproduces the assert without adding a second copy of the owner test.
    // ⚠️ WHICH ACCESSOR THE SOURCE USED (round-2 NIT): the DWARF names
    // `BrnWorld::PropEntityID::operator CgsSceneManager::EntityId()` in this body's local block
    // (dumpfile BrnPropManager.cpp:452), not GetValue(). GetValue() is the STAND-IN, chosen because
    // it runs the same AssertIsProp() while returning the raw word the packing needs; behaviourally
    // identical, so nothing is dropped -- but the DWARF grounding is the conversion operator.

    CgsPhysics::PhysicsSimulationIO::InAddRigidBody lSimulationAddBodyEvent;

    // 0x82627568 / 0x82627574 -- the two parameter vectors go straight into the record.
    lSimulationAddBodyEvent.mRigidBody.mVelocity        = lLinearVelocity;
    lSimulationAddBodyEvent.mRigidBody.mAngularVelocity = lAngularVelocity;

    // ⚠️⚠️ MEASURED CROSS-WIRING -- transcribed as it is, NOT "corrected". The identical pair of
    // stores exists in CreatePart, and PropManager_wQ2_04.cpp's banner records the full two-sided
    // evidence (the address->name map from PropDebugComponent::OnActivate's RegisterVariable
    // labels, and the offset->member map from rw::physics::RigidBody::DynamicUpdate @0x82BC2B78).
    // Re-measured HERE this round: `lfs f0, (flt_82F2A390)` @0x82627598 -> `stfs var_A8`
    // (record+0x88 == Inertia+0x18 == mMaxVelocity, the LINEAR clamp), and
    // `lfs f0, (flt_82F2A394)` @0x826275A0 -> `stfs var_A4` (record+0x8C == Inertia+0x1C ==
    // mMaxOmega, the ANGULAR clamp). So the ANGULAR-named constant lands on the LINEAR clamp and
    // vice versa, in this body too. Whether the shipped SOURCE crossed the arguments or one of the
    // two name maps is mislabelled is NOT decidable from the asm; this file reproduces the stores.
    lSimulationAddBodyEvent.mRigidBody.mInertia.SetMaxLinearVelocity(  KF_PROP_MAX_ANGULAR_VEL ); // -> +0x18
    lSimulationAddBodyEvent.mRigidBody.mInertia.SetMaxAngularVelocity( KF_PROP_MAX_LINEAR_VEL );  // -> +0x1C
    lSimulationAddBodyEvent.mRigidBody.mInertia.SetLinearDrag(  KF_PROP_LINEAR_DRAG );            // -> +0x20
    lSimulationAddBodyEvent.mRigidBody.mInertia.SetAngularDrag( KF_PROP_ANGULAR_DRAG );           // -> +0x24
    // ⚠️ KF_PROP_RESTITUTION @0x82F2A398 is the fifth constant of that rodata run and is NOT read
    //    anywhere in these 167 instructions -- stated because BrnPropManager.h's AddPropToSim banner describes this
    //    body as reading "the five KF_PROP_* drag/limit scalars". It is FOUR. Measured: the only
    //    loads off the flt_82F2A394 anchor are at -0xC / -0x8 / -0x4 / 0, i.e. 0x82F2A388,
    //    0x82F2A38C, 0x82F2A390 and 0x82F2A394. There is no load of +0x4.

    // :570. `sldi r7, r31, 32 ; clrlwi r6, r28, 16 ; or ; std` == (entityWord << 32) | (idx & 0xFFFF)
    // -- the packed CgsPhysics::RigidBodyId (high dword = the EntityId, low 16 bits = the index;
    // CgsRigidBody.h's banner and GetIndex() both say so). The DWARF names the packer
    // CgsPhysics::RigidBodyId::Set; that multi-argument setter is deliberately NOT declared in this
    // tree (CgsRigidBody.h's banner says why), so the shift/or is spelled here, exactly as the
    // three committed siblings in PropManager_wQ2_04.cpp do.
    const CgsPhysics::RigidBodyId lRigidBodyId(
        ( static_cast<u64>( lEntityId.GetValue() ) << 32 )
        | static_cast<u64>( static_cast<u16>( liPropIndex ) ) );

    lSimulationAddBodyEvent.mID = lRigidBodyId;

    // 0x82627558 `lfs f13, 0x38(r29)` == PropTypeData::mfMass (console +0x38), then
    // 0x826275BC `fdivs f0, f31, f13` with f31 == flt_82001C98. That constant was byte-dumped this
    // round as 3f800000 == 1.0f (scratchpad/waveQ2/probe_wQ2_05/ida_basis.txt), which is also what
    // rw/physics/inertia.h:63 already records.
    lSimulationAddBodyEvent.mRigidBody.mInertia.SetInverseMass( 1.0f / lpType->GetMass() );

    // 0x82627550..0x826275C8 -- `clrlwi r11, r27, 24 ; subfic r11, r11, 0 ; subfe r8, r11, r11 ;
    // rlwinm r10, r8, 0,31,29 ; addi r11, r10, 4`. Worked through: subfic leaves CA==1 exactly when
    // lbStatic is 0, so subfe yields 0 for a dynamic prop and -1 for a static one; the rlwinm mask
    // (MB=31 > ME=29) is 0xFFFFFFFD, so r10 is 0 or 0xFFFFFFFD; +4 gives 4 or 1. That is the
    // branchless form of `lbStatic ? STATIC_BODY : ACTIVE_BODY` (rigidbody.h:77-80 -- STATIC_BODY
    // == 1, ACTIVE_BODY == 4).
    lSimulationAddBodyEvent.meState = lbStatic ? rw::physics::STATIC_BODY : rw::physics::ACTIVE_BODY;

    // 0x826275C8/0x826275CC `li r11,1 ; stb r11, var_90` == mbSpy = TRUE.
    // ⚠️ MEASURED DIFFERENCE FROM CreatePart, stated rather than harmonised: the PART path
    //    (PropManager_wQ2_04.cpp, `stb 0` @0x82627D0C) sets mbSpy FALSE. Whole props are spied,
    //    shed parts are not. Both are transcriptions, not choices.
    lSimulationAddBodyEvent.mRigidBody.mbSpy = true;

    // 0x82627550 `lbz r9, 0x49(r30)` == mbUseOverides, tested at 0x82627570 and branched at
    // 0x826275D0; the taken arm reloads `lfs f0, 0x4C(r30)` == mfMassOverride and OVERWRITES the
    // inverse mass just stored (the console really does compute both -- there are two `stfs` into
    // the same slot). Two SetInverseMass calls is exactly what the DWARF callee list shows.
    if ( mbUseOverides )
    {
        lSimulationAddBodyEvent.mRigidBody.mInertia.SetInverseMass( 1.0f / mfMassOverride );
    }

    // :628. 0x826275EC `bl PropManager::GetPropInertia` with r3 = &result (the hidden sret
    // pointer), r4 = this, r5 = lpType -- a normal by-value return here.
    // ⛔ GetPropInertia HAS NO BODY IN THE TREE (BrnPropManager.h's GetPropInertia/GetPartInertia block declares it; its round-1 body
    //    is parked on the rw::collision::AABBox include clash). Link-red, gate-green.
    const Vector3 lInertia = GetPropInertia( lpType );

    // :572. 0x826275F0..0x82627694. Each axis is splatted, multiplied by the WHOLE
    // KVF_INERTIA_SCALE register (`lvx128 v0, flt_82FB9400` -> three vmulfp128), and only LANE 0 of
    // each product is read back (`stvx` then `lfs` at offset 0) before `fdivs f0, f31(1.0f), f0`.
    // So the scalar taken from the VecFloat is its lane 0, and the result is the per-axis
    // reciprocal of the scaled inertia -- identical to CreatePart's block.
    // The vperm/vrlimi128 pair at 0x8262768C/0x82627690 is how the compiler reassembles the three
    // scalars: the control vector unk_82CDA350 was byte-dumped this round as
    // `00 01 02 03 | 14 15 16 17 | 00 01 02 03 | 00 01 02 03`, i.e. dest word0 = vA word0 and dest
    // word1 = vB word1, and the vrlimi128 then drops the third scalar into lane 2. Lane 3 is a
    // duplicate of lane 0 and is never read.
    // ⭐ KVF_INERTIA_SCALE == Splat(3.0f), RECOVERED 2026-08-18 round 3b (thunk 0x82C5E6C0,
    //    rodata flt_82004270 = 0x40400000; see BrnPropManager.h). The warning that stood here --
    //    "reads all-zero on disk and has no recovered initialiser ... every division below is
    //    1.0f/0.0f" -- was half right: the disk bytes really are zero, but the initialiser exists
    //    as a dynamic-initialiser thunk. The divisions are now by 3x the raw inertia, which is what
    //    an INERTIA SCALE of 3 means: props resist spin three times as hard as their box tensor.
    Vector3 lInverseInertia;
    lInverseInertia.x = 1.0f / ( lInertia.x * KVF_INERTIA_SCALE.x );
    lInverseInertia.y = 1.0f / ( lInertia.y * KVF_INERTIA_SCALE.x );
    lInverseInertia.z = 1.0f / ( lInertia.z * KVF_INERTIA_SCALE.x );

    // :617. 0x82627698..0x826276EC -- mInvTens := lInverseInertia, then
    // mSpherical := 1.0f / min(x, min(y, z)), which IS rw::physics::Inertia::SetInverseInertia
    // (inertia.h:117) inlined. GOTCHA 4: both selects are `fcmpu ; blt <skip> ; fmr`, i.e.
    // `(a < b) ? a : b` with the UNORDERED case taking the `fmr`; the committed inline spells it
    // with the same ternaries, so the NaN behaviour matches lane for lane.
    // ⚠️ Same harmless scheduling difference CreatePart records: the committed inline stores the
    //    intermediate min(x,y) to +0x14 before the second compare, and here that intermediate store
    //    is optimised away (only the final `stfs` at 0x826276EC lands). Same final value.
    lSimulationAddBodyEvent.mRigidBody.mInertia.SetInverseInertia( lInverseInertia );

    // 0x826276C8..0x8262770C -- four lvx/stvx pairs off r26 at +0/+0x10/+0x20/+0x30.
    lSimulationAddBodyEvent.mRigidBody.mTransform = lTransform;

    // 0x82627710..0x82627740, gated on `clrlwi r8, r25, 24 ; cmplwi cr6, r8, 0`.
    // The arm splats K_PROP_EXTRA_COM_OFFSET's three lanes and folds
    //     vmulfp128 v13, row0, splat(off.x)                 == row0 * off.x
    //     vmaddfp   v13, row1, v13, splat(off.y)            == row1 * off.y + that
    //     vmaddfp   v0,  row2, v13, splat(off.z)            == row2 * off.z + that
    //     vaddfp    v0,  row3, v0                           == Pos + that
    // (AltiVec vmaddfp vD,vA,vB,vC is vD = vA*vC + vB -- vB is the ADDEND, vC the second multiplier.
    //  Getting that backwards is the operand-order defect wave Q round 1 shipped once already.)
    // So the body origin is shifted by the offset ROTATED into world space by the transform's 3x3,
    // written back over the record's mTransform.wAxis only. ReadUpdatedBodies @0x82632A7C undoes
    // exactly this on the way back, gated on PropInstance::KU_HAS_EXTRA_COM_OFFSET_FLAG.
    // ⚠️ The console writes the shifted position into the RECORD (record+0x40 == mTransform.wAxis);
    //    the caller's lTransform is a by-value copy and is not the storage being written. Spelled
    //    through the record member so that is unambiguous.
    // ⭐ K_PROP_EXTRA_COM_OFFSET == (0, 0, -0.2), RECOVERED 2026-08-18 round 3b (Vector3 thunk
    //    0x82C5E7F0, z from rodata flt_82020A84 = 0xBE4CCCCD; see BrnPropManager.h). The note that
    //    stood here said it "reads all-zero on disk ... this whole arm is a no-op today"; the disk
    //    bytes are zero but the initialiser exists, and this arm is live: a 20 cm centre-of-mass
    //    shift along the prop's own -Z, rotated into world space by the transform below.
    // ⭐ SPELLED THROUGH THE HOMED HELPER (round-2 MUST_FIX, applied): the cascade above IS
    //    rw::math::vpu::TransformVector, which has a real vendor home at
    //    vendor/renderware/include/rw/math/vpu/matrix44affine_operation.h:41 -- a header this file
    //    already includes -- and the DWARF names exactly `Matrix44Affine::Pos` / `TransformVector` /
    //    `operator+=` for this body (dwarfdump BrnPropManager.cpp:501/:502/:503). It was previously
    //    hand-inlined here, which forked a homed helper silently; AGENTS.md's "inlining reversal"
    //    rule says to spell the call. (`operator+=` has no vendor home, so the `Pos() = Pos() + ...`
    //    long form stands in for it; Pos() is types.h:81, a reference to wAxis.)
    // ⚠️ THE ONE LANE THIS CHANGES, RECORDED SO IT IS NOT RE-DERIVED AS A BUG: the console's
    //    `vaddfp v0, v10, v0` @0x8262773C is 4-lane, so its w is wAxis.w + xAxis.w*off.x +
    //    yAxis.w*off.y + zAxis.w*off.z, whereas the committed TransformVector forces its result's
    //    w = 0.0f and the add therefore leaves wAxis.w unchanged. For a well-formed affine -- rows'
    //    w == 0, which is what Matrix44Affine::SetIdentity seeds and what every producer of this
    //    transform ships -- the two are identical. Contrast ReadUpdatedBodies' decode (G)
    //    (PropManager_wQ2_01.cpp), where the mirror-image undo keeps the longhand precisely because
    //    the packed w scalars there are LIVE.
    if ( lbAddExtraComOffset )
    {
        lSimulationAddBodyEvent.mRigidBody.mTransform.Pos() =
            lSimulationAddBodyEvent.mRigidBody.mTransform.Pos()
            + rw::math::vpu::TransformVector( lTransform, K_PROP_EXTRA_COM_OFFSET );
    }

    // 0x8262774C `bl CgsPhysics::PhysicsSimulationIO::InputBuffer::GetAddRigidBodyQueue`
    // (IDA truncates the symbol to `CgsPhysics__Ph`; it is 0x825BCE08, the write-lock-guarded
    // producer overload whose "Not locked for writing\n" assert bakes
    // CgsPhysicsSimulationModuleIO.h:1059 -- the same callee CreatePart uses), then
    // 0x82627754 `bl BaseEventQueue<InAddRigidBody>::AddEvent`.
    lpSimModuleInputBuffer->GetAddRigidBodyQueue()->AddEvent( lSimulationAddBodyEvent );
}


// =================================================================================================
// BrnPhysics::Props::PropManager::BreakJoint @ 0x82628A88   (137 instructions)
// DWARF: class decl BrnPropManager.h:336, body scope BrnPropManager.cpp:2054.
//
// THE BREAK ITSELF for a jointed prop -- the smash gate, the leaning sign post, the tilting
// lamppost. The prop was being driven kinematically about a fixed joint; this cuts it loose:
// remove the old body, free the joint slot, mark the prop dynamic and un-jointed, give it the spin
// the joint had accumulated plus the tangential velocity that spin implies at its centre, and
// re-add it as a free rigid body through AddPropToSim.
//
// ---- REGISTER MAP, prologue 0x82628A94..0x82628AAC ---------------------------------------------
//   r3 = this (r28)   r4 = lEntityId (r25)   r5 = liPropIndex (r26)
//   r6 = &lTransform (r23)   r7 = lpType (r27)   r8 = lpSimModuleInputBuffer (r24)
//
// ---- THE SOURCE LINES THE ASSERTS BAKE ---------------------------------------------------------
//   BrnPropEntityID.h:278 (0x116)   "mEntityId.GetOwner() == E_ENTITYTYPE_PROP"
//   BrnPropInstance.h:326 (0x146)   "IsJointed()"           -- GetJointIndex's own tripwire
//   BrnPropManager.cpp:2066 (0x812) "lpProp->IsJointed()"
//   CgsBitArray.h:241 (0xF1)        "luIndex < NUMBITS"     -- UnSetBit's bound, against 0xF == 15
//
// ---- THE DWARF LOCALS --------------------------------------------------------------------------
//   InRemoveRigidBody lSimulationRemoveBodyEvent @2057 - RigidBodyId lPropRigidBodyId @2058 -
//   PropInstance* lpProp @2064 - int32_t liJointIndex @2065 - Vector3 lAngularVelocity @2072 -
//   Vector3 lLinearVelocity @2073.
// The declaration lines settle the ordering: the angular velocity is built first (:2072) and the
// linear one is derived from it (:2073), which is also what the emission does.
// =================================================================================================
void PropManager::BreakJoint( PropEntityID                                  lEntityId,
                              s32                                           liPropIndex,
                              Matrix44Affine                                lTransform,
                              const PropTypeData*                           lpType,
                              CgsPhysics::PhysicsSimulationIO::InputBuffer* lpSimModuleInputBuffer )
{
    // :2057 / :2058. 0x82628AD8..0x82628AEC -- the same `sldi r10, r25, 32 ; clrlwi r11, r26, 16 ;
    // or ; std` packing every sibling uses, and the owner tripwire fired at 0x82628AB8 is
    // PropEntityID::AssertIsProp() inlined and hoisted to the prologue -- reached through GetValue().
    // ⚠️ Same DWARF caveat as AddPropToSim's (round-2 NIT): this body's DWARF local block names
    // `BrnWorld::PropEntityID::operator CgsSceneManager::EntityId()`, not GetValue(); GetValue() is
    // the stand-in that carries the same AssertIsProp().
    CgsPhysics::PhysicsSimulationIO::InRemoveRigidBody lSimulationRemoveBodyEvent;
    const CgsPhysics::RigidBodyId lPropRigidBodyId(
        ( static_cast<u64>( lEntityId.GetValue() ) << 32 )
        | static_cast<u64>( static_cast<u16>( liPropIndex ) ) );

    lSimulationRemoveBodyEvent.mID = lPropRigidBodyId;
    // ⚠️ Same console quirk RemovePart/RemoveProp record: the 16-byte stack record's
    //    mbFailIfRigidBodyNotFound byte at +8 is never written (measured -- the only store into
    //    var_60 is the `std` at 0x82628AEC), so on the X360 it is uninitialised stack. The host
    //    sets it FALSE == the tolerant remove, the committed treatment of the identical quirk in
    //    PropManager_wQ2_04.cpp's `PropManager::RemovePart` and BrnPhysicalBodyPartPool_Remove.cpp:51.
    lSimulationRemoveBodyEvent.mbFailIfRigidBodyNotFound = false;

    // 0x82628AF0 `bl sub_825BCF58` == InputBuffer::GetRemoveRigidBodyQueue() (the write-lock
    // producer overload; identified in PropManager_wQ2_04.cpp's `PropManager::RemovePart` by its baked
    // CgsPhysicsSimulationModuleIO.h:1080 assert), then
    // 0x82628AF8 `bl BaseEventQueue<InRemoveRigidBody>::AddEvent`.
    lpSimModuleInputBuffer->GetRemoveRigidBodyQueue()->AddEvent( lSimulationRemoveBodyEvent );

    // :2064. `lwz r10, 0x7C(r28)` + `mulli r11, r26, 0x70` == mpaPropInstances + idx*112 (CONSOLE
    // stride); indexed by name here.
    PropInstance* lpProp = &mpaPropInstances[ liPropIndex ];

    // :2065 / :2066. The console reads `lbz 0x6C(r30)` TWICE -- once at 0x82628B08 for
    // GetJointIndex's own "IsJointed()" tripwire (BrnPropInstance.h:326, and note its polarity:
    // `cmplwi 0xFF ; bne <skip>` fires the assert when the slot IS the KU_NOT_JOINTED sentinel),
    // and once at 0x82628B34 for the value plus the .cpp:2066 assert. Calling the committed
    // GetJointIndex() (BrnPropInstance.cpp:66, which carries that exact assert) and then the
    // separate CGS_ASSERT reproduces both, in order.
    const s32 liJointIndex = lpProp->GetJointIndex();
    CGS_ASSERT( lpProp->IsJointed(), "lpProp->IsJointed()" );

    // UnSetBit's own bound tripwire (CgsBitArray.h:241, `cmplwi cr6, r31, 0xF` at 0x82628B64 --
    // 15 == mUsedPropJoints' capacity), then the ld/andc/std at 0x82628BB8..0x82628BC4. The message
    // is the binary's own. `addi r29, r28, 0x670` == &mUsedPropJoints, reached by name.
    CGS_ASSERT( static_cast<u32>( liJointIndex ) < mUsedPropJoints.GetCapacity(), "luIndex < NUMBITS" );
    mUsedPropJoints.UnSetBit( static_cast<u32>( liJointIndex ) );

    // 0x82628BCC `stb r8(=0xFF), 0x6C(r30)` then 0x82628BDC `stb r11(=0), 0x6D(r30)` -- in that
    // order. +0x6C is mu8JointIndex (SetNotJointed writes KU_NOT_JOINTED == 255) and +0x6D is
    // mbIsStatic; both offsets are pinned by BrnPropInstance.h's offsetof block.
    lpProp->SetNotJointed();
    lpProp->SetIsStatic( false );

    // :2072. 0x82628BF0/0x82628C08 -- `lvx128 v11, r5, r28` with r5 == (liJointIndex + 27) * 16 ==
    // 0x1B0 + liJointIndex*16 == &maLastJointRotation[liJointIndex] (CONSOLE offsets; reached by
    // name), multiplied lane-by-lane by the whole KVF_BREAK_JOINT_ANGULAR_VEL register. The
    // constant is a broadcast VecFloat, so taking lane 0 is identical -- the same treatment
    // PropManager_wQ2_04.cpp gives KVF_INERTIA_SCALE.
    const Vector3 lAngularVelocity = maLastJointRotation[ liJointIndex ] * KVF_BREAK_JOINT_ANGULAR_VEL.x;

    // :2073. 0x82628BD0..0x82628C28. `lvx128 v0, r30, 0x30` is the prop's own world position
    // (PropInstance::mWorldTransform.wAxis, +0x30 inside the affine at +0x00) and
    // `lvx128 v12, r7, r28` with r7 == (liJointIndex + 12) * 16 == 0xC0 + liJointIndex*16 ==
    // &maPropJointPositions[liJointIndex]; `vsubfp` gives the joint->prop lever arm.
    //
    // The five instructions at 0x82628C14..0x82628C24 are the textbook AltiVec cross product, and
    // it was decoded rather than assumed: `vpermwi128 v,v,0x63` is the (y,z,x,w) word rotation
    // (0x63 == 01 10 00 11), and `vnmsubfp vD,vA,vB,vC` is vD = vB - vA*vC, so the pair computes
    //     (v2*lever.yzx - v2.yzx*lever) == (cross.z, cross.x, cross.y)
    // which the final vpermwi128 rotates back to (cross.x, cross.y, cross.z) -- i.e. exactly
    // Cross(lAngularVelocity, lever), with lAngularVelocity FIRST. The DWARF callee list for this
    // body names rw::math::vpu::Cross once, between the two operator* calls, which agrees.
    const Vector3 lLinearVelocity =
        Cross( lAngularVelocity,
               lpProp->GetTransform().Pos() - maPropJointPositions[ liJointIndex ] )
        * KVF_BREAK_JOINT_LINEAR_VEL.x;

    // 0x82628C04..0x82628C60 -- `lwz r11, 0x58(r27)` == PropTypeData::muSceneUriId, compared
    // against 0x6894C == 428364 and 0x68964 == 428388, and the boolean drives an OR-2 / AND-~2 on
    // `lbz 0x6F(r30)` == PropInstance::mu8Flags bit 1 == KU_HAS_EXTRA_COM_OFFSET_FLAG. That id pair
    // is PropTypeData::HACKShouldMoveComOffset() (BrnPhysicsPropTypeData.h:222), and the DWARF
    // callee list names it TWICE in this body -- which is what the emission does: it re-loads
    // +0x58 and redoes the same compare at 0x82628C64 for the AddPropToSim argument, rather than
    // reusing the value. Both calls are spelled.
    lpProp->SetExtraComOffsetFlag( lpType->HACKShouldMoveComOffset() );

    // 0x82628C80..0x82628CA0 -- the tail call. r6 is the ORIGINAL lTransform parameter (r23), NOT
    // the prop's own transform; r8 == 0 is lbStatic (a broken prop is dynamic by construction);
    // r9 is the second HACKShouldMoveComOffset(); v1/v2 already hold the two velocities.
    AddPropToSim( lEntityId,
                  liPropIndex,
                  lTransform,
                  lpType,
                  false,
                  lpType->HACKShouldMoveComOffset(),
                  lpSimModuleInputBuffer,
                  lLinearVelocity,
                  lAngularVelocity );
}


// =================================================================================================
// BrnPhysics::Props::PropManager::UpdateJointedProps @ 0x82631260   (938 instructions)
// DWARF: class decl BrnPropManager.h:151, body scope BrnPropManager.cpp:343.
//
// The per-frame jointed-prop driver, and the second-largest body in this class. TWO passes, both
// over mUsedPropJoints (the console re-walks the same bit set from scratch -- measured: the second
// walk re-materialises `addi r10, r22, 0x670` at 0x82631894 rather than reusing pass 1's cursor):
//
//   PASS 1 (0x82631280..0x8263188C) -- for every live joint whose bit is ALSO set in
//     mBreakPropJoints, call BreakJoint. Note BreakJoint clears the joint's mUsedPropJoints bit but
//     NOT its mBreakPropJoints bit; the walk re-reads the array each step, so the freed slot simply
//     stops being visited. Transcribed as-is -- whoever owns the producer of mBreakPropJoints
//     should know the request bit is not self-clearing here.
//
//   PASS 2 (0x82631890..0x82632104) -- for every live joint, lerp the manager's own
//     maCurrentJointTransforms[joint] toward the prop's live world transform, re-orthonormalise the
//     3x3, store it back, and queue one UpdatePropEvent on mUpdatedJointedProps. Props whose
//     transform has not moved enough are skipped entirely.
//
// ---- REGISTER MAP, prologue 0x82631274..0x82631288 ---------------------------------------------
//   r3 = this (r22, and spilled to arg_14 because both passes reload it across calls)
//   r4 = lpSimModuleInputBuffer (spilled to arg_1C at 0x82631278; the ONLY use is BreakJoint's 5th
//        argument at 0x82631690)
//
// ---- THE SOURCE LINES THE ASSERTS BAKE ---------------------------------------------------------
//   CgsBitArray.h:203 (0xCB)        IsBitSet's bound tripwire -- the StrStream form,
//                                   "invalid index : " << i << " < " << 15. THREE copies in pass 1
//                                   (the joint index @0x82631364, the prop index @0x826314BC, and
//                                   the walk cursor @0x826316C8) and ONE in pass 2's walk
//                                   @0x82631F0C.
//                                   ⚠️ Only TWO of those four are spelled below (the joint and prop
//                                   indices). The other two live inside GetNextNonZeroBit's inlined
//                                   IsBitSet, and the committed CgsBitArray.h is deliberately
//                                   assert-free (see its GetBitRange note), so they are
//                                   unreproducible by design -- recorded here rather than spelled.
//   BrnPropManager.cpp:359 (0x167)  "mUsedProps.IsBitSet( liPropIndex )"
//   BrnPropPhysicsDataHeader.h:173/174 (0xAD/0xAE)  GetType's two bounds asserts, inlined
//   BrnPropManager.cpp:392 (0x188)  the IsValid(lLerpMatrix) tripwire, StrStream message
//   BrnPropManager.cpp:393 (0x189)  the IsOrthogonal3x3(lLerpMatrix) tripwire, StrStream message
//
// ---- THE DWARF LOCALS (dwarfdump BrnPropManager.cpp:2090-2148) ---------------------------------
// The nesting is explicit in the dump and it is what settles which loop owns what:
//   function scope : int32_t liPropIndex @345 - PropInstance* lpProp @346 -
//                    Matrix44Affine lTransform @347 - lDesiredTransform @348 - lLerpMatrix @349 -
//                    UpdatePropEvent lUpdateEvent @350 - bool lbNeedsRotating @351
//     nested block : int32_t liJointIndex @354                       <- PASS 1's cursor
//     nested block : int32_t liPropIndex @358 - PropInstance* lpProp @360 -
//                    const PropTypeData* lpType @361                 <- PASS 1's body (shadows)
//     nested block : int32_t liLeaningPropIndex @367                 <- PASS 2's cursor
// so the seven function-scope locals are PASS 2's working set, hoisted to the top of the function
// in the source. INFERENCE, flagged: the dump gives declaration lines, not statement order, so
// "which of the two loops came first in the .cpp" is not directly attested -- what IS attested is
// the emission order (break first, update second), and that is the order written below.
//
// ---- THE CALLEE LIST THE DWARF NAMES, AND WHERE EACH ONE LANDS ---------------------------------
//   BitArray<15>::{GetFirstNonZeroBit, GetNextNonZeroBit, IsBitSet} - ResourcePtr<>::operator-> -
//   PropInstance::GetTypeId - PropPhysicsDataHeader::GetType - Matrix44Affine::Matrix44Affine/
//   operator= - operator- x4, Abs x4, operator+ x3, IsZero  (the lbNeedsRotating test) -
//   NormalizeFast x3, operator*=, operator+ x4              (the lerp) -
//     ⚠️ Those counts were MACHINE-COUNTED over the dumpfile block (round-2 MUST_FIX; the earlier
//     "operator- x3, Abs x3, operator+ x2" was wrong and, worse, was used below as the reason the
//     code emits three Abs): over
//     references/DecFIGS/dwarfdump/GameSource/Physics/PropManager/BrnPropManager.cpp lines
//     2090-2263 the totals are Abs=4, operator-=4, operator+=7 (3 in the test + 4 in the lerp),
//     operator*==1, MagnitudeSquared=4, NormalizeFast=3, IsZero=2, SetZero=2, IsBitSet=2,
//     IsOrthogonal3x3=1, SelfSubtract=1, Matrix44Affine ctor=3, operator= =4. The code below
//     matches that exactly -- four deltas, four Abs, four subtractions.
//   IsValid, IsOrthogonal3x3, SelfSubtract, MagnitudeSquared x4, IsZero  (the two tripwires) -
//   Vector3::SetZero x2, BaseEventQueue<UpdatePropEvent>::AddEvent  (the event).
// Every one of those is spelled below except IsOrthogonal3x3, which has no home in this tree --
// see its block.
// =================================================================================================
void PropManager::UpdateJointedProps( CgsPhysics::PhysicsSimulationIO::InputBuffer* lpSimModuleInputBuffer )
{
    // =============================================================================================
    // PASS 1 -- break every joint flagged this frame.
    // 0x82631280 `addi r10, r22, 0x670` == &mUsedPropJoints, then the ld/cmpldi/cntlzd sequence at
    // 0x8263130C..0x82631338 which is BitArray<15>::GetFirstNonZeroBit inlined (including its
    // `>= tuNumBits -> KI_INVALID_BITINDEX` guard, emitted as `cmpwi r11, 0xF ; bge <exit>`), and
    // the block at 0x826316A8..0x8263188C which is GetNextNonZeroBit inlined.
    // =============================================================================================
    for ( s32 liJointIndex = mUsedPropJoints.GetFirstNonZeroBit();
          liJointIndex != CgsContainers::BitArray<15>::KI_INVALID_BITINDEX;
          liJointIndex = mUsedPropJoints.GetNextNonZeroBit( liJointIndex ) )
    {
        // IsBitSet's own bound tripwire (CgsBitArray.h:203, `cmplwi cr6, r27, 0xF` at 0x82631364).
        // The console builds its message with a StrStream -- "invalid index : " << liJointIndex <<
        // " < " << 15 -- through the same BasePriorityQueue::Clear + AppendFormat pair every other
        // BitArray tripwire in this cluster uses; spelled with the short form here, exactly as the
        // committed siblings in PropManager_wQ2_04.cpp's RemovePart/RemoveProp do.
        CGS_ASSERT( static_cast<u32>( liJointIndex ) < mBreakPropJoints.GetCapacity(), "invalid index" );

        // 0x8263147C..0x826314B0 -- `(liJointIndex>>6 + 0xCF)*8 + this`. 0xCF*8 == 0x678 ==
        // &mBreakPropJoints (mUsedPropJoints is 0x670; the two BitArray<15>s are adjacent). CONSOLE
        // offset, reached by name -- and the 0x678/0x670 pair is the arithmetic self-check that the
        // two names are not swapped.
        if ( mBreakPropJoints.IsBitSet( static_cast<u32>( liJointIndex ) ) )
        {
            // :358. 0x826314B4/0x826314B8 `add r11, r27, r22 ; lbz r28, 0x2A0(r11)` ==
            // mauPropIndexForJoint[liJointIndex] (a u8 array; CONSOLE +0x2A0, by name here).
            const s32 liPropIndex = mauPropIndexForJoint[ liJointIndex ];

            // The second CgsBitArray.h:203 copy, this one on the prop index (`cmplwi cr6, r28, 0xF`
            // at 0x826314BC), then :359 itself at 0x826315D4..0x82631620 --
            // `(liPropIndex>>6 + 0x10)*8 + this`, 0x10*8 == 0x80 == &mUsedProps.
            CGS_ASSERT( static_cast<u32>( liPropIndex ) < mUsedProps.GetCapacity(), "invalid index" );
            CGS_ASSERT( mUsedProps.IsBitSet( static_cast<u32>( liPropIndex ) ),
                        "mUsedProps.IsBitSet( liPropIndex )" );

            // :360. `lwz r10, 0x7C(r22)` + `mulli r11, r28, 0x70` == mpaPropInstances + idx*112.
            PropInstance* lpProp = &mpaPropInstances[ liPropIndex ];

            // :361. 0x82631630..0x82631684 -- `addi r3, r22, 0x54` into
            // CgsResource::ResourcePtr<T>::operator-> (mpPhysicsData), `lwz r30, 0x64(r31)` ==
            // PropInstance::muTypeId, then GetType INLINED: both of its bounds asserts
            // (BrnPropPhysicsDataHeader.h:173 against 0x1F4 == KU_MAX_PROP_TYPES, and :174 against
            // the header's own muNumberOfPropTypes at +0x00) followed by
            // `addi r11, r30, 4 ; slwi r11, r11, 2 ; lwzx r7, r11, r29` == mapPropTypes[typeId]
            // (the array base is header+0x10 == word 4). Calling the committed GetType reproduces
            // all three, in order.
            const PropTypeData* lpType = mpPhysicsData->GetType( lpProp->GetTypeId() );

            // 0x82631688..0x826316A4 -- r4 = `lwz 0x60(r31)` == PropInstance::mEntityId,
            // r6 = r31 itself == &lpProp->mWorldTransform (the affine is at +0x00), r8 reloads the
            // spilled lpSimModuleInputBuffer.
            BreakJoint( lpProp->GetEntityId(),
                        liPropIndex,
                        lpProp->GetTransform(),
                        lpType,
                        lpSimModuleInputBuffer );
        }
    }

    // =============================================================================================
    // PASS 2 -- lerp every live joint's cached transform toward the prop's live transform.
    // 0x82631890 re-walks mUsedPropJoints from the start.
    // =============================================================================================
    for ( s32 liLeaningPropIndex = mUsedPropJoints.GetFirstNonZeroBit();
          liLeaningPropIndex != CgsContainers::BitArray<15>::KI_INVALID_BITINDEX;
          liLeaningPropIndex = mUsedPropJoints.GetNextNonZeroBit( liLeaningPropIndex ) )
    {
        // :345 / :346. 0x8263196C..0x826319B0 -- `lbzx r27, r11, r26` with r11 == this + 0x2A0
        // (mauPropIndexForJoint) and `mulli r11, r27, 0x70` + `lwz r10, 0x7C(r30)` for the
        // instance. ⚠️ MEASURED: this pass emits NO bounds assert on either index -- there is no
        // `cmplwi 0xF` and no `bl BeginAssert` between 0x8263196C and 0x826319B8. Pass 1 has three
        // and this pass has none; that asymmetry is the shipped image's, not an omission here.
        const s32 liPropIndex = mauPropIndexForJoint[ liLeaningPropIndex ];
        PropInstance* lpProp  = &mpaPropInstances[ liPropIndex ];

        // :347 / :348. 0x8263199C `addi r31, r11, 0x2B0` with r11 == this + liLeaningPropIndex*64
        // == &maCurrentJointTransforms[liLeaningPropIndex] (CONSOLE +0x2B0, 64-byte affine stride;
        // reached by name), and the four lvx128 off r28 == the prop's own mWorldTransform.
        // ⚠️ WHICH IS WHICH, and it matters: the SUBTRACTION at 0x826319C0..0x826319D4 is
        //    `prop - cached`, and the sum is added back onto `cached`. So the manager's cached
        //    joint transform is what LERPS, and the prop's live transform is the TARGET. That is
        //    why the DWARF calls the cached one lTransform and the live one lDesiredTransform.
        const Matrix44Affine lTransform        = maCurrentJointTransforms[ liLeaningPropIndex ];
        const Matrix44Affine lDesiredTransform = lpProp->GetTransform();

        const Vector3 lDeltaXAxis = lDesiredTransform.xAxis - lTransform.xAxis;
        const Vector3 lDeltaYAxis = lDesiredTransform.yAxis - lTransform.yAxis;
        const Vector3 lDeltaZAxis = lDesiredTransform.zAxis - lTransform.zAxis;
        const Vector3 lDeltaWAxis = lDesiredTransform.wAxis - lTransform.wAxis;

        // :351. 0x826319D8..0x82631A10 -- four `vandc <row>, 0x80000000` (the sign-bit clear that
        // is Abs; the mask is built by `vspltisw128 v127,-1` + `vslw128`), three `vaddfp`, then a
        // fifth `vandc` and `vcmpgtfp. v13, v8` against KVF_LEAN_PROP_MIN_LERP. The fifth Abs and
        // the compare together ARE rw::math::vpu::IsZero(v, tolerance) inlined
        // (vector3_operation.h:269 -- |lane| <= tolerance over xyz).
        // ⚠️ STATED CORRECTLY (round-2 MUST_FIX -- the earlier wording said the DWARF names
        // "three Abs calls and one IsZero rather than four", and a reader trusting it would delete
        // the wAxis term): the DWARF names Abs FOUR times and IsZero twice. The four SOURCE-level
        // Abs calls are the four deltas here (x/y/z/w -> the four vandc at 0x826319D8/DC/E0/E4) and
        // the FIFTH vandc @0x826319F4 is IsZero's OWN inlined Abs, which has no separate DWARF entry
        // -- so five vandc for four Abs plus one IsZero is exactly what Abs x4 + IsZero predicts.
        // Four Abs is right; do not "correct" it down.
        // GOTCHA 4: the branch is `mfocrf` + CR6 bit 2 ("no lane compared greater") + `bne <skip>`,
        // i.e. the whole body runs only when SOME lane exceeded the tolerance. ⚠️ The committed
        // IsZero spells that as `all lanes <= tolerance`, which is the SAME for every ordered value
        // and the OPPOSITE for a NaN lane -- see the gotcha-4 block in the file banner. Documented,
        // not silently absorbed.
        const bool lbNeedsRotating =
            !IsZero( Abs( lDeltaXAxis ) + Abs( lDeltaYAxis ) + Abs( lDeltaZAxis ) + Abs( lDeltaWAxis ),
                     KVF_LEAN_PROP_MIN_LERP.x );

        if ( lbNeedsRotating )
        {
            // :349. 0x82631A14..0x82631AD0. Each row's delta is scaled by
            // KVF_LEAN_PROP_LERP_SPEED and added back onto the cached row; the three BASIS rows are
            // then renormalised and the translation row is not.
            // The normalise is `vmsum3fp128` + `vrsqrtefp` + ONE Newton-Raphson refinement
            // (`vnmsubfp` / `vmaddfp` with the 1.0f and 0.5f that `vcfsx v13,1,0` / `vcfsx v13,1,1`
            // materialise) -- that estimate-plus-one-step pipeline is rw::math::vpu::NormalizeFast,
            // which the DWARF callee list names three times. The committed NormalizeFast
            // (vector3_operation.h:165) reduces to the exact Normalize on the host: numerically
            // tighter than the console's estimate, never a placeholder. Standing convention of
            // that vendor home.
            Matrix44Affine lLerpMatrix;
            lLerpMatrix.xAxis = NormalizeFast( lTransform.xAxis + lDeltaXAxis * KVF_LEAN_PROP_LERP_SPEED.x );
            lLerpMatrix.yAxis = NormalizeFast( lTransform.yAxis + lDeltaYAxis * KVF_LEAN_PROP_LERP_SPEED.x );
            lLerpMatrix.zAxis = NormalizeFast( lTransform.zAxis + lDeltaZAxis * KVF_LEAN_PROP_LERP_SPEED.x );
            lLerpMatrix.wAxis = lTransform.wAxis + lDeltaWAxis * KVF_LEAN_PROP_LERP_SPEED.x;

            // :392. 0x82631A98..0x82631C4C -- twelve `vspltw` + `vcmpeqfp.` self-equality tests
            // (three lanes x four rows, COUNTED: the first pair is 0x82631A98/0x82631AA4 and the
            // last is 0x82631C14/0x82631C1C) ANDed together at 0x82631C38..0x82631C40, i.e.
            // rw::math::vpu::IsValid(Matrix44Affine) (matrix44affine_operation.h:276) inlined lane
            // for lane. Branch at 0x82631C4C.
            if ( !IsValid( lLerpMatrix ) )
            {
                // 0x82631C50..0x82631CD4 -- the console builds the message in
                // CgsDev::Assert::gpcMessageBuffer with a stack StrStream:
                //     << "Transform: "        (the virtual char* sink, `bctrl` through vtable+4)
                //     << lLerpMatrix          (`bl sub_821F0FC0`)
                //     << "\n"
                //     FireAssert(gpcMessageBuffer, BrnPropManager.cpp, 392)
                // ⚠️ ONE UNAVOIDABLE SPELLING CHANGE, FLAGGED -- the same one, for the same reason,
                //    that PropManager_wQ2_04.cpp's RemoveProp added-this-frame block records for the Vector3
                //    printer. 0x821F0FC0 is
                //    `operator<<(StrStreamBase&, const Matrix44Affine&)`; its format string was
                //    dumped this round from off_82F31970 -> 0x820DBED8 and is exactly
                //    "\n(%f, %f, %f, %f)\n(%f, %f, %f, %f)\n(%f, %f, %f, %f)\n(%f, %f, %f, %f)\n"
                //    (scratchpad/waveQ2/probe_wQ2_05/ida_fmt.txt). That free operator has NO home
                //    anywhere in this tree, and this lander may not add one, so the sixteen lanes go
                //    through StrStreamBase::AppendFormat with the console's OWN format string --
                //    which is what that callee does internally. Byte-identical output, one call
                //    frame fewer. Replace with `<< lLerpMatrix` the moment the operator is homed
                //    (reported as a header request).
                char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
                CgsDev::StrStream lStrStream( lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE );

                lStrStream << "Transform: ";
                lStrStream.AppendFormat(
                    "\n(%f, %f, %f, %f)\n(%f, %f, %f, %f)\n(%f, %f, %f, %f)\n(%f, %f, %f, %f)\n",
                    lLerpMatrix.xAxis.x, lLerpMatrix.xAxis.y, lLerpMatrix.xAxis.z, lLerpMatrix.xAxis.w,
                    lLerpMatrix.yAxis.x, lLerpMatrix.yAxis.y, lLerpMatrix.yAxis.z, lLerpMatrix.yAxis.w,
                    lLerpMatrix.zAxis.x, lLerpMatrix.zAxis.y, lLerpMatrix.zAxis.z, lLerpMatrix.zAxis.w,
                    lLerpMatrix.wAxis.x, lLerpMatrix.wAxis.y, lLerpMatrix.wAxis.z, lLerpMatrix.wAxis.w );
                lStrStream << "\n";

                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert( lacMessageBuffer, __FILE__, __LINE__ ); // console: BrnPropManager.cpp:392
                CgsDev::Assert::EndAssert();
            }

            // :393. 0x82631CFC..0x82631DD8 -- the ORTHOGONALITY tripwire, which the DWARF names
            // `rw::math::vpu::IsOrthogonal3x3` and whose body it also names piecewise
            // (SelfSubtract, MagnitudeSquared x4, IsZero). That helper has NO home in this tree
            // (matrix44affine_operation.h has IsValid / OrthoNormalize3x3 but no predicate), and
            // this lander may not add one, so it is spelled inline here -- with a header request to
            // fold it into the vendor home. Nothing is invented: what follows is the emission.
            //
            // DECODED, instruction by instruction:
            //   0x82631CFC..0x82631D44  three vmrghw/vmrglw pairs that transpose the 3x3 into its
            //       COLUMNS: (r0.x,r1.x,r2.x), (r0.y,r1.y,r2.y), (r0.z,r1.z,r2.z).
            //   0x82631D48..0x82631D7C  three vmulfp + six vmaddfp: each column set is combined
            //       with the splatted components of ONE row, so lane i of each result is
            //       Dot(row_i, that row) -- the three rows of M * transpose(M).
            //   0x82631D80..0x82631D8C  each of those is subtracted from one basis vector:
            //       the row-0 product from gIVector (1,0,0), the row-1 product from
            //       0x82181510 (0,1,0), the row-2 product from 0x82181520 (0,0,1). All three
            //       constants were BYTE-DUMPED this round, they are not inferred.
            //   0x82631D90..0x82631DB0  three vmsum3fp128 (== MagnitudeSquared) reassembled into
            //       one vector by `vperm` with unk_82CDA350 plus `vrlimi128 ..,2,0`.
            //   0x82631DB4..0x82631DC4  a FOURTH vmsum3fp128 over that vector, `vandc` (abs) and
            //       `vcmpgtfp` against KVF_LEAN_PROP_ORTHOGONAL_TOLERANCE -- i.e. IsZero(.., tol)
            //       inlined again, on a broadcast scalar.
            const Vector3 lGramErrorX = Vector3{ Dot( lLerpMatrix.xAxis, lLerpMatrix.xAxis ),
                                                 Dot( lLerpMatrix.yAxis, lLerpMatrix.xAxis ),
                                                 Dot( lLerpMatrix.zAxis, lLerpMatrix.xAxis ),
                                                 0.0f } - K_RW_I_VECTOR;
            const Vector3 lGramErrorY = Vector3{ Dot( lLerpMatrix.xAxis, lLerpMatrix.yAxis ),
                                                 Dot( lLerpMatrix.yAxis, lLerpMatrix.yAxis ),
                                                 Dot( lLerpMatrix.zAxis, lLerpMatrix.yAxis ),
                                                 0.0f } - K_RW_J_VECTOR;
            const Vector3 lGramErrorZ = Vector3{ Dot( lLerpMatrix.xAxis, lLerpMatrix.zAxis ),
                                                 Dot( lLerpMatrix.yAxis, lLerpMatrix.zAxis ),
                                                 Dot( lLerpMatrix.zAxis, lLerpMatrix.zAxis ),
                                                 0.0f } - K_RW_K_VECTOR;

            const Vector3 lOrthogonalError = Vector3{ MagnitudeSquared( lGramErrorX ),
                                                      MagnitudeSquared( lGramErrorY ),
                                                      MagnitudeSquared( lGramErrorZ ),
                                                      0.0f };
            const f32 lfOrthogonalError = MagnitudeSquared( lOrthogonalError );

            if ( !IsZero( Vector3{ lfOrthogonalError, lfOrthogonalError, lfOrthogonalError, 0.0f },
                          KVF_LEAN_PROP_ORTHOGONAL_TOLERANCE.x ) )
            {
                // 0x82631DDC..0x82631E60 -- the same StrStream message and the same un-homed matrix
                // printer as the :392 arm; see that block's flag.
                char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
                CgsDev::StrStream lStrStream( lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE );

                lStrStream << "Transform: ";
                lStrStream.AppendFormat(
                    "\n(%f, %f, %f, %f)\n(%f, %f, %f, %f)\n(%f, %f, %f, %f)\n(%f, %f, %f, %f)\n",
                    lLerpMatrix.xAxis.x, lLerpMatrix.xAxis.y, lLerpMatrix.xAxis.z, lLerpMatrix.xAxis.w,
                    lLerpMatrix.yAxis.x, lLerpMatrix.yAxis.y, lLerpMatrix.yAxis.z, lLerpMatrix.yAxis.w,
                    lLerpMatrix.zAxis.x, lLerpMatrix.zAxis.y, lLerpMatrix.zAxis.z, lLerpMatrix.zAxis.w,
                    lLerpMatrix.wAxis.x, lLerpMatrix.wAxis.y, lLerpMatrix.wAxis.z, lLerpMatrix.wAxis.w );
                lStrStream << "\n";

                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert( lacMessageBuffer, __FILE__, __LINE__ ); // console: BrnPropManager.cpp:393
                CgsDev::Assert::EndAssert();
            }

            // 0x82631E90..0x82631EA4 -- four stvx128 back through r31 ==
            // &maCurrentJointTransforms[liLeaningPropIndex]. This is the ONLY writer of that array
            // in this body.
            maCurrentJointTransforms[ liLeaningPropIndex ] = lLerpMatrix;

            // :350. 0x82631EA8..0x82631EE4 -- the UpdatePropEvent stack record. Offset map, with
            // the record base at var_120 (frame 0x160) and every member DECLARED at
            // BrnPropEvents.h:15-24 (⚠️ DECLARED, not "pinned" -- round-2 NIT: there is no
            // static_assert for UpdatePropEvent anywhere; BrnPropEvents.h's only two pins are
            // RemovePhysicalPropEvent :75 and RemovePhysicalPartEvent :90. The member ORDER at
            // :17-23 is what yields these offsets, and this file has now measured all seven off the
            // asm, so an offsetof/sizeof pin block for them is worth a header request):
            //   +0x00..+0x30 mTransform         four stvx128
            //   +0x40 mLinearVelocity           `stvx128 v11` (v11 == vspltisw 0)
            //   +0x50 mAngularVelocity          `stvx128 v11`
            //   +0x60 mEntityId                 `stw`  from `lwz 0x60(r28)`
            //   +0x64 miPhysicsSlot             `sth r27`  == liPropIndex
            //   +0x66 miTypeId                  `sth 0`
            //   +0x68 mbFrozen                  `stb 0`
            // Nothing writes +0x69..+0x6F, so that tail ships as stack garbage, as on the console.
            UpdatePropEvent lUpdateEvent;
            lUpdateEvent.mTransform = lLerpMatrix;
            lUpdateEvent.mLinearVelocity.SetZero();
            lUpdateEvent.mAngularVelocity.SetZero();
            lUpdateEvent.mEntityId     = lpProp->GetEntityId();
            lUpdateEvent.miPhysicsSlot = static_cast<s16>( liPropIndex );
            lUpdateEvent.miTypeId      = 0;
            lUpdateEvent.mbFrozen      = false;

            // 0x82631E9C `addi r3, r30, 0x5E10` == &mUpdatedJointedProps (the
            // EventQueue<UpdatePropEvent,15>, CONSOLE +0x5E10; reached by name) then
            // 0x82631EE8 `bl BaseEventQueue<UpdatePropEvent>::AddEvent`.
            // ⚠️ mUpdatedJointedProps, NOT mUpdatedProps (+0x680): measured, the base register is
            //    r30 + 0x5E10 and nothing in this body touches +0x680.
            mUpdatedJointedProps.AddEvent( lUpdateEvent );
        }
    }
}

}
}

// ============================================================================
// FOLDED FROM PropManager_wQ2_06.cpp (wave Q2) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================
// =================================================================================================
// GameSource/Physics/PropManager/PropManager_wQ2_06.cpp
//
// Partfile of the TU GameSource/Unity/../Physics/PropManager/BrnPropManager.cpp
// (breakable-props wave Q ROUND 2, lander 06, 2026-08-18). Folds back into BrnPropManager.cpp.
//
// THREE FUNCTIONS WERE ASSIGNED. ALL THREE ARE NOW BODIED HERE:
//
//   * PropManager::Prepare                       @0x8260EE18 (126 insns)  -- BODIED BELOW.
//   * PropManager::ProcessAddPropInstanceEvents  @0x82632108 (515 insns)  -- BODIED BELOW.
//   * PropManager::OutputUpdatedProps            @0x82627EC8 (14 insns)   -- ⭐ LANDED 2026-08-19
//         (wave Q6 cluster A2), at the END of this file. It was parked here from 2026-08-18 with a
//         complete body at scratchpad/waveQ2/parked/PropManager_06_OutputUpdatedProps.cpp; that
//         body was re-verified store-for-store against the raw `assembly` array of
//         .ida-exports/BURNOUT_X360_ARTIST.XEX/0x82627EC8.json this wave and landed unchanged
//         except for the added [DIAG] block. See its own banner further down for the decode.
//
//   ⛔ THE BLOCKER THAT KEPT IT PARKED, and its disposition -- BOTH MEASURED on 2026-08-19, an
//      hour apart, so this note records a transition and not a belief. The body's first statement
//      needs PhysicsModuleIO::OutputBuffer::GetPropManagerOutputInterface() to return the real
//      BrnPhysics::Props::PropOutputInterface:
//        * BEFORE: BrnPhysicsModuleIO.h read `struct PropOutputInterfaceStorage
//          { unsigned char maBytes[1]; };` and the probe
//          scratchpad/waveQ6/probe_outprop/probe_outprop.cpp reported STATUS=fail with EXACTLY one
//          diagnostic -- C2440, "PropOutputInterfaceStorage* cannot be converted to
//          BrnPhysics::Props::PropOutputInterface*", at the accessor's return.
//        * AFTER: wave Q6 cluster A1 (owner `seat`) promoted BrnPhysicsModuleIO.h:112 to
//          `typedef Props::PropOutputInterface PropOutputInterfaceStorage;`, DELETED the
//          maDeformationPad that baked sizeof(the 1-byte placeholder), and added the missing
//          `mPropManagerOutputInterface.Construct()` leg to OutputBuffer::Construct
//          (BrnPhysicsModuleIO_OutputBuffer.cpp:151, X360 0x825ABBF8). The same probe now reports
//          STATUS=pass, and so does this file.
//      That Construct leg is not a nicety: this function is what first DRIVES the interface's
//      event queues, and an EventQueue whose Construct never ran has mpEvents == NULL and fires
//      "Not Constructed" on its first AddEvent -- the recurring boot killer (AGENTS.md gotcha 14).
//      If this file ever stops compiling, the line to look at is BrnPhysicsModuleIO.h:112 and
//      nothing else. Do NOT reinterpret_cast over a placeholder to "fix" it -- that is the type
//      fork AGENTS.md bans, and a 1-byte stand-in also makes the enclosing OutputBuffer's own
//      layout wrong.
//
// ⚠️ A PRIOR, INTERRUPTED RUN of this same lander wrote the banner that used to sit here. It
//    claimed ProcessAddPropInstanceEvents was blocked on
//    `InSceneUpdateInterface::AddCachedObject(s32, f32)` being undeclared, and pointed at two
//    parked files that were NEVER WRITTEN. Re-derived from the raw asm this round (AGENTS.md
//    gotcha 10): the blocker was misidentified. `AddCachedObject` is the SOURCE-level helper the
//    DecFIGS DWARF names, but the ARTIST build INLINES it -- the emission at 0x826325E0..0x82632604
//    is a two-word event fill followed by a direct
//    `bl CgsModule::BaseEventQueue<...InEventAddToCache>::AddEvent`, with the queue pointer formed
//    as `addis r11, r5, 0xC ; addi r11, r11, 0x4930` == lpSceneInput + 0xC4930 ==
//    InSceneUpdateInterface::mAddToCacheQueue (CgsSceneManagerIO_SceneUpdate.h pins that exact
//    console offset on that exact member). That inlined form is spelled below; both the queue
//    member and EventQueue::AddEvent are public and homed, so nothing is blocked. The undeclared
//    `AddCachedObject` helper is filed as a header request, not a blocker.
//
// LINK-LEVEL DUPLICATES -- `cl /c` cannot see these. Both entries this banner used to track are
//    now closed: PropManager::Prepare's link stub was retired 2026-08-18 with the wave-Q4
//    PropManager mount, and PropManager::OutputUpdatedProps's inert one-shot conductor gate has
//    been deleted, so the body below is that symbol's only definition. This file is mounted and it
//    links, which is the proof.
//
// ✅ EVERY CALLEE REACHED FROM THIS FILE HAS A BODY IN THE TREE (round-2 MUST_FIX, applied;
//    re-grepped 2026-08-18). The earlier banner said `PropManager::AddPropToSim` @0x826274D8 was
//    "declared with NO body anywhere in b5-decomp/src or b5-decomp/vendor" -- that was FALSE the day
//    it was written: AddPropToSim is fully bodied by the sibling wave-Q round-2 lander at
//    b5-decomp/src/GameSource/Physics/PropManager/PropManager_wQ2_05.cpp, with a signature matching
//    this file's call site parameter for parameter. Do NOT add a trap stub for it beside that body
//    (an LNK2005 `cl /c` cannot see), and do not hold this file's mount for it.
//    Re-grepped 2026-08-19 for the newly landed body's own two callees, both REAL and both
//    mounted:
//      * PhysicsModuleIO::OutputBuffer::GetPropManagerOutputInterface -- bodied at
//        BrnPhysicsModuleIO_OutputBuffer.cpp:188 (write overload, X360 0x825C0DC8), MOUNTED.
//      * Props::PropOutputInterface::AppendUpdatedProps -- bodied in
//        SharedIO/BrnPropOutputInterface.cpp, which IS mounted; that closed the one link hole
//        this file had. Measured with dumpbin, not asserted: this file's .obj UNDEF for that
//        symbol and the definition in BrnPropOutputInterface.obj are the same decorated name.
//    Every OTHER callee was checked and is real: PropInstance::SetTransform / ::SetLinearVelocity
//    (BrnPropInstance.cpp:24/:56), CgsDev::DebugComponent::Register (CgsDebugComponent.cpp:66),
//    rw::BaseResourceDescriptor::BaseResourceDescriptor (vendor .../BaseResourceDescriptor.cpp:35),
//    rw::IResourceAllocator::DoAllocate (vendor rwcore_alloc.cpp:12), and the rest are header
//    inlines (the BitArray<15> methods, the BaseEventQueue methods, every PropInstance accessor
//    used here, PropPhysicsDataHeader::GetType, the PropTypeData accessors,
//    CgsResource::ResourcePtr<T>::operator->, rw::math::vpu::TransformPoint / Vector3::SetZero).
//
// GROUNDING. Every statement below was re-derived THIS ROUND from the raw `assembly` arrays of
// .ida-exports/BURNOUT_X360_ARTIST.XEX/0x8260EE18.json and /0x82632108.json. The Hex-Rays
// pseudocode in those exports was read only as a cross-check and is wrong in the usual ways (for
// Prepare it prints the two virtual allocator calls as `MEMORY[0x6A0]` / `MEMORY[0x790]` and
// mislabels the parameter list). Every instruction count stated in this file is a count I made of
// the 0xXXXXXXXX-prefixed lines in those listings: Prepare is 126 ((0x8260F00C - 0x8260EE18)/4 + 1)
// and ProcessAddPropInstanceEvents is 515 ((0x82632910 - 0x82632108)/4 + 1).
//
// ⚠️ EVERY ASSERT MESSAGE QUOTED IN THIS FILE WAS READ OUT OF THE IMAGE, not reconstructed from the
// IDA comment (which truncates at ~40 characters). Dumped with headless IDA 9.3 against
// "IDA Files/BURNOUT_X360_ARTIST.XEX.i64" -- e.g. aLipropinstance_0 @0x8209BB60 ==
// "liPropInstanceIndex < static_cast<int32_t>(KU_MAX_PHYSICAL_PROPS)", aLpaddpropevent @0x8209BBA8
// == "lpAddPropEvent->miPropTypeId < static_cast<int32_t>(KU_MAX_PROP_TYPES)". Note the binary's
// own text has NO space inside the static_cast<> parentheses; that spelling is preserved verbatim.
//
// ⚠️ NO CONSOLE LITERAL IS USED AS A HOST VALUE (AGENTS.md gotcha 1). The two allocation sizes the
// console spells as the immediates 0x690 and 0x780 are `KU_MAX_PHYSICAL_PROPS * sizeof(PropInstance)`
// and `KU_MAX_PHYSICAL_PROP_PARTS * sizeof(PropPartInstance)` here; both structs are pointer-free
// so the host sizes happen to agree (112 and 64), and the static_asserts below PIN that agreement
// instead of leaving it as a coincidence a later member addition would silently break.
// The one bare immediate kept is the descriptor ALIGNMENT 0x10, which is an alignment and not a
// size -- it is 16 on both targets because both records lead with a 16-byte SIMD member.
//
// GOTCHA 3 / GOTCHA 4 SITES: NONE IN EITHER BODY, and that is a measurement, not an assumption.
//   * Prepare's emission contains no float or vector register at all (nothing touches f1..f13 or
//     v0..v127 anywhere in the 126 instructions) and no floating-point compare -- so there is no
//     PPC float-arg GPR-skip and no NaN-polarity decision.
//   * ProcessAddPropInstanceEvents takes no float or vector PARAMETER (r3..r6 carry this + the
//     three pointers; v127 is a locally-materialised zero, not an argument), and its only
//     floating-point traffic is one `lfs f0, 0x44(lpType)` / `stfs f0` pair -- a copy, with no
//     fcmpu, no fsel and no vcmp anywhere in the 515 instructions. Its `bge`/`ble` branches are
//     all on integer condition registers (cmpwi/cmplwi/cmpldi), where gotcha 4 does not apply.
// GOTCHA 2 SITE (a store through obj+0xNNNN that targets an embedded sub-object): checked and
// none. Every this-relative store in both bodies lands on a member PropManager declares directly
// (mUsedProps/mUsedParts/mpaPropInstances/mpaPartInstances/maPropJointPositions/
// maLastJointRotation/mauPropIndexForJoint/maCurrentJointTransforms/mUsedPropJoints/
// mBreakPropJoints); the only embedded sub-object, mDebugComponent, is at +0x00 and is reached
// exactly once, by name, in Prepare.
// =================================================================================================


namespace BrnPhysics
{
namespace Props
{

// =================================================================================================
// BrnPhysics::Props::PropManager::Prepare @ 0x8260EE18  (126 instructions)
//
// Stage 5 (E_PREPARESTAGE_PROPMANAGER) of BrnPhysics::PhysicsModule::Prepare @0x825ADB68 -- its
// only caller (xrefs_to is exactly {0x825ADB68}). Clears the two slot bit-sets, registers the debug
// component, and carves the two instance arrays out of the physics resource allocator.
//
// ⚠️ THE ADDRESS. The class declaration used to carry "@0x82C08ED0" and the round-2 shared-header
//    owner corrected it; I re-verified rather than inherited the correction. 0x82C08ED0 is
//    `__savegprlr_22`, the PPC register-save runtime helper -- it appears in THIS function's
//    xrefs_from precisely because `bl __savegprlr_22` is its second instruction (0x8260EE1C).
//    0x8260EE18 is the real body: its own export is named
//    BrnPhysics::Props::PropManager::Prepare and its xrefs_to is exactly PhysicsModule::Prepare.
//
// ---- REGISTER MAP, read off the prologue (0x8260EE18..0x8260EE2C) --------------------------------
//     r3 = this            -> r30
//     r4 = lpPhysicsAllocator -> r28
//     r29 = 0 (the shared zero), r25 = 1, r24 = 0x10 (the descriptor alignment, hoisted)
// No third parameter; nothing reads r5 before it is written.
//
// ---- PARAMETER TYPE: a KNOWN, DELIBERATE DELTA --------------------------------------------------
// The DecFIGS DWARF declares `void PropManager::Prepare(rw::LinearResourceAllocator* lpPhysicsAllocator)`
// (dwarfdump .../BrnPropManager.cpp, source :174) and the one real call site,
// BrnPhysicsModule.cpp:369, already hands over a rw::LinearResourceAllocator*. The committed class
// declaration takes the base rw::IResourceAllocator*, because narrowing it is a PAIR edit -- the
// inert stub at WorldLinkStubs.cpp:516 spells `struct rw::IResourceAllocator *` and stops compiling
// the moment the declaration alone changes. That file is not this partfile's to edit, so the
// definition below matches the COMMITTED declaration and the narrowing stays conductor item C2.
// It is behaviour-neutral either way: the body only ever reaches the allocator through the virtual
// DoAllocate slot, which both types share.
//
// ---- WHAT THE BODY DOES, instruction by instruction ---------------------------------------------
//   0x8260EE30  std r29, 0x80(r30)      mUsedProps  = 0     (BitArray<15>, ONE 64-bit field)
//   0x8260EE34  std r29, 0x90(r30)      mUsedParts  = 0     (BitArray<30>, ONE 64-bit field)
//   0x8260EE38  bl DebugComponent::Register   with r3 STILL == this; mDebugComponent is at +0x00
//                                       (PropManager::Construct @0x82627390 calls
//                                        PropDebugComponent::Construct with r3 == r4 == this), so
//                                       the console really is calling mDebugComponent.Register().
//   0x8260EE44..0x8260EE98  the FIRST log line, gated on `gxMessageFilterFlags & 1`
//                                       (`ld` + `clrldi ...,63` == the 64-bit flag word & 1):
//                                       op<<(const char*) through the stream vtable slot +4, then
//                                       the non-virtual op<<(s32) with r4 = 0x690, then
//                                       op<<(const char*) " bytes\n".
//   0x8260EEA0..0x8260EEBC  the descriptor's identity fill: r10 counts 4,3,2,1,0 (`bge` on the
//                                       POST-decrement value, so FIVE iterations) writing the pair
//                                       {size = 0, alignment = 1} at an 8-byte stride == the
//                                       rw::BaseResourceDescriptor default constructor run over a
//                                       FIVE-entry descriptor. See the <5>-vs-<4> note below.
//   0x8260EED0..0x8260EEEC  entry[0] = { size = 0x690, alignment = 0x10 }  (assembled as two
//                                       stw into a scratch qword then one std into the descriptor)
//   0x8260EEF0..0x8260EEF8  bctrl through allocator vtable slot +0x10 with r3 = &<sret Resource>,
//                                       r4 = allocator, r5 = &descriptor, r6 = "PropInstances"
//                                       -- the (const ResourceDescriptor&, const char*) -> Resource
//                                       shape, i.e. DoAllocate.
//   0x8260EEFC..0x8260EF04  mpaPropInstances (+0x7C) = the Resource's FIRST pool pointer; the
//                                       null test is taken from the same register BEFORE the store
//                                       (`cmplwi` at 0x8260EF00, `stw` at 0x8260EF04), so the store
//                                       happens unconditionally and the assert follows it.
//   0x8260EF14..0x8260EF2C  assert "mpaPropInstances != NULL", BrnPropManager.cpp:205 (0xCD)
//   0x8260EF30..0x8260F000  the identical second half for the parts array: "PropManager: array of
//                                       prop parts required " / 0x780 / "PropPartInstances" /
//                                       mpaPartInstances (+0x8C) / assert :223 (0xDF)
//   0x8260F004  li r3, 1                return true, unconditionally -- there is no other write to
//                                       r3 after the last bctrl, so no failure path exists.
//
// ---- MEASURED NEGATIVES, written down so nothing gets invented later ----------------------------
//  * Prepare stores to EXACTLY FOUR `this` offsets and no others -- +0x80, +0x90, +0x7C, +0x8C. I
//    re-grepped all 126 instructions for `st*` with an r30 base: those four and nothing else. In
//    particular it does NOT write +0x88 muNumberOfPropInstances or +0x98 muNumberOfPartInstances,
//    which is why the round-1 note that Release()/Destruct() loop over an unwritten counter stands.
//    Nothing is invented here to "fix" that: the console does not set them, so neither do we.
//  * The DecFIGS scope for this function (dwarfdump .../BrnPropManager.cpp, the Prepare block) lists
//    BitArray<15u>::UnSetAll, BitArray<30u>::UnSetAll, StrStreamBase::operator<<,
//    rw::ResourceDescriptor::ResourceDescriptor x2, rw::IResourceAllocator::Allocate x2 and the two
//    ~ResourceDescriptor -- and does NOT list DebugComponent::Register. The ARTIST asm has the `bl`
//    explicitly at 0x8260EE38, so the call is real in the SHIPPED build; the DWARF gap is a
//    DecFIGS/ARTIST merge-window delta, not evidence against it. Stated, not smoothed over.
//  * The DWARF spells the allocator method `Allocate`; the tree's committed name for that vtable
//    slot -- the one every sibling Prepare in this engine calls (CgsTriangleCacheManager.cpp:77,
//    CgsSceneManagerModule.cpp:150, CgsCachedTriangleList.cpp:121) -- is `DoAllocate`. Same slot,
//    same (descriptor, name) -> Resource signature; the tree name is used, not a second one forked.
//
// ---- THE <5>-vs-<4> RESOURCE DESCRIPTOR ---------------------------------------------------------
// The console's rw::ResourceDescriptor is rw::BaseResourceDescriptors<5> (40 bytes -- hence the FIVE
// identity pairs the loop writes); the PC rwcore this tree is built against uses <4>
// (rwcore_structs.h:76, and the divergence is documented in that header's own banner at :30). The
// committed sibling CgsSceneManager::TriangleCacheManager::Prepare hit exactly this and resolved it
// by declaring the <5> form and reinterpret_cast-ing it down to DoAllocate's <4> parameter; the same
// resolution is used here so the two prop descriptors have the console's shape. The cast is safe in
// this direction (the callee reads four of the five entries) and only entry 0 is ever non-identity.
// =================================================================================================
bool PropManager::Prepare( rw::IResourceAllocator* lpPhysicsAllocator )
{
    // 0x8260EE30 / 0x8260EE34. Two 64-bit zero stores == one UnSetAll each (both arrays are a
    // single 64-bit field: ceil(15/64) == ceil(30/64) == 1). DWARF names both calls.
    mUsedProps.UnSetAll();
    mUsedParts.UnSetAll();

    // 0x8260EE38. r3 is untouched between the prologue and this `bl`, and mDebugComponent sits at
    // +0x00, so `this` and `&mDebugComponent` are the same address -- the console is calling the
    // component's own Register(). (See the "measured negatives" note above about the DWARF gap.)
    mDebugComponent.Register();

    // ---------------------------------------------------------------------------------------
    // The prop instance array. Console immediate 0x690 == 15 * 112; expressed here as the host
    // arithmetic so a future member addition moves the allocation with the struct.
    // ---------------------------------------------------------------------------------------
    static_assert( sizeof( PropInstance ) == 112,
                   "PropInstance host stride must stay 112 == the console's 0x690/15" );

    const s32 liPropBufferSize =
        static_cast<s32>( KU_MAX_PHYSICAL_PROPS * sizeof( PropInstance ) );   // console `li r4, 0x690`

    // 0x8260EE44..0x8260EE98. The whole three-term chain is inside the filter test; the console
    // does not even load gpDebugPrint when the bit is clear.
    if ( CgsDev::Message::gxMessageFilterFlags & 1 )
    {
        *CgsDev::Log::gpDebugPrint << "PropManager: array of props required "
                                   << liPropBufferSize
                                   << " bytes\n";
    }

    // 0x8260EEA0..0x8260EEEC. Default construction gives every entry the identity {0, 1} pair the
    // console's five-iteration loop writes; only entry 0 is then filled in.
    rw::BaseResourceDescriptors<5> lPropsResDesc;
    lPropsResDesc.m_baseResourceDescriptors[0].m_size      = static_cast<u32>( liPropBufferSize );
    lPropsResDesc.m_baseResourceDescriptors[0].m_alignment = 0x10;   // ALIGNMENT, not a size: 16 on both targets

    // 0x8260EEF0..0x8260EEF8. Virtual dispatch through the allocator; the Resource comes back by
    // value (the console passes the sret pointer in r3) and pool 0 is the array base.
    rw::Resource lPropsRes = lpPhysicsAllocator->DoAllocate(
        reinterpret_cast<const rw::ResourceDescriptor&>( lPropsResDesc ), "PropInstances" );
    mpaPropInstances = static_cast<PropInstance*>( lPropsRes.m_baseResources[0] );

    CGS_ASSERT( mpaPropInstances != NULL, "mpaPropInstances != NULL" );   // BrnPropManager.cpp:205

    // ---------------------------------------------------------------------------------------
    // The prop-part instance array. Console immediate 0x780 == 30 * 64.
    // ---------------------------------------------------------------------------------------
    static_assert( sizeof( PropPartInstance ) == 64,
                   "PropPartInstance host stride must stay 64 == the console's 0x780/30" );

    const s32 liPartBufferSize =
        static_cast<s32>( KU_MAX_PHYSICAL_PROP_PARTS * sizeof( PropPartInstance ) );  // console `li r4, 0x780`

    if ( CgsDev::Message::gxMessageFilterFlags & 1 )
    {
        *CgsDev::Log::gpDebugPrint << "PropManager: array of prop parts required "
                                   << liPartBufferSize
                                   << " bytes\n";
    }

    rw::BaseResourceDescriptors<5> lPropPartsResDesc;
    lPropPartsResDesc.m_baseResourceDescriptors[0].m_size      = static_cast<u32>( liPartBufferSize );
    lPropPartsResDesc.m_baseResourceDescriptors[0].m_alignment = 0x10;

    rw::Resource lPropPartsRes = lpPhysicsAllocator->DoAllocate(
        reinterpret_cast<const rw::ResourceDescriptor&>( lPropPartsResDesc ), "PropPartInstances" );
    mpaPartInstances = static_cast<PropPartInstance*>( lPropPartsRes.m_baseResources[0] );

    CGS_ASSERT( mpaPartInstances != NULL, "mpaPartInstances != NULL" );   // BrnPropManager.cpp:223

    // 0x8260F004  li r3, 1 -- unconditional.
    return true;
}

// =================================================================================================
// BrnPhysics::Props::PropManager::ProcessAddPropInstanceEvents @ 0x82632108  (515 instructions)
//
// Drain PropInputInterface::mAddPropQueue: every AddPhysicalPropEvent promotes one prop entity to a
// live PropInstance, registers its collision sphere with the scene's triangle cache, allocates a
// joint slot when the prop is a LEANING (lamppost-class) prop, and finally hands the whole thing to
// AddPropToSim, which posts the InAddRigidBody. Its only caller is
// ProcessInputsPreScene @0x8263AF30 (xrefs_to is exactly that one address).
//
// ---- REGISTER MAP, read off the prologue (0x82632108..0x82632134) -------------------------------
//     r3 = this               -> r21
//     r4 = lpInput            -> spilled at arg_1C and used DIRECTLY as the queue pointer:
//                                `mr r3,r4 ; lwz r11,8(r3)` == GetLength() at queue+8, i.e.
//                                mAddPropQueue sits at PropInputInterface+0 (the header pins it).
//     r5 = lpSceneInput       -> `addis r11,r5,0xC ; addi r11,r11,0x4930` == +0xC4930 ==
//                                InSceneUpdateInterface::mAddToCacheQueue
//     r6 = lpSimModuleInputBuffer -> spilled at arg_2C, reloaded only to feed AddPropToSim's r10
// No fourth parameter; nothing reads r7+ before it is written.
//
// ---- THE DecFIGS SCOPE (dwarfdump .../BrnPropManager.cpp, source :470) --------------------------
// Locals, verbatim, with their source lines -- these names are used below rather than invented:
//   int32_t luIndex :472 - const PropInputInterface::AddPhysicalPropEventQueue* lpAddPropEventQueue
//   :473 - const AddPhysicalPropEvent* lpAddPropEvent :474 - int32_t liPropInstanceIndex :475 -
//   PropInstance* lpProp :476 - RigidBodyId lRigidBodyId :477 - const PropTypeData* lpType :478 -
//   bool lbStatic :514 - bool lbJointed :515 - int32_t liLampostJointIndex :521
//
// ⚠️ `RigidBodyId lRigidBodyId` (:477) IS DEAD IN THIS BUILD AND NOTHING IS SYNTHESISED FOR IT.
//    Negative evidence, measured: the export's xrefs_from lists exactly twelve targets --
//    __savegprlr_14, BaseEventQueue<AddPhysicalPropEvent>::GetEvent (sub_825BC520),
//    BeginAssert/FireAssert/EndAssert, BasePriorityQueue::Clear, StrStreamBase::AppendFormat,
//    PropInstance::SetTransform, PropInstance::SetLinearVelocity,
//    ResourcePtr<PropPhysicsDataHeader>::operator-> (0x822868E0, the truncated
//    "BrnPhysics::Props::Prop" symbol), BaseEventQueue<InEventAddToCache>::AddEvent, and
//    PropManager::AddPropToSim. There is no RigidBodyId::Set, no GetAddRigidBodyQueue, and no
//    register in the 515 instructions ever holds one -- the rigid body is built entirely inside
//    AddPropToSim. This is the same dead-local pattern round 1 recorded for the sibling
//    ProcessRemovePartInstanceEvents; written down so a later DWARF sweep does not "restore" it.
//
// ---- SOURCE LINES THE ASSERTS BAKE (file aDP4B5MainBurno_225 == this .cpp) ----------------------
//   0x1EE == 494  "liPropInstanceIndex != -1"
//   0x1EF == 495  "!mUsedProps.IsBitSet( liPropInstanceIndex )"
//   0x1F3 == 499  "liPropInstanceIndex < static_cast<int32_t>(KU_MAX_PHYSICAL_PROPS)"
//   0x1FC == 508  "lpAddPropEvent->miPropTypeId < static_cast<int32_t>(KU_MAX_PROP_TYPES)"
//   0x20A == 522  "liLampostJointIndex != -1"
//
// ---- FOUR ASSERT BLOCKS ARE DELIBERATELY NOT REPRODUCED -----------------------------------------
// The emission also carries four complete StrStream assert blocks whose baked FILE is
// aDP4B5MainBurno_51 == ".../gameshared/gameclasses/containers/./CgsBitArray.h", at its lines
// 0xCB == 203 ("invalid index : " << i << " < " << 15), 0xDE == 222 ("Index: " << i <<
// ", Number of bits: " << 15) twice, and 0xF1 == 241 ("luIndex < NUMBITS"). Those are the INLINED
// bodies of BitArray<15>::IsBitSet / ::SetBit / ::UnSetBit -- the callee's tripwires, not this
// function's source statements -- and the tree's CgsBitArray.h states in its own banner (:15-17)
// that it deliberately carries no assert-system dependency. Their bound (15) is identical to the
// :499 assert this body does reproduce, so nothing is lost. Named here so the ~150-instruction
// gaps at 0x82632280..0x8263237C, 0x826323D4..0x826324D0 and 0x82632770..0x8263287C are not read
// as dropped branches.
//
// ---- WHAT THE BODY DOES, in emission order ------------------------------------------------------
//   0x8263212C  lwz r11,8(queue) ; cmpwi 0 ; ble -> exit         GetLength() <= 0, SIGNED
//   0x82632144  vspltisw128 v127,0                               ONE zero vector, hoisted OUT of
//                                                                the loop (after the early-out)
//   0x82632244  bl sub_825BC520                                  GetEvent(luIndex); stride-80
//                                                                (`slwi r11,r30,2 ; add ; slwi 4`)
//   0x8263224C  lhz 0x4A / extsh                                 liPropInstanceIndex = miSlot (s16)
//   0x82632254..0x826323CC   the :494 and :495 asserts
//   0x826324D0  ldx/or/stdx on this+0x80                         mUsedProps.SetBit(...)
//   0x826324D8..0x82632500   the :499 assert (cmpwi 0xF -- SIGNED)
//   0x82632504  lwz 0x7C(this) ; mulli r22,0x70                  &mpaPropInstances[index];
//                                                                0x70 == 112 is the CONSOLE stride
//                                                                and is NOT used here -- the host
//                                                                sizeof does the striding.
//   0x8263251C..0x82632560   the instance seed (see the block comment inside the body)
//   0x82632564..0x82632588   the :508 assert
//   0x8263258C..0x826325F8   ResourcePtr operator-> + the inlined PropPhysicsDataHeader::GetType
//                            (its own two asserts bake BrnPropPhysicsDataHeader.h:173/:174, which
//                            the tree's header-inline GetType reproduces for free)
//   0x826325E0..0x82632604   the inlined AddCachedObject -> BaseEventQueue<InEventAddToCache>::
//                            AddEvent. MEASURED: the radius is copied straight through
//                            (`lfs f0,0x44(lpType)` -> `stfs f0`), with NO cache padding added --
//                            unlike UpdateTriangleCache, which adds KF_TRIANGLE_CACHE_PADDING.
//                            There is no `fadds` anywhere between 0x826325FC and 0x82632604.
//   0x82632608..0x8263261C   meState -> lbJointed (== 2) / lbStatic (== 1)
//   0x82632620..0x82632894   the joint block (taken only when lbJointed)
//   0x82632898..0x826328E4   SetIsStatic / SetExtraComOffsetFlag / AddPropToSim
//   0x826328E8..0x82632900   ++luIndex ; RE-READ the length ; cmpw ; blt -> loop
//
// ⚠️ THE LOOP BOUND IS RE-READ EVERY ITERATION, and that is a real difference from the sibling
//    ProcessAddPartInstanceEvents (which hoists it). 0x826328EC/0x826328F4 reload the spilled
//    queue pointer and `lwz r10,8(r10)` fresh, then `cmpw` (SIGNED) against the incremented
//    counter. Written as `luIndex < lpAddPropEventQueue->GetLength()` in the for-condition so the
//    re-read survives; do not "optimise" it into a hoisted local.
//
// ---- CONSOLE LITERALS THAT ARE COMMENTS ONLY (AGENTS.md gotcha 1) --------------------------------
// 0xC4930 (the mAddToCacheQueue byte offset), 0x70 (the PropInstance stride), 0x40/0x44/0x48/0x4A/
// 0x4C (the AddPhysicalPropEvent field offsets), 0x60/0x64/0x68/0x6C..0x6F (the PropInstance field
// offsets), 0x2A0/0x2B0/0xC0/0x1B0/0x670/0x678 (the joint-block member offsets) and the *8/*16/*64
// strides all appear ONLY in comments. Every one of them is reached by member name below, so the
// x64 pointer widening cannot desync any of them. The only NUMBERS in the code are the two bounds
// constants the assert strings themselves name (KU_MAX_PHYSICAL_PROPS, KU_MAX_PROP_TYPES) and
// KI_PROP_CACHE_START_INDEX -- all three are named constants, not offsets.
// =================================================================================================

// (fold: an identical definition of KI_PROP_CACHE_START_INDEX was dropped here -- this TU defines it once, above)

void PropManager::ProcessAddPropInstanceEvents(
    const PropInputInterface*                                lpInput,
    CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpSceneInput,
    CgsPhysics::PhysicsSimulationIO::InputBuffer*            lpSimModuleInputBuffer )
{
    const PropInputInterface::AddPhysicalPropEventQueue* lpAddPropEventQueue =
        &lpInput->GetAddPhysicalPropQueue();

    // ONE zero vector, materialised once: `vspltisw128 v127, 0` at 0x82632144 sits OUTSIDE the
    // loop (and after the early-out). It is consumed FIVE times per non-jointed iteration and SIX
    // when the joint block is taken (round-2 NIT: the count used to say five flat) -- twice as a
    // direct `stvx128` into the instance's two velocity members (0x82632520 / 0x8263253C, which is
    // the folded Vector3::SetZero pair), three times as a `vmr128` argument (v1 for
    // SetLinearVelocity at 0x82632558, v1/v2 for AddPropToSim at 0x826328BC/0x826328C4), and once
    // more in the JOINTED path as `stvx128 v127, r6, r21` @0x8263273C, the maLastJointRotation
    // clear (spelled below as SetZero).
    // vspltisw zeroes ALL FOUR lanes, w included, which is exactly Vector3::SetZero's semantics.
    // ⚠️ AUTHORED NAME: unlike the sibling ProcessAddPartInstanceEvents (whose DWARF scope names a
    // `Vector3 lVelocity`), THIS function's DecFIGS scope names no Vector3 local at all, so the
    // identifier below is this reconstruction's spelling of the hoisted register, not a recovered
    // name. The hoist itself is measured.
    Vector3 lZeroVelocity;
    lZeroVelocity.SetZero();

    for ( s32 luIndex = 0; luIndex < lpAddPropEventQueue->GetLength(); ++luIndex )
    {
        const AddPhysicalPropEvent* lpAddPropEvent = &lpAddPropEventQueue->GetEvent( luIndex );

        // 0x8263224C `lhz 0x4A(event)` + `extsh` -- the s16 slot sign-extended, which is why the
        // -1 test below is the SIGNED cmpwi and not a cmplwi.
        const s32 liPropInstanceIndex = lpAddPropEvent->miSlot;

        CGS_ASSERT( liPropInstanceIndex != KI_PROP_INDEX_NOT_FOUND,
                    "liPropInstanceIndex != -1" );                                    // :494
        CGS_ASSERT( !mUsedProps.IsBitSet( static_cast<u32>( liPropInstanceIndex ) ),
                    "!mUsedProps.IsBitSet( liPropInstanceIndex )" );                   // :495

        mUsedProps.SetBit( static_cast<u32>( liPropInstanceIndex ) );

        CGS_ASSERT( liPropInstanceIndex < static_cast<s32>( KU_MAX_PHYSICAL_PROPS ),
                    "liPropInstanceIndex < static_cast<int32_t>(KU_MAX_PHYSICAL_PROPS)" );  // :499

        PropInstance* lpProp = &mpaPropInstances[liPropInstanceIndex];

        // -----------------------------------------------------------------------------------
        // The instance seed, 0x8263251C..0x82632560. Ten stores plus two out-of-line calls.
        //
        // ⚠️ SOURCE SHAPE, STATED HONESTLY. The DecFIGS callee list for this function names
        //    `PropInstance::Construct()` -- declared with NO parameters (dwarfdump
        //    .../PropPhysics/BrnPropInstance.h:69) -- immediately before the two
        //    `rw::math::vpu::Vector3::SetZero` entries, so the original source almost certainly
        //    wrote `lpProp->Construct();` here and the compiler inlined it. TWO things stop that
        //    call being written: (a) the tree's PropInstance::Construct is DECLARATION-ONLY with
        //    no body anywhere (BrnPropInstance.h says so in its own banner), so calling it would
        //    open a link hole against a body this site cannot recover; and (b) the split is not
        //    recoverable from here anyway -- the DWARF callee list contains SetTypeId but NOT
        //    SetEntityId, even though the `stw` to mEntityId (+0x60) is plainly in the emission,
        //    so at least one of these stores is attributed to a callee the list does not name.
        //    The MEASURED stores are therefore written out by name. Observable behaviour is
        //    identical either way; only the source-level factoring is lost, and it is flagged
        //    rather than guessed.
        // -----------------------------------------------------------------------------------
        lpProp->SetInstanceID( 0 );                                       // 0x8263251C stw 0,+0x68
        lpProp->mLinearVelocity.SetZero();                                // 0x82632520 stvx +0x40
        lpProp->SetNotJointed();                                          // 0x82632534 stb 0xFF,+0x6C
        lpProp->SetIsStatic( false );                                     // 0x82632528 stb 0,+0x6D
        lpProp->SetMovementState( E_PROP_MOVESTATE_STATIONARY );          // 0x8263252C stb 0,+0x6E
        // 0x82632530 `stb r15(==1), 0x6F`. An assignment, not an OR: the flag word starts as
        // exactly KU_ADDED_THIS_FRAME_FLAG, and the extra-COM bit is written further down by
        // SetExtraComOffsetFlag. There is no set-side accessor for this bit (the header declares
        // only WasAddedThisFrame/ClearAddedThisFrameFlag), so the member is written by name.
        lpProp->mu8Flags = PropInstance::KU_ADDED_THIS_FRAME_FLAG;
        lpProp->mAngularVelocity.SetZero();                               // 0x8263253C stvx +0x50
        lpProp->SetEntityId( lpAddPropEvent->mEntityId );                 // 0x82632544 stw +0x60
        lpProp->SetTypeId(
            static_cast<u32>( static_cast<s32>( lpAddPropEvent->miPropTypeId ) ) );  // 0x82632550 lhz/extsh/stw +0x64

        // 0x82632554 / 0x82632560 -- the only two out-of-line calls in the seed. r4 == the event
        // pointer itself, i.e. &lpAddPropEvent->mTransform (mTransform is at event+0).
        lpProp->SetTransform( lpAddPropEvent->mTransform );
        lpProp->SetLinearVelocity( lZeroVelocity );

        CGS_ASSERT( static_cast<s32>( lpAddPropEvent->miPropTypeId )
                        < static_cast<s32>( KU_MAX_PROP_TYPES ),
                    "lpAddPropEvent->miPropTypeId < static_cast<int32_t>(KU_MAX_PROP_TYPES)" );  // :508

        // 0x8263258C `addi r3, this, 0x54` == &mpPhysicsData, into ResourcePtr<T>::operator->
        // (0x822868E0 -- IDA truncates that symbol to "BrnPhysics::Props::Prop"; I dumped its body
        // and it is the ResourcePtr tripwire, baked CgsResourcePtr.h:544, which pins the identity).
        // ⚠️ The string is "Can not instance resource pointer - it has no main memory resource\n"
        //    (aCanNotInstance @0x820072A0, read out of the image with headless IDA this round). The
        //    committed tree spells it at CgsResourcePtr.h:201/:209 and CgsResourceHandle.h:90/:99.
        //    ⚠️ The bad "...it is NULL" quote lived in PropManager_wQ2_01.cpp's callee census, NOT
        //    in PropManager_wQ2_02.cpp as this note used to say (round-2 MUST_FIX: wQ2_02:180 is an
        //    unrelated cache-slot comment and wQ2_02's own mention of the string is a correctly
        //    truncated prefix). wQ2_01's census has since been corrected.
        // GetType is then INLINED, with its own two asserts (BrnPropPhysicsDataHeader.h:173/:174,
        // baked as `li r5,0xAD`/`li r5,0xAE` against the "...SharedClasses\Physics\Props\
        // BrnPropPhysicsDataHeader.h" filename) -- the tree's header-inline GetType reproduces
        // both. muTypeId is loaded at 0x82632590, BEFORE the call, because the call clobbers r3.
        const PropTypeData* lpType = mpPhysicsData->GetType( lpProp->GetTypeId() );

        // ---------------------------------------------------------------------------------------
        // 0x826325E0..0x82632604 -- the INLINED InSceneUpdateInterface::AddCachedObject. The DWARF
        // names the helper (`void AddCachedObject(int32_t, float32_t)`, dumpfile
        // .../CgsSceneManagerIO_SceneUpdate.h:503 -> source :472); the tree does not declare it,
        // and the ARTIST build emits no call to it -- the two-word event fill and the direct
        // `bl BaseEventQueue<InEventAddToCache>::AddEvent` ARE the whole helper, with the queue
        // pointer formed as lpSceneInput + 0xC4930 == mAddToCacheQueue. The inlined form is spelled
        // here; when AddCachedObject is declared (header request), collapse these four lines onto
        // it. The event's own bounds tripwire lives inside BaseEventQueue::AddEvent, exactly as on
        // the console.
        // ---------------------------------------------------------------------------------------
        CgsSceneManager::TriangleCacheManagerIO::InEventAddToCache lAddToCacheEvent;
        lAddToCacheEvent.miCacheSlot         = KI_PROP_CACHE_START_INDEX + liPropInstanceIndex;
        lAddToCacheEvent.mfCacheSphereRadius = lpType->GetBoundingRadius();   // NO padding added
        lpSceneInput->mAddToCacheQueue.AddEvent( lAddToCacheEvent );

        // 0x82632608..0x8263261C. ONE load of meState feeds both flags: `cmpwi r11,2` selects the
        // jointed arm, and `addi r10,r11,-1 ; cntlzw ; extrwi r8,r11,1,26` is the branchless
        // `(meState == 1)` (the extracted bit is set only when cntlzw(meState-1) == 32, i.e. only
        // when meState-1 == 0). E_STATIC == 1 and E_LEANING == 2 in BrnWorld::EPropState.
        bool       lbStatic  = ( lpAddPropEvent->meState == BrnWorld::E_STATIC );
        const bool lbJointed = ( lpAddPropEvent->meState == BrnWorld::E_LEANING );

        if ( lbJointed )
        {
            // 0x82632624..0x8263268C -- the inlined BitArray<15>::GetFirstZeroBit (the DWARF's
            // name; the tree spells the same method GetFirstClearBit, and its body is the same
            // algorithm: skip an all-ones field, else `x - ((x-1)&x)` to isolate the lowest set
            // bit of the complement, `cntlzd`, `wordIndex*64 + 63 - clz`, and -1 when the result
            // reaches tuNumBits).
            const s32 liLampostJointIndex = mUsedPropJoints.GetFirstClearBit();

            CGS_ASSERT( liLampostJointIndex != KI_PROP_INDEX_NOT_FOUND,
                        "liLampostJointIndex != -1" );                                  // :522

            lpProp->SetJointIndex( liLampostJointIndex );                      // 0x826326AC stb +0x6C
            mBreakPropJoints.UnSetBit( static_cast<u32>( liLampostJointIndex ) );  // 0x826326D0 andc on +0x678
            mauPropIndexForJoint[liLampostJointIndex] =
                static_cast<u8>( liPropInstanceIndex );                        // 0x82632714 stb +0x2A0

            // 0x82632718..0x82632748. The joint anchor in world space.
            // ⚠️ vmaddfp OPERAND ORDER, re-derived rather than assumed (this exact rule shipped a
            //    defect in round 1). IDA prints the RAW FIELD order vD, vA, vB, vC while the PPC
            //    semantics are vD = vA*vC + vB -- so the printed form `vmaddfp vD, vA, vB, vC`
            //    means vD = <2nd operand> * <4th operand> + <3rd operand>. Reading it that way,
            //    the cascade at 0x82632738/0x82632740/0x82632744 is
            //        v13 = xAxis*splat_x + wAxis ; v13 = yAxis*splat_y + v13 ; v0 = zAxis*splat_z + v13
            //    == TransformPoint(propTransform, jointLocator). The other reading would give
            //    row0*row3, which is not any transform at all -- that asymmetry is what settles it.
            //    v9/v8/v0 are `vspltw` of lanes 0/1/2 of *(lpType+0) == PropTypeData::mJointLocator,
            //    and v12/v11/v10/v13 are lpProp's four transform rows at +0x00/+0x10/+0x20/+0x30.
            // ⚠️ ONE LANE DIVERGES, STATED: the console's FMA cascade is full 4-lane, so its w is
            //    jointLocator.w-weighted; the committed helper forces `lvResult.w = 0.0f`
            //    (matrix44affine_operation.h:69 -- :66 is the x-lane expression, :69 is the w
            //    store). maPropJointPositions is a Vector3 array whose
            //    consumers (UpdateJointedProps) read xyz, and the tree helper is the standing
            //    precedent, so it is used -- but do not "restore" the w lane by hand-spelling the
            //    FMA without checking those consumers.
            maPropJointPositions[liLampostJointIndex] =
                rw::math::vpu::TransformPoint( lpProp->GetTransform(), lpType->GetJointLocator() );

            maLastJointRotation[liLampostJointIndex].SetZero();                // 0x8263273C stvx +0x1B0
            maCurrentJointTransforms[liLampostJointIndex] =
                lpAddPropEvent->mTransform;                                    // 0x8263274C..0x82632768
            mUsedPropJoints.SetBit( static_cast<u32>( liLampostJointIndex ) ); // 0x82632880 or on +0x670

            // 0x82632884 `mr r8, r15` (r15 == 1) -- a jointed prop is always static.
            lbStatic = true;
        }

        lpProp->SetIsStatic( lbStatic );                                        // 0x82632898 stb +0x6D
        lpProp->SetExtraComOffsetFlag( lpAddPropEvent->mbAddExtraComOffset );   // 0x8263289C..0x826328D0

        // 0x826328E4. r4/r5/r6/r7/r8/r9/r10 + v1/v2 exactly as the register map above; the
        // Matrix44Affine rides r6 as a hidden reference (the event pointer itself) and the two
        // Vector3s ride v1/v2, consuming NO GPR slot -- which is why lpSimModuleInputBuffer lands
        // in r10 and not r8.
        AddPropToSim( lpAddPropEvent->mEntityId,
                      liPropInstanceIndex,
                      lpAddPropEvent->mTransform,
                      lpType,
                      lbStatic,
                      lpAddPropEvent->mbAddExtraComOffset,
                      lpSimModuleInputBuffer,
                      lZeroVelocity,
                      lZeroVelocity );
    }
}


// =================================================================================================
// BrnPhysics::Props::PropManager::OutputUpdatedProps @ 0x82627EC8   (14 instructions)
// DWARF: class decl BrnPropManager.h:155; body scope BrnPropManager.cpp:943.
//   ⚠️ CORRECTED wave Q6 round 1 (outprop #1): this line read "class decl BrnPropManager.h:294-295",
//   which the DWARF contradicts. The DecFIGS dwarfdump puts `void OutputUpdatedProps(OutputBuffer*)`
//   at source :155 (recorded twice, at dwarfdump .../BrnPropManager.h:211 and :477); source :294
//   and :295 are two unrelated DATA MEMBERS -- `DebugWorldContactInfo* mpDebugWorldContacts` and
//   `int32_t miNumDebugWorldContacts` (dwarfdump :164-171). The rest of this sentence (body scope
//   .cpp:943, local lpOutputQueue at :945) was and remains correct, which is what made the wrong
//   half read as measured.
//   NOTE for a later DWARF-reading sweep: the DWARF body carries a SECOND lexical block
//   (CgsDev::StrStream::StrStream + StrStreamBase::operator<<) with NO counterpart in the ARTIST
//   emission. Do NOT "restore" a log statement the image disproves.
//
// ⭐ LANDED 2026-08-19 (wave Q6 cluster A2). Parked since 2026-08-18 at
//    scratchpad/waveQ2/parked/PropManager_06_OutputUpdatedProps.cpp; re-verified store-for-store
//    against the raw `assembly` array this wave and landed with one added [DIAG] block.
//
// WHY IT MATTERS -- this is the ONLY producer of the UpdatePropEvent stream. PropManager::
// ReadUpdatedBodies fills mUpdatedProps every physics frame from the solver's OutUpdateRigidBody
// queue; until this function ran, nothing ever copied that queue into the module OUTPUT buffer, so
// WorldModule::BridgePhysicsModuleToPropModule_PostPhysics had nothing to forward,
// PropEntityModule::UpdateProps was never fed and PropZoneManager::UpdateInstance never wrote
// PropPartInstance::mWorldTransform -- i.e. a smashed prop's parts simulate correctly and are
// rendered at their ORIGINAL pose. Fourteen instructions were the whole severed edge.
//
// ---- THE FOURTEEN INSTRUCTIONS ------------------------------------------------------------------
// Counted from the 0xXXXXXXXX-prefixed lines of
// .ida-exports/BURNOUT_X360_ARTIST.XEX/0x82627EC8.json: (0x82627EFC - 0x82627EC8)/4 + 1 == 14.
// Nine are prologue/epilogue (mflr/stw/std/stwu ... addi/lwz/mtlr/ld/blr). The body is five:
//
//   0x82627ED8  mr   r31, r3                    save `this`
//   0x82627EDC  mr   r3,  r4                    lpOutput becomes the callee's `this`
//   0x82627EE0  bl   PhysicsModuleIO::OutputBuffer::GetPropManagerOutputInterface   (@0x825C0DC8)
//   0x82627EE4  addi r4,  r31, 0x680            &mUpdatedProps
//   0x82627EE8  bl   Props::PropOutputInterface::AppendUpdatedProps                 (@0x826153A0)
//
// The Hex-Rays pseudocode in the same export agrees and is quoted here only as the cross-check it
// is: `AppendUpdatedProps(GetPropManage(a2), a1 + 1664)` -- 1664 == 0x680.
//
// xrefs_to is exactly {BrnPhysics::PhysicsModule::Update @0x825B0640}; that call site is already
// live at BrnPhysicsModuleUpdateFunctions.cpp:730. xrefs_from is exactly the two callees above --
// there is no third `bl`, no assert, no branch, no early-out.
//
// ---- WHICH ACCESSOR OVERLOAD, and why it is not a guess -----------------------------------------
// The export's xrefs_from names 0x825C0DC8, which BrnPhysicsModuleIO_OutputBuffer.cpp:14 pins as
// the NON-const (write-locked, bit 3) overload -- not the const twin @0x8279F640. `lpOutput` is a
// non-const OutputBuffer*, so ordinary overload resolution picks that same one; nothing is cast
// and nothing is const_cast-ed to make it happen.
//
// ---- AGENTS.md GOTCHA AUDIT, all four measured rather than assumed ------------------------------
//   * gotcha 1 (console literals are not host values): ONE site, `0x680`. It is the console byte
//     offset of mUpdatedProps and it appears ONLY in this comment -- the member is reached BY NAME
//     below, so the x64 pointer widening of the members before it cannot desync the argument.
//   * gotcha 2 (a store through obj+0xNNNN may hit an EMBEDDED sub-object): no site -- the body
//     performs no store at all, and `+0x680` is a member of PropManager itself
//     (BrnPropManager.h:837), not of an embedded sub-object.
//   * gotcha 3 (PPC float args skip a GPR): no site -- the fourteen instructions touch no f* and
//     no v* register, and neither parameter is a float or vector.
//   * gotcha 4 (NaN polarity, `bge` == !(a<b)): no site -- there is no compare and no branch.
//
// ---- THE DWARF LOCAL ----------------------------------------------------------------------------
// The DecFIGS scope for BrnPropManager.cpp:943 names exactly one local,
// `PropOutputInterface* lpOutputQueue` at :945. That is why the interface pointer is bound to a
// named local instead of the two calls being chained into one expression: the X360 tail-shape is
// the emission's, the local is the source's. The parameter name lpOutput is the committed
// declaration's and the DWARF's alike.
//
// ---- ARGUMENT TYPE: NO FORK ---------------------------------------------------------------------
// AppendUpdatedProps takes `const PropOutputInterface::UpdatePropEventQueue*`
// (BrnPropOutputInterface.h:68) and mUpdatedProps is a PropManager::UpdatePropEventQueue
// (BrnPropManager.h:837/:778 -- ⚠️ these two citations read :811/:752 until wave Q6 round 1
// (outprop #2); this same owner's comment-only header edits in this wave moved the member and its
// typedef by 26 lines, and the pre-edit numbers land a reader in the middle of an unrelated member.
// Re-grep both after ANY further edit to BrnPropManager.h). Both typedefs name the SAME instantiation,
// CgsModule::EventQueue<UpdatePropEvent,200>, so `&mUpdatedProps` needs no cast -- pinned at
// compile time by scratchpad/waveQ6/probe_outprop/probe_outprop.cpp's negative-array-bound check.
// =================================================================================================
void PropManager::OutputUpdatedProps( BrnPhysics::PhysicsModuleIO::OutputBuffer* lpOutput )
{
    // 0x82627EDC / 0x82627EE0 -- lpOutput is handed over as the callee's `this`.
    PropOutputInterface* lpOutputQueue = lpOutput->GetPropManagerOutputInterface();   // DWARF :945

    // [DIAG] NOT IN THE X360 BINARY. Wave-Q6 one-shot: the single line that means the
    // UpdatePropEvent stream is alive. Opt in with BRN_PROP_DIAG. It is read BEFORE the append
    // because AppendUpdatedProps is the console's own merge-and-translate pass and this is the
    // producer-side count -- exactly the number of poses this frame hands to the world module.
    // The env latch is a function-local static: getenv per physics frame would be a syscall.
    {
        static const bool sbPropDiag = ( getenv( "BRN_PROP_DIAG" ) != 0 );
        static bool       sbLogged   = false;
        if ( sbPropDiag && !sbLogged && CgsDev::Log::gpDebugPrint != 0
             && mUpdatedProps.GetLength() > 0 )
        {
            sbLogged = true;
            *CgsDev::Log::gpDebugPrint
                << "[Q6-out] first publish: " << mUpdatedProps.GetLength()
                << " UpdatePropEvents\n";
        }
    }

    // 0x82627EE4 / 0x82627EE8 -- `addi r4, r31, 0x680` == &mUpdatedProps, passed by pointer
    // (AppendUpdatedProps takes `const UpdatePropEventQueue*`, BrnPropOutputInterface.h:68).
    lpOutputQueue->AppendUpdatedProps( &mUpdatedProps );
}

}
}

// ============================================================================
// FOLDED FROM PropManager_wQ2_07.cpp (wave Q2) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================
// =================================================================================================
// GameSource/Physics/PropManager/PropManager_wQ2_07.cpp
//
// Partfile of the TU GameSource/Unity/../Physics/PropManager/BrnPropManager.cpp
// (breakable-props wave Q, ROUND 2, group 07, 2026-08-18). Folds back into BrnPropManager.cpp.
//
// GROUP 07 WAS THREE FUNCTIONS. ONE LANDS HERE; the other two used to be PARKED.
// ✅ NOTHING IS PARKED ANY MORE -- re-grepped 2026-08-19 (wave Q6 round 2). This block previously
// read "TWO ARE PARKED ... eight declarations, each PROVEN absent"; every one of those eight
// blockers was supplied and both bodies landed:
//
//   * PropManager::FindPartIndex               @0x82605E10  -- BODIED BELOW (205 insns).
//   * PropManager::HandleContactWithLeanProp   @0x8260FB60  -- BODIED in
//         GameSource/Physics/PropManager/PropManager_wQ6_02.cpp (MOUNTED, bat:1802).
//   * PropManager::HandleContactWithTiltProp   @0x826108B8  -- BODIED in the same file.
//     The parked copies under scratchpad/waveQ2/parked/ are SUPERSEDED (their banners say so).
//     What unblocked them, for the record -- the two game-header declarations
//     (ExternalPhysicsBody::GetLinearMomentum(VecFloat) const, DWARF ExternalPhysicsBody.h:262;
//     Vehicle::Wheel::GetRoadLongSpeed() const == mSpeedAndMassOnWheelVariables.x) and the six
//     rw-math vendor helpers (Matrix44AffineFromAxisRotationAngle, GetVector3_YAxis/ZAxis,
//     CompLessThan/CompGreaterThan in both the VecFloat and Vector3 forms, struct Mask3 with its
//     lane broadcasts, and Select(Vector3, Vector3, MaskScalar)) all exist now.
//
//     Still true and worth keeping: the FOUR file-scope VecFloat constants both bodies need are
//     zero in the image and written by CRT-init thunks at 0x82C5E910..0x82C5E9AC; a headless IDA
//     read recovered them -- KVF_ROTATION_FACTOR 0.07f / KVF_MAX_ROTATION 0.05f /
//     KVF_PENETRATION_RESOLUTION_FACTOR 0.5f / KVF_MOMENTUM_RESOLUTION_FACTOR 0.5f (names from the
//     DecFIGS file-scope list, BrnPropManager.cpp:1755-1758).
//
// ⚠️ LINK, for the conductor (gotcha 12) -- re-grepped 2026-08-19 across b5-decomp/src AND
//    b5-decomp/vendor; the one hole this park used to carry is CLOSED:
//      * ✅ BrnPhysics::ExternallySimulatedBody::Translate(Vector3) -- BODIED at the wave-Q6
//        round-2 integration in GameSource/Physics/PhysicsUtilities/ExternallySimulatedBody.cpp
//        (declared at ExternallySimulatedBody.h:105). It was the landed bodies' last link hole.
//      * ✅ BrnPhysics::ExternalPhysicsBody::GetLocalVelocity IS BODIED -- a real, complete body at
//        GameSource/Physics/PhysicsUtilities/ExternalPhysicsBody.cpp:839 (the WORLD_SPACE /
//        BODY_SPACE branch over mLinearVelocity + mAngularVelocity x r), not a stub. It is NOT a
//        link hole; do NOT write a second definition for it (an LNK2005 `cl /c` cannot see). An
//        earlier banner named it as one, and the superseded parked files carry that wrong claim.
//    FindPartIndex below has no such callee.
//
// ⚠️ ODR / LINK NOTE FOR THE CONDUCTOR. Grepped before writing: FindPartIndex has NO other
//    definition anywhere in b5-decomp/src or b5-decomp/vendor -- not a real body, not a trap stub,
//    and (unlike UpdateTriangleCache / OutputUpdatedProps / ProcessInputsPreScene) never a one-shot
//    body in either of the stub TUs. So this partfile introduces no LNK2005 and retires no gate.
//
// Grounding: .ida-exports/BURNOUT_X360_ARTIST.XEX/0x82605E10.json -- the RAW `assembly` array. The
// Hex-Rays pseudocode in the same export was used only to cross-read the stack-slot map. Nothing
// below is invented; every claim in a comment is either an asm line quoted verbatim from that
// export or is labelled INFERENCE.
// =================================================================================================


namespace BrnPhysics
{
namespace Props
{

// FindPartIndex @0x82605E10 (const, 205 instructions -- counted as
// (0x82606140 - 0x82605E10)/4 + 1 == 205, which matches the owner report's table).
//
// The part-instance twin of the already-committed FindPropIndex
// (BrnPropManager_PropInstanceQueries.cpp): walk the used-PART bit-set, owner-check the query id
// and each stored id, and return the first slot whose stored PropEntityID equals lEntityId.
// Deliberately spelled exactly like that sibling -- same loop shape, same by-name calls -- because
// the two X360 bodies are the same source with mUsedProps/mpaPropInstances swapped for
// mUsedParts/mpaPartInstances.
//
// WHAT THE ASM ACTUALLY CONTAINS, and how each piece maps to a by-name call here (MEASURED):
//
//   * `addi r19, r31, 0x90` @0x82605E24 -- the walked bit-set is the object's SECOND BitArray,
//     mUsedParts (console +0x90), not mUsedProps (+0x80). Capacity 30: every bound in the body is
//     the literal 0x1E, but each site is quoted with ITS OWN verbatim text (round-2 NIT -- one
//     instruction text used to be quoted for two different addresses):
//     `cmpwi cr6, r11, 0x1E` @0x82605E88 (SIGNED, on the freshly computed first-bit index);
//     `cmplwi cr6, r29, 0x1E` @0x826060B4 (UNSIGNED, on the scan cursor r29);
//     `li r5, 0x1E` @0x82606030 and @0x82606054 inside the assert text.
//   * 0x82605E3C..0x82605E88 is CgsContainers::BitArray<30>::GetFirstNonZeroBit inlined: scan the
//     64-bit fields for a non-zero one (`ld r9,0(r10)` / `cmpldi` / `addi r10,r10,8`, bounded by
//     `cmplwi cr6, r11, 1` -- kuNumberOfBitFields == 1 for 30 bits), then isolate the lowest set
//     bit with the field*64 - cntlzd(x & -x) + 63 idiom, then the `>= 0x1E` guard.
//   * 0x82605E94 `cmpwi cr6, r11, -1 ; beq` is the loop's `!= KI_INVALID_BITINDEX` exit.
//   * 0x82605F54..0x82606120 is GetNextNonZeroBit inlined. The console spelling is the two-phase
//     one (scan bit-by-bit to the end of the current 64-bit field via `clrrwi r11,r30,6 ;
//     addi r28,r11,0x40` capped at 30, then fall through to the cross-field field-scan at
//     0x826060B4). The header's GetNextNonZeroBit is the single `for (bit = after+1; bit < N; ++bit)`
//     loop -- VALUE-IDENTICAL for a single-field array, which is what BitArray<30> is, so the
//     by-name call is used here exactly as the committed FindPropIndex uses it.
//   * The two `srwi r11, <id>, 24 ; cmplwi cr6, r11, 3 ; beq` guards (@0x82605EF4 on the QUERY id
//     reloaded from arg_1C, @0x82605F28 on the STORED id) are PropEntityID's baked owner tripwire
//     "mEntityId.GetOwner() == E_ENTITYTYPE_PROP", BrnPropEntityID.h:278 (`li r5, 0x116`) -> the
//     two AssertIsProp() calls below, in the console's own order (query first, stored second).
//   * `lwz r10, 0x8C(r31)` @0x82605F18 + `slwi r11, r30, 6` + `lwz r31, 0x30(r11)` -- the stored id
//     is mpaPartInstances (console +0x8C) indexed at STRIDE 64 with mEntityId at +0x30, i.e.
//     PropPartInstance::GetEntityId(). Console offsets are quoted in this comment only; the code
//     uses the members by name (host layout differs -- pointers widen on LLP64).
//   * `li r3, -1` at 0x82605E58 / 0x826060EC / 0x8260612C is the not-found return; the header
//     names that value KI_PROP_INDEX_NOT_FOUND (== -1, DWARF BrnPropManager.h:245), so it is
//     spelled by name here.
//
// WHAT IS DELIBERATELY NOT REPRODUCED, and why (MEASURED, then INFERENCE):
//   The block at 0x82605F7C..0x82606074 is a CgsBitArray.h:203 ("invalid index : <i> < <30>",
//   `li r5, 0xCB`) bounds assert, emitted inside the inlined GetNextNonZeroBit's IsBitSet. MEASURED:
//   it is guarded by `cmplwi cr6, r29, 0x1E ; blt cr6, loc_82606078` @0x82605F74 -- i.e. it only
//   fires when the scan index has already reached 30. INFERENCE: it is unreachable in the console
//   too, because the enclosing scan's own bound r28 is `min(clrrwi(r30,6) + 64, 30)`
//   (0x82605F54..0x82605F64), so r29 can never reach 0x1E inside that loop. The committed sibling
//   FindPropIndex makes the same omission for the same reason. No control flow is added or
//   inverted relative to the asm.
s32 PropManager::FindPartIndex( PropEntityID lEntityId ) const
{
    for ( s32 liPartIndex = mUsedParts.GetFirstNonZeroBit();
          liPartIndex != CgsContainers::BitArray<30>::KI_INVALID_BITINDEX;
          liPartIndex = mUsedParts.GetNextNonZeroBit( liPartIndex ) )
    {
        lEntityId.AssertIsProp();
        PropEntityID lStoredEntityId = mpaPartInstances[liPartIndex].GetEntityId();
        lStoredEntityId.AssertIsProp();

        if ( lStoredEntityId == lEntityId )
        {
            return liPartIndex;
        }
    }

    return KI_PROP_INDEX_NOT_FOUND;
}

}
}

// ============================================================================
// FOLDED FROM PropManager_wQ2_08.cpp (wave Q2) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================
// GameSource/Physics/PropManager/PropManager_wQ2_08.cpp
//
// BrnPhysics::Props::PropManager -- breakable-props wave Q, ROUND 2, partfile 08.
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   ProcessInputsPreScene @0x8263AF30   (209 instructions, 0x8263AF30..0x8263B270 --
//                                        COUNTED: (0x8263B270-0x8263AF30)/4 + 1 == 209)
//
// A slice of the TU's own BrnPropManager.cpp, split out per the wave's partfile convention
// (same spirit as BrnPropManager_PropInstanceQueries.cpp / PropManager_wQ_02.cpp).
//
// The second function of this group -- ApplyPropRaceCarCollisionImpulse @0x825E3560 -- is
// NOT here. Its body is complete and PARKED at
//   scratchpad/waveQ2/parked/PropManager_08_ApplyPropRaceCarCollisionImpulse.cpp
// because it needs two declarations this partfile may not add (both are named in that file's
// banner and in the round-2 report): RaceCarPhysics::AddPropCollisionImpulse(Vector3) and
// BrnPhysics::Props::KVF_MAX_PROP_SPEED_MPS. Nothing is stubbed in its place.
//
// ==========================================================================================
// WHAT THE 209 INSTRUCTIONS DO  (all offsets below are CONSOLE offsets, quoted from the asm;
// every one of them is reached BY NAME in the C++ -- AGENTS.md gotcha 1, they are meaningless
// on the LLP64 host).
//
// Register map, MEASURED from the prologue (0x8263AF3C..0x8263AF54):
//   r3 -> r17 = this
//   r4 -> r30 = lpInput                  (spilled to arg_1C at 0x8263AF58)
//   r5 -> r31 = lpSceneInput             (spilled to arg_24 at 0x8263AF64)
//   r6 -> stb arg_2F                     = lbSimPaused  (a bool, kept on the stack across the
//                                          whole body and re-read ONCE at 0x8263B254)
//   r7 -> r14 = lpSimModuleInputBuffer
//
// Body, in emission order:
//
//  1. 0x8263AF4C `addi r21, r17, 0x80` == &mUsedProps, then the fully inlined
//     BitArray<15>::GetFirstNonZeroBit / ::GetNextNonZeroBit / ::IsBitSet walk
//     (0x8263AF68..0x8263B1E4). Per live prop: `lwz r9,0x7C(r17)` == mpaPropInstances,
//     `mulli r11, r10, 0x70` == the 112-byte PropInstance stride, then
//     `lbz r9,0x6F(r11) ; clrrwi r9,r9,1 ; stb r9,0x6F(r11)` -- clearing bit 0 of
//     PropInstance::mu8Flags, i.e. PropInstance::ClearAddedThisFrameFlag().
//     The DecFIGS scope for this function (dwarfdump BrnPropManager.cpp:2285, source :299)
//     names exactly ONE local -- `int32_t liPropIndex` (source :302) -- and lists exactly the
//     three BitArray<15u> methods plus PropInstance::ClearAddedThisFrameFlag, which is what
//     the loop below spells.
//     ⚠️ The console emission carries an inlined bounds ASSERT ("invalid index : %u < 0xF",
//     CgsDev::Assert::BeginAssert/FireAssert/EndAssert at 0x8263B038..0x8263B130). Precisely
//     (round-2 NIT -- the earlier wording got both the justification and the attribution wrong):
//       * it is IsBitSet's OWN index precondition, inlined THROUGH GetNextNonZeroBit -- not
//         GetNextNonZeroBit's own -- and it prints the candidate index against tuNumBits == 15;
//       * `li r5, 0xCB` @0x8263B120 == 203 IS measured; "CgsBitArray.h" is an INFERENCE from the
//         truncated path string (0x8263AFEC `lis r11, aDP4B5MainBurno_51@ha`, which IDA renders
//         only as "d:\p45_mainurnout\main\code\g...");
//       * the committed CgsBitArray.h does NOT "own the bounds behaviour" -- its IsBitSet (:28-33)
//         has no check at all, and the header states it is deliberately assert-free ("so this stays
//         assert-free like the rest of the header").
//     The MEASURED reason not to spell it is stronger than the old one: the assert is UNREACHABLE
//     in this emission. r28 = min(wordBase + 64, 15) (0x8263B004 `clrrwi` / 0x8263B008 `addi 0x40` /
//     0x8263B010 `cmplwi 0xF` / 0x8263B020 `li r28,0xF`) and the inner-scan guard 0x8263B028
//     `cmplw r29,r28` + `bge` means r29 < 15 whenever 0x8263B030 `cmplwi r29,0xF ; blt` is
//     evaluated, so the assert block can never be entered. Behaviour is identical, provably.
//     (The function's xrefs_from therefore lists `CgsContainers::BasePriorityQueue::Clear`
//     @0x82815E58 -- called at 0x8263B04C, INSIDE that assert block. It is an ICF fold, not a
//     priority queue: the call is the assert message-stream reset. Noted so the next reader
//     does not chase a container this TU never touches.)
//
//  2. 0x8263B1E8..0x8263B20C -- the two REMOVE drains, both with (r4,r5,r6) ==
//     (lpInput, lpSceneInput, lpSimModuleInputBuffer).
//
//  3. 0x8263B210..0x8263B228 -- `lbz r11, 0x2C00(r30)` is PropInputInterface::
//     mbRemoveAllPropsAndParts (the committed accessor ShouldRemoveAllPropsAndParts(), whose
//     own home records that same +0x2C00). When set, RemoveAllPropsAndParts is called -- and
//     ⚠️ ITS ARGUMENT ORDER IS THE OTHER WAY ROUND from the four drains:
//     `mr r5,r31 ; mr r4,r14` == (lpSimModuleInputBuffer, lpSceneInput), which is exactly the
//     committed declaration's (lpSimInputBuffer, lpSceneInterface). Measured, not assumed.
//
//  4. 0x8263B22C..0x8263B250 -- the two ADD drains, same (r4,r5,r6) as the removes.
//
//  5. 0x8263B254..0x8263B268 -- `lbz r11,arg_2F ; cmplwi r11,0 ; bne <skip> ; bl
//     UpdateJointedProps` with `mr r4,r14`, i.e. `if (!lbSimPaused)
//     UpdateJointedProps(lpSimModuleInputBuffer);`. This single byte read is the ONLY use of
//     the third parameter in the whole body.
//
// ⚠️ ONE DWARF/ARTIST DIVERGENCE, STATED RATHER THAN SMOOTHED OVER. The DecFIGS scope for
// this function also lists InSceneUpdateInterface::RemoveCachedObject and the BitArray<30u>
// methods. Those belong to RemoveAllPropsAndParts, which the PS3 compiler INLINED here and
// which ARTIST emits as a real `bl` to 0x8260F010. The ARTIST form is what is reconstructed
// (rung 1 arbitrates); nothing from the PS3 inline expansion is written into this body.
//
// LINK-LEVEL. ProcessInputsPreScene once had an inert conductor-gate twin; that gate is gone,
// and this body is the only definition of the symbol. All six callees below are bodied in
// mounted partfiles (PropManager_wQ_02 / wQ2_05 / wQ2_06 / wQ4_02 / wQ6_01), so compile-green
// here is link-green too. Re-verify at mount time rather than trusting this list: it went
// stale once already, inside a single afternoon.
// ==========================================================================================


namespace BrnPhysics
{
namespace Props
{
    // ======================================================================================
    // ProcessInputsPreScene  --  X360 0x8263AF30, 209 instructions.
    // DecFIGS: dwarfdump BrnPropManager.h:147 (declaration) / BrnPropManager.cpp:2285
    // (scope, source line :299). Parameter names and the sole local are the DWARF's.
    // ======================================================================================
    void
    PropManager::ProcessInputsPreScene(
        const PropInputInterface*                                lpInput,
        CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpSceneInput,
        bool                                                     lbSimPaused,
        CgsPhysics::PhysicsSimulationIO::InputBuffer*            lpSimModuleInputBuffer )
    {
        // Step 1 -- clear the "added this frame" latch on every live prop. The console walks
        // mUsedProps with the inlined BitArray<15> iterators; re-rolled here into the named
        // calls (AGENTS.md "inlining reversal"). The loop guard is the console's own
        // `cmpwi r10,-1 ; bne` at 0x8263B1E0/0x8263B1E4.
        for ( s32 liPropIndex = mUsedProps.GetFirstNonZeroBit();
              liPropIndex != CgsContainers::BitArray<15>::KI_INVALID_BITINDEX;
              liPropIndex = mUsedProps.GetNextNonZeroBit( liPropIndex ) )
        {
            mpaPropInstances[ liPropIndex ].ClearAddedThisFrameFlag();
        }

        // Step 2 -- drain the two remove queues.
        ProcessRemovePropInstanceEvents( lpInput, lpSceneInput, lpSimModuleInputBuffer );
        ProcessRemovePartInstanceEvents( lpInput, lpSceneInput, lpSimModuleInputBuffer );

        // Step 3 -- the world-side "tear everything down" request (console `lbz 0x2C00`).
        if ( lpInput->ShouldRemoveAllPropsAndParts() )
        {
            RemoveAllPropsAndParts( lpSimModuleInputBuffer, lpSceneInput );
        }

        // Step 4 -- drain the two add queues.
        ProcessAddPropInstanceEvents( lpInput, lpSceneInput, lpSimModuleInputBuffer );
        ProcessAddPartInstanceEvents( lpInput, lpSceneInput, lpSimModuleInputBuffer );

        // Step 5 -- re-solve the jointed props, unless the simulation is paused.
        if ( !lbSimPaused )
        {
            UpdateJointedProps( lpSimModuleInputBuffer );
        }
    }
}
}

// ============================================================================
// FOLDED FROM PropManager_wQ2_09.cpp (wave Q2) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================
// GameSource/Physics/PropManager/PropManager_wQ2_09.cpp
//
// BrnPhysics::Props::PropManager -- breakable-props wave (waveQ) ROUND 2, 2026-08-18.
// Part-file of the TU GameSource/Unity/../Physics/PropManager/BrnPropManager.cpp.
//
//     GetPropInertia( const PropTypeData* )     @ 0x82612640  (289 insns)  -- NOT LANDED
//     GetPartInertia( const PropPartTypeData* ) @ 0x82612AC8  (272 insns)  -- NOT LANDED
//
// ⛔ THIS FILE INTENTIONALLY CONTAINS NO BODIES. It is the SINGLE banner for this function
// pair: the round-1 duplicate banner PropManager_wQ_04.cpp has since been DELETED by the
// conductor (verified 2026-08-18: the directory holds only PropManager_wQ_01/_02/_03.cpp), so
// there is no second, staler description of this blocker left to trust. That deletion was the
// point -- wQ_04's BLOCKER 1a asked for a declaration that had already landed, and re-landing
// its requested `s32 GetBBox(...)` spelling beside the real `RwBool` one would have forked a
// non-virtual dispatcher.
//
// The complete, corrected reconstruction of BOTH bodies -- round-1 verify items applied, and
// re-derived instruction by instruction against the raw ARTIST asm by the round-2 lander --
// is parked at
//
//     scratchpad/waveQ2/parked/PropManager_09_GetPropInertia_GetPartInertia.cpp
//
// and it is the intended drop-in for THIS file the moment the single remaining blocker below
// is cleared. It supersedes the round-1 park
// (scratchpad/waveQ/parked/PropManager_04_GetPropInertia_GetPartInertia.cpp), whose banner
// carries three claims that are now wrong: blocker 1a is closed; its instruction counts
// ("~180"/"~170") are wrong (289/272, counted); and its W-lane note says "four instructions"
// where it is one instruction (four BYTES).
//
// -----------------------------------------------------------------------------------------
// BLOCKER 1a -- ⛔ CLOSED 2026-08-18. DO NOT RE-REQUEST IT.
//   `rw::collision::Volume::GetBBox` now exists, non-virtual, at
//   SDKs/EATech/rwcollision/volume_debug_access.h:257 --
//       RwBool GetBBox(const Matrix44Affine*, RwBool, AABBox&) const
//   dispatching through the rwcollision per-TYPE descriptor at `volume+0x40`, function pointer
//   at descriptor+0x04 (NOT a C++ vptr; `sizeof(rw::collision::Volume) == 96` is asserted).
//   ⚠️ The only file that still claimed this was missing (PropManager_wQ_04.cpp) has been
//   deleted; note the landed signature's second parameter is `RwBool`, not the `s32` that stale
//   banner requested -- do not land a second overload.
//
// BLOCKER 1b -- ⚠️ STILL OPEN (re-measured 2026-08-18), and it is now the ONLY thing between
//   the parked bodies and the tree. `rw::collision::AABBox` cannot be NAMED as a complete type
//   in any TU that also names the game's `Vector3`, because two definitions of
//   `rw::math::vpu::Vector3` exist:
//       vendor/renderware/include/rw/math/vpu/types.h:24   struct { float x,y,z,w; }
//           <- what BrnCommonTypes.h pulls, i.e. what `Vector3` IS
//       src/SDKs/EATech/include/rw/math/vpu/vector3.h:26   class  { VectorIntrinsic mV; }
//           <- what vendor/renderware/collision/AABBox.hpp:4 pulls
//   Both bodies declare TWO AABBox OBJECTS BY VALUE (the DWARF's own `lAccumulatedAABBox` and
//   `lVolumeAABBox`, the second being the 32-byte out-parameter GetBBox writes), so an
//   incomplete type is not enough and no include ordering avoids it.
//   The same hazard is already recorded at BrnPropManager.cpp:149-157.
//
//   THE EXACT MISSING LINE: vendor/renderware/collision/AABBox.hpp:4-5
//       #include "SDKs/EATech/include/rw/math/vpu/vector3.h"
//       #include "SDKs/EATech/include/rw/math/vpu/matrix44.h"
//   must both become the vendor POD home `#include "rw/math/vpu/types.h"`.
//   ⚠️ BUT NOT ON ITS OWN -- THE BLAST RADIUS IS MEASURED, NOT ESTIMATED:
//     * 14 files in the tree reach AABBox.hpp today (counted 2026-08-18 by grepping the whole of
//       b5-decomp/src + vendor for the include; the 15th hit is this banner). They span
//       the vendor/renderware/collision directory, SDKs/EATech/rwcollision/volume_debug_access.h,
//       GameShared/GameClasses/SceneManager/CgsAABBoxBuilder.{h,cpp} and BrnPropManager.cpp.
//     * The waveQ2 rwcollision owner RAN the swap and then REVERTED it (AABBox.hpp md5 restored):
//       8 of the 11 TUs that compile AABBox.hpp fail, in THREE independent ways --
//       `.mV.mafLane[i]` member access (AABBox.cpp 4 sites, AggregateVolume.cpp 2,
//       ClusteredMeshQuery.cpp 2, VolumeBBoxQuery.cpp 4, CgsAABBoxBuilder.cpp 10), a missing 3-arg
//       `Vector3(x,y,z)` ctor on the POD (Capsule/Cylinder/TriangleVolume), and a missing
//       `VectorIntrinsic` (VolumeBBoxQuery.cpp:147). AggregateVolume.hpp ALSO pulls the EATech
//       matrix44.h on its own, so the swap does not even remove the clash from the collision
//       family: `Matrix44Affine`/`Mult` is a second duplicated pair and `VectorIntrinsic` a third.
//       This is a vocabulary collapse, not an include swap.
//   The costed work list is scratchpad/waveQ2/rwvol.owner.md §4.5 (six items; direction: the
//   vendor POD survives, the EATech class migrates). ONE line item is outside rwcollision
//   ownership and needs a separate owner grant:
//   GameShared/GameClasses/SceneManager/CgsAABBoxBuilder.cpp (10 `.mV.mafLane` sites).
//
// -----------------------------------------------------------------------------------------
// THE BLOCK IS EXACTLY THAT AND NOTHING ELSE -- both bounds MEASURED against the CURRENT tree
// on 2026-08-18, with the probes regenerated from the parked file's own code section
// (scratchpad/waveQ2/probe_wQ2_09/):
//   * probe_land.cpp           (the parked code verbatim)                   -> STATUS=fail,
//     FIRST diagnostic `C2011 "rw::math::vpu::Vector3": Typneudefinition`, and 109 error lines.
//     ⚠️ THAT RUN BOUNDS NOTHING BY ITSELF (round-2 NIT -- the earlier banner argued from "of
//     the 109 error lines, ZERO mention GetBBox or HACKShouldMoveComOffset"): the compile ABORTS
//     at MSVC's error cap before it ever parses the bodies -- the log's last line is
//     `fatal error C1003: Mehr als 100 Fehler gefunden` from vector3_type_inline.h(282) -- so the
//     absence of those names proves only that the cap was hit. The upper bound comes from the
//     podshim probe below (STATUS=pass), and more strongly from
//     scratchpad/waveQ2/probe_verify_land_land-inertia/probe_post45.cpp, which compiles the parked
//     code against a REAL repointed AABBox.hpp and also passes.
//   * probe_land_podshim.cpp   (the same, with ONLY the AABBox.hpp include swapped for a
//     probe-local `class AABBox { Vector3 mMin; Vector3 mMax; };` over the vendor POD)
//                                                                            -> STATUS=pass.
//     So every other declaration these bodies need is landed and binding today:
//     Volume::GetBBox, PropTypeData::HACKShouldMoveComOffset, Matrix44Affine::SetIdentity,
//     Get{NumberOfVolumes,CollisionVolume,Mass} on both records, K_LAMPOST_INERTIA_BOX.
//   * probe_land_noaabbox.cpp  (the same, with the include simply deleted)  -> STATUS=fail
//     with exactly the two `C2079 undefined class "rw::collision::AABBox"` and the C2664 they
//     cause on GetBBox's third argument. Nothing else is missing.
//
// -----------------------------------------------------------------------------------------
// LINK-LEVEL, so nobody mistakes a green gate for a working game (AGENTS gotcha 12):
//   * There is NO inert boot gate and NO other definition of either function anywhere in
//     b5-decomp (grepped src + vendor, .cpp/.h/.hpp): landing the parked file creates no
//     LNK2005, and there is no gate for the conductor to retire.
//   * `Volume::GetBBox` is an SDK HEADER INLINE with no X360 address -- it is not an export
//     hole and must not be added to the ledger as a function. But the descriptors it
//     dispatches through are LINK STUBS today: volume.cpp's `gVolumeVTable[1..6]` point at
//     `gVolumeHandler_82F91*` symbols defined as single zero bytes in
//     SDKs/EATech/AptRenderLinkStubs.cpp, and `Volume::InitializeVTable` binds to the inert
//     gate at WorldLinkStubs.cpp:2573 rather than its real body (static/non-static mangling
//     mismatch -- rwvol.owner.md §7.5). So even after 1b is cleared these two bodies will
//     compile and link but NULL-DEREFERENCE at run time until real descriptors exist;
//     `SphereVolume::GetBBox @0x82BA8020` and `BoxVolume::GetBBox @0x82BA9FC8` have no home
//     in this tree at all. That is a separate TU of work, flagged here, not fixed here.
//
// ⚠️ LEDGER (unchanged from round 1, still true): progress/status.json carries
//    `status: reviewed` for BOTH GetPropInertia and GetPartInertia while NEITHER is
//    implemented -- coverage_check still lists both as missing. Do not count them toward any
//    wave total and do not close this TU on that basis.

// (No code. See the parked file named above.)
