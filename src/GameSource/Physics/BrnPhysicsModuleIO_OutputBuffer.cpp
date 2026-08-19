#include "GameSource/Physics/BrnPhysicsModuleIO.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

#include <cstddef>   // offsetof

// BrnPhysics::PhysicsModuleIO::OutputBuffer member functions, reconstructed from
// BURNOUT_X360_ARTIST.XEX. This TU bodies the 7 X360-emitted OutputBuffer accessors:
//
//   GetVehicleOutputRequestInterface() @ 0x8259FF30 -> +16     (this+16),     write (bit 3)
//   GetVehicleOutputInterface() const  @ 0x8279F598 -> +44128  (this+44128),  read  (bit 4)
//   GetVehicleOutputInterface()        @ 0x825A0080 -> +44128  (this+44128),  write (bit 3)
//   GetPropManagerOutputInterface() const @ 0x8279F640 -> +71792 (this+71792), read (bit 4)
//   GetPropManagerOutputInterface()    @ 0x825C0DC8 -> +71792  (this+71792),  write (bit 3)
//   GetDeformationOutputInterface()    @ 0x825A0128 -> +148656 (this+148656), write (bit 3)
//   GetContactSpyInterface()           @ 0x825A0320 -> +998192 (this+998192), write (bit 3)
//
// The const (read) handle tests the read-lock bit (((*a1 >> 4) & 1)); the non-const (write)
// handles test the write-lock bit (((*a1 >> 3) & 1)) -- matching CgsModule::IOBuffer's
// IsBufferLockedForReading()/IsBufferLockedForWriting(). Each returns the member's address.

namespace BrnPhysics
{
namespace PhysicsModuleIO
{
    void OutputBuffer::_AssertLayout()
    {
        static_assert(offsetof(OutputBuffer, mVehicleOutputRequestInterface) == 16,     "mVehicleOutputRequestInterface @16");
        // ⭐ 2026-08-09 (conductor wave): the :378 manager seat, previously folded into
        // padding, is a real member now -- 16 + 41936 == 41952 (the request interface is
        // byte-identical on both targets, so this pin stays ABSOLUTE).
        static_assert(offsetof(OutputBuffer, mVehicleManagerOutputInterface) == 41952,  "mVehicleManagerOutputInterface @41952");
        // ⚠ From here down the buffer GROWS on the host (12- vs 16-byte console queue
        // headers inside the two vehicle interfaces -- see the header note), so the gates
        // are the console DELTAS between seats, not absolutes. Console deltas: 44128-41952
        // is sizeof(VMOI)'s console span (adjacent members, gated trivially by adjacency);
        // 148656-71792 == 76864; 159648-148656 == 11000... NO -- the deltas below are the
        // PAD-under-written spans, which this file's own pad arrays hold by construction:
        // ⭐ 2026-08-19 (wave Q6/A1): this relation is no longer held by a pad -- it is held by
        // sizeof(Props::PropOutputInterface) itself, now that mPropManagerOutputInterface IS
        // that type. Console delta 76,864; MEASURED host sizeof 76,864 -- exactly equal, because
        // all four of the interface's EventQueue<T,200> element types are 16-byte aligned, so
        // the host's 16-byte BaseEventQueue header lands inside the space the console's 12-byte
        // header already padded out to. The gate is written `>=` (not `==`) for the same reason
        // the scene seat's is: benign host GROWTH in a later wave only pushes the opaque
        // deformation seat further out, which nothing reads by absolute offset, whereas a
        // SHRINK below the console span would silently re-seat it inside data the prop
        // interface owns -- and that is what this must catch.
        static_assert(offsetof(OutputBuffer, mDeformationOutputInterface)
                    - offsetof(OutputBuffer, mPropManagerOutputInterface)    >= 148656 - 71792,
                      "prop -> deformation console delta (>=: measured host == console, 76864)");
        static_assert(offsetof(OutputBuffer, mDeformationOutputInterfaceForEntityModules)
                    - offsetof(OutputBuffer, mDeformationOutputInterface)    == 159648 - 148656,
                      "deformation -> entity-modules console delta");
        static_assert(offsetof(OutputBuffer, mSceneInputInterface)
                    - offsetof(OutputBuffer, mDeformationOutputInterfaceForEntityModules) == 179424 - 159648,
                      "entity-modules -> scene console delta");
        // ⭐ 2026-08-19 (wave Q5/F2): this relation is no longer held by a pad -- it is held by
        // sizeof(InSceneUpdateInterface) itself, now that mSceneInputInterface IS that type.
        // Console delta 818,768; measured host 818,944 (the 16-vs-12-byte queue headers). The
        // `>=` is what makes the promotion safe: if a later wave ever SHRINKS the interface
        // below the console span this gate fails instead of silently re-seating the spy.
        static_assert(offsetof(OutputBuffer, mContactSpyInterface)
                    - offsetof(OutputBuffer, mSceneInputInterface)           >= 998192 - 179424,
                      "scene -> contact-spy console delta (>=: the spy seat 8-aligns)");
    }

