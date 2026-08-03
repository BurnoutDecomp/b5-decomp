#pragma once

// Queue-element homes for the CgsPhysics::PhysicsSimulationIO input/output event queues.
// Each event is the payload stored in a CgsModule::EventQueue<EventT, N> member of the
// PhysicsSimulationIO::InputBuffer / OutputBuffer aggregates (X360
// PhysicsSimulationIO::InputBuffer::Construct @ 0x828A71B8 wires one queue per event type
// at fixed byte offsets). This header exists so the per-instantiation Construct TUs can see
// a COMPLETE element type (the queue embeds EventT maEvents[N] inline).
//
// No DecFIGS DWARF hint covers these event payloads, so their internal field layout is
// NOT recovered. What IS X360-attested is each event's STRIDE, read off the
// InputBuffer::Construct offset map (the byte gap between consecutive queues, minus the
// 16-byte EventQueue base, divided by the queue capacity):
//   InAddPotentialContact : (189280 - 107344 - 16) / 1024 =  80 bytes
//   InAddJoint            : (196208 - 189280  - 16) /   36 = 192 bytes
//   InAddDrive            : (203632 - 203472  - 16) /    1 = 144 bytes
// Each event is therefore modelled as an opaque, correctly-sized, 16-byte-aligned byte span
// (alignas(16) forces the 12-byte BaseEventQueue base to pad to +0x10 before maEvents,
// exactly the asm's `addi r30, r31, 0x10`; the span size makes sizeof(EventQueue<EventT,N>)
// match the InputBuffer gap). The Construct bodies only take &maEvents[0], store the
// capacity N and clear the count, so they are store-for-store faithful regardless of the
// span's internal (unrecovered) field layout. Field names are intentionally NOT invented.
#include "types.hpp"
#include "GameShared/GameClasses/Module/CgsEventQueue.h"

namespace CgsPhysics
{
namespace PhysicsSimulationIO
{
    // Empty per-module event base (CgsModule event-queue convention; the queue stores
    // events by byte image), matching the other PhysicsSimulationIO IO event payloads.
    struct Event {};

    // Add a potential narrow-phase contact pair. Stride 80 bytes (X360-attested, see above).
    struct alignas(16) InAddPotentialContact : public Event
    {
        u8 macOpaquePayload[80];  // internal layout not recovered (no DWARF/source)
    };

    // Add a constraint joint. Stride 192 bytes (X360-attested, see above).
    //
    // ⭐ ADDITIVE GROW 2026-08-03 (the ArticulatedJointPool de-fork): the first three 8-byte
    // slots are NAMED, and the names are the binary's own. BrnPhysics::Vehicle::
    // VehicleOutputRequestInterface::AddJoint @0x825E7170 (an .ida-exports HOLE, pulled with
    // headless IDA 9.3) validates exactly these three fields before queueing the event, and its
    // three FireAssert strings spell them out verbatim:
    //     ld r10, 0(r29)     -> "!lAddJointEvent.mId.IsInvalid()"                        (:718)
    //     ld r10, 8(r29)     -> "lAddJointEvent.mParentBodyId != CgsPhysics::K_INVALID_RIGID_BODY_ID" (:719)
    //     ld r10, 0x10(r29)  -> "lAddJointEvent.mChildBodyId  != CgsPhysics::K_INVALID_RIGID_BODY_ID" (:720)
    // So the names come from the image and the offsets/widths from the asm; nothing is invented.
    // ⚠️ TYPED u64 DELIBERATELY, NOT AS CgsPhysics::JointId / CgsPhysics::RigidBodyId. Those two
    // names are each defined TWICE in this tree -- CgsRigidBody.h:24 (class, private mId,
    // `static const K_INVALID_RIGID_BODY_ID`) and CgsPhysicsSimulationModule.h:102/111 (structs,
    // public mId, `extern const K_INVALID_RIGID_BODY_ID` defined in the MOUNTED
    // CgsPhysicsSimulationModule.cpp:183) -- an open ODR fork that has simply never met in one TU.
    // Including either header here would make them meet and fail with a hard C2011. The handles are
    // a single u64 in both readings, so the width and the stores are identical either way.
    // sizeof stays 192 and alignof stays 16 (three u64 + 168 tail bytes), gated below.
    struct alignas(16) InAddJoint : public Event
    {
        u64 mu64Id;            // @+0x00  "mId"           (a CgsPhysics::JointId handle)
        u64 mu64ParentBodyId;  // @+0x08  "mParentBodyId" (a CgsPhysics::RigidBodyId handle)
        u64 mu64ChildBodyId;   // @+0x10  "mChildBodyId"  (a CgsPhysics::RigidBodyId handle)
        u8  macOpaquePayload[168];  // @+0x18 .. +0xC0: joint frames/limits, not recovered (no DWARF/source)
    };

