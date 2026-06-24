#ifndef BRN_SOUND_VEHICLES_ENGINES_BRN_LOOP_MODEL_EFFECT_H
#define BRN_SOUND_VEHICLES_ENGINES_BRN_LOOP_MODEL_EFFECT_H

#include "types.hpp"
#include "GameSource/Sound/Module/LogicModule/BrnEffectObject.h"

// =============================================================================
// BrnSound::Vehicles::Engines::LoopModelEffect
//   GameSource/Sound/Vehicles/Engines/BrnLoopModelEffect.h (DWARF home) +
//   GameSource/Sound/Vehicles/Engines/BrnLoopModelEffect.cpp
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. LoopModelEffect is an engine
// loop-model sound-logic effect object. DWARF (BrnLoopModelEffect.h:39):
//   struct BrnSound::Vehicles::Engines::LoopModelEffect
//       : public BrnSound::Logic::BrnEffectObject
// so it multiply-inherits (via BrnEffectObject) the engine effect-object base
// (primary vptr @ this+0) and IResourceRequester (sub-object vptr @ this+4),
// matching the X360 dual-vptr teardown observed below.
//
// This TU's recon'd function set is exactly ONE entry:
//   `scalar deleting destructor'  @ 0x826AFBA0
// whose X360 teardown (verified store-for-store against the assembly) is:
//   stw  &off_820AE9BC, 0(this)        ; primary vptr settle (LoopModelEffect vtable)
//   stw  &off_820AE988, 4(this)        ; IResourceRequester sub-object vptr (intermediate)
//   stw  3,            0x28(this)      ; meDetachState = E_DETACH_STATE_FINISHED
//   stw  &off_820AA820, 4(this)        ; IResourceRequester sub-object vptr (final settle,
//                                        the shared base vtable used by BrnEffectObject)
//   stb  0,            0x31(this)      ; mbResourcesReady = false
//   stw  0,            0x24(this)      ; meAttachState = E_ATTACH_STATE_NONE
//   if (a2 & 1) { deallocate via off_82FFB954 (the global sound allocator) }
//   return this
// NOTE: unlike Car3DControl, this destructor does NOT call Attrib::Instance::
// ~Instance — LoopModelEffect's own data members (mDataHandle: ResourceHandle,
// mpControl: EngineControl*) are not Attrib-managed and need no sub-object teardown
// here. The two vptr stores at +0/+4 and the member clears at +0x28/+0x31/+0x24 are
// the BrnEffectObject dual-base teardown the compiler emits, so the leaf destructor
// body adds nothing of its own (same shape as the committed BrnEffectObject dtor).
//
// FLAG (shape vs full surface): MINIMAL home for the boot-trace deleting-destructor
// TU. The full DWARF surface (RTTI GetTypeInfo/GetTypeName/CreateObject/
// GetStaticTypeInfo; GetController/AttachController/UpdateParams/ProcessUpdate/
// LoadAsset; and the data members mDataHandle (ResourceHandle) + mpControl
// (Engines::EngineControl*)) is DEFERRED to its own TU(s). Only the inheritance
// spine needed to settle the destructor is materialised here. The (a2 & 1)
// deallocation tail dispatches the global sound allocator (off_82FFB954), not homed
// here, so the `delete` half is left to the host toolchain (same treatment as the
// committed BrnEffectObject sibling).
//
// LAYOUT NOTE (X360 32-bit vs host 64-bit): the X360 offsets (meDetachState @
// +0x28, mbResourcesReady @ +0x31, meAttachState @ +0x24, IResourceRequester
// sub-object vptr @ +4) assume 4-byte pointers/vptr; members are pinned BY NAME via
// the inherited BrnEffectObject bases and absolute offsets are NOT static_asserted
// across pointer members on the 64-bit host.
// =============================================================================

namespace BrnSound
{
namespace Vehicles
{
namespace Engines
{

// BrnLoopModelEffect.h:39 (DWARF). Reuses the BrnEffectObject dual base by name; the
// virtual `scalar deleting destructor' @ 0x826AFBA0 runs the inherited
// BrnEffectObject teardown (both vptr settles + the attach/detach/resources-ready
// member clears).
struct LoopModelEffect : public BrnSound::Logic::BrnEffectObject
{
    LoopModelEffect() {}
    virtual ~LoopModelEffect();
};

} // namespace Engines
} // namespace Vehicles
} // namespace BrnSound

#endif // BRN_SOUND_VEHICLES_ENGINES_BRN_LOOP_MODEL_EFFECT_H
