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
    struct alignas(16) InAddJoint : public Event
    {
        u8 macOpaquePayload[192];  // internal layout not recovered (no DWARF/source)
    };

    // Add a vehicle drive. Stride 144 bytes (X360-attested, see above).
    struct alignas(16) InAddDrive : public Event
    {
        u8 macOpaquePayload[144];  // internal layout not recovered (no DWARF/source)
    };

    // Add a rigid body to the simulation. Queued with capacities 1 / 50 / 200 across the
    // input/output buffers (X360 Construct @ 0x825A8228 / 0x825A7C78 / 0x825A7AB8). The
    // per-instantiation Construct bodies are store-for-store faithful regardless of the
    // event's internal layout (they only take &maEvents[0]==this+0x10, store N and clear the
    // count). The event STRIDE is NOT X360-attested by any function reachable here -- the
    // InputBuffer::Construct offset map @ 0x828A71B8 (which would pin it) is not in scope --
    // so the payload is sized only to the 16-byte alignment class the asm proves
    // (`addi r30, r31, 0x10`). Stride/field layout intentionally NOT invented.
    struct alignas(16) InAddRigidBody : public Event
    {
        u8 macOpaquePayload[16];  // stride NOT recovered; sized to attested 16B alignment only
    };

    // Apply a force to a body. Queued with capacity 250 in PhysicsSimulationIO::InputBuffer
    // (X360 Construct @ 0x828A6068). Same recovery caveat as InAddRigidBody: stride NOT
    // X360-attested in scope, payload sized only to the attested 16-byte alignment class.
    struct alignas(16) InApplyForce : public Event
    {
        u8 macOpaquePayload[16];  // stride NOT recovered; sized to attested 16B alignment only
    };

    // Change a rigid body's inertia tensor. Queued with capacity 200 across the input/output
    // buffers (X360 Construct @ 0x825A7B28). Same recovery caveat as InAddRigidBody: stride
    // NOT X360-attested in scope, payload sized only to the attested 16-byte alignment class.
    struct alignas(16) InChangeRigidBodyInertia : public Event
    {
        u8 macOpaquePayload[16];  // stride NOT recovered; sized to attested 16B alignment only
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
    // @ 0x828A5FF8, capacity 0xC8). Same recovery caveat as InAddRigidBody: only Construct is
    // in scope (no Append/AddEvent to pin the stride, and the InputBuffer::Construct offset map
    // @ 0x828A71B8 that would pin it is not in scope), so the payload is sized only to the
    // 16-byte alignment class the asm proves (`addi r30, r31, 0x10`). Stride/field layout
    // intentionally NOT invented.
    struct alignas(16) InUpdateRigidBody : public Event
    {
        u8 macOpaquePayload[16];  // stride NOT recovered; sized to attested 16B alignment only
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
    // EventQueue<OutContactSpy,800>::Construct @ 0x828A68B8, capacity 0x320). Only Construct is in
    // scope (no Append/AddEvent to pin the stride, and the OutputBuffer::Construct offset map that
    // would pin it is not in scope), so the payload is sized only to the 16-byte alignment class
    // the asm proves (`addi r30, r31, 0x10`). Stride/field layout intentionally NOT invented.
    struct alignas(16) OutContactSpy : public Event
    {
        u8 macOpaquePayload[16];  // stride NOT recovered; sized to attested 16B alignment only
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
}
}