    // Add a vehicle drive. Stride 144 bytes (X360-attested, see above).
    struct alignas(16) InAddDrive : public Event
    {
        u8 macOpaquePayload[144];  // internal layout not recovered (no DWARF/source)
    };

    // Add a rigid body to the simulation. Queued with capacities 1 / 50 / 200 across the
    // input/output buffers (X360 Construct @ 0x825A8228 / 0x825A7C78 / 0x825A7AB8). The
    // event STRIDE *is* X360-attested: the matching BaseEventQueue<InAddRigidBody>::AddEvent
    // @ 0x825A3000 copies each element with `li r5,0xC0; memcpy` (192-byte Size) at a
    // 192-byte stride (`slwi r9,r11,1; add r11,r11,r9; slwi r11,r11,6` == miLength*192), and
    // Append @ 0x825A3898 block-copies at the same 192-byte stride
    // (`slwi r9,r29,1; add r9,r29,r9; slwi r5,r9,6` == count*192). So this payload is sized to
    // that attested 192-byte stride. Internal field layout is still NOT recovered (no DWARF/
    // source), so it is modelled as an opaque, 16-byte-aligned byte span; the Construct bodies
    // remain store-for-store faithful regardless (they only take &maEvents[0]==this+0x10,
    // store N and clear the count).
    struct alignas(16) InAddRigidBody : public Event
    {
        u8 macOpaquePayload[192];  // stride 192B X360-attested (AddEvent @0x825A3000); fields not recovered
    };

    // Apply a force to a body. Queued with capacity 250 in PhysicsSimulationIO::InputBuffer
    // (X360 Construct @ 0x828A6068). The event STRIDE *is* X360-attested: the matching
    // BaseEventQueue<InApplyForce>::AddEvent @ 0x825E3CC8 and AddEventSafe @ 0x825E3E20 each
    // copy an element as exactly four 64-bit block moves (ld/std x4 == 32 bytes) at a 32-byte
    // stride (`slwi r11,r11,5` == miLength*32). So this payload is sized to that attested
    // 32-byte stride. Internal field layout is still NOT recovered (no DWARF/source), so it is
    // modelled as an opaque, 16-byte-aligned byte span; the Construct body remains
    // store-for-store faithful regardless.
    struct alignas(16) InApplyForce : public Event
    {
        u8 macOpaquePayload[32];  // stride 32B X360-attested (AddEvent @0x825E3CC8); fields not recovered
    };

    // Change a rigid body's inertia tensor. Queued with capacity 200 across the input/output
    // buffers (X360 Construct @ 0x825A7B28). The event STRIDE *is* now X360-attested: the
    // matching BaseEventQueue<InChangeRigidBodyInertia>::Append @ 0x825A40E8 block-copies at an
    // 80-byte stride (dest offset `slwi r8,r11,2; add` == miLength*5, `slwi r11,r11,4` == *16 ==
    // miLength*80; count `slwi r9,r29,2; add` == count*5, `slwi r5,r9,4` == *16 == count*80;
    // Hex-Rays XMemCpy(80*a1[2]+*a1, *a2, 80*v4)). So this payload is sized to that attested
    // 80-byte stride. 80 is 16-byte aligned so alignas(16) is preserved (and the committed
    // EventQueue<...,200>::Construct stays store-for-store faithful -- it depends only on the
    // base size for the +0x10 buffer padding and stores N / clears the count, element-size-
    // agnostic). Internal field layout is still NOT recovered (no DWARF/source), so it is
    // modelled as an opaque, 16-byte-aligned byte span.
    struct alignas(16) InChangeRigidBodyInertia : public Event
    {
        u8 macOpaquePayload[80];  // stride 80B X360-attested (Append @0x825A40E8); fields not recovered
    };

