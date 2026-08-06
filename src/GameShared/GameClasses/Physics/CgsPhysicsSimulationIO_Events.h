#pragma once

// Queue-element homes for the CgsPhysics::PhysicsSimulationIO input/output event queues.
// Each event is the payload stored in a CgsModule::EventQueue<EventT, N> member of the
// PhysicsSimulationIO::InputBuffer / OutputBuffer aggregates (X360
// PhysicsSimulationIO::InputBuffer::Construct @ 0x828A71B8 wires one queue per event type
// at fixed byte offsets). This header exists so the per-instantiation Construct TUs can see
// a COMPLETE element type (the queue embeds EventT maEvents[N] inline).
//
// ⛔⛔ RETRACTED 2026-08-04 (task #142). This banner used to open with "No DecFIGS DWARF hint
// covers these event payloads, so their internal field layout is NOT recovered", and that
// sentence was then copied down into SEVENTEEN individual struct banners as
// "fields not recovered (no DWARF/source)". **IT IS FALSE, AND IT WAS FALSE FOR EVERY ONE.**
// `references/DecFIGS/dwarfdump/GameShared/GameClasses/Physics/CgsPhysicsSimulationModuleIO.h`
// declares ALL nineteen In* events and all four Out* events with EVERY FIELD NAMED
// (:69..:315), and it declares all nineteen input queues in memory order with their
// capacities (:330..:459). Task #140 had already caught the claim being false for
// InAddRigidBody and warned "treat the sentence as a per-event claim to check". It was never
// re-checked for the other sixteen; instead the sentence kept spreading by citation. This is
// the fifth false "no DWARF/source exists" claim disproved in this subsystem.
//
// ⚠️⚠️ AND THE CLAIM WAS NOT MERELY COSMETIC -- IT HELD FOUR REAL SIZE BUGS IN PLACE.
// Because the fields were believed unrecoverable, four events were sized "to the 16-byte
// alignment class the asm proves" instead of to their real stride, and nothing gated them:
//       InUpdateJointFrames  16 -> 96 | InUpdateJointLimits    16 -> 80
//       InUpdateDriveFrames  16 -> 80 | InUpdateDriveDynamics  16 -> 48
// All four are fixed below, each with real typed members and a hard size pin.
//
// What IS X360-attested is each event's STRIDE, read off the InputBuffer::Construct offset
// map -- the byte gap between consecutive queues, minus the 16-byte EventQueue base, divided
// by the queue capacity. That map is now COMPLETE for all nineteen input queues: every
// `InputBuffer::GetXxxQueue() const` accessor ends in `addis r3,r28,H` / `addi r3,r3,L`, and
// they form a uniform block at 0x8289E408 + k*0xA8. The full table lives in
// CgsPhysicsSimulationModuleIO.h next to the members it pins. Worked examples:
//   InAddPotentialContact : (189280 - 107344 - 16) / 1024 =  80 bytes
//   InAddJoint            : (196208 - 189280  - 16) /   36 = 192 bytes
//   InAddDrive            : (203632 - 203472  - 16) /    1 = 144 bytes
//   InUpdateJointFrames   : (199984 - 196512  - 16) /   36 =  96 bytes
//   InUpdateJointLimits   : (202880 - 199984  - 16) /   36 =  80 bytes
//   InUpdateDriveFrames   : (203760 - 203664  - 16) /    1 =  80 bytes
//   InUpdateDriveDynamics : (203824 - 203760  - 16) /    1 =  48 bytes
// alignas(16) forces the 12-byte BaseEventQueue base to pad to +0x10 before maEvents, exactly
// the asm's `addi r30, r31, 0x10`; the element size makes sizeof(EventQueue<EventT,N>) match
// the InputBuffer gap. The Construct bodies only take &maEvents[0], store the capacity N and
// clear the count, so they stay store-for-store faithful across these size corrections.
//
// The events still modelled as opaque spans below are opaque BY COST, not by evidence: giving
// them typed members means promoting CgsPhysics::RigidBodyId / JointId / DriveId through ~30
// EventQueue_* TUs, which the tree already (correctly) reasoned is its own change. Their
// DWARF field lists are now recorded in each banner so no future wave has to re-derive them.
#include <cstddef>   // offsetof (the layout pins)

#include "types.hpp"
#include "GameShared/GameClasses/Module/CgsEventQueue.h"
#include "rw/math/vpu/types.h"      // Matrix44Affine / Vector3  (NewRigidBody)
#include "rw/physics/inertia.h"     // rw::physics::Inertia      (NewRigidBody)
#include "rw/physics/rigidbody.h"   // rw::physics::BodyState    (InAddRigidBody::meState)

// ⚠️ NOTE THE TWO SEPARATE rw TREES -- this trips every wave that meets it (task #140 T+3).
// `rw/...` above resolves under `vendor/renderware/include/` (Simulation/RigidBody/Inertia);
// `vendor/renderware/physics/...` below is the OTHER tree, at the submodule root, and is where
// Joint/Drive/Jacobian live. Both are real; they are not duplicates of each other.
#include "vendor/renderware/physics/JointFrames.hpp"     // rw::physics::JointFrames    (80B)
#include "vendor/renderware/physics/JointLimits.hpp"     // rw::physics::JointLimits    (64B)
#include "vendor/renderware/physics/DriveFrames.hpp"     // rw::physics::DriveFrames    (64B)
#include "vendor/renderware/physics/DriveDynamics.hpp"   // rw::physics::DriveDynamics  (32B)

namespace CgsPhysics
{
    // ---- CgsPhysics::NewRigidBody (DWARF CgsRigidBody.h:96) -----------------------------
    // The "please create this body" description the game hands the physics-simulation module
    // inside an InAddRigidBody event. NOT a live body -- rw::physics::RigidBody is that.
    //
    // ⭐ 2026-08-04 (task #140). Field NAMES/TYPES/ORDER are the DWARF's own (CgsRigidBody.h
    // :39..:43); the OFFSETS are X360-attested off the single consumer, PhysicsSimulation-
    // Module::ProcessAddRigidBodyQueue @0x828A2708, and the two agree exactly.
    //
    // ⚠️ HOMED HERE, NOT IN CgsRigidBody.h WHERE THE DWARF PUTS IT -- but ⭐ NOT FOR THE REASON
    // THIS NOTE USED TO GIVE. It said CgsRigidBody.h could not be included from here because
    // CgsPhysicsSimulationModule.h carried a second `struct CgsPhysics::RigidBodyId` and the two
    // would collide with a hard C2011. That fork was retired on 2026-08-04 (task #141) and the
    // C2011 no longer exists. What remains is only that this header is included by ~30
    // EventQueue_* TUs, so re-homing the record is a churn decision, not a blocked one.
    // Same correction applies to InAddJoint's u64 handles below.
    //
    // ⭐ mInertia IS `rw::physics::Inertia`, the vendor type, and since 2026-08-04 (task #141)
    // that is now the ONLY definition of this record in the tree. CgsPhysicsSimulationModule.h
    // used to carry a second copy as `CgsPhysics::Inertia`, which ProcessAddRigidBodyQueue
    // reinterpret_cast'ed to and from on the live AddBody -> AddRigidBody path; it is a typedef
    // onto the vendor class now, and the casts are gone. ⛔ Do not spell this field's type out
    // locally to avoid an include -- a duplicated layout here would link silently against the
    // real one and disagree about every offset.
    struct alignas(16) NewRigidBody
    {
        rw::math::vpu::Matrix44Affine mTransform;        // @+0x00  DWARF :39  (64 bytes)
        rw::math::vpu::Vector3        mVelocity;         // @+0x40  DWARF :40
        rw::math::vpu::Vector3        mAngularVelocity;  // @+0x50  DWARF :41
        rw::physics::Inertia          mInertia;          // @+0x60  DWARF :42  (48 bytes)
        bool                          mbSpy;             // @+0x90  DWARF :43
    };
    static_assert(sizeof(NewRigidBody) == 160, "NewRigidBody 145 bytes -> 160 at align 16 (makes InAddRigidBody 192)");
    static_assert(offsetof(NewRigidBody, mTransform)       == 0,   "mTransform @+0x00 (asm lvx128 event+16..+64)");
    static_assert(offsetof(NewRigidBody, mVelocity)        == 64,  "mVelocity @+0x40 (asm lvx128 event+80 -> mVel)");
    static_assert(offsetof(NewRigidBody, mAngularVelocity) == 80,  "mAngularVelocity @+0x50 (asm lvx128 event+96 -> mOmega)");
    static_assert(offsetof(NewRigidBody, mInertia)         == 96,  "mInertia @+0x60 (asm 6x ld/std from event+112)");
    static_assert(offsetof(NewRigidBody, mbSpy)            == 144, "mbSpy @+0x90 (asm lbz event+160)");

namespace PhysicsSimulationIO
{
    // Empty per-module event base (CgsModule event-queue convention; the queue stores
    // events by byte image), matching the other PhysicsSimulationIO IO event payloads.
    struct Event {};