    // ⛔⛔ 2026-08-10 (root-cause wave) -- THIS BUFFER HAD NO Construct AT ALL.
    // The X360 CreateIOBuffer<T> stack template runs T::Construct after the alloc, and so does
    // the PC one: CreateIOBuffer<T> runs T::Construct (2026-08-15). While the PC template only
    // placement-new'd, every embedded queue in the physics module's OUTPUT
    // buffer stayed un-Constructed, and the moment PhysicsModule::Update actually ran,
    // BridgeVehicleManagerToOutput's `GetVehicleOutputRequestInterface()->Append(...)` hit an
    // unconstructed VariableEventQueue<13440,16> (mRequestFineLineQueue) and fired
    // "Not Constructed" (CgsVariableEventQueue.h:759) every frame. Exactly the InputBuffer
    // partial-Construct family fixed on 2026-08-09.
    //
    // Console body X360 0x825ABB10 (64 instructions), read from the asm. r30 == this;
    // r28/r29 are re-bases the compiler hoisted. Full call list with each member's seat:
    //   stb 1, 0(r30)                                        status = 1
    //   +148656  Deformation::DeformationOutputInterface::Construct
    //   +171552  Deformation::DetachedPartRenderEvent<50>::Construct   ) inside
    //   +175568  Deformation::GlassSmashOrCrackEvent<20>::Construct    ) mDeformationOutput-
    //            + zero stores at +159648/+171088/+171316/+171560/+175576 ) InterfaceForEntityModules
    //            (⚠ TWO SEATS CORRECTED 2026-08-10: this line read +171072/+171300. r28 is
    //             `addis 2; addi 0x6FA0` == this+159648, and the two stores are `stw r31,
    //             0x2CB0(r28)` and `stw r31, 0x2D94(r28)` -- 11440 and 11668, i.e. 171088 and
    //             171316. Both sit inside the opaque entity-modules span, so nothing consumed
    //             the wrong numbers; corrected so the next wave that unfolds that span does not
    //             inherit them.)
    //   +53888   Vehicle::PhysicalTrafficState<20>::Construct  ) inside
    //   +53104   Vehicle::ImpactEvent<16>::Construct           ) mVehicleOutputInterface
    //   +70224   VariableEventQueue<1536,16>::Construct        ) (+44128)
    //            + `std 0` at +44128 and five zero bytes at +71776 )
    //   +16      InAddRigidBody<50> / VariableEventQueue<13440,16> / InRemoveRigidBody<50> /
    //            InChangeRigidBodyInertia<200> / InAddJoint<10> / InRemoveJoint<10>
    //            == Vehicle::VehicleOutputRequestInterface::Construct, inlined
    //   +41952   Vehicle::VehicleManagerOutputInterface::Construct
    //   +179424  SceneManagerIO::InSceneUpdateInterface::Construct
    //   +71792   Props::PropOutputInterface::Construct
    //   +998192  stwx 0        == mContactSpyInterface (drop the data pointer)
    //
    // ⭐ 2026-08-10 (create-path wave): TWO OF THE SIX BLOCKED LEGS ARE NOW EMITTED.
    // mVehicleManagerOutputInterface (+41952) and mVehicleOutputInterface (+44128) were never
    // opaque -- both are real committed types that simply had no Construct member. They have
    // one now, recovered from the console: VehicleManagerOutputInterface::Construct is an
    // out-of-line symbol at X360 0x822E6790, and VehicleOutputInterface::Construct is the
    // inline block this very function emits at 0x825ABB58..0x825ABBA0. Both are DWARF-declared
    // (BrnVehicleOutputInterface.h:86 and :312), so neither name is minted here.
    // The game-event-queue leg inside VehicleOutputInterface runs through that class's
    // sanctioned span cast, gated by a static_assert on the span size -- see its .cpp.
    //
    // ⭐⭐ 2026-08-19 (wave Q5 cluster F2): THE SCENE LEG IS LIVE. mSceneInputInterface was the
    // fourth of the blocked legs, parked only because the member was a 1-byte opaque span. It is
    // the real CgsSceneManager::SceneManagerIO::InSceneUpdateInterface now -- the CONSOLE names
    // that type in this very function (0x825ABBEC: `addis r3,r30,3 ; addi r3,r3,-0x4320` ==
    // this+179424, `bl InSceneUpdateInterface::Construct`) -- so the console's own call is
    // reproduced literally below. Two live consequences, both previously worked around:
    //   * WorldModule::BridgePhysicsSceneUpdateToScene @0x827ABA40 (WorldBridgePhysicsToScene.cpp)
    //     can be mounted: its source is this member and it is now typed + Constructed.
    //   * BrnDeformableObject_Update.cpp:1436's marked-deviation host guard ("conductor gate:
    //     module-output scene interface unprepared -- SetEntityRadius skipped") exists ONLY
    //     because nothing constructed this interface's queue storage. It can be un-guarded now.
    //     Reported, not edited -- that file belongs to another owner.
    //
    // ⭐⭐ 2026-08-19 (wave Q6 cluster A1): THE PROP LEG IS LIVE. mPropManagerOutputInterface was
    // the fifth of the blocked legs, parked only because the member was a 1-byte opaque span. It
    // is the real BrnPhysics::Props::PropOutputInterface now -- the CONSOLE names that type in
    // this very function (0x825ABBF8: `addis r3,r30,1 ; addi r3,r3,0x1870` == this+71792,
    // `bl Props::PropOutputInterface::Construct`) -- so the console's own call is reproduced
    // literally below, in the console's own position: after the scene leg, before the
    // contact-spy zero store. This is the leg without which the interface's four embedded
    // EventQueue<T,200>s stay un-Constructed and PropManager::OutputUpdatedProps @0x82627EC8
    // would fire "mpEvents != NULL" on its first AppendUpdatedProps -- the never-Constructed
    // EventQueue family that has broken every previous producer bring-up.
    //
    // ⚠️ TWO legs still CANNOT be emitted and are NOT faked: mDeformationOutputInterface
    // (+148656) and mDeformationOutputInterfaceForEntityModules (+159648) are 1-byte opaque
    // *Storage spans (each size-pinned by the pad that follows it) with no members to construct.
    // Their seats and exact console call lists are transcribed above. Any consumer reaching one
    // of those will fire the same loud "Not Constructed" this buffer did before it had a
    // Construct at all, by design.
    void OutputBuffer::Construct()
    {
        CgsModule::IOBuffer::Construct();               // status = 1

        mVehicleOutputRequestInterface.Construct();     // +16      (the six sim-request queues)
        mVehicleOutputInterface.Construct();            // +44128   (X360-inline @0x825ABB58)
        mVehicleManagerOutputInterface.Construct();     // +41952   (X360 0x822E6790)
        mSceneInputInterface.Construct();               // +179424  (X360 0x825ABBEC)
        mPropManagerOutputInterface.Construct();        // +71792   (X360 0x825ABBF8)
        mContactSpyInterface.Construct();               // +998192  (the console's trailing stwx 0)
    }