    // Remove a previously-added vehicle drive from the simulation (symmetric partner of
    // InAddDrive). Queued with capacity 1 in PhysicsSimulationIO::InputBuffer (X360
    // EventQueue<InRemoveDrive,1>::Construct @ 0x828A64C8). Only Construct is in scope (no
    // Append/AddEvent to pin the stride, and the InputBuffer::Construct offset map @ 0x828A71B8
    // that would pin it is not in scope), so the payload is sized only to the 16-byte alignment
    // class the asm proves (addi r30, r31, 0x10). Stride/field layout intentionally NOT invented.
    struct alignas(16) InRemoveDrive : public Event
    {
        u8 macOpaquePayload[16];  // stride NOT recovered; sized to attested 16B alignment only
    };

    // Remove ALL rigid bodies belonging to one owner from the simulation, addressed by an
    // 8-bit owner id. Queued with capacity 8 in PhysicsSimulationIO::InputBuffer as
    // mRemoveAllRigidBodiesQueue (X360 EventQueue<InRemoveAllRigidBodies,8>::Construct
    // @ 0x828A6148). Unlike the 16-byte-aligned siblings, this element is a single uint8_t: the
    // Construct asm points the base at this+0x0C (`addi r30, r31, 0xC`) -- the 12-byte
    // BaseEventQueue base is NOT padded before maEvents -- proving align < 16. DWARF
    // (CgsPhysicsSimulationModuleIO.h:154/156) names the one member outright:
    // { uint8_t mu8OwnerId; } over an empty Event base == sizeof 1, align 1. So this is a
    // fully-attested struct, not an opaque span, and no alignas.
    struct InRemoveAllRigidBodies : public Event
    {
        u8 mu8OwnerId;  // DWARF CgsPhysicsSimulationModuleIO.h:156; sizeof 1, align 1 (base at this+0x0C X360-attested)
    };

    // Push an external (non-simulation) body's updated state into the simulation. Queued with
    // capacities 1 / 60 / 200 across the input/output buffers (X360 Construct @ 0x828A6538 /
    // 0x825A8370 / 0x828A6688). The event STRIDE *is* X360-attested here: the matching
    // EventQueue<InUpdateExternalBody>::Append @ 0x825A41D8 block-copies at a 112-byte stride
    // (`mulli r5,r29,0x70`, `mulli r11,r11,0x70` == count*0x70 == count*112). So this payload is
    // sized to that attested 112-byte stride. Internal field layout is still NOT recovered
    // (no DWARF/source), so it is modelled as an opaque, 16-byte-aligned byte span; the Construct
    // bodies only take &maEvents[0]==this+0x10, store N and clear the count, so they remain
    // store-for-store faithful regardless of the span's internal layout.
    struct alignas(16) InUpdateExternalBody : public Event
    {
        u8 macOpaquePayload[112];  // stride 112B X360-attested (Append @ 0x825A41D8); fields not recovered
    };

    // Push updated per-frame vehicle drive state into the simulation. Queued with capacity 1
    // in PhysicsSimulationIO (X360 Construct @ 0x828A6538). Same recovery caveat as
    // InAddRigidBody: no Append/AddEvent for this type is in scope to pin the stride, and the
    // InputBuffer::Construct offset map @ 0x828A71B8 that would pin it is not in scope either,
    // so the payload is sized only to the 16-byte alignment class the asm proves
    // (`addi r30, r31, 0x10`). Stride/field layout intentionally NOT invented.
    struct alignas(16) InUpdateDriveFrames : public Event
    {
        u8 macOpaquePayload[16];  // stride NOT recovered; sized to attested 16B alignment only
    };

    // Update a rigid body's per-frame state in the simulation. Queued with capacity 200 in
    // PhysicsSimulationIO::InputBuffer (X360 EventQueue<InUpdateRigidBody,200>::Construct
    // @ 0x828A5FF8, capacity 0xC8). The event STRIDE *is* now X360-attested: the matching
    // BaseEventQueue<InUpdateRigidBody>::AddEvent @ 0x82614928 copies the element at index
    // miLength via `slwi r9,r11,1; add r11,r11,r9` (miLength*3), `slwi r11,r11,6` (*64) ==
    // miLength*192 (the Hex-Rays pseudocode renders this literally as `192 * v11 + *a1`). So
    // this payload is sized to that attested 192-byte stride. Internal field layout is still
    // NOT recovered (no DWARF/source), so it is modelled as an opaque, 16-byte-aligned byte
    // span; the Construct body stays store-for-store faithful (it only takes &maEvents[0] ==
    // this+0x10, stores N and clears the count) regardless of the span's internal layout.
    struct alignas(16) InUpdateRigidBody : public Event
    {
        u8 macOpaquePayload[192];  // stride 192B X360-attested (AddEvent @0x82614928); fields not recovered
    };