    // Add a potential narrow-phase contact pair. Stride 80 bytes (X360-attested, see above).
    //
    // ⭐ FIELDS RECOVERED 2026-08-05 (the contact-drain close). The "not recovered (no
    // DWARF/source)" line that stood here was the same false claim retracted in this file's
    // banner: DWARF CgsPhysicsSimulationModuleIO.h:200..:208 names all nine members. TWO
    // INDEPENDENT DERIVATIONS AGREE: the consumer ProcessAddContactQueue @0x828A3458 does
    // `ld 0x30/0x38(event)` into two GetIndexFromGameID calls, three `lvx128` at
    // event+0/+0x10/+0x20, and `lfs 0x40/0x44/0x48(event)` for the frictions/restitution;
    // the DWARF field ORDER lands on exactly those offsets and the total falls out at
    // 0x4C + 4 -> 80, the attested stride.
    //
    // ⚠️ muTag (+0x4C) IS NEVER READ BY THE DRAIN -- it stores its own loop index into
    // Contact::mTag instead (verified: no load of 0x4C(event) anywhere in @0x828A3458).
    // The field itself is real (DWARF :208, and producers write it -- e.g.
    // BridgeSimpleTrafficWithWorldContactsToSimulation @0x825A5618 stores
    // `eventIndex | 0xA000000`); only this consumer ignores it.
    //
    // ⚠️ The two ids stay u64 rather than RigidBodyId for the same documented reason as
    // InAddDrive/InAddJoint -- a ~30-TU type change that wants its own build + boot test.
    struct alignas(16) InAddPotentialContact : public Event
    {
        rw::math::vpu::Vector3 mPointOnA;        // @+0x00  DWARF :200 -- contact point on A
        rw::math::vpu::Vector3 mPointOnB;        // @+0x10  DWARF :201 -- contact point on B
        rw::math::vpu::Vector3 mNormal;          // @+0x20  DWARF :202
        u64                    mIDA;             // @+0x30  DWARF :203 (a CgsPhysics::RigidBodyId handle)
        u64                    mIDB;             // @+0x38  DWARF :204
        f32                    mStaticFriction;  // @+0x40  DWARF :205 -> Contact::mMus
        f32                    mDynamicFriction; // @+0x44  DWARF :206 -> Contact::mMud
        f32                    mRestitution;     // @+0x48  DWARF :207 -> Contact::mRes
        u32                    muTag;            // @+0x4C  DWARF :208 -- see the banner
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
    // ⚠️ TYPED u64, AND THE ORIGINAL REASON EXPIRED ON 2026-08-04 (task #141). The note here
    // used to say these could not be `CgsPhysics::RigidBodyId` / `JointId` because each name was
    // defined TWICE in this tree and pulling either header in would make the fork meet and fail
    // with a hard C2011. That justification is now retired on BOTH counts, and the second count
    // was never true in the first place:
    //   * RigidBodyId -- the fork WAS real and IS now gone: CgsPhysicsSimulationModule.h dropped
    //     its copy and includes CgsRigidBody.h. Measured, not assumed: BrnPhysicsModule.cpp is
    //     mounted, it pulls that chain AND BrnVehicleManager.h, and it compiles clean.
    //   * ⛔ JointId -- THERE WAS NEVER A FORK. CgsRigidBody.h declares exactly one type,
    //     RigidBodyId; it has no JointId at all. The only other `JointId` in the tree is
    //     `BrnPhysics::Vehicle::JointId` in BrnArticulatedJoint.h -- a DIFFERENT NAMESPACE from
    //     `CgsPhysics::JointId` (declared just above the RigidBodyId note in
    //     CgsPhysicsSimulationModule.h). Two distinct types that share a simple name are not an
    //     ODR fork. ⚠️ The same false claim is committed in BrnVehicleOutputInterface.{h,cpp};
    //     it spread BY CITATION, which is how it survived. All sites corrected in one commit,
    //     and the cross-references are now by-note, because every line cite involved had drifted.
    // ⭐ So what actually blocks promoting these three fields is only COST, not a collision:
    // it is a type change across ~30 EventQueue_* TUs and deserves its own build + boot test
    // rather than a ride on the de-fork. The handles are a single u64 in every reading, so the
    // width and the stores are identical meanwhile.
    // sizeof stays 192 and alignof stays 16, gated below.
    //
    // ⭐⭐ FIELDS RECOVERED 2026-08-04 (task #144). The comment that stood on the tail here said
    // "joint frames/limits, not recovered (no DWARF/source)" over a `u8 macOpaquePayload[168]`.
    // That was the **SEVENTH** false "no DWARF/source" claim in this subsystem -- DWARF
    // CgsPhysicsSimulationModuleIO.h:256..:261 names all six members outright, and types the two
    // payloads `NonConstructedClassContainer<rw::physics::JointFrames>` and
    // `<rw::physics::JointLimits>`. ⇒ re-verify every such claim before acting on it.
    //
    // ⭐ TWO INDEPENDENT DERIVATIONS AGREE ON EVERY FIELD, which is the control that makes this
    // safe. The offsets were read out of the CONSUMER'S asm first --
    // `PhysicsSimulationModule::ProcessAddJointQueue` @0x828A40F0 does `ld 0(event)` for the
    // joint id, `ld 8(event)` / `ld 0x10(event)` into two RigidBodyData::GetIndexFromGameID
    // calls, five `lvx128` from event+0x20 (80B == JointFrames), eight `ld/std` from event+0x70
    // (64B == JointLimits) and `lbz 0xB0(event)` for the spy flag -- and only afterwards checked
    // against the DWARF field ORDER. The DWARF order lands on exactly those offsets with nothing
    // invented, and the total falls out at 0xB0 + 1 -> **192, the attested stride**.
    //
    // ⚠️ The ids stay u64 rather than JointId/RigidBodyId for the same documented reason as
    // InAddDrive/InAddRigidBody -- a ~30-TU type change that wants its own build + boot test.
    struct alignas(16) InAddJoint : public Event
    {
        u64                       mu64Id;            // @+0x00  DWARF :256 "mId"           (a CgsPhysics::JointId handle)
        u64                       mu64ParentBodyId;  // @+0x08  DWARF :257 "mParentBodyId" (a CgsPhysics::RigidBodyId handle)
        u64                       mu64ChildBodyId;   // @+0x10  DWARF :258 "mChildBodyId"  (a CgsPhysics::RigidBodyId handle)
        u64                       mu64IdPad;         // @+0x18  pad to the payload's 16-byte slot (as InAddDrive)
        rw::physics::JointFrames  mJointFrames;      // @+0x20  DWARF :259 -- 80B, the FIVE lvx128 lanes the drain copies
        rw::physics::JointLimits  mJointLimits;      // @+0x70  DWARF :260 -- 64B, the EIGHT ld/std the drain copies
        bool                      mbSpy;             // @+0xB0  DWARF :261 -- `lbz 0xB0(event)` -> Joint::m_spy
    };

    // Add a vehicle drive. Stride 144 bytes (X360-attested, see above).
    //
    // ⭐⭐ FIELDS RECOVERED 2026-08-04 (task #143). The comment that stood here said
    // "internal layout not recovered (no DWARF/source)". That was the SIXTH false
    // "no DWARF/source" claim in this subsystem -- DWARF
    // CgsPhysicsSimulationModuleIO.h:261..:279 names all six members outright.
    //
    // ⭐ TWO INDEPENDENT DERIVATIONS AGREE ON EVERY FIELD, which is the control that makes
    // this safe. The offsets below were read out of the CONSUMER'S asm first --
    // `PhysicsSimulationModule::ProcessAddDriveQueue` @0x828A4CB8 does
    // `ld 8(event)` / `ld 0x10(event)` into two RigidBodyData::GetIndexFromGameID calls,
    // `ld 0(event)` for the drive id, four `lvx128` from event+0x20 (64B == DriveFrames),
    // four `ld/std` from event+0x60 (32B == DriveDynamics) and `lbz 0x80(event)` for the
    // spy flag -- and only afterwards checked against the DWARF field ORDER. The DWARF
    // order lands on exactly those offsets with nothing invented, and the total falls out
    // at 0x80 + 1 -> **144, the attested stride**.
    //
    // ⚠️ The ids are typed u64 rather than DriveId/RigidBodyId for the same documented
    // reason as InAddJoint/InAddRigidBody above: promoting them is a ~30-TU type change
    // that wants its own build + boot test. One u64 in both readings.
    struct alignas(16) InAddDrive : public Event
    {
        u64                        mu64Id;            // @+0x00  DWARF :304 "mId"           (a CgsPhysics::DriveId handle)
        u64                        mu64ParentBodyId;  // @+0x08  DWARF :305 "mParentBodyId" (a CgsPhysics::RigidBodyId handle)
        u64                        mu64ChildBodyId;   // @+0x10  DWARF :306 "mChildBodyId"  (a CgsPhysics::RigidBodyId handle)
        u64                        mu64IdPad;         // @+0x18  pad to the payload's 16-byte slot (see note on InUpdateDriveFrames)
        rw::physics::DriveFrames   mDriveFrames;      // @+0x20  DWARF :307 -- 4 lanes, the 4 lvx128 the drain copies
        rw::physics::DriveDynamics mDriveDynamics;    // @+0x60  DWARF :308 -- 32B, the 4 ld/std the drain copies
        bool                       mbSpy;             // @+0x80  DWARF :309 -- `lbz 0x80(event)` -> Drive::m_spy
    };