    // X360 0x8279F4F0 (read sibling block): read-lock; return this + 41952.
    const Vehicle::VehicleManagerOutputInterface* OutputBuffer::GetVehicleManagerOutputInterface() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return &mVehicleManagerOutputInterface;
    }

    // X360 0x8259FFD8: write-lock; return this + 41952. The accessor
    // PhysicsModule::Update @0x825B0640 calls five times per frame (DoCrashPrediction /
    // UpdateDrivers / ProcessResetEvents / ProcessContactSpies / WriteOut seats).
    Vehicle::VehicleManagerOutputInterface* OutputBuffer::GetVehicleManagerOutputInterface()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return &mVehicleManagerOutputInterface;
    }

    // X360 0x825A01D0 (DWARF :364): write-lock; return this + 159648 (`addis 2; addi 28576`).
    // Consumed by PhysicsModule::Update's OutputData leg.
    OutputBuffer::DeformationOutputInterfaceForEntityModulesStorage*
    OutputBuffer::GetDeformationOutputInterfaceForEntityModules()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return &mDeformationOutputInterfaceForEntityModules;
    }

    // X360 0x8259FF30: write-lock; return this + 16.
    OutputBuffer::VehicleOutputRequestInterfaceStorage* OutputBuffer::GetVehicleOutputRequestInterface()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return &mVehicleOutputRequestInterface;
    }

    // X360 0x8279F598: read-lock; return this + 44128.
    const OutputBuffer::VehicleOutputInterfaceStorage* OutputBuffer::GetVehicleOutputInterface() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return &mVehicleOutputInterface;
    }

    // X360 0x825A0080: write-lock; return this + 44128.
    OutputBuffer::VehicleOutputInterfaceStorage* OutputBuffer::GetVehicleOutputInterface()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return &mVehicleOutputInterface;
    }

    // X360 0x8279F640: read-lock; return this + 71792.
    // (Return RETYPED 2026-08-19 with the member promotion: PropOutputInterfaceStorage is a
    // typedef of the real Props::PropOutputInterface now, so both overloads hand out a typed
    // interface and the callers' reinterpret_cast seams retire. Signature text unchanged.)
    const OutputBuffer::PropOutputInterfaceStorage* OutputBuffer::GetPropManagerOutputInterface() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return &mPropManagerOutputInterface;
    }

    // X360 0x825C0DC8: write-lock; return this + 71792.
    OutputBuffer::PropOutputInterfaceStorage* OutputBuffer::GetPropManagerOutputInterface()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return &mPropManagerOutputInterface;
    }

    // X360 0x825A0128: write-lock; return this + 148656.
    OutputBuffer::DeformationOutputInterfaceStorage* OutputBuffer::GetDeformationOutputInterface()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return &mDeformationOutputInterface;
    }

    // X360 0x825A0320: write-lock; return this + 998192.
    // (Return RETYPED 2026-08-06 with the member promotion to the real ContactSpyInterface.)
    ContactSpy::ContactSpyInterface* OutputBuffer::GetContactSpyInterface()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return &mContactSpyInterface;
    }

    // X360 0x8279F640's sibling @0x8279F8E0 (DWARF :369): read-lock; return this + 998192.
    // ⭐ ADDITIVE 2026-08-18 (wave Q4, prop bridges) -- the const twin of the accessor above.
    // The consumers are the two post-physics bridges that carry the contact-spy handle out of
    // the physics module; both hold a `const OutputBuffer*` because their callers read-lock the
    // source buffer. See the declaration's banner in BrnPhysicsModuleIO.h for why this was a
    // standing two-line follow-up rather than a new finding.
    const ContactSpy::ContactSpyInterface* OutputBuffer::GetContactSpyInterface() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return &mContactSpyInterface;
    }

    // ---- wave5 ADDITIVE accessors (const twins + non-const scene-input) --------------
    // Assert strings match this file's existing convention (no trailing \n); the X360 rodata
    // carries \n but the committed bodies above omit it -- kept consistent within this file.

    // X360 0x8279F448 (DWARF :298): read-lock; return this + 16.
    const OutputBuffer::VehicleOutputRequestInterfaceStorage* OutputBuffer::GetVehicleOutputRequestInterface() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return &mVehicleOutputRequestInterface;
    }

    // X360 0x8279F6E8 (DWARF :322): read-lock; return this + 148656.
    const OutputBuffer::DeformationOutputInterfaceStorage* OutputBuffer::GetDeformationOutputInterface() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return &mDeformationOutputInterface;
    }

    // X360 0x8279F838 (DWARF :366): read-lock; return this + 179424.
    const OutputBuffer::SceneInputInterfaceStorage* OutputBuffer::GetSceneInputInterface() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return &mSceneInputInterface;
    }

    // X360 0x825A0278 (DWARF :337): write-lock; return this + 179424.
    OutputBuffer::SceneInputInterfaceStorage* OutputBuffer::GetSceneInputInterface()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return &mSceneInputInterface;
    }
}
}