    // Update a constraint joint's limits in the simulation. Queued with capacity 36 in
    // PhysicsSimulationIO::InputBuffer (X360 EventQueue<InUpdateJointLimits,36>::Construct
    // @ 0x828A6378, capacity 0x24). Only Construct is in scope (no Append/AddEvent to pin the
    // stride, and the InputBuffer::Construct offset map @ 0x828A71B8 that would pin it is not in
    // scope), so the payload is sized only to the 16-byte alignment class the asm proves
    // (`addi r30, r31, 0x10`). Stride/field layout intentionally NOT invented.
    struct alignas(16) InUpdateJointLimits : public Event
    {
        u8 macOpaquePayload[16];  // stride NOT recovered; sized to attested 16B alignment only
    };

    // An output "spy" report of a resolved contact, drained from the simulation back to the game.
    // Queued with capacity 800 in PhysicsSimulationIO::OutputBuffer (X360
    // EventQueue<OutContactSpy,800>::Construct @ 0x828A68B8, capacity 0x320). The event STRIDE
    // *is* now X360-attested: the matching BaseEventQueue<OutContactSpy>::AddEvent @ 0x825E44C8
    // and AddEventSafe @ 0x828A1B78 both copy the element with `li r9,0xE`/`mtctr r9` (14 ld/std
    // 64-bit block moves == 14*8 == 112 bytes) at a 112-byte stride (`mulli rX,rX,0x70` ==
    // miLength*112). So this payload is sized to that attested 112-byte stride; 112 is still
    // 16-byte aligned, so the Construct @ 0x828A68B8 stays store-for-store faithful (it only
    // takes &maEvents[0] == this+0x10, stores N=800 and clears the count). Internal field layout
    // is still NOT recovered (no DWARF/source), so it is modelled as an opaque byte span.
    struct alignas(16) OutContactSpy : public Event
    {
        u8 macOpaquePayload[112];  // stride 112B X360-attested (AddEvent @0x825E44C8 / AddEventSafe @0x828A1B78); fields not recovered
    };

    // An output "spy" report of per-frame vehicle drive state, drained from the simulation back to
    // the game. Queued with capacity 1 in PhysicsSimulationIO::OutputBuffer (X360
    // EventQueue<OutDriveSpy,1>::Construct @ 0x828A6998). The event STRIDE *is* X360-attested here:
    // the matching BaseEventQueue<OutDriveSpy>::AddEvent @ 0x828A1D90 copies each element as exactly
    // eight 64-bit block moves (`li r9,8`; ld/std loop) at a 64-byte (0x40) stride
    // (`slwi r10,r10,6` == miLength*64), i.e. sizeof(OutDriveSpy) == 64. So this payload is sized to
    // that attested 64-byte stride. Internal field layout is still NOT recovered (no DWARF/source),
    // so it is modelled as an opaque, 16-byte-aligned byte span; the Construct/AddEvent bodies remain
    // store-for-store faithful regardless of the span's internal layout.
    struct alignas(16) OutDriveSpy : public Event
    {
        u8 macOpaquePayload[64];  // stride 64B X360-attested (AddEvent @ 0x828A1D90); fields not recovered
    };

    // An output "spy" report of a resolved constraint joint, drained from the simulation back to the
    // game. Queued with capacity 64 in PhysicsSimulationIO::OutputBuffer (X360
    // EventQueue<OutJointSpy,64>::Construct @ 0x828A6928, capacity 0x40). The event STRIDE *is*
    // X360-attested here: the matching BaseEventQueue<OutJointSpy>::AddEvent @ 0x828A1C30 copies each
    // element as exactly six 64-bit block moves (ctr == 6, ld/std loop == 48 bytes) at a 48-byte
    // stride (`slwi r7,r11,1; add r11,r11,r7` == miLength*3, `slwi r11,r11,4` == *16 == miLength*48),
    // i.e. sizeof(OutJointSpy) == 48. So this payload is sized to that attested 48-byte stride.
    // Internal field layout is still NOT recovered (no DWARF/source), so it is modelled as an opaque,
    // 16-byte-aligned byte span; the Construct/AddEvent bodies remain store-for-store faithful
    // regardless of the span's internal layout.
    struct alignas(16) OutJointSpy : public Event
    {
        u8 macOpaquePayload[48];  // stride 48B X360-attested (AddEvent @ 0x828A1C30); fields not recovered
    };

