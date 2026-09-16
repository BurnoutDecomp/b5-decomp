#ifndef BRN_SOUND_LOGIC_BRN_SUBMIXES_EFFECT_H
#define BRN_SOUND_LOGIC_BRN_SUBMIXES_EFFECT_H

#include "types.hpp"
#include "GameSource/Sound/Module/LogicModule/BrnEffectObject.h"

// Forward decl -- Notify takes a message header by pointer only (read by offset).
namespace CgsSound { namespace Io { class MessageHeader; } }

// =============================================================================
// BrnSound::Logic::SubmixesEffect
//   GameSource/Sound/Module/LogicModule/BrnSubmixesEffect.h (DWARF home) +
//   GameSource/Sound/Module/LogicModule/BrnSubmixesEffect.cpp
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. SubmixesEffect is a leaf sound-logic
// effect object. It reuses the committed BrnSound::Logic::BrnEffectObject dual base
// BY NAME, so it multiply-inherits (via BrnEffectObject) the engine effect-object
// base (CgsSound::Logic::EffectObject -> EffectBase -> CgsSound::MemBase, primary
// vptr @ this+0) and IResourceRequester (sub-object vptr @ this+4) -- matching the
// X360 dual-vptr teardown observed below.
//
// This TU's recon'd function set is the destructor pair only:
//   `scalar deleting destructor'             @ 0x826BC608
//   `vector deleting destructor'`adjustor{4}' @ 0x826BC600
// The adjustor thunk just does `this - 4` (recover the primary object from the
// IResourceRequester sub-object) and tail-calls the scalar deleting destructor;
// that adjustment + the deleting-destructor flavour are compiler-synthesised, so
// only the source-level destructor needs a body here.
//
// X360 teardown of the scalar deleting destructor @ 0x826BC608 (verified
// store-for-store against the assembly; identical to the committed sibling leaves
// LoopModelEffect @ 0x826AFBA0 and SingleGinsuEffect @ 0x826AFAF0):
//   stw  &off_820AE9BC, 0(this)        ; primary vptr settle (SubmixesEffect vtable)
//   stw  &off_820AE988, 4(this)        ; IResourceRequester sub-object vptr (intermediate)
//   stw  3,            0x28(this)      ; meDetachState = E_DETACH_STATE_FINISHED
//   stw  &off_820AA820, 4(this)        ; IResourceRequester sub-object vptr (final settle,
//                                        the shared CgsSound::MemBase base vtable)
//   stb  0,            0x31(this)      ; mbResourcesReady = false
//   stw  0,            0x24(this)      ; meAttachState = E_ATTACH_STATE_NONE
//   if (a2 & 1) { deallocate via off_82FFB954 (the global sound allocator) }
//   return this
// The two vptr stores at +0/+4 and the member clears at +0x28/+0x31/+0x24 are the
// inherited BrnEffectObject dual-base teardown the compiler emits; this leaf
// destructor body adds nothing of its own (no leaf-specific store -- the X360 asm
// writes only +0/+4/+0x24/+0x28/+0x31).
//
// FLAG (shape vs full surface): MINIMAL home for the boot-trace deleting-destructor
// TU. The full DWARF surface (RTTI GetTypeInfo/GetTypeName/CreateObject; the
// per-submix members and any UpdateParams/ProcessUpdate hooks) is DEFERRED to its
// own TU(s). Only the inheritance spine needed to settle the destructor is
// materialised here. The (a2 & 1) deallocation tail dispatches the global sound
// allocator (off_82FFB954), not homed here, so the `delete` half is left to the
// host toolchain (same treatment as the committed BrnEffectObject siblings).
//
// LAYOUT NOTE (X360 32-bit vs host 64-bit): the X360 offsets (meDetachState @
// +0x28, mbResourcesReady @ +0x31, meAttachState @ +0x24, IResourceRequester
// sub-object vptr @ +4) assume 4-byte pointers/vptr; members are pinned BY NAME via
// the inherited BrnEffectObject bases and absolute offsets are NOT static_asserted
// across pointer members on the 64-bit host.
// =============================================================================

namespace BrnSound
{
namespace Logic
{

// SubmixesEffect leaf. Reuses the committed BrnEffectObject dual base by name; the
// virtual `scalar deleting destructor' @ 0x826BC608 runs the inherited
// BrnEffectObject teardown (both vptr settles + the attach/detach/resources-ready
// member clears).
struct SubmixesEffect : public BrnSound::Logic::BrnEffectObject
{
    SubmixesEffect() : mbHoldVolumes(false) {}
    virtual ~SubmixesEffect();

    CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject>* GetTypeInfo() const override;
    const char* GetTypeName() const override;
    static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject>* GetStaticTypeInfo();
    static CgsSound::Logic::EffectObject* CreateObject(u32 auType);

