#include "SharedClasses/DataLists/BrnPopupController.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnResource::PopupController::GetIndexFromPopupHash  @ 0x8267D6C0
//
// Construct, GetPopup (@0x8267EB98) and AddPopupResource (@0x82678D88) are declared in
// the header but bodied by their own TUs; they are intentionally not defined here.

namespace BrnResource
{
// @ 0x8267D6C0 -- linear-scan the loaded popup table for the record whose 64-bit
// name-id hash matches lPopupId; -1 when the table is empty or the hash is absent.
// The not-loaded guard (cpp:142) is non-gating.
s32 PopupController::GetIndexFromPopupHash(CgsID lPopupId) const
{
    CGS_ASSERT(mbIsPopupLoaded, "Trying to use a popup resource before it is loaded");   // cpp:142

    s32 liIndex = 0;                              // cpp:144
    const s32 liTotal = mPopupsPtr->miPopupCount; // cpp:145 (s16 sign-extended)
    if (liTotal <= 0)
        return -1;

    s32 liEntry = 0;
    while (mPopupsPtr->mppPopupData[liEntry]->mNameId != lPopupId)
    {
        ++liIndex;
        ++liEntry;
        if (liIndex >= liTotal)
            return -1;
    }
    return liIndex;
}
}
