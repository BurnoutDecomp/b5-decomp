#include "SDKs/Realmc/RealmcIfaceMessageSetAutosaveDone.h"

// ===========================================================================
// RealmcIface::MessageSetAutosaveDone -- reconstructed from
// BURNOUT_X360_ARTIST.XEX.
//
// No leak source / no DWARF: SHAPE and BODY both come from the X360 asm. See
// RealmcIfaceMessageSetAutosaveDone.h for the dispatch layout (target vtable
// slot +0x18) and the class layout.
//
// Bodied here (2 ledger functions, class:RealmcIface::MessageSetAutosaveDone):
//   MessageSetAutosaveDone::Apply @0x82B56B90
//   MessageSetAutosaveDone::`vector deleting destructor` @0x82B56BB0
//     (compiler-generated from the virtual dtor + the class operator delete in
//      the header)
// ===========================================================================

namespace RealmcIface
{

// ---------------------------------------------------------------------------
// MessageSetAutosaveDone::Apply @ 0x82B56B90
//
//   mr r11,r4 ; mr r4,r3 ; mr r3,r11        -> swap: r3 = pTarget, r4 = pThis
//   lwz r10,0(r11) ; lwz r11,0x18(r10)      -> pTarget vtable slot +0x18 (6)
//   mtctr r11 ; bctr                        -> tail-call (pTarget, pThis)
//
// i.e. pTarget->OnSetAutosaveDone(this). As a non-static member the message is
// already `this` (r3) and the target arrives in r4, so no explicit swap is
// needed to express the same dispatch as the pseudocode `(*(*a2 + 24))(a2, a1)`.
// ---------------------------------------------------------------------------
int MessageSetAutosaveDone::Apply(IRealmcSetAutosaveDoneTarget* lpTarget)
{
    return lpTarget->OnSetAutosaveDone(this);
}

} // namespace RealmcIface
