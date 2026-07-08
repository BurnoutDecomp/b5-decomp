#include "SDKs/Realmc/RealmcIfaceMessageLoadDone.h"

// ===========================================================================
// RealmcIface::MessageLoadDone -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// No leak source / no DWARF: SHAPE and BODY both come from the X360 asm. See
// RealmcIfaceMessageLoadDone.h for the dispatch layout (target vtable slot
// +0x30) and the class layout, and for the note on the BLOCKED ctor.
//
// Bodied here:
//   MessageLoadDone::Apply @0x82B570B8
//   MessageLoadDone::`vector deleting destructor` @0x82B570D8 (compiler-generated
//     from the virtual dtor + the class operator delete in the header)
// ===========================================================================

namespace RealmcIface
{

// ---------------------------------------------------------------------------
// MessageLoadDone::Apply @ 0x82B570B8
//
//   mr r11,r4 ; mr r4,r3 ; mr r3,r11        -> swap: r3 = pTarget, r4 = pThis
//   lwz r10,0(r11) ; lwz r11,0x30(r10)      -> pTarget vtable slot +0x30 (12)
//   mtctr r11 ; bctr                        -> tail-call (pTarget, pThis)
//
// i.e. pTarget->OnLoadDone(this). As a non-static member the message is already
// `this` (r3) and the target arrives in r4, so no explicit swap is needed to
// express the same dispatch as the pseudocode `(*(*a2 + 48))(a2, a1)`.
// ---------------------------------------------------------------------------
int MessageLoadDone::Apply(IRealmcLoadDoneTarget* pTarget)
{
    return pTarget->OnLoadDone(this);
}

} // namespace RealmcIface
