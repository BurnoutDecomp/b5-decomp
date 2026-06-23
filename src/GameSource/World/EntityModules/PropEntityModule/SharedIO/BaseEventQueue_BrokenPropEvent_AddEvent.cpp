#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"                        // BaseEventQueue<T>::AddEvent (inline generic)
#include "GameSource/World/EntityModules/PropEntityModule/BrnPropEntityModuleIO.h" // BrnWorld::PropEntityIO::BrokenPropEvent (1-byte element)

// CgsModule::BaseEventQueue<BrnWorld::PropEntityIO::BrokenPropEvent>::AddEvent @ 0x822C9838
//   (ledger id: class:BrnWorld::PropEntityIO::BrokenPropEvent>)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match). The
// generic BaseEventQueue<T>::AddEvent body is already committed inline in
// CgsBaseEventQueue.h; this is the thin explicit instantiation that forces the
// per-element out-of-line emission the X360 build emitted.
//
// X360 body (asm at 0x822C9838) matches the generic exactly -- an UNCONDITIONAL append
// guarded by two non-gating assert tripwires:
//   - assert mpEvents != NULL                        (CgsBaseEventQueue.h:312, `lwz r11,0(this)`)
//   - assert miLength < miMaxLength ("...Reached Max length", :313 -- the bge-to-assert
//     path falls through and still appends; not a gate)
//   - write the event: `lbz r10, 0(src); lwz idx,8(this); lwz buf,0(this); stbx r10, idx, buf`
//     -- a single BYTE copy to mpEvents[miLength] at stride 1 (no index scaling), confirming
//     sizeof(BrokenPropEvent) == 1.
//   - `++miLength` (`lwz r11,8(this); addi r11,r11,1; stw r11,8(this)`); returns 1 (true).
// The 1-byte stride is the same one EventQueue<BrokenPropEvent,50>::Construct @ 0x822E5060
// asserts (inline maEvents[50] at +0xC, no alignment pad).
//
// The Hex-Rays `int AddEvent(_DWORD*, _BYTE*)` prototype is the ABI shape of
// `bool AddEvent(this, const BrokenPropEvent&)`; the asm uses r3 (this) / r4 (event ref).
// Caller (X360): BrnWorld::PropEntityModule::ProcessContacts.
template bool
CgsModule::BaseEventQueue<BrnWorld::PropEntityIO::BrokenPropEvent>::AddEvent(
    const BrnWorld::PropEntityIO::BrokenPropEvent&);
