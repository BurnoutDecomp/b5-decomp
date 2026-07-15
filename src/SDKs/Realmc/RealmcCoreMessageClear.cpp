#include "SDKs/Realmc/RealmcCoreMessageClear.h"

// ===========================================================================
// RealmcCore::MessageClear -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// No leak source / no DWARF: SHAPE and BODY both come from the X360 asm. See
// RealmcCoreMessageClear.h for the dispatch layout (target vtable slot +0x44),
// the class layout (no own members, 8-byte base), and the note on why the class
// is left abstract (the IRunnableTask task-body virtuals are not attested here).
//
// Bodied here (2 ledger functions, class:RealmcCore::MessageClear):
//   MessageClear::Apply @0x82B55370
//   MessageClear::`scalar deleting destructor' @0x82B554B8 (compiler-generated
//     from the virtual dtor + the class operator delete in the header)
// ===========================================================================

namespace RealmcCore
{

// ---------------------------------------------------------------------------
// MessageClear::Apply @ 0x82B55370
//
//   mr r11,r4 ; mr r4,r3 ; mr r3,r11        -> swap: r3 = pTarget, r4 = pThis
//   lwz r10,0(r11) ; lwz r11,0x44(r10)      -> pTarget vtable slot +0x44 (17)
//   mtctr r11 ; bctr                        -> tail-call (pTarget, pThis)
//
// i.e. pTarget->OnClear(this). As a non-static member the message is already
// `this` (r3) and the target arrives in r4, so no explicit swap is needed to
// express the same dispatch as the pseudocode `(*(*a2 + 68))(a2, a1)`.
// ---------------------------------------------------------------------------
int MessageClear::Apply(IRealmcClearTarget* pTarget)
{
    return pTarget->OnClear(this);
}

} // namespace RealmcCore
