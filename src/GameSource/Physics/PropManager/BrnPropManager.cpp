// GameSource/Physics/PropManager/BrnPropManager.cpp
//
// BrnPhysics::Props::PropManager -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//   Construct()                               @ 0x82627390   (82 asm lines)  -- DONE below
//   ConstructContactGenerationPerfMonitors()  @ 0x825BAC60   (4 asm lines)   -- DONE below
//   ConstructPreScenePerfMonitors()           @ 0x825BAC70   (6 asm lines)   -- DONE below
//   CreateContactEvent()                      @ 0x825A53A0                   -- DONE below
//                                                             (a `class:` TU's function, bodied here)
//   SetupAndValidatePropContact()             @ 0x82628190                   -- TRAP STUB below
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

// ⚠️ AUTHORED NAME -- see the declaration's block in BrnPropManager.h. This is PropManager_wQ2_03.cpp's
// HEADER REQUEST D, landed. Splat(0.3f), thunk 0x82C5E750, tbl slot 0x82CD19AC, rodata
// flt_82004740 = 0x3E99999A. (Request D's other half, KB_USE_CONTACT_GEN_STREAM, is NOT landed --
// no address, no thunk, nothing to measure. It stays open.)
const ::VecFloat KVF_MAX_CONTACT_GEN_PADDING      = { 0.3f, 0.3f, 0.3f, 0.3f };          // X360 0x82FB94F0

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
    for (u32 luIndex = 1; luIndex < 4; ++luIndex)
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
//   * WHOLE-PROP id: the instance is mpaPropInstances[mIDA's LOW dword] (112-byte stride);
//       muType = instance.muTypeId (+0x64), tripwired < 1000 (:563); if the instance's
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
        // PART id: the part-instance table, 64-byte stride, index = the id's low 16 bits.
        const u32 luTypeId = mpaPartInstances[luSpyIdALow & 0xFFFFu].GetType();
        CGS_ASSERT(luTypeId < 1000u, "luTypeId < 1000");   // :550
        lpOutPropContact->muType  = static_cast<u16>(luTypeId);
        lpOutPropContact->muState = 1;
    }
    else
    {
        // Whole-prop id: the prop-instance table, 112-byte stride, index = the id's low dword.
        PropInstance& lrInstance = mpaPropInstances[luSpyIdALow];
        const u32 luTypeId = lrInstance.muTypeId;
        CGS_ASSERT(luTypeId < 1000u, "luTypeId < 1000");   // :563
        lpOutPropContact->muType = static_cast<u16>(luTypeId);
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
// ⚠⚠ TRAP STUB (closure enforcement, 2026-08-06 big-five #2 wave) -- the REAL body (572 X360 asm
// lines / 24 callees: validate + set up one prop-vs-X potential contact for the simulation) is
// NOT reconstructed yet. Dead code today: the only caller chain is PhysicsModule::
// BridgeContactsToSimulation <- Update @0x825B0640, still a link stub, so /OPT:REF strips this.
// RECONSTRUCT-NEXT.
// =================================================================================================
bool PropManager::SetupAndValidatePropContact(
    CgsPhysics::PhysicsSimulationIO::InAddPotentialContact* /*lpAddContactEvent*/,
    const CgsSceneManager::SceneManagerIO::PotentialContact* /*lpPotentialContact*/,
    BrnPhysics::Vehicle::VehicleManager* /*lpVehicleManager*/,
    CgsPhysics::PhysicsSimulationIO::InputBuffer* /*lpSimModuleInputBuffer*/,
    PropRaceCarContactBuffer* /*lpPropRaceCarContactBuffer*/,
    CgsPhysics::RigidBodyId /*lWorldRigidBodyId*/,
    bool /*lbFrozen*/,
    f32 /*lfTimeStep*/)
{
    CGS_ASSERT(false,
               "TRAP: PropManager::SetupAndValidatePropContact @0x82628190 "
               "not reconstructed (big-five #2 closure stub)\n");
    return false;
}

}
}