    // Add a rigid body to the simulation. Queued with capacities 1 / 50 / 200 across the
    // input/output buffers (X360 Construct @ 0x825A8228 / 0x825A7C78 / 0x825A7AB8). The
    // event STRIDE *is* X360-attested: the matching BaseEventQueue<InAddRigidBody>::AddEvent
    // @ 0x825A3000 copies each element with `li r5,0xC0; memcpy` (192-byte Size) at a
    // 192-byte stride (`slwi r9,r11,1; add r11,r11,r9; slwi r11,r11,6` == miLength*192), and
    // Append @ 0x825A3898 block-copies at the same 192-byte stride
    // (`slwi r9,r29,1; add r9,r29,r9; slwi r5,r9,6` == count*192).
    //
    // ⭐⭐ 2026-08-04 (task #140) -- THE FIELDS ARE REAL NOW, AND THE COMMENT THAT USED TO
    // STAND HERE WAS FALSE. It said "Internal field layout is still NOT recovered (no DWARF/
    // source)". A DecFIGS DWARF hint covers this payload and its sub-record exactly, and this
    // is the FOURTH "no DWARF exists for this" claim in the physics subsystem to be disproved
    // by simply looking -- see the same retraction in rw/physics/simulation.h.
    //
    //   DWARF CgsPhysicsSimulationModuleIO.h:67
    //     struct InAddRigidBody : public Event {
    //         RigidBodyId mID;                                                    :69
    //         NonConstructedClassContainer<CgsPhysics::NewRigidBody> mRigidBody;  :70
    //         rw::physics::BodyState meState;                                     :71 }
    //   DWARF CgsRigidBody.h:96  -> struct CgsPhysics::NewRigidBody, below.
    //
    // ⭐ TWO INDEPENDENT DERIVATIONS AGREE. The offsets below were ALSO read straight out of
    // the consumer's asm -- PhysicsSimulationModule::ProcessAddRigidBodyQueue @0x828A2708
    // loads six vectors from event+16/+32/+48/+64 (the transform), +80 and +96 (the two
    // velocities), copies 48 bytes from +112 (the inertia), reads a byte at +160 (mbSpy) and
    // a word at +176 (meState). The DWARF field order lands on exactly those offsets, and the
    // total falls out at 176 + 4 -> **192, the attested stride**, with nothing invented.
    //
    // ⚠️ mID IS TYPED u64, NOT CgsPhysics::RigidBodyId -- the same deliberate choice as
    // InAddJoint's three handles above, but ⭐ NO LONGER FOR THE SAME REASON. RigidBodyId was
    // defined twice in this tree (CgsRigidBody.h:24 and CgsPhysicsSimulationModule.h) and
    // pulling either header in would have been a hard C2011; the fork was retired on 2026-08-04
    // (task #141) and that collision is gone. Promoting this field is now merely a ~30-TU type
    // change that wants its own build + boot test. It is one u64 in both readings, so the width
    // and the stores are identical meanwhile.
    struct alignas(16) InAddRigidBody : public Event
    {
        u64          mID;          // @+0x00  DWARF :69 (a CgsPhysics::RigidBodyId handle)
        NewRigidBody mRigidBody;   // @+0x10  DWARF :70 (NonConstructedClassContainer -- raw storage)
        rw::physics::BodyState meState;  // @+0xB0 (176) DWARF :71
    };
    static_assert(sizeof(InAddRigidBody) == 192, "InAddRigidBody stride 192 (AddEvent @0x825A3000)");
    static_assert(offsetof(InAddRigidBody, mID)       == 0,   "mID @+0x00 (asm ld 0(event))");
    static_assert(offsetof(InAddRigidBody, mRigidBody) == 16, "mRigidBody @+0x10 (asm lvx128 from event+16)");
    static_assert(offsetof(InAddRigidBody, meState)   == 176, "meState @+0xB0 (asm lwz 0xB0(event))");

    // Apply a force to a body. Queued with capacity 250 in PhysicsSimulationIO::InputBuffer
    // (X360 Construct @ 0x828A6068). The event STRIDE *is* X360-attested: the matching
    // BaseEventQueue<InApplyForce>::AddEvent @ 0x825E3CC8 and AddEventSafe @ 0x825E3E20 each
    // copy an element as exactly four 64-bit block moves (ld/std x4 == 32 bytes) at a 32-byte
    // stride (`slwi r11,r11,5` == miLength*32).
    //
    // ⭐ FIELDS RECOVERED 2026-08-05 (the rigid-body drain group). The "fields not recovered
    // (no DWARF/source)" sentence that stood here was the same false claim as its sixteen
    // siblings -- DWARF CgsPhysicsSimulationModuleIO.h:102/:103 names both members. TWO
    // INDEPENDENT DERIVATIONS AGREE: the consumer ProcessApplyForceQueue @0x828A6B80 reads
    // `ld 0(event)` into RigidBodyData::GetIndexFromGameID and `lvx128 v0, r30, 0x10` as the
    // force vector, and the DWARF order lands on exactly those offsets. 8 + pad-to-16 + 16
    // == 32, the attested stride, nothing invented.
    // ⚠️ mID stays u64, not CgsPhysics::RigidBodyId -- the same documented ~30-TU cost
    // decision as every sibling event.
    struct alignas(16) InApplyForce : public Event
    {
        u64                    mID;     // @+0x00  DWARF :102 (a CgsPhysics::RigidBodyId handle)
        rw::math::vpu::Vector3 mForce;  // @+0x10  DWARF :103 -- the drain's `lvx128 v0,r30,0x10`
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
    // agnostic).
    //
    // ⭐ FIELDS RECOVERED 2026-08-05 (the rigid-body drain group). DWARF
    // CgsPhysicsSimulationModuleIO.h:118..:120 names all three members
    // ({ RigidBodyId mID; NonConstructedClassContainer<rw::physics::Inertia> mInertia;
    // uint32_t mu32Flags; }), and the consumer ProcessChangeRigidBodyInertiaQueue @0x828A4A78
    // pins every offset: `ld 0(event)` for the id, the flags==0x3F fast path copies SIX
    // 64-bit words from event+0x10 (== the 48-byte Inertia), and every flag test reads
    // `lwz 0x40(event)`. 0x40 + 4 -> pads to 80, the attested stride. Per-bit consumer map
    // (each bit copies ONE Inertia field; the module .cpp quotes the offsets):
    //     bit0 mAngularDrag  bit1 mInvTens(+recompute mSpherical)  bit2 mInvMass
    //     bit3 mLinearDrag   bit4 mMaxOmega                        bit5 mMaxVelocity
    // ⚠️ The DWARF's NonConstructedClassContainer means RAW STORAGE on the console; embedding
    // the Inertia by value runs its default ctor per queue slot on the PC. Behaviourally
    // invisible (AddEvent overwrites the slot before any GetEvent reads it) -- the same
    // accepted cost as InUpdateJointLimits' embedded JointLimits.
    struct alignas(16) InChangeRigidBodyInertia : public Event
    {
        u64                  mID;        // @+0x00  DWARF :118 (a CgsPhysics::RigidBodyId handle)
        u64                  mIDPad;     // @+0x08  pad to the payload's 16-byte slot
        rw::physics::Inertia mInertia;   // @+0x10  DWARF :119 -- 48B, the 6x ld/std fast path
        u32                  mu32Flags;  // @+0x40  DWARF :120 -- `lwz 0x40(event)`, the bit map above
    };

    // Remove a previously-added vehicle drive from the simulation (symmetric partner of
    // InAddDrive). Queued with capacity 1 in PhysicsSimulationIO::InputBuffer (X360
    // EventQueue<InRemoveDrive,1>::Construct @ 0x828A64C8). Only Construct is in scope (no
    // Append/AddEvent to pin the stride, and the InputBuffer::Construct offset map @ 0x828A71B8
    // that would pin it is not in scope), so the payload is sized only to the 16-byte alignment
    // class the asm proves (addi r30, r31, 0x10).
    //
    // ⭐ FIELD RECOVERED 2026-08-04 (task #143). DWARF CgsPhysicsSimulationModuleIO.h:283..:288
    // declares exactly ONE member, and the consumer agrees: `ProcessRemoveDriveQueue`
    // @0x8289FF98 issues a single `ld 0(event)` and reads nothing else out of the element.
    // The stride 16 already committed is unchanged (u64 under alignas(16)).
    struct alignas(16) InRemoveDrive : public Event
    {
        u64 mu64Id;  // @+0x00  DWARF :286 "mId" (a CgsPhysics::DriveId handle) -- the drain's only read
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
    // (`mulli r5,r29,0x70`, `mulli r11,r11,0x70` == count*0x70 == count*112).
    //
    // ⭐ FIELDS RECOVERED 2026-08-05 (the rigid-body drain group). DWARF
    // CgsPhysicsSimulationModuleIO.h:151..:154 names all four members, and the consumer
    // ProcessUpdateExternalBodyQueue @0x828A3B30 pins every offset: `ld 0(event)` for the id,
    // four `lvx128` rows from event+0x10..+0x40 into the body's basis/position (the inlined
    // RigidBody::SetTransform), `lvx128` from +0x50 into mVel(+0x20) and from +0x60 into
    // mOmega(+0x30). 0x60 + 16 == 112, the attested stride, nothing invented.
    struct alignas(16) InUpdateExternalBody : public Event
    {
        u64                           mID;          // @+0x00  DWARF :151 (a CgsPhysics::RigidBodyId handle)
        u64                           mIDPad;       // @+0x08  pad to the payload's 16-byte slot
        rw::math::vpu::Matrix44Affine mTransform;   // @+0x10  DWARF :152 -- 64B, the four SetTransform rows
        rw::math::vpu::Vector3        mVel;         // @+0x50  DWARF :153 -- `lvx128 v0,r30,0x50` -> body mVel
        rw::math::vpu::Vector3        mAngularVel;  // @+0x60  DWARF :154 -- `lvx128 v0,r30,0x60` -> body mOmega
    };

