#include "SDKs/Realmc/RealmcMessage.h"

// ===========================================================================
// RealmcIface::MessageShowAutosaveIcon -- reconstructed from
// BURNOUT_X360_ARTIST.XEX.
//
// No leak source / no DWARF: SHAPE and BODY both come from the X360 asm. See
// RealmcMessage.h for the dispatch layout (target vtable slot +0x40). The body
// is the same one-line virtual dispatch idiom as RealmcCore::Message::Apply
// (@0x82C44C08, RealmcCore.cpp), differing only in the dispatched slot.
// ===========================================================================

namespace RealmcIface
{

// ---------------------------------------------------------------------------
// MessageShowAutosaveIcon::Apply @ 0x82B552A0
//
//   mr r11, r4 ; mr r4, r3 ; mr r3, r11   -> swap: r3 = pTarget, r4 = pThis
//   lwz r10, 0(r11) ; lwz r11, 0x40(r10)  -> pTarget vtable slot +0x40 (16)
//   mtctr r11 ; bctr                       -> tail-call (pTarget, pThis)
//
// i.e. pTarget->ShowAutosaveIcon(pThis). IDA's signature lists (a1=pThis,
// a2=pTarget); the asm swaps them so the *target* is `this` for the dispatch,
// exactly like RealmcCore::Message::Apply.
// ---------------------------------------------------------------------------
int MessageShowAutosaveIcon::Apply(MessageShowAutosaveIcon* pThis,
                                   IRealmcAutosaveTarget* pTarget)
{
    return pTarget->ShowAutosaveIcon(pThis);
}

} // namespace RealmcIface