    // Remove a previously-added constraint joint from the simulation, addressed by its 8-byte joint
    // id/handle. Queued in PhysicsSimulationIO::InputBuffer. The event STRIDE *is* X360-attested: the
    // matching BaseEventQueue<InRemoveJoint>::AddEvent @ 0x825E4208 stores each element as a SINGLE
    // 64-bit store (`stdx r10,r11,r9`) at an 8-byte stride (`slwi r11,r11,3` == miLength*8), and Append
    // @ 0x825A3B58 block-copies at the same 8-byte stride (`slwi r5,r29,3` == count*8,
    // `slwi r11,r11,3` == miLength*8), i.e. sizeof(InRemoveJoint) == 8. So this payload is exactly an
    // 8-byte id. The id's semantic interpretation (a JointId/handle) is not field-named here (no
    // DWARF/source), so it is modelled as an opaque 8-byte span; the AddEvent/Append bodies remain
    // store-for-store faithful regardless.
    // ⭐ ADDITIVE GROW 2026-08-03 (the ArticulatedJointPool de-fork): the single 8-byte slot is now
    // NAMED, from the image. VehicleOutputRequestInterface::RemoveJoint is INLINED into
    // ArticulatedJointPool::SendCreateRemoveJointEvents @0x826013C0, where the inlined guard is
    //     ld r10, 0(r30) ; cmpld against qword_82F2A3B0  ->  "!lRemoveJointEvent.mId.IsInvalid()"
    // (FireAssert file SharedIO/BrnVehicleOutputInterface.h, line 730). Same u64-vs-JointId note as
    // InAddJoint above. sizeof stays 8; alignof rises 1 -> 8, which changes nothing: on BOTH targets
    // the EventQueue<InRemoveJoint,N> element array already starts at +0x10 (the console because a
    // JointId is 8-aligned, the host because BaseEventQueue<T> is 8+4+4 == 16 bytes).
    struct InRemoveJoint : public Event
    {
        u64 mu64Id;  // @+0x00  "mId" -- stride 8B X360-attested (AddEvent @ 0x825E4208 stdx)
    };

    // Remove a previously-added rigid body from the simulation, addressed by its 8-byte rigid-body
    // id/handle plus a fail-if-not-found flag. Queued with capacities 1 / 50 / 200 across the
    // input/output buffers (X360 EventQueue<InRemoveRigidBody,N>::Construct @ 0x825A7CE8 etc.). The
    // event STRIDE *is* X360-attested: the matching BaseEventQueue<InRemoveRigidBody>::AddEvent
    // @ 0x825E3ED8 stores the element at `slwi r11,r11,4` (miLength*16) via two 8-byte block stores
    // (ld/std r10 @0/@8 == 16 bytes), and Append @ 0x825A3988 block-copies at the same 16-byte stride
    // (`slwi r11,r11,4` == miLength*16, `slwi r5,r29,4` == count*16). This matches the DWARF shape
    // (CgsPhysicsSimulationModuleIO.h:167): { RigidBodyId mID (u64); bool mbFailIfRigidBodyNotFound; }
    // == 8 + 1 padded to 16. So this payload is sized to that attested 16-byte stride. Field names
    // are intentionally NOT invented; it is modelled as an opaque, 16-byte-aligned byte span and the
    // Construct/AddEvent/Append bodies stay store-for-store faithful regardless.
    struct alignas(16) InRemoveRigidBody : public Event
    {
        u8 macOpaquePayload[16];  // stride 16B X360-attested (AddEvent @0x825E3ED8); DWARF { RigidBodyId mID(u64); bool mbFailIfRigidBodyNotFound; }; fields not invented
    };

    // Install/replace the drive "spy" tap (the simulation->game per-frame vehicle-drive report
    // channel). Queued with capacity 1 in PhysicsSimulationIO::InputBuffer (X360
    // EventQueue<InSetDriveSpy,1>::Construct @ 0x828A6618). Only Construct is in scope (no
    // Append/AddEvent to pin the stride, and the InputBuffer::Construct offset map @ 0x828A71B8 that
    // would pin it is not in scope), so the payload is sized only to the 16-byte alignment class the
    // asm proves (`addi r30, r31, 0x10`). Stride/field layout intentionally NOT invented.
    struct alignas(16) InSetDriveSpy : public Event
    {
        u8 macOpaquePayload[16];  // stride NOT recovered; sized to attested 16B alignment only
    };