    // Push updated per-frame vehicle drive state into the simulation. Queued with capacity 1
    // in PhysicsSimulationIO (X360 Construct @ 0x828A6538).
    //
    // ⛔⛔ SIZE CORRECTED 2026-08-04 (task #142): 16 -> 80, and this one has the STRONGEST
    // witness of the four -- its own consumer copies the payload lane by lane.
    // `PhysicsSimulationModule::ProcessUpdateDriveFramesQueue` @0x8289FC28 reads the id as
    // `*result` (event+0), then sets `_R26=16,_R27=32,_R28=48`, takes `_R11 = event + 0x10`
    // and issues FOUR `lvx128`/`stvx128` pairs at 0/16/32/48 -- **exactly 64 bytes, from
    // +0x10** -- into a destination stepped by `v12 << 6` (a 64-byte-stride array). 64 bytes
    // at +0x10 is 80. Cross-checked by the queue chain: GetUpdateDriveFramesQueue @0x8289ED38
    // == +203664, GetUpdateDriveDynamicsQueue @0x8289EDE0 == +203760, (203760-203664-16)/1 = 80.
    //
    // DWARF CgsPhysicsSimulationModuleIO.h:290..:295: { DriveId mId; DriveFrames mDriveFrames; }
    // -- note the payload is held BY VALUE here, not in a NonConstructedClassContainer, which
    // is why the drain can `stvx128` straight into it. rw::physics::DriveFrames is 4x16 == 64.
    struct alignas(16) InUpdateDriveFrames : public Event
    {
        u64                     mu64Id;         // @+0x00  "mId" (a CgsPhysics::DriveId handle; u64 -- see InAddJoint)
        u64                     mu64IdPad;      // @+0x08  pad to the payload's 16-byte slot
        rw::physics::DriveFrames mDriveFrames;   // @+0x10  DWARF :295 -- the 4 lanes the drain copies
    };

    // Update a rigid body's per-frame state in the simulation. Queued with capacity 200 in
    // PhysicsSimulationIO::InputBuffer (X360 EventQueue<InUpdateRigidBody,200>::Construct
    // @ 0x828A5FF8, capacity 0xC8). The X360 event STRIDE is attested: the matching
    // BaseEventQueue<InUpdateRigidBody>::AddEvent @ 0x82614928 copies the element at index
    // miLength via `slwi r9,r11,1; add r11,r11,r9` (miLength*3), `slwi r11,r11,6` (*64) ==
    // miLength*192, and the GetEvent instantiation @0x8289D620 indexes with the same *192.
    //
    // ⭐ FIELDS RECOVERED 2026-08-05 (the rigid-body drain group). DWARF
    // CgsPhysicsSimulationModuleIO.h:86/:87 names both members, and the payload type is the
    // FULL rw::physics::RigidBody BY VALUE (the :87 member is typed through the CgsRigidBody.h:33
    // `typedef RigidBody RigidBody` alias): X360 16 (id slot) + 176 (console RigidBody) == 192.
    // The consumer ProcessUpdateRigidBodyQueue @0x828A3A08 pins both offsets: `ld 0(event)`
    // for the id, RigidBody::operator=(body, event+0x10) for the payload, plus the state
    // compare `lwz 0x9C(event)` == (event+0x10)+0x8C == mRigidBody.mState.
    //
    // ⚠️⚠️ THE HOST STRIDE IS NOT 192, AND MUST NOT BE PINNED TO 192. rw::physics::RigidBody
    // carries five pointer lanes that widen on x64 (the ten packed w-lane scalars are real
    // members on the PC -- see rigidbody.h's banner), so sizeof(InUpdateRigidBody) grows with
    // it. This is a RUNTIME queue element, not a serialized record, so [[serialized slots
    // stay 32-bit]] does NOT apply; the queue machinery is sizeof-driven end to end. The pin
    // below is therefore the ADJACENCY form (16 + sizeof(RigidBody)) -- on the console that
    // evaluates to the attested 192, on the host it tracks the widened body -- plus the
    // offsetof pin that catches a transposed id/payload.
    struct alignas(16) InUpdateRigidBody : public Event
    {
        u64                    mID;         // @+0x00  DWARF :86 (a CgsPhysics::RigidBodyId handle)
        u64                    mIDPad;      // @+0x08  pad to the payload's 16-byte slot
        rw::physics::RigidBody mRigidBody;  // @+0x10  DWARF :87 -- BY VALUE, the operator= source
    };

    // Update a constraint joint's limits in the simulation. Queued with capacity 36 in
    // PhysicsSimulationIO::InputBuffer (X360 EventQueue<InUpdateJointLimits,36>::Construct
    // @ 0x828A6378, capacity 0x24).
    //
    // ⛔⛔ SIZE CORRECTED 2026-08-04 (task #142): 16 -> 80. The previous note claimed the stride
    // was unrecoverable because "the InputBuffer::Construct offset map @0x828A71B8 that would
    // pin it is not in scope". THE OFFSET MAP WAS NEVER NEEDED -- the two neighbouring
    // `InputBuffer::GetXxxQueue() const` accessors pin it directly and they were always
    // readable: GetUpdateJointLimitsQueue @0x8289EA98 ends `addis r3,r28,3 / addi r3,r3,0xD30`
    // == +199984, and GetSetJointSpyQueue @0x8289EB40 ends `addis 3 / 0x1880` == +202880.
    // (202880 - 199984 - 16) / 36 == 80. The class was 64 bytes too small and NOTHING GATED IT.
    //
    // DWARF CgsPhysicsSimulationModuleIO.h:241..:246 names both fields:
    //     { JointId mId;  NonConstructedClassContainer<rw::physics::JointLimits> mJointLimits; }
    // and the tree's own rw::physics::JointLimits is exactly the 64 bytes that leaves
    // (Vector3 mPprism, Vector3 mVprism, 6x f32, SwingType, TwistType). The id occupies the
    // leading 16-byte slot, the payload starts at +0x10 -- the same {id, payload@+0x10} shape
    // its two siblings' drain asm proves outright (see InUpdateJointFrames below).
    struct alignas(16) InUpdateJointLimits : public Event
    {
        u64                     mu64Id;         // @+0x00  "mId" (a CgsPhysics::JointId handle; u64 -- see InAddJoint)
        u64                     mu64IdPad;      // @+0x08  pad to the payload's 16-byte slot
        rw::physics::JointLimits mJointLimits;   // @+0x10  DWARF :246
    };

    // An output "spy" report of a resolved contact, drained from the simulation back to the game.
    // Queued with capacity 800 in PhysicsSimulationIO::OutputBuffer (X360
    // EventQueue<OutContactSpy,800>::Construct @ 0x828A68B8, capacity 0x320). The event STRIDE
    // *is* X360-attested: the matching BaseEventQueue<OutContactSpy>::AddEvent @ 0x825E44C8
    // and AddEventSafe @ 0x828A1B78 both copy the element with `li r9,0xE`/`mtctr r9` (14 ld/std
    // 64-bit block moves == 14*8 == 112 bytes) at a 112-byte stride (`mulli rX,rX,0x70` ==
    // miLength*112).
    //
    // ⭐ FIELDS RECOVERED 2026-08-06 (the game-side spy wave). The "no DWARF" clause of the old
    // note here was FALSE -- the DWARF (CgsPhysicsSimulationModuleIO.h:373, :384..:391) names
    // all eight members, and the PRODUCER pins every offset:
    // PhysicsSimulationModule::AddContactSpiesToOutputQueue @0x828A4ED8 builds the event on its
    // stack frame and each store lands where the DWARF order puts it --
    //     +0x00 mFrictionStress <- spy mForceT * ts   (stvx var_110)
    //     +0x10 mNormalStress   <- spy mForceN * ts   (stvx var_100)
    //     +0x20 mNormal         <- drain event +0x20  (stvx var_E0)
    //     +0x30 mPointOnA       <- drain event +0x00  (stvx var_E0+0x10... the four rows land
    //     +0x40 mPointOnB       <- drain event +0x10   in DWARF field order)
    //     +0x50 mIDA / +0x58 mIDB  (`std` x2 -- RigidBodyId handles resolved via body mTag)
    //     +0x60 muTag           <- drain event +0x4C  (`stw`)
    // 5*16 + 8 + 8 + 4 == 100 -> alignas(16) == the attested 112; pointer-free, host == console.
    // The DWARF's SwingType-style helper methods (SwapEntityOrder :379 / Clear :382) have no
    // X360 body or caller in the image and are not declared (the standing gating rule).
    struct alignas(16) OutContactSpy : public Event
    {
        rw::math::vpu::Vector3 mFrictionStress;  // @+0x00  DWARF :384 -- tangential impulse / ts
        rw::math::vpu::Vector3 mNormalStress;    // @+0x10  DWARF :385 -- normal impulse / ts
        rw::math::vpu::Vector3 mNormal;          // @+0x20  DWARF :386 -- the drain event's mNormal
        rw::math::vpu::Vector3 mPointOnA;        // @+0x30  DWARF :387 -- the drain event's mPointOnA
        rw::math::vpu::Vector3 mPointOnB;        // @+0x40  DWARF :388 -- the drain event's mPointOnB
        u64                    mIDA;             // @+0x50  DWARF :389 (a CgsPhysics::RigidBodyId handle)
        u64                    mIDB;             // @+0x58  DWARF :390
        u32                    muTag;            // @+0x60  DWARF :391 -- the drain event's muTag
    };

