// GameSource/Physics/PropManager/BrnPropManager.cpp
//
// BrnPhysics::Props::PropManager -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//   ConstructContactGenerationPerfMonitors()  @ 0x825BAC60   (4 asm lines)   -- DONE below
//   ConstructPreScenePerfMonitors()           @ 0x825BAC70   (6 asm lines)   -- DONE below
// The other 13 functions of this TU are still open; the block list is at the bottom.
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
// STILL BLOCKED, and why (the parts of the old note that hold up):
//
//   1. PropDebugComponent -- embedded BY VALUE at +0x00, so any TU that instantiates
//      PropManager needs its vtable, and PropDebugComponent's four virtual overrides
//      (RenderHUD / GetName / OnActivate / OnRegister) have no bodies in-tree. That is what
//      keeps Construct itself out for now, not the layout. (The two functions bodied below
//      touch only scalars, so they do not need it.)
//   2. PropInstance / PropPartInstance -- pointers here, but dereferenced pervasively by the
//      other bodies (112-byte / 64-byte element strides at +0x7C / +0x8C).
//   3. The rest of the physics/IO/collision dependency family used by the non-trivial bodies:
//      BrnPhysics::Vehicle::RaceCarPhysics, CgsPhysics::PhysicsSimulationIO::InApplyForce,
//      CgsSceneManager::{SceneManagerIO, TriangleCacheManagerIO}, CgsMemory::
//      DataStreamCommandPoster, the Prop/PropTypeData/PropPartTypeData/PropPhysicsDataHeader
//      accessors, and the rw::physics::RigidBody helpers.
//
// No types or bodies are fabricated here.

#include "GameSource/Physics/PropManager/BrnPropManager.h"

namespace BrnPhysics
{
namespace Props
{

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

}
}