    // Install/replace the joint "spy" tap (the simulation->game per-frame joint report channel).
    // Queued with capacity 36 in PhysicsSimulationIO::InputBuffer (X360
    // EventQueue<InSetJointSpy,36>::Construct @ 0x828A63E8, capacity 0x24). Only Construct is in
    // scope (no Append/AddEvent to pin the stride, and the InputBuffer::Construct offset map
    // @ 0x828A71B8 that would pin it is not in scope), so the payload is sized only to the 16-byte
    // alignment class the asm proves (`addi r30, r31, 0x10`). Stride/field layout intentionally NOT
    // invented.
    struct alignas(16) InSetJointSpy : public Event
    {
        u8 macOpaquePayload[16];  // stride NOT recovered; sized to attested 16B alignment only
    };

    // Install/replace the rigid-body "spy" tap (the simulation->game per-frame rigid-body report
    // channel). Queued with capacity 200 in PhysicsSimulationIO::InputBuffer (X360
    // EventQueue<InSetRigidBodySpy,200>::Construct @ 0x828A60D8, capacity 0xC8). Only Construct is
    // in scope (no Append/AddEvent to pin the stride, and the InputBuffer::Construct offset map
    // @ 0x828A71B8 that would pin it is not in scope), so the payload is sized only to the 16-byte
    // alignment class the asm proves (`addi r30, r31, 0x10`). Stride/field layout intentionally NOT
    // invented.
    struct alignas(16) InSetRigidBodySpy : public Event
    {
        u8 macOpaquePayload[16];  // stride NOT recovered; sized to attested 16B alignment only
    };

    // Push updated per-frame vehicle-drive dynamics into the simulation. Queued with capacity 1 in
    // PhysicsSimulationIO::InputBuffer (X360 EventQueue<InUpdateDriveDynamics,1>::Construct
    // @ 0x828A65A8). Only Construct is in scope (no Append/AddEvent to pin the stride, and the
    // InputBuffer::Construct offset map @ 0x828A71B8 that would pin it is not in scope), so the
    // payload is sized only to the 16-byte alignment class the asm proves (`addi r30, r31, 0x10`).
    // Stride/field layout intentionally NOT invented.
    struct alignas(16) InUpdateDriveDynamics : public Event
    {
        u8 macOpaquePayload[16];  // stride NOT recovered; sized to attested 16B alignment only
    };

    // Update a constraint joint's frames (anchor transforms) in the simulation. Queued with
    // capacity 36 in PhysicsSimulationIO::InputBuffer (X360
    // EventQueue<InUpdateJointFrames,36>::Construct @ 0x828A6308, capacity 0x24). Only Construct is
    // in scope (no Append/AddEvent to pin the stride, and the InputBuffer::Construct offset map
    // @ 0x828A71B8 that would pin it is not in scope), so the payload is sized only to the 16-byte
    // alignment class the asm proves (`addi r30, r31, 0x10`). Stride/field layout intentionally NOT
    // invented.
    struct alignas(16) InUpdateJointFrames : public Event
    {
        u8 macOpaquePayload[16];  // stride NOT recovered; sized to attested 16B alignment only
    };

    // An output report pushing a rigid body's resolved per-frame state from the simulation back to
    // the game. Queued in PhysicsSimulationIO::OutputBuffer. The event STRIDE *is* X360-attested:
    // the matching BaseEventQueue<OutUpdateRigidBody>::AddEvent @ 0x828A66F8 copies the element at
    // index miLength via `slwi r9,r11,1; add r11,r11,r9` (miLength*3), `slwi r11,r11,6` (*64) ==
    // miLength*192 (a leading 8-byte ld/std of the Event base + a delegated
    // rw::physics::RigidBody::operator=(dest+0x10, src+0x10) for the remainder == a full 192-byte
    // element copy). So this payload is sized to that attested 192-byte stride. This is distinct
    // from the 16-byte-input InUpdateRigidBody/192 above -- do NOT conflate. Internal field layout
    // is still NOT recovered (no DWARF/source), so it is modelled as an opaque, 16-byte-aligned byte
    // span; the AddEvent body stays store-for-store faithful regardless.
    struct alignas(16) OutUpdateRigidBody : public Event
    {
        u8 macOpaquePayload[192];  // stride 192B X360-attested (AddEvent @0x828A66F8); fields not recovered
    };
}
}