    // An output "spy" report of per-frame vehicle drive state, drained from the simulation back to
    // the game. Queued with capacity 1 in PhysicsSimulationIO::OutputBuffer (X360
    // EventQueue<OutDriveSpy,1>::Construct @ 0x828A6998). The event STRIDE *is* X360-attested:
    // the matching BaseEventQueue<OutDriveSpy>::AddEvent @ 0x828A1D90 copies each element as exactly
    // eight 64-bit block moves at a 64-byte stride (`slwi r10,r10,6` == miLength*64).
    //
    // ⭐ FIELDS RECOVERED 2026-08-06 (the game-side spy wave) -- the "no DWARF" clause of the old
    // note was FALSE: the DWARF (CgsPhysicsSimulationModuleIO.h:421..:427) names all five members,
    // and the PRODUCER pins each offset -- PhysicsSimulationModule::AddDriveSpiesToOutputQueue
    // @0x828A5D10 stores `std id` @+0x00, `stvx` spy mForce @+0x10, `stvx` spy mTorque @+0x20,
    // `stfs` spy mSeparation @+0x30, `stfs` spy mAngSeparation @+0x34 (frame slots var_A0/-90/-80/
    // -70/-6C). 16 + 16 + 16 + 4 + 4 -> alignas(16) == the attested 64; pointer-free.
    struct alignas(16) OutDriveSpy : public Event
    {
        u64                    mID;                    // @+0x00  DWARF :423 (a CgsPhysics::DriveId handle)
        u64                    mIDPad;                 // @+0x08  pad to the payload's 16-byte slot
        rw::math::vpu::Vector3 mLinearStress;          // @+0x10  DWARF :424 -- the spy record's mForce row
        rw::math::vpu::Vector3 mAngularStress;         // @+0x20  DWARF :425 -- the spy record's mTorque row
        f32                    mLinearDistanceToKey;   // @+0x30  DWARF :426 -- the spy's mSeparation
        f32                    mAngularDistanceToKey;  // @+0x34  DWARF :427 -- the spy's mAngSeparation
    };

    // An output "spy" report of a resolved constraint joint, drained from the simulation back to the
    // game. Queued with capacity 64 in PhysicsSimulationIO::OutputBuffer (X360
    // EventQueue<OutJointSpy,64>::Construct @ 0x828A6928, capacity 0x40). The event STRIDE *is*
    // X360-attested: the matching BaseEventQueue<OutJointSpy>::AddEvent @ 0x828A1C30 copies each
    // element as exactly six 64-bit block moves at a 48-byte stride (`miLength*3` then `*16`).
    //
    // ⭐ FIELDS RECOVERED 2026-08-06 (the game-side spy wave) -- the "no DWARF" clause of the old
    // note was FALSE: the DWARF (CgsPhysicsSimulationModuleIO.h:404..:408) names all three members,
    // and the PRODUCER pins each offset -- PhysicsSimulationModule::AddJointSpiesToOutputQueue
    // @0x828A58E0 stores `std id` @+0x00 (var_E0), `stvx` spy mForce * (ts*59.999996f) @+0x10
    // (var_D0), `stvx` spy mTorque * (ts*59.999996f) @+0x20 (var_C0).
    // 16 + 16 + 16 == the attested 48; pointer-free, host == console.
    struct alignas(16) OutJointSpy : public Event
    {
        u64                    mID;             // @+0x00  DWARF :406 (a CgsPhysics::JointId handle)
        u64                    mIDPad;          // @+0x08  pad to the payload's 16-byte slot
        rw::math::vpu::Vector3 mLinearStress;   // @+0x10  DWARF :407 -- spy mForce  * (ts * 59.999996f)
        rw::math::vpu::Vector3 mAngularStress;  // @+0x20  DWARF :408 -- spy mTorque * (ts * 59.999996f)
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
    // == 8 + 1 padded to 16.
    //
    // ⭐ FIELDS PROMOTED 2026-08-05 (the rigid-body drain group). The names were never invented --
    // they were already quoted from the DWARF in the note above; what was missing was a CONSUMER,
    // and ProcessRemoveRigidBodyQueue @0x828A2BD0 now pins both offsets: `ld 0(event)` feeds
    // RigidBodyData::GetIndexFromGameID and `lbz 8(event)` gates the "Couldn't find rigid body
    // with id" assert (the not-found-tolerant remove).
    struct alignas(16) InRemoveRigidBody : public Event
    {
        u64  mID;                       // @+0x00  DWARF :169 (a CgsPhysics::RigidBodyId handle)
        bool mbFailIfRigidBodyNotFound; // @+0x08  DWARF :170 -- `lbz 8(event)`
    };

    // Install/replace the drive "spy" tap (the simulation->game per-frame vehicle-drive report
    // channel). Queued with capacity 1 in PhysicsSimulationIO::InputBuffer (X360
    // EventQueue<InSetDriveSpy,1>::Construct @ 0x828A6618). Only Construct is in scope (no
    // Append/AddEvent to pin the stride, and the InputBuffer::Construct offset map @ 0x828A71B8 that
    // would pin it is not in scope), so the payload is sized only to the 16-byte alignment class the
    // asm proves (`addi r30, r31, 0x10`).
    //
    // ⭐ FIELDS RECOVERED 2026-08-04 (task #143). DWARF CgsPhysicsSimulationModuleIO.h:310..:317
    // names both members, and the consumer pins both offsets: `ProcessSetDriveSpyQueue`
    // @0x8289FE88 does `ld 0(event)` for the id and `lbz 8(event)` for the flag. The u64 id
    // puts the bool at +0x08 exactly. Stride 16 unchanged.
    //
    // ⚠️ The drain stores that byte with a PLAIN `stw` into Drive::m_spy (+0x1C) -- a whole
    // word, not a bit. This is NOT the `ori 8` / `clrlwi ...,29` bitfield fork that the
    // rw::physics `SetSpy(bool)` sibling compiles to; do not import that shape here.
    struct alignas(16) InSetDriveSpy : public Event
    {
        u64  mu64Id;  // @+0x00  DWARF :313 "mId" (a CgsPhysics::DriveId handle)
        bool mbSpy;   // @+0x08  DWARF :316 -- `lbz 8(event)`, stored whole-word into Drive::m_spy
    };

    // Install/replace the joint "spy" tap (the simulation->game per-frame joint report channel).
    // Queued with capacity 36 in PhysicsSimulationIO::InputBuffer (X360
    // EventQueue<InSetJointSpy,36>::Construct @ 0x828A63E8, capacity 0x24).
    //
    // ⭐ FIELDS RECOVERED 2026-08-04 (task #144). This was a `u8 macOpaquePayload[16]` whose note
    // read "stride/field layout intentionally NOT invented" -- correctly cautious at the time,
    // because no Append/AddEvent was in scope to pin it. The CONSUMER settles it instead:
    // ProcessSetJointSpyQueue @0x8289F768 reads `ld 0(event)` for the id and `lbz 8(event)` for
    // the flag, and the DWARF (CgsPhysicsSimulationModuleIO.h:293-294) names exactly those two
    // members in that order. The 16-byte stride was already pinned by the queue-offset chain.
    //
    // ⚠️ The drain stores that byte with a PLAIN `stw` into Joint::m_spy (+0x1C) -- a whole word,
    // not a bit. This is NOT the `ori 8` / `clrlwi ...,29` bitfield fork that the rw::physics
    // `SetSpy(bool)` sibling compiles to; do not import that shape here. Same as the drive twin.
    struct alignas(16) InSetJointSpy : public Event
    {
        u64  mu64Id;  // @+0x00  DWARF :293 "mId" (a CgsPhysics::JointId handle)
        bool mbSpy;   // @+0x08  DWARF :294 -- `lbz 8(event)`, stored whole-word into Joint::m_spy
    };

    // Install/replace the rigid-body "spy" tap (the simulation->game per-frame rigid-body report
    // channel). Queued with capacity 200 in PhysicsSimulationIO::InputBuffer (X360
    // EventQueue<InSetRigidBodySpy,200>::Construct @ 0x828A60D8, capacity 0xC8). The 16-byte
    // stride is pinned by the queue-offset chain (see the stride-pin block below).
    //
    // ⭐ FIELDS RECOVERED 2026-08-05 (the rigid-body drain group). Same settlement as
    // InSetJointSpy one group earlier: the CONSUMER pins it -- ProcessSetRigidBodySpyQueue
    // @0x828A49A8 reads `ld 0(event)` for the id and `lbz 8(event)` for the flag, and the
    // DWARF (CgsPhysicsSimulationModuleIO.h:135/:136) names exactly those two members.
    // ⚠️ UNLIKE the joint/drive twins, the payload feeds the REAL RigidBody::SetSpy bitfield
    // fork -- `ori r11,r11,8` / `clrlwi r11,r11,29` on mState(+0x8C) -- not a plain whole-word
    // spy store. The committed rw/physics/rigidbody.h SetSpy already carries that exact shape.
    // ⚠️ DWARF spells this one `mSpy`, not `mbSpy`; the DWARF's own name is kept.
    struct alignas(16) InSetRigidBodySpy : public Event
    {
        u64  mID;   // @+0x00  DWARF :135 (a CgsPhysics::RigidBodyId handle)
        bool mSpy;  // @+0x08  DWARF :136 -- `lbz 8(event)` -> RigidBody::SetSpy(bool)
    };

