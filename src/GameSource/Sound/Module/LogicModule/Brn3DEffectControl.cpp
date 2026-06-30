#include "GameSource/Sound/Module/LogicModule/Brn3DEffectControl.h"

// =============================================================================
// BrnSound::Logic::Brn3DEffectControl — out-of-line bodies.
// Reconstructed from BURNOUT_X360_ARTIST.XEX. See Brn3DEffectControl.h for the
// minimal-owning-slice rationale and the X360-32-bit-vs-host-64-bit offset note.
//
// This TU's recon'd function set is exactly two entries (from the postmortem):
//   Brn3DEffectControl::Brn3DEffectControl            @ 0x826AF7F0
//   Brn3DEffectControl::`scalar deleting destructor'  @ 0x826C85F8
//
// The constructor is bodied INLINE in the header (it only constructs the single
// HOMED member, mEngineDataAtrib; the rest of its stores hit the DEFERRED
// Cgs3dEffectControl transform/emitter surface — see the FLAG there). The scalar
// deleting destructor is bodied here.
// =============================================================================

namespace BrnSound
{
namespace Logic
{

// ---------------------------------------------------------------------------
// Brn3DEffectControl::`scalar deleting destructor'  @ 0x826C85F8
//
//   addi r3, r31, 0xB0
//   bl   Attrib::Instance::~Instance        ; mEngineDataAtrib teardown (+0xB0)
//   li   r9, 3 ; stw r9, 0x24(r31)          ; meDetachState = E_DETACH_STATE_FINISHED
//   stw  off_820AA820, 0(r31)               ; vptr -> final base sub-object vtable
//   stb  0, 0x2D(r31)                        ; (un-homed byte flag) = 0
//   stw  0, 0x20(r31)                        ; meAttachState = E_ATTACH_STATE_NONE
//   if (a2 & 1) { ... deallocate via off_82FFB954 (the MemBase allocator) }
//   return this
//
// The leading addi+bl is the mEngineDataAtrib (Attrib::Instance) destructor at
// this+0xB0; in reconstructed C++ that teardown is produced by the member's own
// destructor in the destructor chain, so the BODY here is the observable
// EffectBase member teardown that maps to committed NAMED members. The vptr store
// (off_820AA820) is the compiler-emitted base sub-object devirtualization, emitted
// implicitly by the destructor chain.
//
// Store ORDER mirrors the asm: meDetachState (+0x24) first, then meAttachState
// (+0x20). (Single-inheritance chain here, so the EffectBase members sit 4 bytes
// earlier than in the IResourceRequester-bearing BrnEffectControl sibling; the
// by-NAME mapping is identical.)
//
// FLAG: the X360 `stb 0, 0x2D(r31)` clears a byte flag that has NO homed member in
// the minimal Cgs3dEffectControl chain (the resources-ready flag lives on the
// DEFERRED control surface, not on this owning slice); it is NOT fabricated here.
// FLAG: the (a2 & 1) tail invokes the global sound allocator (off_82FFB954) to free
// the object; that allocator is not homed here, so operator-delete dispatch is left
// to the host toolchain (the `delete` half of the X360 scalar deleting destructor)
// rather than reproducing the raw allocator vtable call (same treatment as the
// BrnEffectControl / BrnEffectObject sibling homes).
// ---------------------------------------------------------------------------
Brn3DEffectControl::~Brn3DEffectControl()
{
    meDetachState = CgsSound::Logic::EffectBase::E_DETACH_STATE_FINISHED; // stw 3, 0x24
    meAttachState = CgsSound::Logic::EffectBase::E_ATTACH_STATE_NONE;     // stw 0, 0x20
}

} // namespace Logic
} // namespace BrnSound
