#ifndef BRN_SOUND_LOGIC_BRN_SUBMIXES_EFFECT_H
#define BRN_SOUND_LOGIC_BRN_SUBMIXES_EFFECT_H

#include "types.hpp"
#include "GameSource/Sound/Module/LogicModule/BrnEffectObject.h"

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
    SubmixesEffect() {}
    virtual ~SubmixesEffect();
};

} // namespace Logic
} // namespace BrnSound

#endif // BRN_SOUND_LOGIC_BRN_SUBMIXES_EFFECT_H