    // Push updated per-frame vehicle-drive dynamics into the simulation. Queued with capacity 1 in
    // PhysicsSimulationIO::InputBuffer (X360 EventQueue<InUpdateDriveDynamics,1>::Construct
    // @ 0x828A65A8).
    //
    // ⛔⛔ SIZE CORRECTED 2026-08-04 (task #142): 16 -> 48. Pinned by its two neighbouring
    // accessors: GetUpdateDriveDynamicsQueue @0x8289EDE0 == +203760 and GetSetDriveSpyQueue
    // @0x8289EE88 == +203824, so (203824 - 203760 - 16) / 1 == 48.
    // ⚠️ GetSetDriveSpyQueue is an .ida-exports HOLE and was recovered headless from
    // BURNOUT_X360_ARTIST.XEX.i64 for this pin (task #142) -- see [[ida-export-set-has-holes]].
    //
    // DWARF CgsPhysicsSimulationModuleIO.h:300..:305:
    //     { DriveId mId; NonConstructedClassContainer<rw::physics::DriveDynamics> mDriveDynamics; }
    // rw::physics::DriveDynamics is two 16-byte Params (mLinear, mAngular) == 32; + the leading
    // 16-byte id slot == 48.
    struct alignas(16) InUpdateDriveDynamics : public Event
    {
        u64                        mu64Id;           // @+0x00  "mId" (a CgsPhysics::DriveId handle; u64 -- see InAddJoint)
        u64                        mu64IdPad;        // @+0x08  pad to the payload's 16-byte slot
        rw::physics::DriveDynamics mDriveDynamics;    // @+0x10  DWARF :305
    };

    // Update a constraint joint's frames (anchor transforms) in the simulation. Queued with
    // capacity 36 in PhysicsSimulationIO::InputBuffer (X360
    // EventQueue<InUpdateJointFrames,36>::Construct @ 0x828A6308, capacity 0x24).
    //
    // ⛔⛔ SIZE CORRECTED 2026-08-04 (task #142): 16 -> 96, the largest of the four errors.
    // Witnessed the same way as InUpdateDriveFrames, by its own drain:
    // `PhysicsSimulationModule::ProcessUpdateJointFramesQueue` @0x8289F2F0 reads the id as
    // `*result` (event+0) and sets `_R17=16,_R18=32,_R19=48,_R20=64` -- FIVE 16-byte lanes at
    // 0/16/32/48/64 from the payload base == **80 bytes**, one more lane than the Drive
    // sibling, because rw::physics::JointFrames carries a fifth quaternion (mQuatL, the parent
    // LINEAR frame) that DriveFrames does not. 80 bytes at +0x10 is 96.
    // Cross-checked by the queue chain: GetUpdateJointFramesQueue @0x8289E9F0 == +196512,
    // GetUpdateJointLimitsQueue @0x8289EA98 == +199984, (199984 - 196512 - 16) / 36 == 96.
    //
    // DWARF CgsPhysicsSimulationModuleIO.h:231..:236: { JointId mId; JointFrames mJointFrames; }
    // -- by value, like InUpdateDriveFrames and unlike the two Limits/Dynamics siblings.
    struct alignas(16) InUpdateJointFrames : public Event
    {
        u64                     mu64Id;         // @+0x00  "mId" (a CgsPhysics::JointId handle; u64 -- see InAddJoint)
        u64                     mu64IdPad;      // @+0x08  pad to the payload's 16-byte slot
        rw::physics::JointFrames mJointFrames;   // @+0x10  DWARF :236 -- the 5 lanes the drain copies
    };

    // An output report pushing a rigid body's resolved per-frame state from the simulation back to
    // the game. Queued in PhysicsSimulationIO::OutputBuffer. The console STRIDE is X360-attested:
    // the matching BaseEventQueue<OutUpdateRigidBody>::AddEvent @ 0x828A66F8 copies the element at
    // miLength*192 -- a leading 8-byte ld/std of the Event base + a delegated
    // rw::physics::RigidBody::operator=(dest+0x10, src+0x10) for the remainder.
    //
    // ⭐ FIELDS RECOVERED 2026-08-06 (the game-side spy wave) -- the "no DWARF" clause of the old
    // note was FALSE: the DWARF (CgsPhysicsSimulationModuleIO.h:356..:359) names both members and
    // types the payload through CgsRigidBody.h:33's `typedef RigidBody RigidBody` alias == the FULL
    // rw::physics::RigidBody BY VALUE, exactly InUpdateRigidBody's shape (see it above). The two
    // PRODUCERS pin both offsets: PhysicsSimulationModule::AddActiveBodiesToOutputQueue @0x828A6D90
    // and ActivateAndFreezeAsNeeded @0x828A6F94 each build the event as
    // `RigidBody::operator=(frame+0x10, body)` then `std gameId, frame+0x00`.
    //
    // ⚠️⚠️ THE HOST STRIDE IS NOT 192 AND MUST NOT BE PINNED TO 192 -- same rule, same reason as
    // InUpdateRigidBody (the payload's five pointer lanes widen on x64; runtime queue element, not
    // a serialized record; the queue machinery is sizeof-driven end to end). The pin below is the
    // ADJACENCY form: 16 (id slot) + sizeof(RigidBody) == the attested 192 on the console's 4-byte
    // ABI, and it tracks the widened body on the host.
    struct alignas(16) OutUpdateRigidBody : public Event
    {
        u64                    mID;         // @+0x00  DWARF :358 (a CgsPhysics::RigidBodyId handle)
        u64                    mIDPad;      // @+0x08  pad to the payload's 16-byte slot
        rw::physics::RigidBody mRigidBody;  // @+0x10  DWARF :359 -- BY VALUE, the operator= target
    };

    // =====================================================================================
    // ⭐⭐ THE STRIDE PINS -- ALL NINETEEN INPUT EVENTS, 2026-08-04 (task #142).
    //
    // WHY THIS BLOCK EXISTS. Before today exactly two of these types were gated
    // (InAddRigidBody and NewRigidBody). The other seventeen were ungated opaque spans, and
    // FOUR OF THEM WERE WRONG -- InUpdateJointFrames/JointLimits/DriveFrames/DriveDynamics
    // were all 16 bytes when they are 96/80/80/48. A wrong stride here is invisible: the type
    // compiles, every EventQueue<T,N>::Construct over it stays "store-for-store faithful"
    // (Construct only touches &maEvents[0] and the count), and the error only surfaces as a
    // silently short InputBuffer -- i.e. as wrong-but-plausible data much later. Exactly the
    // [[silent-drop-stubs]] shape. So every stride is now pinned, not just the ones in doubt.
    //
    // EACH RIGHT-HAND SIDE IS AN X360 CONSTANT, NOT A RESTATEMENT OF THIS HEADER. It is
    // (nextQueueOffset - thisQueueOffset - 16) / capacity, where both queue offsets are read
    // out of the corresponding `InputBuffer::GetXxxQueue() const` accessor's closing
    // `addis r3,r28,H` / `addi r3,r3,L`. The accessors form a uniform block at
    // 0x8289E408 + k*0xA8; the full offset table is in CgsPhysicsSimulationModuleIO.h.
    // ⚠️ That distinction is the point: a gate whose terms are all spelled in this header's
    // own types would be invariant under a member re-typing and would have passed with the
    // four defects in place. These pin against the binary.
    // ---------------------------------------------------------------------------------
    // rigid-body group
    // ⚠️ InUpdateRigidBody's pin is deliberately the ADJACENCY form, not the X360 constant:
    // its payload is a full rw::physics::RigidBody BY VALUE, whose five pointer lanes widen on
    // x64, so the host stride is 16 + sizeof(RigidBody) (== the attested 192 ONLY on a 4-byte-
    // pointer target). Every other event in this table is pointer-free and its host size still
    // equals the X360 stride. See the banner on the struct itself.
    static_assert(sizeof(InUpdateRigidBody) == 16 + sizeof(rw::physics::RigidBody),
                  "InUpdateRigidBody = 16B id slot + the full RigidBody (X360: 16+176 == the attested 192 ((76848-38432-16)/200))");
    static_assert(sizeof(InApplyForce)             ==  32, "InApplyForce stride 32        ((84864-76848-16)/250)");
    static_assert(sizeof(InChangeRigidBodyInertia) ==  80, "InChangeRigidBodyInertia 80   ((100880-84864-16)/200)");
    static_assert(sizeof(InSetRigidBodySpy)        ==  16, "InSetRigidBodySpy stride 16   ((104096-100880-16)/200)");
    static_assert(sizeof(InRemoveRigidBody)        ==  16, "InRemoveRigidBody stride 16   ((107312-104096-16)/200)");
    static_assert(sizeof(InAddPotentialContact)    ==  80, "InAddPotentialContact 80      ((189280-107344-16)/1024)");
    static_assert(sizeof(InUpdateExternalBody)     == 112, "InUpdateExternalBody 112      (Append @0x825A41D8 mulli 0x70)");
    // InRemoveAllRigidBodies is deliberately NOT pinned by the chain. The 32-byte span between
    // its queue (+107312) and mAddContactQueue (+107344) is 16 + 8*sizeof(T) PLUS alignment
    // padding up to the next queue's 16-aligned start, so sizeof 1 and 2 both fit and the
    // chain cannot decide. DWARF (:154/:156) says one uint8_t over an empty Event base == 1,
    // and the Construct asm's `addi r30, r31, 0xC` independently proves align < 16. Pinning
    // the DWARF answer only; NOT inventing a chain result the arithmetic does not support.
    static_assert(sizeof(InRemoveAllRigidBodies)   ==   1, "InRemoveAllRigidBodies 1B (DWARF :156; Construct base at this+0xC proves align<16)");
    // joint group
    static_assert(sizeof(InAddJoint)               == 192, "InAddJoint stride 192         ((196208-189280-16)/36)");
    static_assert(sizeof(InRemoveJoint)            ==   8, "InRemoveJoint stride 8        ((196512-196208-16)/36)");
    static_assert(sizeof(InUpdateJointFrames)      ==  96, "InUpdateJointFrames 96        ((199984-196512-16)/36)  [was 16 -- CORRECTED #142]");
    static_assert(sizeof(InUpdateJointLimits)      ==  80, "InUpdateJointLimits 80        ((202880-199984-16)/36)  [was 16 -- CORRECTED #142]");
    static_assert(sizeof(InSetJointSpy)            ==  16, "InSetJointSpy stride 16       ((203472-202880-16)/36)");
    // drive group
    static_assert(sizeof(InAddDrive)               == 144, "InAddDrive stride 144         ((203632-203472-16)/1)");
    static_assert(sizeof(InRemoveDrive)            ==  16, "InRemoveDrive stride 16       ((203664-203632-16)/1)");
    static_assert(sizeof(InUpdateDriveFrames)      ==  80, "InUpdateDriveFrames 80        ((203760-203664-16)/1)   [was 16 -- CORRECTED #142]");
    static_assert(sizeof(InUpdateDriveDynamics)    ==  48, "InUpdateDriveDynamics 48      ((203824-203760-16)/1)   [was 16 -- CORRECTED #142]");
    static_assert(sizeof(InSetDriveSpy)            ==  16, "InSetDriveSpy stride 16       ((203856-203824-16)/1)");