    // @ 0x82687EE8 -- overrides EffectBase::Notify. Type-15 messages carry a single-byte
    // hold-volumes flag in their body at +0x10 (past the 12-byte MessageHeader); Notify
    // latches it into mbHoldVolumes. NOT marked `override`: the committed minimal EffectBase
    // reconstruction (BrnEffectObject.h) elides the engine Notify virtual, so this is declared
    // as a plain virtual on the leaf (grow-additive, no base re-home) until that base surface
    // is materialised.
    virtual void Notify(const CgsSound::Io::MessageHeader* apMessageHeader);

private:
    // The "hold submix volumes" latch. Written by Notify (type-15 message) and, once
    // SubmixesEffect::Attach is homed, by Attach.
    //
    // ⭐⭐ THE "IRRECONCILABLE LAYOUT" THAT BLOCKED Attach IS THE ±4 BASE-OFFSET TRAP, AND
    // THERE IS NO CONFLICT. The note that stood here said Attach's asm proves a canonical
    // EffectBase layout (mpState@+0x08, mu16AttachCount@+0x0E, meDetachState@+0x24,
    // mpLogicModule@+0x28) that cannot be reconciled with the committed BrnEffectObject model
    // (meAttachState@+0x24, meDetachState@+0x28, mpLogicModule@+0x2C). Both readings are of the
    // SAME object -- Attach simply does not receive the primary pointer.
    //
    // MEASURED 2026-09-16, three independent ways:
    //
    //  (1) THE VTABLE. Attach/ProcessUpdate/Notify each appear in EXACTLY ONE pointer slot in
    //      the whole image, and it is the same table -- 0x820B2530, immediately after the
    //      ":VolumeStream" string:
    //        +0x00  0x826BC600   `vector deleting destructor'`adjustor{4}'   <-- SLOT 0
    //        +0x04  0x82661058
    //        +0x08  0x8284CB38   (the shared do-nothing body)
    //        +0x0C  0x8268CEC8
    //        +0x10  0x826805E0
    //        +0x14  0x826D2DA8   Attach
    //        +0x18  0x8284CB38   UpdateParams (the shared do-nothing body)
    //        +0x1C  0x826D2E48   ProcessUpdate
    //        +0x20  0x826EBF88   Detach
    //        +0x24  0x82687EE8   Notify
    //      That order is the DWARF EffectBase virtual order (Prepare, SetupLoadData, Attach,
    //      UpdateParams, ProcessUpdate, Detach, Notify) one-for-one. Slot 0 being the
    //      `adjustor{4}' thunk (`addi r3, r3, -4; b 0x826BC608`) is what pins the base: every
    //      virtual reached through this table is entered with `this == <primary> + 4`.
    //
    //  (2) APPLY THE +4 AND EVERY OFFSET LANDS ON THE COMMITTED MODEL:
    //        Attach  lwz  r11, 8(r31)    -> this+0x0C  mpState
    //        Attach  lhz  r11, 0xE(r31)  -> this+0x12  mu16AttachCount   (++, i.e. an ATTACH)
    //        Attach  stw  r29, 0x24(r31) -> this+0x28  meDetachState = 0 (E_DETACH_STATE_NONE)
    //        Attach  lwz  r30, 0x28(r31) -> this+0x2C  mpLogicModule     (+0x2490 == Environment)
    //        PU      lwz  r3,  0x30(r30) -> this+0x34  mpDynamicMixIo    (-> DMixIO::GetDMixOutput)
    //      meDetachState is the clincher: Attach writes 0 (NONE) at the SAME primary offset
    //      +0x28 where ~SubmixesEffect writes 3 (FINISHED). Read without the +4 it is a
    //      pointer-typed field being cleared in one function and enum-typed in the other.
    //
    //  (3) A SECOND LANE FOUND THE SAME SHIFT INDEPENDENTLY. BrnHUDEffect.cpp:406 annotates
    //      `82703814  lwz r4, 0x28(r31)` as `this+0x2C == mpLogicModule` -- the identical
    //      sub-object base, in a different TU, derived from different asm.
    //
    //  [FLAG] Attach @0x826D2DA8 and ProcessUpdate @0x826D2E48 are still REPORTED, not written.
    //  What is removed is the false blocker, not the work: ProcessUpdate is four
    //  DMixIO::GetDMixOutput(bus, preset) reads scaled by 820AA8F8 feeding Voice::SetGain on
    //  the voices hanging off mpState->+0x24 (stored into this+0x38 by Attach), and it needs
    //  CgsSound::Logic::Voice plus that State member homed first. Neither needs the class
    //  re-based and neither touches the committed destructor.
    //
    //  ⚠️ The offset in this member's own comment was wrong for the same reason: Notify's
    //  `stb r11, 0x39(r3)` is sub-object-relative, so mbHoldVolumes is at PRIMARY +0x3D, and
    //  it is the leaf's SECOND byte -- Attach sets the one before it (+0x3C) from
    //  `Environment::GetAudioMode() == 1`. The member is pinned BY NAME here, so the bodies
    //  are unaffected; only the number was.
    bool mbHoldVolumes;
};

} // namespace Logic
} // namespace BrnSound

#endif // BRN_SOUND_LOGIC_BRN_SUBMIXES_EFFECT_H
