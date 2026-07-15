#include "SDKs/Realmc/RealmcIfaceMessageSetActiveCardDone.h"

// ===========================================================================
// RealmcIface::MessageSetActiveCardDone -- reconstructed from
// BURNOUT_X360_ARTIST.XEX.
//
// No leak source / no DWARF: SHAPE and BODY both come from the X360 asm. See
// RealmcIfaceMessageSetActiveCardDone.h for the dispatch layout (target vtable
// slot +0x04) and the class layout.
//
// Bodied here (2 ledger functions, class:RealmcIface::MessageSetActiveCardDone):
//   MessageSetActiveCardDone::Apply @0x82B54F38
//   MessageSetActiveCardDone::`vector deleting destructor` @0x82B54F58
//     (compiler-generated from the virtual dtor + the class operator delete in
//      the header)
// ===========================================================================

namespace RealmcIface
{

// ---------------------------------------------------------------------------
// MessageSetActiveCardDone::Apply @ 0x82B54F38
//
//   mr r11,r4 ; mr r4,r3 ; mr r3,r11        -> swap: r3 = pTarget, r4 = pThis
//   lwz r10,0(r11) ; lwz r11,4(r10)         -> pTarget vtable slot +0x04 (1)
//   mtctr r11 ; bctr                        -> tail-call (pTarget, pThis)
//
// i.e. pTarget->OnSetActiveCardDone(this). As a non-static member the message is
// already `this` (r3) and the target arrives in r4, so no explicit swap is
// needed to express the same dispatch as the pseudocode `(*(*a2 + 4))(a2, a1)`.
// ---------------------------------------------------------------------------
int MessageSetActiveCardDone::Apply(IRealmcSetActiveCardDoneTarget* lpTarget)
{
    return lpTarget->OnSetActiveCardDone(this);
}

} // namespace RealmcIface
