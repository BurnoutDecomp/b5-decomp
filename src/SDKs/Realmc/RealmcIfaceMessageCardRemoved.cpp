#include "SDKs/Realmc/RealmcIfaceMessageCardRemoved.h"

// ===========================================================================
// RealmcIface::MessageCardRemoved -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// No leak source / no DWARF: SHAPE and BODY both come from the X360 asm. See
// RealmcIfaceMessageCardRemoved.h for the dispatch layout (target vtable slot
// +0x0C) and the class layout, and for the note on the abstract-here base.
//
// Bodied here:
//   MessageCardRemoved::Apply @0x82B51FF0
//   MessageCardRemoved::`vector deleting destructor` @0x82B52010 (compiler-
//     generated from the virtual dtor + the class operator delete in the header)
// ===========================================================================

namespace RealmcIface
{

// ---------------------------------------------------------------------------
// MessageCardRemoved::Apply @ 0x82B51FF0
//
//   mr r11,r4 ; mr r4,r3 ; mr r3,r11        -> swap: r3 = pTarget, r4 = pThis
//   lwz r10,0(r11) ; lwz r11,0xC(r10)       -> pTarget vtable slot +0x0C (3)
//   mtctr r11 ; bctr                        -> tail-call (pTarget, pThis)
//
// i.e. pTarget->OnCardRemoved(this). As a non-static member the message is
// already `this` (r3) and the target arrives in r4, so no explicit swap is
// needed to express the same dispatch as the pseudocode `(*(*a2 + 12))(a2, a1)`.
// ---------------------------------------------------------------------------
int MessageCardRemoved::Apply(IRealmcCardRemovedTarget* lpTarget)
{
    return lpTarget->OnCardRemoved(this);
}

} // namespace RealmcIface
