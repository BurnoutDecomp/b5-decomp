#include "GameSource/Sound/Module/LogicModule/BrnSubmixesEffect.h"

// =============================================================================
// BrnSound::Logic::SubmixesEffect -- out-of-line deleting-destructor body.
// Reconstructed from BURNOUT_X360_ARTIST.XEX. See BrnSubmixesEffect.h for the
// inheritance rationale and the X360-32-bit-vs-host-64-bit offset note.
//
// ~SubmixesEffect  @ 0x826BC608  (the X360 `scalar deleting destructor')
//   stw  &off_820AE9BC, 0(this)        ; primary vptr settle (SubmixesEffect vtable)
//   stw  &off_820AE988, 4(this)        ; IResourceRequester sub-object vptr (intermediate)
//   stw  3,            0x28(this)      ; meDetachState = E_DETACH_STATE_FINISHED
//   stw  &off_820AA820, 4(this)        ; IResourceRequester sub-object vptr (final settle,
//                                        the shared CgsSound::MemBase base vtable)
//   stb  0,            0x31(this)      ; mbResourcesReady = false
//   stw  0,            0x24(this)      ; meAttachState = E_ATTACH_STATE_NONE
//   if (a2 & 1) { deallocate via off_82FFB954 (the global sound allocator) }
//   return this
//
// The `vector deleting destructor'`adjustor{4}' @ 0x826BC600 does `this - 4` (to
// recover the primary object from the IResourceRequester sub-object) and tail-calls
// the scalar deleting destructor above; that sub-object adjustment is the
// compiler-synthesised MI thunk, so it is not reproduced as source.
//
// The dual-vptr settle (+0/+4) and the attach/detach/resources-ready member clears
// (+0x24/+0x28/+0x31) are the inherited BrnEffectObject teardown the compiler emits
// (identical store-for-store to the committed sibling leaves LoopModelEffect
// @ 0x826AFBA0 and SingleGinsuEffect @ 0x826AFAF0); this leaf destructor body adds
// nothing of its own.
// FLAG: the (a2 & 1) tail invokes the global sound allocator (off_82FFB954) to free
// the object; that allocator is not homed here, so operator-delete dispatch is left
// to the host toolchain (the `delete` half of the X360 deleting destructor) rather
// than reproducing the raw allocator vtable call.
// =============================================================================

namespace BrnSound
{
namespace Logic
{

SubmixesEffect::~SubmixesEffect()
{
}

} // namespace Logic
} // namespace BrnSound