    // The four corrected types now carry real members, so pin WHERE the payload sits as well as
    // how big the whole is -- a size-only pin would still pass if the id slot and the payload
    // were transposed. The +0x10 payload base is the drains' own `_R11 = event + 0x10`.
    static_assert(offsetof(InUpdateJointFrames,   mJointFrames)   == 16, "JointFrames @+0x10   (drain @0x8289F2F0 _R11 = event+0x10)");
    static_assert(offsetof(InUpdateDriveFrames,   mDriveFrames)   == 16, "DriveFrames @+0x10   (drain @0x8289FC28 _R11 = event+0x10)");
    static_assert(offsetof(InUpdateJointLimits,   mJointLimits)   == 16, "JointLimits @+0x10");
    static_assert(offsetof(InUpdateDriveDynamics, mDriveDynamics) == 16, "DriveDynamics @+0x10");
    // And pin the payload classes themselves at the sizes the drains' lane counts prove --
    // spelled as the MEMBER, so a re-typing of the member cannot leave the gate passing.
    static_assert(sizeof(InUpdateJointFrames::mJointFrames)     == 80, "JointFrames 80B  = the FIVE lvx128 lanes at _R17..R20 (0/16/32/48/64) in drain @0x8289F2F0");
    static_assert(sizeof(InUpdateDriveFrames::mDriveFrames)     == 64, "DriveFrames 64B  = the FOUR lvx128 lanes at _R26..R28 (0/16/32/48) in drain @0x8289FC28");
    static_assert(sizeof(InUpdateJointLimits::mJointLimits)     == 64, "JointLimits 64B  (Vector3 x2 + 6x f32 + SwingType + TwistType)");
    static_assert(sizeof(InUpdateDriveDynamics::mDriveDynamics) == 32, "DriveDynamics 32B (two 16-byte Params: mLinear, mAngular)");

    // ---- task #143: the three drive events promoted from opaque spans to real members -----
    // Same discipline as above: every term is spelled `sizeof(Class::member)` / `offsetof`, so
    // a re-TYPING of a member (which leaves sizeof(Class) invariant) still fails the gate.
    // Every constant here is the drain's own load offset, not a re-statement of the header.
    static_assert(offsetof(InAddDrive, mu64Id)           ==   0, "InAddDrive::mId @+0x00           (drain @0x828A4CB8 `ld 0(event)`)");
    static_assert(offsetof(InAddDrive, mu64ParentBodyId) ==   8, "InAddDrive::mParentBodyId @+0x08 (drain `ld 8(event)`  -> GetIndexFromGameID)");
    static_assert(offsetof(InAddDrive, mu64ChildBodyId)  ==  16, "InAddDrive::mChildBodyId @+0x10  (drain `ld 0x10(event)` -> GetIndexFromGameID)");
    static_assert(offsetof(InAddDrive, mDriveFrames)     ==  32, "InAddDrive::mDriveFrames @+0x20  (drain `addi r10,r31,0x20` + 4x lvx128)");
    static_assert(offsetof(InAddDrive, mDriveDynamics)   ==  96, "InAddDrive::mDriveDynamics @+0x60 (drain `ld 0x60/0x68/0x70/0x78(event)`)");
    static_assert(offsetof(InAddDrive, mbSpy)            == 128, "InAddDrive::mbSpy @+0x80         (drain `lbz 0x80(event)` -> Drive::m_spy)");
    static_assert(sizeof(InAddDrive::mDriveFrames)       ==  64, "InAddDrive DriveFrames 64B  (the FOUR lvx128 lanes the drain copies)");
    static_assert(sizeof(InAddDrive::mDriveDynamics)     ==  32, "InAddDrive DriveDynamics 32B (the FOUR ld/std pairs the drain copies)");

    static_assert(offsetof(InRemoveDrive, mu64Id)        ==   0, "InRemoveDrive::mId @+0x00        (drain @0x8289FF98 `ld 0(event)`, its only read)");

    static_assert(offsetof(InSetDriveSpy, mu64Id)        ==   0, "InSetDriveSpy::mId @+0x00        (drain @0x8289FE88 `ld 0(event)`)");
    static_assert(offsetof(InSetDriveSpy, mbSpy)         ==   8, "InSetDriveSpy::mbSpy @+0x08      (drain @0x8289FE88 `lbz 8(event)`)");

    // ---- task #144: the joint events promoted from opaque spans to real members ------------
    // Same discipline: every term is spelled `sizeof(Class::member)` / `offsetof`, so a
    // re-TYPING of a member (which leaves sizeof(Class) invariant) still fails the gate. Every
    // constant is the drain's own load offset, not a re-statement of the header.
    static_assert(offsetof(InAddJoint, mu64Id)           ==   0, "InAddJoint::mId @+0x00           (drain @0x828A40F0 `ld 0(event)`)");
    static_assert(offsetof(InAddJoint, mu64ParentBodyId) ==   8, "InAddJoint::mParentBodyId @+0x08 (drain `ld 8(event)`  -> GetIndexFromGameID)");
    static_assert(offsetof(InAddJoint, mu64ChildBodyId)  ==  16, "InAddJoint::mChildBodyId @+0x10  (drain `ld 0x10(event)` -> GetIndexFromGameID)");
    static_assert(offsetof(InAddJoint, mJointFrames)     ==  32, "InAddJoint::mJointFrames @+0x20  (drain `addi r11,r26,0x20` + 5x lvx128)");
    static_assert(offsetof(InAddJoint, mJointLimits)     == 112, "InAddJoint::mJointLimits @+0x70  (drain `addi r11,r26,0x70` + 8x ld/std)");
    static_assert(offsetof(InAddJoint, mbSpy)            == 176, "InAddJoint::mbSpy @+0xB0         (drain `lbz 0xB0(event)` -> Joint::m_spy)");
    static_assert(sizeof(InAddJoint::mJointFrames)       ==  80, "InAddJoint JointFrames 80B  (the FIVE lvx128 lanes the drain copies)");
    static_assert(sizeof(InAddJoint::mJointLimits)       ==  64, "InAddJoint JointLimits 64B  (the EIGHT ld/std pairs the drain copies)");

    static_assert(offsetof(InRemoveJoint, mu64Id)        ==   0, "InRemoveJoint::mId @+0x00        (drain @0x8289F970 `ld 0(event)`, its only read)");

    static_assert(offsetof(InSetJointSpy, mu64Id)        ==   0, "InSetJointSpy::mId @+0x00        (drain @0x8289F768 `ld 0(event)`)");
    static_assert(offsetof(InSetJointSpy, mbSpy)         ==   8, "InSetJointSpy::mbSpy @+0x08      (drain @0x8289F768 `lbz 8(event)`)");

    // ---- 2026-08-05: the six rigid-body events promoted from opaque spans ------------------
    // Same discipline as #143/#144: every term is spelled `sizeof(Class::member)` / `offsetof`
    // so a re-TYPING of a member still fails the gate, and every constant is the drain's own
    // load offset, not a re-statement of this header.
    static_assert(offsetof(InApplyForce, mID)                 ==  0, "InApplyForce::mID @+0x00              (drain @0x828A6B80 `ld 0(event)`)");
    static_assert(offsetof(InApplyForce, mForce)              == 16, "InApplyForce::mForce @+0x10           (drain `lvx128 v0,r30,0x10`)");
    static_assert(sizeof(InApplyForce::mForce)                == 16, "InApplyForce::mForce one 16B lane     (single lvx128, xyz consumed)");

    static_assert(offsetof(InChangeRigidBodyInertia, mID)       ==  0, "InChangeRigidBodyInertia::mID @+0x00       (drain @0x828A4A78 `ld 0(event)`)");
    static_assert(offsetof(InChangeRigidBodyInertia, mInertia)  == 16, "InChangeRigidBodyInertia::mInertia @+0x10  (drain fast path `addi r11,r28,0x10` + 6x ld/std)");
    static_assert(offsetof(InChangeRigidBodyInertia, mu32Flags) == 64, "InChangeRigidBodyInertia::mu32Flags @+0x40 (drain `lwz 0x40(event)`, every bit test)");
    static_assert(sizeof(InChangeRigidBodyInertia::mInertia)    == 48, "InChangeRigidBodyInertia::mInertia 48B     (the SIX ld/std pairs the fast path copies)");

