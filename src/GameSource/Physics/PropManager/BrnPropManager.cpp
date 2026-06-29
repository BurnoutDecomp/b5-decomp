// GameSource/Physics/PropManager/BrnPropManager.cpp
//
// BLOCKED ON OUT-OF-SCOPE DEPENDENCY RECONSTRUCTION.
//
// This translation unit reconstructs BrnPhysics::Props::PropManager (15 functions:
// Construct, ConstructContactGenerationPerfMonitors, ConstructPreScenePerfMonitors,
// Release, Destruct, ProcessAddPartInstanceEvents, ProcessRemovePartInstanceEvents,
// ReadUpdatedBodies, BeginPropWorldContactGeneration, EndPropWorldContactGeneration,
// GetPropInertia, GetPartInertia, SetupAndValidatePropContact, ApplyAntiHerdingForce,
// UpdateTriangleCache).
//
// It cannot be reconstructed to a byte-faithful, by-name, compiling state yet because
// the layout of PropManager and the bodies of every method depend on a set of types
// that have NO committed home in b5-decomp/src and are out of scope for this TU. Per
// the project rules (no raw *(this+off) offset hacks; never fake a TYPE with a stub;
// never fabricate a body), the only honest action is to record the precise block:
//
//   1. BASE CLASS — CgsSceneManager::CgsCollision::BaseCollisionGenerator.
//      Destruct() calls BaseCollisionGenerator::Destruct(this) on the SAME 'this',
//      and BeginPropWorldContactGeneration/EndPropWorldContactGeneration call
//      BaseCollisionGenerator::Prepare/Finish/RunCollidePrimitiveListWith on 'this'.
//      => PropManager derives from BaseCollisionGenerator. The X360 ASM writes the
//      first PropManager-owned fields at this+0x48 (mbRenderCOM), this+0x4C/0x50
//      (mfMassOverride/mfMaxLeanAngleOverride = 10.0f/0.0f), this+0x74 (mfStaticFriction
//      = 0.30000001f), this+0x78 (mfDynamicFriction = 0.60000002f), miNumJobsAdded at
//      this+0x9C (a1[39]); the perf-monitor ints at this+0xA4..0xB8. The whole ~0x48-byte
//      prefix is the BaseCollisionGenerator sub-object, whose layout the DecFIGS DWARF for
//      THIS path does not contain. Without it, NO member offset in any body can be mapped
//      to a name -> the class layout itself is ungroundable here.
//      DWARF home of the base: GameShared/GameClasses/SceneManager/Collision/
//      ContactGenerator/CgsCollisionGenerator.h (not yet reconstructed).
//
//   2. PropInstance / PropPartInstance — embedded as pointers (mpaPropInstances /
//      mpaPartInstances) but DEREFERENCED pervasively by the bodies
//      (PropInstance::SetTransform/SetLinearVelocity/SetAngularVelocity/GetJointIndex,
//      PropPartInstance::SetPosition/SetLinearVelocity, the 112-byte / 64-byte element
//      strides at this+124 / this+140). DWARF home:
//      GameSource/Physics/PropManager/PropPhysics/BrnPropInstance.h (no committed header).
//
//   3. The prop event-queue specializations actually instantiated by Construct
//      (UpdatePropEvent_200_::Construct @ this+1664, UpdatePropEvent_15_::Construct @
//      this+24080, and UpdatePropEvent::AddEvent/Append/AddEventSafe) plus
//      PropRaceCarContactBuffer — EventQueue<UpdatePropEvent,200/15> and
//      EventQueue<PropRaceCarContact,30>. The payload struct UpdatePropEvent exists
//      (SharedIO/BrnPropEvents.h) but the EventQueue<> capacities used here are not
//      instantiated/defined in the committed tree.
//
//   4. The physics/IO/collision dependency family used by the non-trivial bodies:
//      BrnPhysics::Vehicle::RaceCarPhysics, CgsPhysics::PhysicsSimulationIO::InApplyForce,
//      CgsSceneManager::{SceneManagerIO, TriangleCacheManagerIO, EntityId},
//      CgsMemory::{SimpleDataStreamProducer, DataStreamCommandPoster},
//      BrnResource::HeapResourceAllocator, the Prop/PropTypeData/PropPartTypeData/
//      PropPhysicsDataHeader accessors, and the rw::physics::RigidBody helpers — none
//      with a committed definition reachable by the compile gate.
//
// Reconstructing those dependency TUs first (especially the BaseCollisionGenerator base
// and BrnPropInstance.h) is a prerequisite. Once they exist, this TU can be reconstructed
// with the full DWARF member layout (already captured in
// references/DecFIGS/dwarfdump/GameSource/Physics/PropManager/BrnPropManager.h) and the
// X360 ASM bodies. No types or bodies are fabricated here.

#include "GameSource/Physics/PropManager/BrnPropManager.h"
