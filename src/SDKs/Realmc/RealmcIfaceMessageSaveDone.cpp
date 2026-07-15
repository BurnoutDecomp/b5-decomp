#include "SDKs/Realmc/RealmcIfaceMessageSaveDone.h"

// ===========================================================================
// RealmcIface::MessageSaveDone -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// No leak source / no DWARF: SHAPE and BODY both come from the X360 asm. See
// RealmcIfaceMessageSaveDone.h for the dispatch layout (target vtable slot
// +0x34) and the class layout, and for the note that this TU has no ctor.
//
// Bodied here:
//   MessageSaveDone::Apply @0x82B56088
//   MessageSaveDone::`vector deleting destructor` @0x82B560A8 (compiler-generated
//     from the virtual dtor + the class operator delete in the header)
// ===========================================================================

namespace RealmcIface
{

// ---------------------------------------------------------------------------
// MessageSaveDone::Apply @ 0x82B56088
//
//   mr r11,r4 ; mr r4,r3 ; mr r3,r11        -> swap: r3 = pTarget, r4 = pThis
//   lwz r10,0(r11) ; lwz r11,0x34(r10)      -> pTarget vtable slot +0x34 (13)
//   mtctr r11 ; bctr                        -> tail-call (pTarget, pThis)
//
// i.e. pTarget->OnSaveDone(this). As a non-static member the message is already
// `this` (r3) and the target arrives in r4, so no explicit swap is needed to
// express the same dispatch as the pseudocode `(*(*a2 + 52))(a2, a1)`.
// ---------------------------------------------------------------------------
int MessageSaveDone::Apply(IRealmcSaveDoneTarget* pTarget)
{
    return pTarget->OnSaveDone(this);
}

} // namespace RealmcIface