    static_assert(offsetof(InUpdateExternalBody, mID)         ==  0, "InUpdateExternalBody::mID @+0x00          (drain @0x828A3B30 `ld 0(event)`)");
    static_assert(offsetof(InUpdateExternalBody, mTransform)  == 16, "InUpdateExternalBody::mTransform @+0x10   (drain `addi r11,r30,0x10` + 4 rows -> SetTransform)");
    static_assert(offsetof(InUpdateExternalBody, mVel)        == 80, "InUpdateExternalBody::mVel @+0x50         (drain `lvx128 v13,r30,0x50` -> body mVel +0x20)");
    static_assert(offsetof(InUpdateExternalBody, mAngularVel) == 96, "InUpdateExternalBody::mAngularVel @+0x60  (drain `lvx128 v0,r30,0x60` -> body mOmega +0x30)");
    static_assert(sizeof(InUpdateExternalBody::mTransform)    == 64, "InUpdateExternalBody::mTransform 64B      (the FOUR w-preserving rows SetTransform copies)");

    static_assert(offsetof(InRemoveRigidBody, mID)                       == 0, "InRemoveRigidBody::mID @+0x00                       (drain @0x828A2BD0 `ld 0(event)`)");
    static_assert(offsetof(InRemoveRigidBody, mbFailIfRigidBodyNotFound) == 8, "InRemoveRigidBody::mbFailIfRigidBodyNotFound @+0x08 (drain `lbz 8(event)`)");

    static_assert(offsetof(InSetRigidBodySpy, mID)  == 0, "InSetRigidBodySpy::mID @+0x00  (drain @0x828A49A8 `ld 0(event)`)");
    static_assert(offsetof(InSetRigidBodySpy, mSpy) == 8, "InSetRigidBodySpy::mSpy @+0x08 (drain @0x828A49A8 `lbz 8(event)` -> SetSpy fork)");

    static_assert(offsetof(InUpdateRigidBody, mID)        ==  0, "InUpdateRigidBody::mID @+0x00        (drain @0x828A3A08 `ld 0(event)`)");
    static_assert(offsetof(InUpdateRigidBody, mRigidBody) == 16, "InUpdateRigidBody::mRigidBody @+0x10 (drain `addi r4,r30,0x10` -> RigidBody::operator=; state cmp `lwz 0x9C(event)` == +0x10 + mState 0x8C)");

    // ---- 2026-08-05: the contact event promoted (drain 19 close) ---------------------------
    // Same discipline; every constant is ProcessAddContactQueue @0x828A3458's own load offset.
    static_assert(offsetof(InAddPotentialContact, mPointOnA)        ==  0, "InAddPotentialContact::mPointOnA @+0x00        (drain `lvx128 v0,r0,r8` -> Contact::mPosA row)");
    static_assert(offsetof(InAddPotentialContact, mPointOnB)        == 16, "InAddPotentialContact::mPointOnB @+0x10        (drain `lvx128 v12,r8,0x10` -> Contact::mPosB row)");
    static_assert(offsetof(InAddPotentialContact, mNormal)          == 32, "InAddPotentialContact::mNormal @+0x20          (drain `lvx128 v0,r8,0x20` -> the frame build)");
    static_assert(offsetof(InAddPotentialContact, mIDA)             == 48, "InAddPotentialContact::mIDA @+0x30             (drain `ld 0x30(event)` -> GetIndexFromGameID)");
    static_assert(offsetof(InAddPotentialContact, mIDB)             == 56, "InAddPotentialContact::mIDB @+0x38             (drain `ld 0x38(event)` -> GetIndexFromGameID)");
    static_assert(offsetof(InAddPotentialContact, mStaticFriction)  == 64, "InAddPotentialContact::mStaticFriction @+0x40  (drain `lfs 0x40(event)` -> Contact::mMus)");
    static_assert(offsetof(InAddPotentialContact, mDynamicFriction) == 68, "InAddPotentialContact::mDynamicFriction @+0x44 (drain `lfs 0x44(event)` -> Contact::mMud)");
    static_assert(offsetof(InAddPotentialContact, mRestitution)     == 72, "InAddPotentialContact::mRestitution @+0x48     (drain `lfs 0x48(event)` -> Contact::mRes)");
    static_assert(offsetof(InAddPotentialContact, muTag)            == 76, "InAddPotentialContact::muTag @+0x4C            (NOT read by the drain -- pinned off the producer @0x825A5618 `stw tag,0x12C(sp)` in its event image)");

    // ---- 2026-08-06: the four OUTPUT events promoted from opaque spans (the spy wave) ------
    // Same discipline; the constants are the PRODUCERS' own store offsets (the four
    // PhysicsSimulationModule output emitters) plus the AddEvent-instantiation strides that
    // pinned the old opaque spans. OutUpdateRigidBody is the one ADJACENCY pin, for exactly
    // InUpdateRigidBody's reason (pointer-bearing payload).
    static_assert(sizeof(OutContactSpy)                     == 112, "OutContactSpy stride 112 (AddEvent @0x825E44C8 / AddEventSafe @0x828A1B78 mulli 0x70)");
    static_assert(offsetof(OutContactSpy, mFrictionStress)  ==   0, "OutContactSpy::mFrictionStress @+0x00 (emitter @0x828A4ED8 stvx of spy mForceT*ts)");
    static_assert(offsetof(OutContactSpy, mNormalStress)    ==  16, "OutContactSpy::mNormalStress @+0x10   (emitter stvx of spy mForceN*ts)");
    static_assert(offsetof(OutContactSpy, mNormal)          ==  32, "OutContactSpy::mNormal @+0x20         (emitter stvx of drain event mNormal)");
    static_assert(offsetof(OutContactSpy, mPointOnA)        ==  48, "OutContactSpy::mPointOnA @+0x30       (emitter stvx of drain event mPointOnA)");
    static_assert(offsetof(OutContactSpy, mPointOnB)        ==  64, "OutContactSpy::mPointOnB @+0x40       (emitter stvx of drain event mPointOnB)");
    static_assert(offsetof(OutContactSpy, mIDA)             ==  80, "OutContactSpy::mIDA @+0x50            (emitter `std` of GetGameID(bodyA tag))");
    static_assert(offsetof(OutContactSpy, mIDB)             ==  88, "OutContactSpy::mIDB @+0x58            (emitter `std` of GetGameID(bodyB tag))");
    static_assert(offsetof(OutContactSpy, muTag)            ==  96, "OutContactSpy::muTag @+0x60           (emitter `stw` of drain event muTag +0x4C)");

    static_assert(sizeof(OutJointSpy)                       ==  48, "OutJointSpy stride 48 (AddEvent @0x828A1C30 miLength*3 then *16)");
    static_assert(offsetof(OutJointSpy, mID)                ==   0, "OutJointSpy::mID @+0x00              (emitter @0x828A58E0 `std` of JointData::GetGameID)");
    static_assert(offsetof(OutJointSpy, mLinearStress)      ==  16, "OutJointSpy::mLinearStress @+0x10    (emitter stvx of spy mForce  * ts*59.999996f)");
    static_assert(offsetof(OutJointSpy, mAngularStress)     ==  32, "OutJointSpy::mAngularStress @+0x20   (emitter stvx of spy mTorque * ts*59.999996f)");

    static_assert(sizeof(OutDriveSpy)                       ==  64, "OutDriveSpy stride 64 (AddEvent @0x828A1D90 slwi 6)");
    static_assert(offsetof(OutDriveSpy, mID)                ==   0, "OutDriveSpy::mID @+0x00                   (emitter @0x828A5D10 `std` of DriveData::GetGameID)");
    static_assert(offsetof(OutDriveSpy, mLinearStress)      ==  16, "OutDriveSpy::mLinearStress @+0x10         (emitter stvx of spy mForce row)");
    static_assert(offsetof(OutDriveSpy, mAngularStress)     ==  32, "OutDriveSpy::mAngularStress @+0x20        (emitter stvx of spy mTorque row)");
    static_assert(offsetof(OutDriveSpy, mLinearDistanceToKey)  == 48, "OutDriveSpy::mLinearDistanceToKey @+0x30  (emitter `stfs` of spy mSeparation +0x20)");
    static_assert(offsetof(OutDriveSpy, mAngularDistanceToKey) == 52, "OutDriveSpy::mAngularDistanceToKey @+0x34 (emitter `stfs` of spy mAngSeparation +0x24)");

    static_assert(sizeof(OutUpdateRigidBody) == 16 + sizeof(rw::physics::RigidBody),
                  "OutUpdateRigidBody = 16B id slot + the full RigidBody (X360: 16+176 == the attested 192 of AddEvent @0x828A66F8)");
    static_assert(offsetof(OutUpdateRigidBody, mID)        ==  0, "OutUpdateRigidBody::mID @+0x00        (both emitters `std gameId, frame+0`)");
    static_assert(offsetof(OutUpdateRigidBody, mRigidBody) == 16, "OutUpdateRigidBody::mRigidBody @+0x10 (both emitters RigidBody::operator=(frame+0x10, body))");
    // =====================================================================================
}
}
