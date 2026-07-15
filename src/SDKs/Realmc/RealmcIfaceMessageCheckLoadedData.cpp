#include "SDKs/Realmc/RealmcIfaceMessageCheckLoadedData.h"

// ===========================================================================
// RealmcIface::MessageCheckLoadedData -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// No leak source / no DWARF: SHAPE and BODY both come from the X360 asm. See
// RealmcIfaceMessageCheckLoadedData.h for the dispatch layout (target vtable slot
// +0x10) and the class layout. Mirrors RealmcIface::MessageBootupDone.
//
// Bodied here (2 ledger functions, class:RealmcIface::MessageCheckLoadedData):
//   MessageCheckLoadedData::Apply @0x82B56E78
//   MessageCheckLoadedData::`scalar deleting destructor` @0x82B56E98
//     (compiler-generated from the virtual dtor + the class operator delete in
//      the header)
// ===========================================================================

namespace RealmcIface
{

// ---------------------------------------------------------------------------
// MessageCheckLoadedData::Apply @ 0x82B56E78
//
//   mr r11,r4 ; mr r4,r3 ; mr r3,r11        -> swap: r3 = lpTarget, r4 = lpThis
//   lwz r10,0(r11) ; lwz r11,0x10(r10)      -> lpTarget vtable slot +0x10 (slot 4)
//   mtctr r11 ; bctr                        -> tail-call (lpTarget, lpThis)
//
// i.e. lpTarget->OnCheckLoadedData(this). As a non-static member the message is
// already `this` (r3) and the target arrives in r4, so no explicit swap is needed
// to express the same dispatch as the pseudocode `(*(*a2 + 16))(a2, a1)`.
// ---------------------------------------------------------------------------
int MessageCheckLoadedData::Apply(IRealmcCheckLoadedDataTarget* lpTarget)
{
    return lpTarget->OnCheckLoadedData(this);
}

} // namespace RealmcIface
